// Core/broker/gateway/robot fixture riêng; không gửi lệnh tới xe thật.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {createGateway}=require('../AGVwebserver/server');
const {createDatabase}=require('../AGVwebserver/db');
const {MqttFixture}=require('./helpers/mqtt-client');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,timeout=8000) {const end=Date.now()+timeout;while(Date.now()<end){if(await check())return;await pause(40);}throw Error('Heading fixture timed out');}
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function available(port){return new Promise(r=>{const s=net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();r(true);});s.once('error',()=>r(false));});}
async function main(){
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.join(root,'.qa/heading-test-')),mqttPort=await freePort(),corePort=await freePort();
  const bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(dir,'map.json'));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_COMMAND_ID_PATH:path.join(dir,'counter.json'),
    AGV_TASK_ID_PATH:path.join(dir,'task-counter.json'),AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),
    AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),
    AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  delete env.AGV_CORE_TOKEN;delete env.AGV_TEST_MODE;
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],commands=[];let output='',mqtt,gateway,pump;const db=createDatabase(path.join(dir,'test.db'));
  const child=(file,args)=>{const p=spawn(file,args,{env,cwd:dir,windowsHide:true});children.push(p);p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;};
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await until(()=>available(mqttPort));
    child(process.env.AGV_CORE_BIN||path.join(root,'.qa/station-heading-build/agv_core.exe'),['--mode','robot01']);
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:80});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async()=>(await fetch(base+'/api/state')).json();
    const robot=async()=>(await state()).fleet.agvs.find(a=>a.id==='01');
    const post=async(route,body)=>{const r=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return {status:r.status,data:await r.json()};};
    await until(async()=>{const s=await state();return s.core_ready&&s.mqtt_connected;});
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected);
    mqtt.on('publish',(topic,payload)=>{if(topic==='agv/01/command'){const c=JSON.parse(payload);commands.push(c);mqtt.publish('agv/01/ack',String(c.cmd_id));}});
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    let node='S1',heading='S',headingSource='bay_default',moving=false,docking=false,rfidAge=0;
    const diagnostic=()=>mqtt.publish('agv/01/diagnostic',JSON.stringify({version:1,agvId:'01',session:1,uptimeMs:100,
      node,rfidAgeMs:rfidAge,state:moving?'AGV_MOVING':'AGV_IDLE',error:'NONE',motor:moving?(docking?'PIVOT_RIGHT':'FORWARD'):'STOP',taskActive:moving,
      leftDuty:moving?205:0,rightDuty:moving?245:0,wifi:true,mqtt:true,batteryCalibrated:true,batteryPercentage:60,
      headingSupported:true,heading,headingNode:node,headingSource,stationDockingSupported:true,
      stationReady:!moving&&(node==='S1'||node==='S2')}));
    const publish=()=>{mqtt.publish('agv/01/telemetry',`${node}|?|0`);diagnostic();};
    pump=setInterval(publish,250);publish();await until(async()=>(await robot())?.heading_supported);
    await until(async()=>(await robot()).heading==='S');
    assert.equal((await robot()).heading_source,'bay_default');
    const manual=await fetch(base+'/api/agvs/01/heading',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({node:'S1',heading:'E',physically_confirmed:true})});
    assert.equal(manual.status,404);
    assert.equal((await post('/api/shift/start',{})).status,200);
    moving=true;rfidAge=null;headingSource='route';publish();await pause(350);
    assert.equal((await robot()).heading,'S'); // Forward station departure preserves the outward front.
    moving=false;rfidAge=0;node='GS1';publish();
    await until(async()=>(await robot()).node==='GS1'&&(await robot()).heading==='S');
    heading='W';headingSource='operator';publish();await pause(350);
    assert.equal((await robot()).heading,'UNKNOWN'); // Former manual source is no longer accepted.
    heading='E';headingSource='route';publish();await until(async()=>(await robot()).heading==='E');
    const returned=await post('/api/agvs/01/goto',{target:'S1'});assert.equal(returned.status,200);
    const task=async()=>(await state()).fleet.tasks.find(t=>t.task_id===returned.data.task_id);
    await until(()=>commands.some(c=>c.cmd==='GOTO'&&c.target==='S1'));
    node='S1';heading='UNKNOWN';moving=true;docking=true;publish();await pause(400);
    assert.equal((await task()).status,'dispatched'); // RFID at S is still part of the physical parking maneuver.
    assert.equal((await robot()).state,'WORKING');
    assert.equal((await post('/api/shift/end',{})).status,200);
    assert.equal((await state()).shift.phase,'CLOSING');
    assert.equal((await post('/api/agvs/01/goto',{target:'GS1'})).status,409);
    moving=false;docking=false;heading='N';headingSource='bay_default';publish();await pause(350);
    assert.equal((await task()).status,'dispatched'); // Stopped but facing inward is insufficient.
    heading='S';rfidAge=null;publish();await pause(350);
    assert.equal((await task()).status,'dispatched'); // No RFID read in this session is insufficient.
    rfidAge=175278;publish();await until(async()=>(await task()).status==='completed');
    // The reader left S1 during the completed pivot; fresh diagnostic confirms parking.
    assert.equal((await robot()).state,'IDLE');assert.equal((await robot()).heading,'S');
    assert.equal((await robot()).task_id,'');assert.equal((await robot()).target,'');
    await until(async()=>(await state()).shift.phase==='CLOSED');
    await until(()=>db.getTaskHistory().some(t=>t.core_task_id===returned.data.task_id&&t.status==='completed'));
    publish();publish();await pause(400);
    const history=db.getTaskHistory().filter(t=>t.core_task_id===returned.data.task_id);
    assert.equal(history.length,1);assert.equal(history[0].status,'completed');
    assert.equal((await state()).fleet.tasks.filter(t=>t.task_id===returned.data.task_id&&t.status==='completed').length,1);
    clearInterval(pump);await pause(350);
    mqtt.publish('agv/01/status','offline');await until(async()=>(await robot()).state==='OFFLINE');
    assert.equal((await robot()).heading,'UNKNOWN');
    assert.equal(commands.filter(c=>c.cmd==='GOTO').length,1);
    assert.equal(commands.filter(c=>c.cmd==='SET_HEADING').length,0);
    fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify({pass:true,commands},null,2));
    console.log(`PASS heading: outward station front, RFID/ACK waits for parking, stopped inward/unread RFID rejected, ready with aged RFID completes, removed manual API, offline unknown; ${dir}`);
  }catch(e){console.error(output.slice(-6000));throw e;}
  finally{clearInterval(pump);mqtt?.close();await gateway?.close();db.close();for(const p of children){if(p.exitCode===null){p.kill();await Promise.race([once(p,'exit'),pause(1000)]);}}}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
