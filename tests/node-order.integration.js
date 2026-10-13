// Isolated MQTT/Core/gateway fixture; no packets go to the live robot broker.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {createGateway}=require('../AGVwebserver/server');
const {createDatabase}=require('../AGVwebserver/db');
const {MqttFixture}=require('./helpers/mqtt-client');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(check,timeout=8000){const end=Date.now()+timeout;while(Date.now()<end){if(await check())return;await pause(40);}throw Error('Node order fixture timed out');}
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function available(port){return new Promise(r=>{const s=net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();r(true);});s.once('error',()=>r(false));});}
async function main(){
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.join(root,'.qa/node-order-test-')),mqttPort=await freePort(),corePort=await freePort();
  const bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(dir,'map.json'));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_COMMAND_ID_PATH:path.join(dir,'counter.json'),AGV_TASK_ID_PATH:path.join(dir,'task-counter.json'),
    AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  delete env.AGV_CORE_TOKEN;delete env.AGV_TEST_MODE;
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],commands=[],grants=[];let output='',mqtt,gateway,pump;
  const db=createDatabase(path.join(dir,'test.db'));
  const child=(file,args)=>{const p=spawn(file,args,{env,cwd:dir,windowsHide:true});children.push(p);p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;};
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await until(()=>available(mqttPort));
    child(process.env.AGV_CORE_BIN||path.join(root,'.qa/node-order-build/agv_core.exe'),['--mode','robot01']);
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:80});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async()=>(await fetch(base+'/api/state')).json();
    const robot=async()=>(await state()).fleet.agvs.find(a=>a.id==='01');
    const post=async(route,body={})=>{const r=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return {status:r.status,data:await r.json()};};
    await until(async()=>{const s=await state();return s.core_ready&&s.mqtt_connected;});
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected);
    let r={node:'S1',heading:'S',headingSource:'bay_default',state:'AGV_IDLE',error:'NONE',motor:'STOP',taskActive:false,leftDuty:0,rightDuty:0,
      rfidAgeMs:0,routeCommandId:0,pathIndex:0,nextNode:'',awaitingNodeOrder:false,stationTurnRequired:false,stationReady:true};
    mqtt.on('publish',(topic,payload)=>{
      if(topic==='agv/01/command'){
        const c=JSON.parse(payload);commands.push(c);
        if(c.cmd==='GOTO')Object.assign(r,{routeCommandId:c.cmd_id,pathIndex:0,nextNode:c.path[0]||'',taskActive:true,state:'AGV_WAITING',awaitingNodeOrder:true,stationReady:false});
        // Keep LEFT unacknowledged to exercise transport retry with the same ID.
        if(!(c.cmd==='NODE_ORDER'&&c.maneuver==='LEFT'))mqtt.publish('agv/01/ack',String(c.cmd_id));
      }
      if(topic==='agv/01/mutex_grant'){const g=JSON.parse(payload);grants.push(g);mqtt.publish('agv/01/mutex_ack',JSON.stringify(g));}
    });
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    const publish=()=>{mqtt.publish('agv/01/telemetry',`${r.node}|?|0`);mqtt.publish('agv/01/diagnostic',JSON.stringify({version:1,agvId:'01',session:1,uptimeMs:Date.now()%1000000,
      ...r,headingSupported:true,headingNode:r.node,nodeOrdersSupported:true,stationDockingSupported:true,wifi:true,mqtt:true,batteryPercentage:r.batteryPercentage ?? 60,batteryCalibrated:true}));};
    const set=values=>{Object.assign(r,values);publish();};
    const orders=()=>commands.filter(c=>c.cmd==='NODE_ORDER'&&c.route_cmd_id===r.routeCommandId);
    const orderAt=step=>orders().find(c=>c.path_index===step&&c.maneuver!=='STATION_TURN_RIGHT');
    pump=setInterval(publish,200);publish();await until(async()=>(await robot())?.heading==='S');
    assert.equal((await post('/api/shift/start')).status,200);
    if(process.platform==='win32'){
      const held=path.join(dir,'checkpoint.held'),release=path.join(dir,'checkpoint.release');
      child('powershell.exe',['-NoProfile','-File',path.join(root,'tests/helpers/hold-checkpoint.ps1'),'-Path',path.join(dir,'runtime.json'),'-Held',held,'-Release',release]);
      await until(()=>fs.existsSync(held));
      set({batteryPercentage:61}); // Force a changed durable snapshot while replacement is locked.
      await until(async()=>(await state()).storage_healthy===false,15000);
      const h=await (await fetch(base+'/api/health')).json();assert.equal(h.core_ready,false);assert.equal(h.storage_healthy,false);
      assert.equal((await post('/api/agvs/01/goto',{target:'GS1'})).status,503);
      fs.writeFileSync(release,'unlock');
      await until(async()=>(await state()).storage_healthy===true,15000);
      await until(async()=>(await robot()).state==='IDLE',8000);
      assert.equal(commands.filter(c=>c.cmd==='GOTO').length,0); // Recovery alone never invents a route.
    }
    r.rfidAgeMs=5000; // Scan then click later: the stopped confirmed start is still valid.
    const trip=await post('/api/agvs/01/goto',{target:'M2'});assert.equal(trip.status,200);
    await until(()=>orderAt(0));assert.equal(orderAt(0).maneuver,'STRAIGHT');
    set({awaitingNodeOrder:false,state:'AGV_MOVING',motor:'FORWARD',leftDuty:205,rightDuty:245});
    await pause(200);assert.equal(orders().length,1);
    set({node:'GS1',pathIndex:1,nextNode:'M2',state:'AGV_WAITING',motor:'STOP',leftDuty:0,rightDuty:0,awaitingNodeOrder:true,rfidAgeMs:null,headingSource:'route'});
    await pause(350);assert.equal(orderAt(1),undefined); // No actual read in this session must not trigger a turn.
    const route=r.routeCommandId;r.routeCommandId=route+500;set({rfidAgeMs:0});await pause(300);
    assert.equal(commands.filter(c=>c.cmd==='NODE_ORDER'&&c.node==='GS1').length,0);
    r.routeCommandId=route;publish();mqtt.publish('agv/01/mutex_request','M2');
    await until(()=>orderAt(1)&&grants.length);const left=orderAt(1);assert.equal(left.maneuver,'LEFT');assert.equal(left.node,'GS1');
    await until(()=>orders().filter(c=>c.cmd_id===left.cmd_id).length>=2);
    assert.equal(new Set(orders().filter(c=>c.path_index===1).map(c=>c.cmd_id)).size,1);
    mqtt.publish('agv/01/ack',String(left.cmd_id));
    set({awaitingNodeOrder:false,state:'AGV_MOVING',motor:'PIVOT_LEFT',heading:'UNKNOWN',leftDuty:245,rightDuty:245});await pause(250);
    assert.equal((await state()).fleet.tasks.find(t=>t.task_id===trip.data.task_id).status,'dispatched');
    assert.equal((await post('/api/agvs/01/goto',{target:'C1'})).status,409);
    set({heading:'E',motor:'FORWARD',leftDuty:205});await until(async()=>(await robot()).heading==='E');
    set({node:'M2',pathIndex:2,nextNode:'',state:'AGV_IDLE',taskActive:false,motor:'STOP',leftDuty:0,rightDuty:0});
    await until(async()=>(await state()).fleet.tasks.find(t=>t.task_id===trip.data.task_id).status==='completed');
    set({state:'AGV_SAFE_STOP',error:'LINE_LOST'});mqtt.publish('agv/01/error','LINE_LOST');
    await until(async()=>(await robot()).state==='ERROR');
    const ownerBefore=(await state()).mutex.M2;
    assert.equal((await post('/api/agvs/01/clear-error')).status,200); // No home-only block in robot01 mode.
    await until(()=>commands.some(c=>c.cmd==='RESET_FAULT'));
    await pause(250);assert.equal((await robot()).state,'ERROR'); // Receipt alone cannot clear the MCU latch.
    set({state:'AGV_IDLE',error:'NONE',rfidAgeMs:5000});
    await until(async()=>(await robot()).state==='IDLE');
    assert.equal((await state()).mutex.M2.owner,ownerBefore.owner);
    assert.equal((await state()).mutex.M2.grant_id,ownerBefore.grant_id);
    assert.equal((await post('/api/shift/end')).status,200);
    await until(async()=>(await state()).shift.phase==='CLOSED');
    assert.equal((await state()).shift.can_start,true);assert.equal((await state()).shift.all_home,false);
    assert.equal((await post('/api/shift/start')).status,200); // Start the next test at M2, with the owned token intact.
    // Simulated relocation between completed trials; no inference from that movement.
    mqtt.publish('agv/01/mutex_release',JSON.stringify(grants[0]));
    set({node:'K1',heading:'W',headingSource:'bay_default',rfidAgeMs:0});await until(async()=>(await robot()).node==='K1');
    const rightTrip=await post('/api/agvs/01/goto',{target:'C1'});assert.equal(rightTrip.status,200);
    await until(()=>orderAt(0));assert.equal(orderAt(0).maneuver,'REVERSE');
    set({node:'G1',pathIndex:1,nextNode:'C1',heading:'W',headingSource:'route',awaitingNodeOrder:true,state:'AGV_WAITING'});
    await until(()=>orderAt(1));assert.equal(orderAt(1).maneuver,'RIGHT');
    set({awaitingNodeOrder:false,state:'AGV_MOVING',motor:'PIVOT_RIGHT',heading:'UNKNOWN',leftDuty:245,rightDuty:245});
    await pause(200);set({node:'C1',pathIndex:2,nextNode:'',state:'AGV_IDLE',taskActive:false,motor:'STOP',leftDuty:0,rightDuty:0,heading:'N'});
    await until(async()=>(await state()).fleet.tasks.find(t=>t.task_id===rightTrip.data.task_id).status==='completed');
    set({node:'GS1',heading:'E',headingSource:'route'});await until(async()=>(await robot()).node==='GS1');
    const home=await post('/api/agvs/01/goto',{target:'S1'});assert.equal(home.status,200);
    await until(()=>orderAt(0));assert.equal(orderAt(0).maneuver,'LEFT');
    set({node:'S1',heading:'N',pathIndex:1,nextNode:'',stationTurnRequired:true,awaitingNodeOrder:true,state:'AGV_WAITING'});
    await until(()=>orders().find(c=>c.maneuver==='STATION_TURN_RIGHT'));
    assert.equal((await state()).fleet.tasks.find(t=>t.task_id===home.data.task_id).status,'dispatched');
    assert.equal((await post('/api/agvs/01/goto',{target:'GS1'})).status,409);
    set({stationTurnRequired:false,awaitingNodeOrder:false,state:'AGV_IDLE',motor:'STOP',taskActive:false,heading:'S',headingSource:'bay_default',stationReady:true});
    await until(async()=>(await state()).fleet.tasks.find(t=>t.task_id===home.data.task_id).status==='completed');
    assert.equal((await robot()).heading,'S');
    fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify({pass:true,commands,grants},null,2));
    console.log(`PASS node orders: RFID -> STOP -> server LEFT/RIGHT -> pivot -> following; stale route/tag rejected, same-ID retry, mutex and station readiness; ${dir}`);
  }catch(e){console.error(output.slice(-5000));throw e;}
  finally{clearInterval(pump);mqtt?.close();await gateway?.close();db.close();for(const p of children)if(p.exitCode===null){p.kill();await Promise.race([once(p,'exit'),pause(1000)]);}}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
