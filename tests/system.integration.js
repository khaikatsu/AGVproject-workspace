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
  const end = Date.now()+timeout;while (Date.now()<end) {if (await fn()) return;await pause(30);}
  throw new Error('Integration condition timed out');
}
async function freePort() {const s = net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const port = s.address().port;await new Promise(r=>s.close(r));return port;}
async function available(port) {return new Promise(resolve=>{const s = net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();resolve(true);});s.once('error',()=>resolve(false));});}
async function main() {
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory = fs.mkdtempSync(path.resolve(__dirname,'../.qa/system-test-'));
  const mqttPort = await freePort(),corePort = await freePort();
  const map = {map_name:'Isolated integration',nodes:[{id:'A',x:0,y:0,type:'charging'},{id:'B',x:0,y:20,type:'charging'},
    {id:'M',x:10,y:0,type:'mutex'},{id:'Q',x:20,y:0,type:'waypoint'},{id:'T',x:30,y:0,type:'station'}],
    edges:[{from:'A',to:'T',weight:300},{from:'B',to:'Q',weight:1},{from:'Q',to:'T',weight:1},{from:'A',to:'M',weight:10},{from:'M',to:'Q',weight:10}],obstacles:[]};
  const mapPath = path.join(directory,'map.json');fs.writeFileSync(mapPath,JSON.stringify(map));
  const config = path.join(directory,'broker.conf');fs.writeFileSync(config,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin = process.env.AGV_MOSQUITTO_BIN || (process.platform === 'win32' ? 'C:/msys64/ucrt64/bin' : '/usr/bin');
  const executable = process.argv[2] ? path.resolve(process.argv[2]) : process.env.AGV_CORE_BIN || path.resolve(__dirname,'../build/agv_core.exe');
  const env = {...process.env,AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1',AGV_MAP_PATH:mapPath};
  const pathKey = Object.keys(env).find(k=>k.toLowerCase() === 'path') || 'PATH';env[pathKey] = bin+path.delimiter+env[pathKey];
  let output = '',gateway,mqtt,heartbeat,core,broker;const children = [];
  function child(command,args) {
    const p = spawn(command,args,{env,cwd:directory,windowsHide:true});children.push(p);
    p.stdout.on('data',data=>output += data);p.stderr.on('data',data=>output += data);p.on('error',error=>output += error.message);return p;
  }
  const db = createDatabase(path.join(directory,'test.db'));
  try {
    broker = child(path.join(bin,process.platform === 'win32' ? 'mosquitto.exe' : 'mosquitto'),['-c',config]);
    await until(()=>available(mqttPort));core = child(executable,[]);await until(()=>available(corePort));
    gateway = createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:100});
    const address = await gateway.start(0,'127.0.0.1'),base = `http://127.0.0.1:${address.port}`;
    async function api(method,route,body) {
      const response = await fetch(base+route,{method,headers:body ? {'Content-Type':'application/json'} : undefined,body:body ? JSON.stringify(body) : undefined});
      return {status:response.status,data:await response.json()};
    }
    const state = async ()=>(await api('GET','/api/state')).data;
    await until(async ()=>(await state()).core_ready);
    assert.equal((await api('POST','/api/map/snapshots',{name:'Integration snapshot'})).status,201);
    const snapshots = (await api('GET','/api/map/snapshots')).data;assert.equal(snapshots.length,1);
    assert.equal((await api('POST',`/api/map/snapshots/${snapshots[0].id}/apply`,{})).status,200);
    assert.equal((await api('POST','/api/map/save',{})).status,200);
    assert.ok(Array.isArray(JSON.parse(fs.readFileSync(mapPath)).obstacles));
    const old = JSON.stringify((await state()).map);
    const bad = {...map,edges:[{from:'A',to:'missing',weight:1}]};const badId = db.saveMapSnapshot(bad,'invalid');
    assert.equal((await api('POST',`/api/map/snapshots/${badId}/apply`,{})).status,400);
    assert.equal(JSON.stringify((await state()).map),old);
    const one = await api('POST','/api/tasks',{target:'T'}),two = await api('POST','/api/tasks',{target:'T'});
    assert.equal(one.status,200);assert.notEqual(one.data.task_id,two.data.task_id);
    assert.equal((await api('POST','/api/tasks/cancel',{task_id:one.data.task_id})).data.cancelled,1);
    assert.equal((await api('GET','/api/tasks')).data.length,1);
    await api('POST','/api/tasks/cancel',{task_id:two.data.task_id});
    mqtt = new MqttFixture(mqttPort);mqtt.on('error',error=>output += error.message);await once(mqtt,'connected');
    mqtt.subscribe('agv/+/command');await once(mqtt,'subscribed');
    const positions = new Map([['01','A'],['02','B']]);
    mqtt.on('publish',(topic,body)=>{
      const command = JSON.parse(body),robot = topic.split('/')[1];mqtt.publish(`agv/${robot}/ack`,String(command.cmd_id));
    });
    const telemetry = ()=>{for (const [robot,node] of positions) mqtt.publish(`agv/${robot}/telemetry`,`${node}|80|0`);};
    heartbeat = setInterval(telemetry,300);telemetry();
    await until(async ()=>(await state()).fleet.agvs.length === 2);
    mqtt.publish('agv/01/telemetry','A|abc|0');await pause(150);
    assert.equal((await state()).fleet.agvs.find(r=>r.id === '01').state,'IDLE');
    assert.equal((await state()).fleet.agvs.find(r=>r.id === '01').battery,80);
    const weighted = await api('POST','/api/tasks',{target:'T'});assert.equal(weighted.status,200);
    let rows = (await api('GET','/api/tasks/history')).data;
    assert.equal(rows.find(r=>r.core_task_id === weighted.data.task_id).assigned_agv,'02');
    positions.set('02','T');telemetry();
    await until(async ()=>(await api('GET','/api/tasks/history')).data.find(r=>r.core_task_id === weighted.data.task_id).status === 'completed');
    assert.equal((await api('POST',`/api/map/snapshots/${snapshots[0].id}/apply`,{})).status,409);
    assert.equal((await api('POST','/api/agvs/missing/goto',{target:'T'})).status,409);
    const interrupted = await api('POST','/api/agvs/01/goto',{target:'Q'});assert.equal(interrupted.status,200);
    mqtt.publish('agv/01/error','Motor test fault');
    await until(async ()=>(await api('GET','/api/tasks/history')).data.find(r=>r.core_task_id === interrupted.data.task_id).status === 'interrupted');
    assert.equal((await api('POST','/api/agvs/01/clear-error',{})).status,200);
    positions.set('01','M');telemetry();await pause(100);
    const lost = await api('POST','/api/agvs/01/goto',{target:'Q'});assert.equal(lost.status,200);
    mqtt.publish('agv/01/mutex_request','M');await until(async ()=>(await state()).mutex.M.owner === '01');
    positions.delete('01');
    await until(async ()=>(await state()).fleet.agvs.find(r=>r.id === '01').state === 'OFFLINE',13000);
    assert.equal((await state()).mutex.M.owner,'01');
    assert.equal((await api('GET','/api/tasks/history')).data.find(r=>r.core_task_id === lost.data.task_id).status,'interrupted');
    mqtt.publish('agv/02/mutex_request','M');await pause(200);assert.equal((await state()).mutex.M.owner,'01');
    const report = await api('POST','/api/kpi/generate',{});assert.equal(report.data.report.completed_in_period,1);
    broker.kill();await until(async ()=>(await state()).mqtt_connected === false);
    assert.equal((await api('POST','/api/tasks',{target:'Q'})).status,503);
    console.log('PASS real integration: REST → gateway → C++ → isolated MQTT, snapshots/apply/save/reject, exact task cancellation, weighted assignment, telemetry completion, robot error/offline and zone retention');
    console.log('Artifacts: '+directory);
  } catch (error) {console.error(output.slice(-8000));throw error;}
  finally {
    clearInterval(heartbeat);mqtt?.close();if (gateway) await gateway.close();
    for (const child of children) if (child.exitCode == null) child.kill();
    db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
