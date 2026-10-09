// Isolated real Core/MQTT; software evidence only, no robot hardware.
const assert = require('node:assert/strict');
const fs = require('node:fs'),path = require('node:path'),net = require('node:net');
const {spawn} = require('node:child_process'),{once} = require('node:events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {createGateway} = require('../AGVwebserver/gateway');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');
const pause = ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,timeout = 7000) {const end=Date.now()+timeout;while(Date.now()<end){if(await fn())return;await pause(30);}throw new Error('Hardening condition timed out');}
async function freePort() {const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const port=s.address().port;await new Promise(r=>s.close(r));return port;}
async function main() {
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.join(root,'.qa/hardening-test-')),mqttPort=await freePort(),corePort=await freePort();
  const map = {nodes:[{id:'M',type:'mutex',x:1,y:0},{id:'T',type:'station',x:2,y:0}],edges:[{from:'M',to:'T',weight:1}]};
  const ids = Array.from({length:8},(_,i)=>'r'+(i+1));
  for (const [i,id] of ids.entries()) {map.nodes.push({id:'A'+id,type:'waypoint',x:0,y:i});map.edges.push({from:'A'+id,to:'M',weight:1});}
  const mapPath=path.join(dir,'map.json');fs.writeFileSync(mapPath,JSON.stringify(map));
  const config=path.join(dir,'broker.conf');fs.writeFileSync(config,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin=process.env.AGV_MOSQUITTO_BIN || (process.platform==='win32' ? 'C:/msys64/ucrt64/bin' : '/usr/bin');
  const exe=process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe');
  const coreToken='isolated-core-token-'.repeat(3),env={...process.env,AGV_HOME_STATIONS:'{}',AGV_MAP_PATH:mapPath,AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(corePort),AGV_MQTT_PORT:String(mqttPort),AGV_MQTT_HOST:'127.0.0.1',AGV_CORE_TOKEN:coreToken};
  const children=[],peers=[],db=createDatabase(path.join(dir,'test.db'));let log='',core,gateway,mqtt,pump;
  function child(command,args=[]) {const p=spawn(command,args,{cwd:dir,env,windowsHide:true});children.push(p);p.on('error',e=>log+=e.message);p.stdout.on('data',d=>log+=d);p.stderr.on('data',d=>log+=d);return p;}
  try {
    child(path.join(bin,process.platform==='win32' ? 'mosquitto.exe' : 'mosquitto'),['-c',config,'-v']);
    await pause(250);core=child(exe);
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,coreToken,reconnectMs:100});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async()=>{const s=await (await fetch(base+'/api/state')).json();if(s.storage_healthy===false)throw new Error('Core storage failed: '+JSON.stringify(s.logs));return s;};
    const api=async(route,body)=>{const r=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return {status:r.status,data:await r.json()};};
    await until(async()=>(await state()).core_ready);
    const denied=new WebSocket(`ws://127.0.0.1:${corePort}`);peers.push(denied);denied.on('error',()=>{});
    const [,response]=await once(denied,'unexpected-response');assert.equal(response.statusCode,401);response.resume();denied.terminate();
    const monitor=new WebSocket(`ws://127.0.0.1:${corePort}`,{headers:{Authorization:`Bearer ${coreToken}`}});peers.push(monitor);
    const frames=[];monitor.on('error',()=>{});monitor.on('message',raw=>frames.push(JSON.parse(raw)));await once(monitor,'open');
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>log+=e.message);await once(mqtt,'connected');
    const grants=[];mqtt.on('publish',(topic,payload)=>{
      if(topic.endsWith('/command')) mqtt.publish(topic.replace('/command','/ack'),String(JSON.parse(payload).cmd_id));
      if(topic.endsWith('/mutex_grant')) grants.push({robot:topic.split('/')[1],...JSON.parse(payload)});
    });mqtt.subscribe('agv/#');await once(mqtt,'subscribed');
    const heartbeat=()=>{for(const id of ids)mqtt.publish(`agv/${id}/telemetry`,`A${id}|80|0`);};
    pump=setInterval(heartbeat,300);heartbeat();await until(async()=>(await state()).fleet.agvs.length===8);
    let ownerRequest=0;
    await until(async()=>{await state();if(grants.length>=1)return true;if(Date.now()-ownerRequest>500){ownerRequest=Date.now();mqtt.publish('agv/r1/mutex_request','M');}return false;});
    for(const id of ids.slice(1))mqtt.publish(`agv/${id}/mutex_request`,'M');
    let lastRequest=0;
    await until(async()=>{
      const mutex=(await state()).mutex.M;if(mutex.waiting.length===7)return true;
      if(Date.now()-lastRequest>500){lastRequest=Date.now();for(const id of ids.slice(1))if(!mutex.waiting.includes(id))mqtt.publish(`agv/${id}/mutex_request`,'M');}
      return false;
    });
    await until(()=>grants.length>=2);assert.equal(grants[0].grant_id,grants[1].grant_id);
    mqtt.publish('agv/r2/mutex_ack',JSON.stringify({zone:'M',grant_id:grants[0].grant_id}));
    await pause(150);assert.equal((await state()).mutex.M.grant_acked,false);
    mqtt.publish('agv/r1/mutex_ack',JSON.stringify({zone:'M',grant_id:grants[0].grant_id}));
    await until(async()=>(await state()).mutex.M.grant_acked);const count=grants.length,frameCount=frames.length;await pause(1300);assert.equal(grants.length,count);
    assert.ok(frames.length-frameCount<=7,'state broadcast is throttled to at most 5/s');
    const active=await api('/api/agvs/r1/goto',{target:'T'});assert.equal(active.status,200);
    const pending=await api('/api/tasks',{target:'T',priority:8});assert.equal(pending.status,200);
    await until(async()=>(await state()).fleet.pending_tasks.some(t=>t.task_id===pending.data.task_id));
    const oldId=grants[0].grant_id,oldSession=(await state()).core_session_id;
    const durable=JSON.parse(fs.readFileSync(path.join(dir,'runtime_state.json'),'utf8'));assert.equal(durable.mutex.M.owner,'r1');
    core.kill();await once(core,'exit');core=child(exe);
    await until(async()=>{const s=await state();return s.core_ready && s.core_session_id!==oldSession;});
    const restored=await state();assert.equal(restored.mutex.M.owner,'r1');assert.equal(restored.mutex.M.recovery_required,true);
    assert.equal(restored.fleet.tasks.find(t=>t.task_id===active.data.task_id).status,'interrupted');
    assert.ok(restored.fleet.pending_tasks.some(t=>t.task_id===pending.data.task_id));
    assert.equal(db.getTaskHistory().filter(t=>t.core_task_id===pending.data.task_id).length,1);
    mqtt.publish('agv/r1/mutex_release',JSON.stringify({zone:'M',grant_id:oldId}));await pause(300);assert.equal((await state()).mutex.M.owner,'r1');
    await api('/api/agvs/r1/clear-error',{});mqtt.publish('agv/r1/mutex_request','M');
    await until(async()=>(await state()).mutex.M.grant_id!==oldId);
    clearInterval(pump);mqtt.publish('agv/r1/status','offline',true);await until(async()=>(await state()).fleet.agvs.find(a=>a.id==='r1').state==='OFFLINE');
    assert.equal((await state()).mutex.M.owner,'r1');
    const current=(await state()).mutex.M.grant_id;
    assert.equal((await api('/api/mutex/M/confirm-clear',{owner:'r1',grant_id:oldId,physically_clear:true})).status,409);
    assert.equal((await api('/api/mutex/M/confirm-clear',{owner:'r1',grant_id:current,physically_clear:true})).status,200);
    assert.equal((await state()).mutex.M.owner,'r2');
    const deltas=frames.filter(f=>f.type==='state_delta');assert.ok(deltas.length>5);assert.ok(deltas.some(f=>!('map' in f.changes)));
    console.log('PASS hardening: 8 robots/FIFO, grant retry+ACK, token, checkpoint before grant, restart keeps queue/IDs/mutex, interrupted task, LWT, verified clear, deltas');
    console.log('Artifacts: '+dir);
  } catch(error) {fs.writeFileSync(path.join(dir,'failure.log'),log);console.error(log.slice(-2000));throw error;}
  finally {clearInterval(pump);mqtt?.close();for(const p of peers)p.terminate();if(gateway)await gateway.close();for(const p of children)if(p.exitCode==null)p.kill();db.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
