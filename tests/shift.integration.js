// Core/gateway/broker thật, hai robot telemetry giả lập; không tác động phần cứng.
const assert = require('assert/strict');
const fs = require('fs'),path = require('path'),net = require('net');
const {spawn} = require('child_process'),{once} = require('events');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');
const {sketchRobots} = require('./helpers/sketch-robots');
const useSketch=process.argv.includes('--sketch');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout=6500) {
  const end=Date.now()+timeout;while(Date.now()<end) {if(await fn()) return;await pause(40);}
  throw new Error('Shift condition timed out');
}
async function freePort() {
  const server=net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
  const port=server.address().port;await new Promise(resolve=>server.close(resolve));return port;
}
async function available(port) {
  return new Promise(resolve=>{const s=net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();resolve(true);});s.once('error',()=>resolve(false));});
}
async function main() {
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.join(root,'.qa/shift-test-'));
  const mqttPort=await freePort(),corePort=await freePort(),mapPath=path.join(directory,'map.json');
  fs.copyFileSync(path.join(root,'data/map_layout.json'),mapPath);
  const brokerPath=path.join(directory,'broker.conf');
  fs.writeFileSync(brokerPath,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin=process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env={...process.env,AGV_HOME_STATIONS:JSON.stringify({'01':'S1','02':'S2'}),
    AGV_MAP_PATH:mapPath,AGV_COMMAND_ID_PATH:path.join(directory,'counter.json'),AGV_SHIFT_STATE_PATH:path.join(directory,'shift.json'),
    AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path') || 'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const db=createDatabase(path.join(directory,'test.db')),children=[],commands=[],positions=new Map(),batteries=new Map();
  let output='',gateway,mqtt,heartbeat,core,sketches;
  function child(command,args=[]) {
    const process=spawn(command,args,{env,cwd:directory,windowsHide:true});children.push(process);
    process.stdout.on('data',d=>output+=d);process.stderr.on('data',d=>output+=d);process.on('error',e=>output+=e.message);return process;
  }
  const telemetry=()=>{if(useSketch) sketches?.telemetry();else if(mqtt?.connected) for(const [id,node] of positions) mqtt.publish(`agv/${id}/telemetry`,`${node}|${batteries.get(id) ?? 85}|0`);};
  try {
    child(path.join(bin,'mosquitto.exe'),['-c',brokerPath]);await until(()=>available(mqttPort));
    core=child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'));
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:100});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async ()=>(await fetch(base+'/api/state')).json();
    const robot=async id=>(await state()).fleet.agvs.find(a=>a.id===id);
    async function api(route,body={}) {
      const response=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      return {status:response.status,data:await response.json()};
    }
    await until(async ()=>{const s=await state();return s.core_ready && s.mqtt_connected;});
    assert.equal((await state()).fleet.agvs.length,0,'no invented robots at boot');
    assert.equal((await api('/api/shift/start')).status,409,'must wait for actual RFID');
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected);
    mqtt.on('publish',(topic,payload)=>{
      const id=topic.split('/')[1];
      if(topic.endsWith('/command')) {
        const command=JSON.parse(payload);commands.push({id,...command});
        if(useSketch) sketches?.receive(id,topic,payload);else mqtt.publish(`agv/${id}/ack`,String(command.cmd_id));
      } else if(useSketch && topic.endsWith('/mutex_grant')) sketches?.receive(id,topic,payload);
    });
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    if(useSketch) {
      sketches=sketchRobots({root,child,mqtt,map:JSON.parse(fs.readFileSync(mapPath,'utf8')),ids:['01','02']});
      telemetry();
      await until(async ()=>(await state()).fleet.agvs.length===2 && (await state()).fleet.agvs.every(a=>a.state==='UNLOCALIZED' && !a.position_valid));
      assert.equal((await state()).shift.can_start,false);
      assert.ok((await state()).fleet.agvs.every(a=>!a.node),'no boot G1/S1/S2 invented');
    }
    heartbeat=setInterval(telemetry,350);
    positions.set('01','G1');positions.set('02','S2');
    if(useSketch) {sketches.scan('01','G1');sketches.scan('02','S2');}else telemetry();
    await until(async ()=>(await robot('01'))?.target==='S1');
    assert.equal((await robot('01')).node,'G1','GOTO/ACK must not change measured position');
    assert.equal((await state()).shift.phase,'CLOSING');
    assert.equal((await api('/api/tasks',{target:'K1'})).status,409);
    async function walk(id,target) {
      await until(async ()=>{const a=await robot(id);return a?.target===target && a.path.length;});
      const route=(await robot(id)).path.slice(),zones=new Set((await state()).map.mutex_zones),held=new Set();
      for(const node of route) {
        if(zones.has(node)) {
          if(useSketch) sketches.serial(id,'request '+node);else mqtt.publish(`agv/${id}/mutex_request`,node);
          await until(async ()=>(await state()).mutex[node].owner===id);held.add(node);
        }
        positions.set(id,node);if(useSketch) sketches.scan(id,node);else telemetry();await until(async ()=>(await robot(id)).node===node);
        // Fixture đo đã rời node cũ trước khi trả quyền; không mô phỏng chiều dài thân xe.
        for(const zone of [...held]) if(zone!==node) {
          if(useSketch) {
            await until(()=>sketches.stats.get(id)?.held_zones & (1<<(Number(zone.slice(1))-1)));
            sketches.serial(id,'release '+zone);
          }else mqtt.publish(`agv/${id}/mutex_release`,JSON.stringify({zone,grant_id:(await state()).mutex[zone].grant_id}));
          held.delete(zone);
          await until(async ()=>(await state()).mutex[zone].owner!==id);
        }
      }
      assert.equal(held.size,0,'stations/warehouses lie outside mutex zones');
    }
    await walk('01','S1');await until(async ()=>(await state()).shift.phase==='CLOSED');
    assert.equal((await state()).shift.can_start,true);
    const chargeCount=commands.filter(c=>c.id==='01' && c.cmd==='CHARGE_START').length;
    if(useSketch) sketches.serial('01','charged 90');else mqtt.publish('agv/01/charging_done','90');
    await until(async ()=>(await robot('01')).state==='IDLE');await pause(850);
    assert.equal(commands.filter(c=>c.id==='01' && c.cmd==='CHARGE_START').length,chargeCount,'no repeated charging after done');
    assert.equal((await api('/api/shift/start')).status,200);
    const first=await api('/api/agvs/01/goto',{target:'K3'}),second=await api('/api/agvs/02/goto',{target:'K4'});
    assert.equal(first.status,200);assert.equal(second.status,200);
    const queued=await api('/api/tasks',{target:'K6'});assert.equal(queued.status,200);
    assert.equal((await api('/api/shift/end')).status,200);
    assert.equal((await api('/api/shift/end')).status,200,'ending is idempotent');
    assert.equal((await robot('01')).target,'K3');assert.equal((await robot('02')).target,'K4');
    assert.equal((await api('/api/agvs/01/goto',{target:'K1'})).status,409);
    assert.equal((await api('/api/tasks',{target:'K1'})).status,409);
    await walk('01','K3');await walk('02','K4');
    await until(async ()=>(await robot('01')).target==='S1' && (await robot('02')).target==='S2');
    let s=await state();assert.equal(s.shift.phase,'CLOSING');
    assert.equal(s.fleet.tasks.find(t=>t.task_id===first.data.task_id).status,'completed');
    assert.equal(s.fleet.tasks.find(t=>t.task_id===second.data.task_id).status,'completed');
    assert.equal(s.fleet.tasks.find(t=>t.task_id===queued.data.task_id).status,'pending','queued task must stay paused');
    await walk('01','S1');assert.equal((await state()).shift.phase,'CLOSING','01 at home is insufficient');
    await walk('02','S2');await until(async ()=>(await state()).shift.phase==='CLOSED');
    assert.equal((await state()).fleet.tasks.find(t=>t.task_id===queued.data.task_id).status,'pending');
    // Mất vị trí/telemetry và lỗi không được coi là đã về trạm.
    batteries.set('01',25);if(useSketch) sketches.serial('01','battery 25');else telemetry();await until(async ()=>(await state()).shift.can_start===false);
    assert.equal((await api('/api/shift/start')).status,409);
    batteries.set('01',85);
    if(useSketch) {sketches.serial('01','battery 85');sketches.serial('01','error TEST_HOME_ERROR');}
    else mqtt.publish('agv/01/error','TEST_HOME_ERROR');
    await until(async ()=>(await robot('01')).state==='ERROR');assert.equal((await api('/api/shift/start')).status,409);
    telemetry();assert.equal((await api('/api/agvs/01/clear-error')).status,200);
    await until(async ()=>(await state()).shift.phase==='CLOSED');
    // Lưu ca đóng qua restart nhưng không lưu vị trí robot: vẫn phải nhận RFID mới.
    clearInterval(heartbeat);heartbeat=null;positions.clear();
    if(useSketch) sketches.resetLocalization();
    core.kill();await once(core,'exit');await until(async ()=>(await state()).agv_connected===false);
    core=child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'));
    await until(async ()=>{const s=await state();return s.core_ready && s.mqtt_connected && s.fleet.agvs.length===2 && s.fleet.agvs.every(a=>!a.position_valid && !a.online);});
    assert.equal((await state()).shift.phase,'CLOSING');assert.equal((await api('/api/shift/start')).status,409);
    positions.set('01','S1');positions.set('02','S2');
    if(useSketch) {sketches.scan('01','S1');sketches.scan('02','S2');}else telemetry();heartbeat=setInterval(telemetry,350);
    await until(async ()=>(await state()).fleet.agvs.every(a=>a.position_valid && a.state==='ERROR'));
    assert.equal((await api('/api/agvs/01/clear-error')).status,200);
    assert.equal((await api('/api/agvs/02/clear-error')).status,200);
    await until(async ()=>(await state()).shift.phase==='CLOSED');
    assert.equal((await state()).shift.can_start,true);
    // Không tự nhả mutex khi xe ở trạm nhưng quyền vùng chưa được xác minh trả lại.
    if(useSketch) sketches.serial('01','request M5');else mqtt.publish('agv/01/mutex_request','M5');
    await until(async ()=>(await state()).mutex.M5.owner==='01');
    await until(async ()=>(await state()).shift.phase==='CLOSING');
    assert.equal((await api('/api/shift/start')).status,409);
    if(useSketch) {await until(()=>sketches.stats.get('01')?.held_zones & (1<<4));sketches.serial('01','release M5');}
    else mqtt.publish('agv/01/mutex_release',JSON.stringify({zone:'M5',grant_id:(await state()).mutex.M5.grant_id}));await until(async ()=>(await state()).shift.phase==='CLOSED');
    s=await state();fs.writeFileSync(path.join(directory,'state.json'),JSON.stringify(s,null,2));
    fs.writeFileSync(path.join(directory,'commands.json'),JSON.stringify(commands,null,2));
    if(useSketch) fs.writeFileSync(path.join(directory,'sketch-events.json'),JSON.stringify(sketches.events,null,2));
    console.log('PASS shift: measured boot location, fixed homes, start gating, finish active tasks, pause queue, ACK not arrival, return via mutex, battery/error gates, fresh RFID after restart');
    console.log('Evidence: '+directory);
    if(useSketch) console.log('PASS two actual .ino receivers: RC522 UID/NVS adapter, UNKNOWN boot, commands/ACK/mutex/charge and full shift/home flow');
  } catch(error) {fs.writeFileSync(path.join(directory,'failure.log'),output);console.error(output);throw error;}
  finally {
    clearInterval(heartbeat);mqtt?.close();if(gateway) await gateway.close();db.close();
    for(const process of children) if(process.exitCode===null) process.kill();
    fs.writeFileSync(path.join(directory,'runtime.log'),output);
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
