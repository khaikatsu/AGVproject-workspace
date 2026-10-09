// Core/gateway/broker thật + .ino được biên dịch host; WiFi/Serial MCU là adapter.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const {spawn} = require('node:child_process');
const {once} = require('node:events');
const readline = require('node:readline');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');
const tags = require('./fixtures/rfid-tags.json');
const root = path.resolve(__dirname,'..');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout = 6000) {
  const end = Date.now()+timeout;
  while (Date.now()<end) {if (await fn()) return;await pause(40);}
  throw new Error('Sketch/core integration timed out');
}
async function freePort() {
  const server=net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
  const port=server.address().port;await new Promise(resolve=>server.close(resolve));return port;
}
async function available(port) {
  return new Promise(resolve=>{const socket=net.connect(port,'127.0.0.1');
    socket.once('connect',()=>{socket.destroy();resolve(true);});socket.once('error',()=>resolve(false));});
}
async function main() {
  const probePath=process.env.AGV_SKETCH_PROBE || path.join(root,'.qa/firmware-host-build/sketch_probe.exe');
  assert.ok(fs.existsSync(probePath),'Chạy scripts/test-firmware.ps1 trước');
  const directory=fs.mkdtempSync(path.join(root,'.qa/firmware-test-'));
  const mqttPort=await freePort(),corePort=await freePort();
  const mapPath=path.join(directory,'map.json');fs.copyFileSync(path.join(root,'data/map_layout.json'),mapPath);
  const brokerPath=path.join(directory,'broker.conf');
  fs.writeFileSync(brokerPath,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin=process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env={...process.env,AGV_HOME_STATIONS:'{}',AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),
    AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(corePort),AGV_MAP_PATH:mapPath,
    AGV_COMMAND_ID_PATH:path.join(directory,'counter.json')};
  const pathKey=Object.keys(env).find(key=>key.toLowerCase()==='path') || 'PATH';
  env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],events=[],commands=[],acknowledgements=[];
  const db=createDatabase(path.join(directory,'test.db'));
  let gateway,mqtt,probe,heartbeat,output='',latestStats;
  function child(command,args=[]) {
    const process=spawn(command,args,{env,cwd:directory,windowsHide:true});children.push(process);
    process.stderr.on('data',data=>output+=data);process.on('error',error=>output+=error.message);return process;
  }
  const drive = event=>probe.stdin.write(JSON.stringify(event)+'\n');
  function scan(node) {
    const nodes=JSON.parse(fs.readFileSync(mapPath,'utf8')).nodes;
    assert.ok(nodes.some(n=>n.id===node) && tags[node],`Unknown RFID node ${node}`);
    drive({type:'rfid',uid:tags[node]});
  }
  try {
    child(path.join(bin,'mosquitto.exe'),['-c',brokerPath]);await until(()=>available(mqttPort));
    child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'));
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:100});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async ()=>(await fetch(base+'/api/state')).json();
    async function api(route,body) {
      const response=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      return {status:response.status,data:await response.json()};
    }
    await until(async ()=>{const value=await state();return value.core_ready && value.mqtt_connected;});
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',error=>output+=error.message);await once(mqtt,'connected');
    mqtt.subscribe('agv/01/#');await once(mqtt,'subscribed');
    mqtt.publish('agv/02/telemetry','S2|?|0');
    await until(async ()=>(await state()).fleet.agvs.some(robot=>
      robot.id==='02' && robot.node==='S2' && robot.position_valid && !robot.battery_measured));
    probe=child(probePath);
    readline.createInterface({input:probe.stdout}).on('line',line=>{
      const event=JSON.parse(line);events.push(event);
      if (event.type==='publish') {assert.equal(event.retained,false);mqtt.publish(event.topic,event.payload);}
      else if (event.type==='stats') latestStats=event;
    });
    mqtt.on('publish',(topic,payload)=>{
      if (topic==='agv/01/command') {
        commands.push(JSON.parse(payload));drive({type:'mqtt',topic,payload});
      } else if (topic==='agv/01/mutex_grant') drive({type:'mqtt',topic,payload});
      else if (topic==='agv/01/ack') acknowledgements.push(payload);
    });
    drive({type:'serial',line:'ack off'});
    heartbeat=setInterval(()=>drive({type:'telemetry'}),1000);drive({type:'telemetry'});
    await until(async ()=>(await state()).fleet.agvs.some(robot=>robot.id==='01' && robot.state==='UNLOCALIZED' && !robot.position_valid));
    scan('G1');
    await until(async ()=>(await state()).fleet.agvs.some(robot=>robot.id==='01' && robot.node==='G1'));
    const task=await api('/api/agvs/01/goto',{target:'K4'});assert.equal(task.status,200);
    await until(()=>commands.filter(command=>command.cmd==='GOTO').length>=2);
    const go=commands.find(command=>command.cmd==='GOTO');
    assert.deepEqual(go.path,['M5','M6','G4','K4']);
    assert.ok(commands.filter(command=>command.cmd==='GOTO').every(command=>command.cmd_id===go.cmd_id));
    await until(()=>latestStats?.duplicates>=1);
    assert.equal(latestStats.accepted,new Set(commands.map(command=>command.cmd_id)).size);
    assert.equal(latestStats.node,'G1');
    drive({type:'serial',line:'ack on'});
    await until(()=>acknowledgements.includes(String(go.cmd_id)),5000);
    const copiesAfterAck=commands.filter(command=>command.cmd_id===go.cmd_id).length;
    await pause(3800);
    assert.equal(commands.filter(command=>command.cmd_id===go.cmd_id).length,copiesAfterAck,'ACK phải dừng resend');
    assert.equal((await state()).fleet.tasks.find(row=>row.task_id===task.data.task_id).status,'dispatched','ACK không hoàn thành task');
    for (const [zone,nextNode] of [['M5','M6'],['M6','G4']]) {
      drive({type:'serial',line:'request '+zone});
      await until(async ()=>(await state()).mutex[zone].owner==='01');
      await until(()=>latestStats?.held_zones & (1<<(zone==='M5'?4:5)));
      scan(zone);
      await until(async ()=>(await state()).fleet.agvs.find(robot=>robot.id==='01').node===zone);
      scan(nextNode);
      await until(async ()=>(await state()).fleet.agvs.find(robot=>robot.id==='01').node===nextNode);
      drive({type:'serial',line:'release '+zone});
      await until(async ()=>(await state()).mutex[zone].owner==='');
    }
    scan('K4');
    await until(async ()=>(await state()).fleet.tasks.find(row=>row.task_id===task.data.task_id).status==='completed');
    assert.equal(db.getTaskHistory(10).find(row=>row.core_task_id===task.data.task_id).status,'completed');
    const cancelTask=await api('/api/agvs/01/goto',{target:'K5'});assert.equal(cancelTask.status,200);
    await until(()=>commands.some(command=>command.target==='K5' && latestStats?.last_id===command.cmd_id));
    assert.equal((await api('/api/tasks/cancel',{task_id:cancelTask.data.task_id})).status,200);
    await until(()=>commands.some(command=>command.cmd==='WAIT' && command.cmd_id>go.cmd_id && acknowledgements.includes(String(command.cmd_id))));
    drive({type:'serial',line:'error TEST_STOP'});
    await until(()=>commands.some(command=>command.cmd==='STOP' && acknowledgements.includes(String(command.cmd_id))));
    fs.writeFileSync(path.join(directory,'events.json'),JSON.stringify({commands,acknowledgements,events},null,2));
    console.log('PASS sketch/core MQTT: boot UNKNOWN/UNLOCALIZED, RC522 UID scans, .ino GOTO/STOP/ACK/retry/dedup, ACK not completion, M5/M6 grant/release');
    console.log('Artifacts: '+directory);
  } catch (error) {console.error(output.slice(-7000));throw error;}
  finally {
    clearInterval(heartbeat);mqtt?.close();if (gateway) await gateway.close();
    for (const process of children) if (process.exitCode===null && process.signalCode===null) process.kill();
    db.close();fs.writeFileSync(path.join(directory,'host-serial.log'),output);
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
