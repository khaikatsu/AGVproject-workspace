// Isolated broker/Core/WS fixture: fake stopped robot, no live robot commands.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {randomUUID}=require('node:crypto');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {MqttFixture}=require('./helpers/mqtt-client');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(check,label,timeout=10000){const end=Date.now()+timeout;while(Date.now()<end){if(await check())return;await pause(30);}throw Error('Timeout: '+label);}
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function available(port){return new Promise(r=>{const s=net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();r(true);});s.once('error',()=>r(false));});}
async function main(){
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.join(root,'.qa/stopped-mqtt-')),mqttPort=await freePort(),corePort=await freePort();
  const graph={nodes:[],edges:[],map_name:'Stopped edge fixture'};
  for(const id of ['S1','S2','B','M1','M2','U','T'])graph.nodes.push({id,x:0,y:0,type:id.startsWith('S')?'charging':id==='B'||id.startsWith('M')?'mutex':'waypoint'});
  for(const [from,to] of [['S1','B'],['B','M1'],['M1','M2'],['M2','U'],['S2','M1'],['M2','T']])graph.edges.push({from,to,weight:50,two_way:true});
  fs.writeFileSync(path.join(dir,'map.json'),JSON.stringify(graph));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_COMMAND_ID_PATH:path.join(dir,'counter.json'),
    AGV_TASK_ID_PATH:path.join(dir,'task-counter.json'),AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),
    AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(corePort),
    AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort)};
  delete env.AGV_CORE_TOKEN;delete env.AGV_TEST_MODE;
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],commands=[],grants=[],durabilityErrors=[];let peer,mqtt,pump,proofPump,output='',state,fault=false,route=0,seq=0,robot02Active=false;
  const bootTime=Date.now(),uptime=()=>1000+Date.now()-bootTime;
  const child=(file,args)=>{const p=spawn(file,args,{env,cwd:dir,windowsHide:true});children.push(p);p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;};
  const robot=id=>state?.fleet?.agvs.find(a=>a.id===id);
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await until(()=>available(mqttPort),'broker');
    child(process.env.AGV_CORE_BIN||path.join(root,'.qa/stopped-occupancy/agv_core.exe'),['--mode','active-both']);
    await until(()=>available(corePort),'Core listener');
    const results=new Map();peer=new WebSocket(`ws://127.0.0.1:${corePort}`);peer.on('error',e=>output+=e.message);
    peer.on('message',raw=>{const m=JSON.parse(raw);if(m.type==='command_result'){results.set(m.request_id,m);if(m.state)state=m.state;}
      else state=m.type==='state_delta'?{...state,...m.changes}:m;});
    await until(()=>state?.mqtt_connected,'Core MQTT');
    const command=async body=>{const request_id=randomUUID();peer.send(JSON.stringify({...body,request_id}));await until(()=>results.has(request_id),'command '+body.cmd);return results.get(request_id);};
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected,'MQTT client');
    mqtt.on('publish',(topic,payload)=>{
      if(topic.endsWith('/command')){
        const c=JSON.parse(payload),id=topic.split('/')[1];commands.push({id,...c});mqtt.publish(`agv/${id}/ack`,String(c.cmd_id));
        if(id==='01'&&c.cmd==='GOTO')route=c.cmd_id;
        if(id==='02') {
          if(c.cmd==='GOTO') {if(robot02Active)durabilityErrors.push('GOTO overwrote active MCU route');robot02Active=true;}
          if(c.cmd==='WAIT'||c.cmd==='STOP')robot02Active=false;
        }
        if(fault && id==='02' && c.cmd==='GOTO'){
          try{const saved=JSON.parse(fs.readFileSync(env.AGV_RUNTIME_STATE_PATH,'utf8'));
            assert.deepEqual(saved.fleet.agvs.find(a=>a.id==='01').stopped_nodes,['S1','B']);}
          catch(e){durabilityErrors.push(e.message);}
        }
      }
      if(topic.endsWith('/mutex_grant')){grants.push({id:topic.split('/')[1],...JSON.parse(payload)});mqtt.publish(topic.replace('mutex_grant','mutex_ack'),payload);}
    });
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed,'subscription');
    const telemetry=()=>{for(const id of ['01','02'])mqtt.publish(`agv/${id}/telemetry`,`${id==='01'?'S1':'S2'}|80|0`);
      mqtt.publish('agv/01/diagnostic',JSON.stringify({version:1,agvId:'01',encoderSupported:true,encoderCoreSessionId:state.core_session_id,
        encoderBootId:10,session:1,uptimeMs:uptime(),node:'S1',state:fault?'AGV_SAFE_STOP':'AGV_IDLE',error:fault?'MOTOR_FAULT':'NONE',
        motor:'STOP',leftDuty:0,rightDuty:0,taskActive:false,wifi:true,mqtt:true,rfidAgeMs:fault?10000:0,batteryCalibrated:true,batteryPercentage:80}));
      mqtt.publish('agv/02/diagnostic',JSON.stringify({version:1,agvId:'02',encoderSupported:true,encoderCoreSessionId:state.core_session_id,
        encoderBootId:20,session:1,uptimeMs:uptime(),node:'S2',nodeOrdersSupported:true,
        state:robot02Active?'AGV_MOVING':'AGV_IDLE',error:'NONE',motor:robot02Active?'FORWARD':'STOP',
        leftDuty:robot02Active?100:0,rightDuty:robot02Active?100:0,taskActive:robot02Active,
        wifi:true,mqtt:true,rfidAgeMs:0,batteryCalibrated:true,batteryPercentage:80}));};
    pump=setInterval(telemetry,150);telemetry();await until(()=>state?.shift?.can_start,'ready');
    assert.equal((await command({cmd:'start_shift'})).ok,true);
    assert.equal((await command({cmd:'direct_goto',agv_id:'01',target:'U'})).ok,true);
    assert.equal((await command({cmd:'direct_goto',agv_id:'02',target:'T'})).ok,true);
    await until(()=>route&&robot('02')?.state==='BLOCKED','route reservation');
    for(const zone of ['B','M1','M2']){mqtt.publish('agv/01/mutex_request',zone);await until(()=>state.mutex[zone].owner==='01','owner '+zone);}
    const oldGrant=state.mutex.M1.grant_id;
    fault=true;mqtt.publish('agv/01/error','MOTOR_FAULT');telemetry();await until(()=>robot('01')?.state==='ERROR','fault');
    await pause(650);
    const packet=()=>({schema_version:1,robot_id:'01',core_session_id:state.core_session_id,boot_id:10,connection_session:1,
      seq:++seq,uptime_ms:uptime(),left_ticks:30,right_ticks:30,pulses_per_100cm:161,route_cmd_id:0,path_index:0,
      from_node:'S1',to_node:'',tracking:false,translating:false,reverse:false,distance_cm:0,speed_cm_s:0,
      stopped_position:{version:1,from_node:'S1',to_node:'B',route_cmd_id:route,path_index:0,stationary_ms:600+seq*200,distance_cm:10}});
    const send=()=>mqtt.publish('agv/01/odometry',JSON.stringify(packet()));
    // Wrong Core session is ignored before the new proof can mutate ownership.
    const bad=packet();bad.core_session_id='old-core';mqtt.publish('agv/01/odometry',JSON.stringify(bad));await pause(100);
    assert.equal(state.mutex.M1.owner,'01');
    send();await pause(80);assert.equal(state.mutex.M1.owner,'01','one sample cannot release');
    proofPump=setInterval(send,220);
    await until(()=>robot('01')?.stopped_occupancy_verified&&state.mutex.M1.owner==='','verified stopped region');
    assert.deepEqual(robot('01').stopped_nodes,['S1','B']);assert.equal(state.mutex.B.owner,'01');
    assert.equal(state.mutex.M2.owner,'');await until(()=>robot('02')?.state==='WORKING','02 rerouted');
    assert.deepEqual(durabilityErrors,[],'checkpoint before GOTO');
    const before=grants.length;mqtt.publish('agv/02/mutex_request','B');await pause(200);
    assert.equal(grants.length,before,'blocked endpoint cannot grant');
    mqtt.publish('agv/01/mutex_release',JSON.stringify({zone:'M1',grant_id:oldGrant}));await pause(100);assert.equal(state.mutex.M1.owner,'');
    clearInterval(proofPump);proofPump=null;
    await until(()=>robot('01')?.stopped_occupancy_verified===false&&robot('02')?.state==='BLOCKED','stale proof expands region');
    mqtt.publish('agv/02/mutex_request','M1');await pause(200);assert.equal(state.mutex.M1.owner,'');
    assert.equal(state.mutex.M1.blocked_by_stopped_robot,true);
    assert.equal(robot('01').state,'ERROR');
    assert.deepEqual(durabilityErrors,[],'no active route overwrite');
    console.log('PASS MQTT stopped occupancy: bound proof, endpoint block, release outside, replan, durable before command, stale fallback, old token');
    console.log('Evidence: '+dir);
  }finally{
    clearInterval(pump);clearInterval(proofPump);peer?.terminate();mqtt?.close();
    for(const p of children)if(p.exitCode===null)p.kill();
    fs.writeFileSync(path.join(dir,'runtime.log'),output);
  }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
