// Broker/Core/WS riêng; đo heartbeat và odometry với lịch sử lớn khi chạy qua GDB.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {MqttFixture}=require('./helpers/mqtt-client');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(check,timeout=15000){const end=Date.now()+timeout;while(Date.now()<end){if(check())return;await pause(30);}throw Error('Heartbeat fixture timeout');}
async function port(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function main(){
  const root=path.resolve(__dirname,'..'),dir=fs.mkdtempSync(path.join(root,'.qa/heartbeat-test-'));
  const mqttPort=await port(),corePort=await port(),bin='C:/msys64/ucrt64/bin';
  const map=JSON.parse(fs.readFileSync(path.join(root,'data/map_layout.json'),'utf8'));
  const history=Array.from({length:1000},(_,i)=>({task_id:`history-${i}`,target:'G1',priority:0,status:'completed',assigned_agv:'01',updated_sequence:i}));
  const zones=Object.fromEntries(map.mutex_zones.map(z=>[typeof z==='string'?z:z.id,{owner:'',waiting:[]}]));
  // RuntimeStore so sánh dump map; MapGraph xuất tọa độ double, kể cả 100.0.
  const canonicalMap=JSON.stringify(map).replace(/("(?:x|y)":)(-?\d+)(?=[,}])/g,'$1$2.0');
  fs.writeFileSync(path.join(dir,'runtime.json'),`{"version":1,"map":${canonicalMap},"mutex":${JSON.stringify(zones)},"fleet":${JSON.stringify({agvs:[],
    tasks:history,pending_tasks:[],queue_sequence:1000,charge_reservations:{},accepting_tasks:false})}}`);
  fs.writeFileSync(path.join(dir,'map.json'),JSON.stringify(map));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),
    AGV_COMMAND_ID_PATH:path.join(dir,'command.json'),AGV_TASK_ID_PATH:path.join(dir,'task.json'),AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),
    AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(corePort),_NO_DEBUG_HEAP:'1'};
  for(const k of ['AGV_CORE_TOKEN','AGV_AUTH_TOKEN','AGV_TEST_MODE','AGV_ONE_ROBOT_TRIAL','AGV_TWO_ROBOT_TRIAL','AGV_MONITOR_ONLY_02',
    'AGV_SECURITY_REQUIRED','AGV_MQTT_CA','AGV_MQTT_USER','AGV_MQTT_PASSWORD','AGV_HTTPS_CERT','AGV_HTTPS_KEY'])delete env[k];
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  if(process.env.AGV_DEBUG_HEAP_TEST==='1')delete env._NO_DEBUG_HEAP;
  const children=[],heartbeats=[],latencies=[],commands=[],errors=[],sent=new Map();
  let mqtt,ws,pump,view,coreSession='',seq=0,output='';const start=Date.now(),uptime=()=>Date.now()-start;
  const child=(file,args)=>{const p=spawn(file,args,{cwd:root,env,windowsHide:true});children.push(p);
    p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>errors.push(e.message));return p;};
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await pause(500);
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>errors.push(e.message));await until(()=>mqtt.connected);
    mqtt.on('publish',(topic,payload)=>{
      if(topic==='agv/server/heartbeat'){coreSession=payload;heartbeats.push(Date.now());}
      if(topic==='agv/01/command'){const c=JSON.parse(payload);commands.push(c);mqtt.publish('agv/01/ack',String(c.cmd_id));}
    });mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    const core=process.env.AGV_CORE_BIN||path.join(root,'.qa/network-fix-build/agv_core.exe');
    if(process.env.AGV_HEARTBEAT_GDB==='1')child(path.join(bin,'gdb.exe'),['--batch','-ex','run','--args',core,'--mode','robot01']);
    else child(core,['--mode','robot01']);
    await until(()=>coreSession);
    ws=new WebSocket(`ws://127.0.0.1:${corePort}`);ws.on('error',e=>errors.push(e.message));
    ws.on('message',raw=>{const m=JSON.parse(raw);view=m.type==='state_delta'?{...view,...m.changes}:m;
      const received=view.fleet?.agvs.find(a=>a.id==='01')?.odometry?.seq;
      if(sent.has(received)){latencies.push(Date.now()-sent.get(received));sent.delete(received);}
    });await once(ws,'open');
    const publish=()=>{
      mqtt.publish('agv/01/telemetry','S1|85|0');
      mqtt.publish('agv/01/diagnostic',JSON.stringify({version:1,agvId:'01',session:1,uptimeMs:uptime(),
        encoderSupported:true,encoderBootId:1234,encoderCoreSessionId:coreSession,batteryCalibrated:true,batteryPercentage:85,
        node:'S1',state:'AGV_IDLE',error:'NONE',motor:'STOP',taskActive:false,wifi:true,mqtt:true,leftDuty:0,rightDuty:0,rfidAgeMs:0}));
      const sample={schema_version:1,robot_id:'01',core_session_id:coreSession,boot_id:1234,connection_session:1,
        seq:++seq,uptime_ms:uptime(),left_ticks:0,right_ticks:0,route_cmd_id:0,path_index:0,from_node:'S1',to_node:'',
        tracking:false,translating:false,reverse:false,distance_cm:0,speed_cm_s:0,pulses_per_100cm:161};
      sent.set(seq,Date.now());mqtt.publish('agv/01/odometry',JSON.stringify(sample));
    };
    pump=setInterval(publish,200);publish();await until(()=>latencies.length>0);
    const measuredAt=Date.now();await pause(20000);clearInterval(pump);
    const times=heartbeats.filter(t=>t>=measuredAt),gaps=times.slice(1).map((t,i)=>t-times[i]);
    const report={pass:false,history_tasks:history.length,under_gdb:process.env.AGV_HEARTBEAT_GDB==='1',
      debug_heap_disabled:env._NO_DEBUG_HEAP==='1',duration_ms:Date.now()-measuredAt,heartbeat_count:times.length,
      heartbeat_max_gap_ms:Math.max(...gaps),odometry_received:latencies.length,odometry_max_latency_ms:Math.max(...latencies),commands,errors};
    fs.writeFileSync(path.join(dir,'report.json'),JSON.stringify(report,null,2));
    assert(times.length>=30,JSON.stringify(report));assert(report.heartbeat_max_gap_ms<1500,JSON.stringify(report));
    assert(latencies.length>=60,JSON.stringify(report));assert(report.odometry_max_latency_ms<1500,JSON.stringify(report));
    assert.equal(view.fleet.tasks.length,1000);assert.deepEqual(errors,[]);assert(!commands.some(c=>c.cmd==='GOTO'||c.cmd==='NODE_ORDER'));
    report.pass=true;fs.writeFileSync(path.join(dir,'report.json'),JSON.stringify(report,null,2));console.log(`PASS Core heartbeat: ${JSON.stringify(report)} ${dir}`);
  }catch(e){fs.writeFileSync(path.join(dir,'failure.json'),JSON.stringify({error:e.stack,output,view,errors},null,2));throw e;}
  finally{clearInterval(pump);ws?.terminate();mqtt?.close();
    for(const p of children)if(p.exitCode===null){p.kill();await Promise.race([once(p,'exit'),pause(1500)]);}}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
