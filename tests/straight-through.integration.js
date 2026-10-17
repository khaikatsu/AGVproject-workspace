// Private broker/Core/gateway fixture. No requests reach the robot broker.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {createGateway}=require('../AGVwebserver/server');
const {createDatabase}=require('../AGVwebserver/db');
const {MqttFixture}=require('./helpers/mqtt-client');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,timeout=8000){const end=Date.now()+timeout;while(Date.now()<end){if(await check())return;await pause(35);}throw Error('Straight-through fixture timed out');}
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function available(port){return new Promise(r=>{const s=net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();r(true);});s.once('error',()=>r(false));});}
async function main(){
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.join(root,'.qa/straight-through-test-'));
  const mqttPort=await freePort(),corePort=await freePort(),bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(dir,'map.json'));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_COMMAND_ID_PATH:path.join(dir,'command.json'),
    AGV_TASK_ID_PATH:path.join(dir,'task.json'),AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),
    AGV_HOME_STATIONS:'{"01":"S1","02":"S2"}',AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  for(const key of ['AGV_CORE_TOKEN','AGV_AUTH_TOKEN','AGV_TEST_MODE','AGV_ONE_ROBOT_TRIAL','AGV_TWO_ROBOT_TRIAL','AGV_MONITOR_ONLY_02',
    'AGV_SECURITY_REQUIRED','AGV_MQTT_CA','AGV_MQTT_USER','AGV_MQTT_PASSWORD','AGV_HTTPS_CERT','AGV_HTTPS_KEY'])delete env[key];
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],commands=[],grants=[],observed=[];let output='',mqtt,gateway,pump,readState,browserSocket,browserView,arrivalProbe,robot02Online=true;
  const db=createDatabase(path.join(dir,'test.db'));
  const child=(file,args)=>{const p=spawn(file,args,{env,cwd:dir,windowsHide:true});children.push(p);p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;};
  const r={node:'S1',heading:'S',state:'AGV_IDLE',error:'NONE',motor:'STOP',taskActive:false,leftDuty:0,rightDuty:0,
    headingSource:'bay_default',rfidAgeMs:0,routeCommandId:0,pathIndex:0,nextNode:'',awaitingNodeOrder:false,straightThroughSupported:true};
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await until(()=>available(mqttPort));
    child(process.env.AGV_CORE_BIN||path.join(root,'.qa/straight-through-build/agv_core.exe'),['--mode','both']);
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:80});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async()=>(await fetch(base+'/api/state')).json();
    readState=state;
    const robot=async()=>(await state()).fleet.agvs.find(a=>a.id==='01');
    const post=async(route,body={})=>{const response=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      const result=await response.json();assert.equal(response.status,200,JSON.stringify(result));return result;};
    await until(async()=>{const s=await state();return s.core_ready&&s.mqtt_connected;});
    browserSocket=new WebSocket(base.replace('http:','ws:')+'/ws?delta=1');
    browserSocket.on('message',raw=>{
      const message=JSON.parse(raw);
      if(message.type==='state_delta'){
        assert.equal(message.base_state_seq,browserView.state_seq);
        browserView={...browserView,...message.changes,state_seq:message.state_seq};
      }else browserView=message;
      const node=browserView.fleet?.agvs?.find(a=>a.id==='01')?.node;
      if(arrivalProbe && arrivalProbe.node===node && arrivalProbe.receivedAt==null)
        arrivalProbe.receivedAt=Date.now();
    });
    await once(browserSocket,'open');await until(()=>browserView);
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected);
    mqtt.on('publish',(topic,payload)=>{
      if(topic==='agv/01/command'){
        const c=JSON.parse(payload);commands.push(c);mqtt.publish('agv/01/ack',String(c.cmd_id));
        if(c.cmd==='GOTO')Object.assign(r,{routeCommandId:c.cmd_id,pathIndex:0,nextNode:c.path[0],taskActive:true,state:'AGV_WAITING',awaitingNodeOrder:true});
      }
      if(topic.endsWith('/mutex_grant')){const g=JSON.parse(payload),id=topic.split('/')[1];grants.push({id,...g});mqtt.publish(`agv/${id}/mutex_ack`,JSON.stringify(g));}
    });
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    const publish=()=>{
      mqtt.publish('agv/01/telemetry',`${r.node}|85|0`);
      mqtt.publish('agv/01/diagnostic',JSON.stringify({version:1,agvId:'01',session:1,uptimeMs:Date.now()%1000000,...r,
        headingSupported:true,headingNode:r.node,nodeOrdersSupported:true,wifi:true,mqtt:true,batteryCalibrated:true,batteryPercentage:85}));
      if(robot02Online)mqtt.publish('agv/02/telemetry','S2|85|0');
    };
    const set=async(values)=>{
      const changed=values.node!=null && values.node!==r.node;
      const started=Date.now();Object.assign(r,values);
      arrivalProbe=changed ? {node:r.node,receivedAt:null} : null;publish();
      await until(async()=>(await robot())?.node===r.node);
      await until(()=>browserView?.fleet.agvs.find(a=>a.id==='01')?.node===r.node);
      const latency=changed ? arrivalProbe.receivedAt-started : null;
      if(changed)assert(latency>=0 && latency<1000,`RFID ${r.node} took ${latency}ms from fixture publish to browser WS`);
      observed.push({node:r.node,pathIndex:r.pathIndex,state:r.state,mqtt_to_web_ms:latency});arrivalProbe=null;
    };
    const orders=()=>commands.filter(c=>c.cmd==='NODE_ORDER'&&c.route_cmd_id===r.routeCommandId);
    const orderAt=step=>orders().find(c=>c.path_index===step);
    const latestGrant=(id,zone)=>grants.findLast(g=>g.id===id&&g.zone===zone);
    const release=g=>mqtt.publish(`agv/${g.id}/mutex_release`,JSON.stringify({zone:g.zone,grant_id:g.grant_id}));
    const task=async(id)=>(await state()).fleet.tasks.find(t=>t.task_id===id);
    const goto=async(target)=>{
      const result=await post('/api/agvs/01/goto',{target});await until(()=>commands.some(c=>c.cmd==='GOTO'&&c.cmd_id===r.routeCommandId));
      return {id:result.task_id,command:commands.findLast(c=>c.cmd==='GOTO')};
    };
    pump=setInterval(publish,180);publish();await until(async()=>(await state()).shift.can_start);
    await post('/api/shift/start');await set({node:'G6',heading:'S',headingSource:'route'});await until(async()=>(await robot()).heading==='S');
    // Robot 02 owns M3: 01 may enter M4, but must wait before M3.
    mqtt.publish('agv/02/mutex_request','M3');await until(()=>latestGrant('02','M3'));
    const blocked=await goto('G1');assert.deepEqual(blocked.command.path,['M4','GS2','M3','G1']);assert.deepEqual(blocked.command.pass_through,[0,1,2]);
    await until(()=>orderAt(0));assert.equal(orderAt(0).maneuver,'STRAIGHT');
    assert.equal((await task(blocked.id)).status,'dispatched'); // GOTO/ACK does not finish a trip.
    mqtt.publish('agv/01/mutex_request','M4');await until(()=>latestGrant('01','M4'));
    await set({node:'M4',pathIndex:1,nextNode:'GS2',heading:'W',state:'AGV_MOVING',motor:'FORWARD',leftDuty:180,rightDuty:220,awaitingNodeOrder:false});
    await pause(200);assert.equal(orderAt(1),undefined);
    mqtt.publish('agv/01/mutex_request','M3');await pause(200);assert.equal(latestGrant('01','M3'),undefined);
    await set({node:'GS2',pathIndex:2,nextNode:'M3',state:'AGV_WAITING',motor:'STOP',leftDuty:0,rightDuty:0,awaitingNodeOrder:true});
    await until(()=>orderAt(2));assert.equal(orderAt(2).maneuver,'STRAIGHT');release(latestGrant('01','M4'));
    const held=latestGrant('02','M3');mqtt.publish('agv/02/mutex_release',JSON.stringify({zone:'M3',grant_id:held.grant_id-1}));
    await pause(200);assert.equal((await state()).mutex.M3.owner,'02');
    robot02Online=false;mqtt.publish('agv/02/status','offline');
    await until(async()=>(await state()).fleet.agvs.find(a=>a.id==='02')?.state==='OFFLINE');
    assert.equal((await state()).mutex.M3.owner,'02');assert.equal((await task(blocked.id)).status,'dispatched');
    release(held);await until(()=>latestGrant('01','M3'));
    await until(async()=>(await state()).mutex.M3.owner==='01');
    await set({node:'M3',pathIndex:3,nextNode:'G1',state:'AGV_MOVING',motor:'FORWARD',leftDuty:180,rightDuty:220,awaitingNodeOrder:false});
    assert.equal((await task(blocked.id)).status,'dispatched');
    await set({node:'G1',pathIndex:4,nextNode:'',state:'AGV_IDLE',motor:'STOP',taskActive:false,leftDuty:0,rightDuty:0});
    await until(async()=>(await task(blocked.id)).status==='completed');release(latestGrant('01','M3'));
    await until(async()=>(await state()).mutex.M3.owner==='');
    // Clear route: Core gets all intermediate RFIDs while MCU never requests a node order there.
    await set({node:'G6',heading:'S'});const continuous=await goto('G1');await until(()=>orderAt(0));
    mqtt.publish('agv/01/mutex_request','M4');const priorM4=latestGrant('01','M4').grant_id;
    await until(()=>latestGrant('01','M4').grant_id!==priorM4);
    await set({node:'M4',pathIndex:1,nextNode:'GS2',heading:'W',state:'AGV_MOVING',motor:'FORWARD',leftDuty:180,rightDuty:220,awaitingNodeOrder:false});
    const priorM3=latestGrant('01','M3').grant_id;mqtt.publish('agv/01/mutex_request','M3');
    await until(()=>latestGrant('01','M3').grant_id!==priorM3);
    for(const [node,index,next] of [['GS2',2,'M3'],['M3',3,'G1']]){
      await set({node,pathIndex:index,nextNode:next});assert.equal((await task(continuous.id)).status,'dispatched');
      if(node==='GS2')release(latestGrant('01','M4'));
    }
    await set({node:'G1',pathIndex:4,nextNode:'',taskActive:false,state:'AGV_IDLE',motor:'STOP',leftDuty:0,rightDuty:0});
    await until(async()=>(await task(continuous.id)).status==='completed');assert.equal(orders().length,1);
    release(latestGrant('01','M3'));await until(async()=>(await state()).mutex.M3.owner==='');
    await set({node:'G6'});const turn=await goto('K5');assert(!turn.command.pass_through.includes(0));await until(()=>orderAt(0));
    await set({node:'M4',pathIndex:1,nextNode:'G5',state:'AGV_WAITING',motor:'STOP',awaitingNodeOrder:true});
    await until(()=>orderAt(1));assert.equal(orderAt(1).maneuver,'RIGHT');assert.equal((await task(turn.id)).status,'dispatched');
    await post('/api/tasks/cancel',{task_id:turn.id});
    await set({node:'G6',state:'AGV_IDLE',taskActive:false,awaitingNodeOrder:false,straightThroughSupported:false});
    await pause(200);const legacy=await goto('G1');assert.equal(legacy.command.pass_through,undefined);
    await post('/api/tasks/cancel',{task_id:legacy.id});
    fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify({pass:true,commands,grants,observed},null,2));
    console.log(`PASS straight-through: route metadata, per-RFID progress, no intermediate NODE_ORDER; M3 contention/offline holds token, correct release; turn/legacy fallback. ${dir}`);
  }catch(error){
    const snapshot=readState ? await readState().catch(()=>null) : null;
    fs.writeFileSync(path.join(dir,'failure.json'),JSON.stringify({error:error.stack,r,commands,grants,observed,snapshot,output},null,2));
    console.error(output.slice(-4000));console.error(`Failure trace: ${dir}`);throw error;
  }
  finally{clearInterval(pump);browserSocket?.terminate();mqtt?.close();await gateway?.close();db.close();for(const p of children)if(p.exitCode===null){p.kill();await Promise.race([once(p,'exit'),pause(1000)]);}}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
