// Kiểm chứng bản đồ v3 với core/broker thật và telemetry robot giả lập.
const assert = require('assert/strict');
const fs = require('fs');
const path = require('path');
const net = require('net');
const {spawn} = require('child_process');
const {once} = require('events');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout = 5000) {
  const end = Date.now()+timeout;
  while (Date.now()<end) {if (await fn()) return;await pause(40);}
  throw new Error('Warehouse integration condition timed out');
}
async function freePort() {
  const server = net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
  const port = server.address().port;await new Promise(resolve=>server.close(resolve));return port;
}
async function main() {
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory = fs.mkdtempSync(path.resolve(__dirname,'../.qa/map-test-'));
  const mqttPort = await freePort(),corePort = await freePort();
  const mapPath = path.join(directory,'map.json');fs.copyFileSync(path.resolve(__dirname,'../data/map_layout.json'),mapPath);
  const brokerPath = path.join(directory,'broker.conf');
  fs.writeFileSync(brokerPath,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin = process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env = {...process.env,AGV_MAP_PATH:mapPath,AGV_COMMAND_ID_PATH:path.join(directory,'counter.json'),
    AGV_HOME_STATIONS:'{}', // ca kiểm tra bản đồ riêng; luồng ca nằm trong shift.integration.js
    AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  const pathKey = Object.keys(env).find(key=>key.toLowerCase()==='path') || 'PATH';
  env[pathKey] = bin+path.delimiter+env[pathKey];
  const children = [];let output = '',gateway,mqtt,heartbeat;
  const db = createDatabase(path.join(directory,'test.db'));
  function child(command,args) {
    const process = spawn(command,args,{env,cwd:directory,windowsHide:true});children.push(process);
    process.stdout.on('data',data=>output+=data);process.stderr.on('data',data=>output+=data);
    process.on('error',error=>output+=error.message);return process;
  }
  try {
    child(path.join(bin,'mosquitto.exe'),['-c',brokerPath]);
    child(process.env.AGV_CORE_BIN || path.resolve(__dirname,'../build/agv_core.exe'),[]);
    gateway = createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:100});
    const address = await gateway.start(0,'127.0.0.1'),base = `http://127.0.0.1:${address.port}`;
    const state = async ()=>(await fetch(base+'/api/state')).json();
    async function api(route,body) {
      const response = await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      return {status:response.status,data:await response.json()};
    }
    await until(async ()=>{const s=await state();return s.core_ready && s.mqtt_connected;});
    const initial = await state();assert.equal(initial.map.nodes.length,22);
    assert.equal(initial.map.edges.filter(e=>e.geometry).length,4);
    assert.deepEqual(Object.keys(initial.mutex).sort(),['M1','M2','M3','M4','M5','M6']);
    assert.equal((await api('/api/map/save',{})).status,200);
    assert.equal(JSON.parse(fs.readFileSync(mapPath,'utf8')).edges.filter(e=>e.geometry).length,4);
    const snapshots=await (await fetch(base+'/api/map/snapshots')).json();
    assert.equal((await api(`/api/map/snapshots/${snapshots[0].id}/apply`,{})).status,200);
    assert.equal((await state()).map.edges.filter(e=>e.geometry).length,4);
    assert.equal((await api('/api/tasks',{target:'KHO_4'})).status,404);
    const commands = [];
    mqtt = new MqttFixture(mqttPort);mqtt.on('error',error=>output+=error.message);await once(mqtt,'connected');
    mqtt.subscribe('agv/+/command');await once(mqtt,'subscribed');
    mqtt.on('publish',(topic,payload)=>{
      const command = JSON.parse(payload);commands.push({robot:topic.split('/')[1],...command});
      mqtt.publish(`agv/${topic.split('/')[1]}/ack`,String(command.cmd_id));
    });
    const positions = new Map([['01','G1'],['02','K2']]);
    const telemetry = ()=>{for (const [robot,node] of positions) mqtt.publish(`agv/${robot}/telemetry`,`${node}|80|0`);};
    heartbeat = setInterval(telemetry,250);telemetry();
    await until(async ()=>(await state()).fleet.agvs.length===2);
    const task = await api('/api/agvs/01/goto',{target:'K4'});assert.equal(task.status,200);
    await until(()=>commands.some(c=>c.cmd==='GOTO'));
    const command = commands.find(c=>c.cmd==='GOTO');
    assert.deepEqual(command.path,['M5','M6','G4','K4']);assert.equal(command.target,'K4');
    for (const zone of ['M5','M6']) {
      mqtt.publish('agv/01/mutex_request',zone);
      await until(async ()=>(await state()).mutex[zone].owner==='01');
      mqtt.publish('agv/02/mutex_request',zone);
      await until(async ()=>(await state()).mutex[zone].waiting.includes('02'));
      mqtt.publish('agv/02/mutex_release',JSON.stringify({zone,grant_id:(await state()).mutex[zone].grant_id}));await pause(100);
      assert.equal((await state()).mutex[zone].owner,'01');
      positions.set('01',zone);telemetry();
      await until(async ()=>(await state()).fleet.agvs.find(a=>a.id==='01').node===zone);
      const s=await state();assert.equal(s.fleet.tasks.find(t=>t.task_id===task.data.task_id).status,'dispatched');
    }
    // Fixture cung cấp vị trí sau khi rời vùng; không giả lập kích thước/thân robot.
    positions.set('01','G4');telemetry();
    await until(async ()=>(await state()).fleet.agvs.find(a=>a.id==='01').node==='G4');
    for (const zone of ['M5','M6']) {
      mqtt.publish('agv/01/mutex_release',JSON.stringify({zone,grant_id:(await state()).mutex[zone].grant_id}));
      await until(async ()=>(await state()).mutex[zone].owner==='02');
      mqtt.publish('agv/02/mutex_release',JSON.stringify({zone,grant_id:(await state()).mutex[zone].grant_id}));
      await until(async ()=>(await state()).mutex[zone].owner==='');
    }
    positions.set('01','K4');telemetry();
    await until(async ()=>(await state()).fleet.tasks.find(t=>t.task_id===task.data.task_id).status==='completed');
    assert.equal(db.getTaskHistory(10).find(t=>t.core_task_id===task.data.task_id).status,'completed');
    console.log('PASS warehouse MQTT: compact IDs, six zones, GOTO through M5/M6, two-robot queue/ownership/release, intermediate telemetry and exact task completion');
    console.log('Artifacts: '+directory);
  } catch (error) {console.error(output.slice(-6000));throw error;}
  finally {
    clearInterval(heartbeat);mqtt?.close();if (gateway) await gateway.close();
    for (const process of children) if (process.exitCode==null) process.kill();db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
