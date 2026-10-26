const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process');
const {once}=require('node:events');
const root=path.resolve(__dirname,'..');
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function main(){
  const dir=fs.mkdtempSync(path.join(root,'.qa/startup-errors-'));
  const source=JSON.parse(fs.readFileSync(path.join(root,'data/map_layout.json'),'utf8'));
  // MapGraph stores node coordinates as doubles; preserve 0.0 rather than
  // changing its serialized checkpoint graph to integer 0 during JS roundtrip.
  const coordinate=value=>JSON.rawJSON(Number.isInteger(value)?value.toFixed(1):String(value));
  const nodes=source.nodes.map(n=>({id:n.id,x:coordinate(n.x??0),y:coordinate(n.y??0),type:n.type??'waypoint'}));
  const edges=source.edges.map(e=>({from:e.from,to:e.to,weight:e.weight??1,type:e.type??'main',path_class:e.path_class??'',two_way:e.two_way??false,...(e.geometry?{geometry:e.geometry}:{})}));
  for(const e of [...edges])if(e.two_way&&!edges.some(r=>r.from===e.to&&r.to===e.from)) {
    const reverse=structuredClone(e);[reverse.from,reverse.to]=[e.to,e.from];
    if(reverse.geometry)reverse.geometry.segment_lengths_cm.reverse();edges.push(reverse);
  }
  const normalized={...source,nodes,edges,mutex_zones:nodes.filter(n=>n.type==='mutex').map(n=>n.id),charging_stations:nodes.filter(n=>n.type==='charging').map(n=>n.id)};
  const checkpoint=()=>({version:1,map:normalized,mutex:{},fleet:{queue_sequence:0,tasks:[],pending_tasks:[],agvs:[],charge_reservations:{},accepting_tasks:false}});
  const old=checkpoint();old.map={...normalized,nodes:[...normalized.nodes],edges:[...normalized.edges]};
  for(const [id,from,to,x,y,first,second] of [['C1','G1','M1',0,0,70,50],['C2','M2','G6',200,0,50,70],
    ['C3','M3','G1',0,140,50,70],['C4','G6','M4',200,140,70,50]]) {
    old.map.nodes.push({id,x:coordinate(x),y:coordinate(y),type:'waypoint'});
    old.map.edges=old.map.edges.filter(e=>e.from!==from||e.to!==to);
    old.map.edges.push({from,to:id,weight:first,two_way:false},{from:id,to,weight:second,two_way:false});
  }
  old.fleet.agvs=[{id:'01',node:'C1',battery:80,path:[],uncertain_path:['UNKNOWN_BAD'],stopped_nodes:['C1']}];
  const cases=[
    ['command','command.json','invalid-json'],['task','task.json','{"reserved_through":-1}'],
    ['runtime-json','runtime.json','invalid-json'],['runtime-schema','runtime.json','{"version":1}'],
    ['restore-fleet','runtime.json',JSON.stringify({...checkpoint(),fleet:{...checkpoint().fleet,agvs:[{id:'01',node:'S1',battery:80,uncertain_path:[],path:[],stopped_nodes:['C1']}]}})],
    ['restore-mutex','runtime.json',JSON.stringify({...checkpoint(),mutex:{NotAZone:{owner:'01',waiting:[]}}})],
    ['migration-unknown-node','runtime.json',JSON.stringify(old)],
    ['shift','shift.json','{"phase":"BAD"}'],['bad-port',null,null],['bad-token',null,null]
  ];
  for(const [name,file,content] of cases){
    const folder=path.join(dir,name);fs.mkdirSync(folder);const mapPath=path.join(folder,'map.json');fs.writeFileSync(mapPath,JSON.stringify(source));
    if(file)fs.writeFileSync(path.join(folder,file),content);
    const port=await freePort();const env={...process.env,AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(port),
      AGV_MAP_PATH:mapPath,AGV_COMMAND_ID_PATH:path.join(folder,'command.json'),AGV_TASK_ID_PATH:path.join(folder,'task.json'),
      AGV_RUNTIME_STATE_PATH:path.join(folder,'runtime.json'),AGV_SHIFT_STATE_PATH:path.join(folder,'shift.json'),
      AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:'1'};
    for(const key of ['AGV_CORE_TOKEN','AGV_TEST_MODE','AGV_ONE_ROBOT_TRIAL','AGV_TWO_ROBOT_TRIAL','AGV_MONITOR_ONLY_02','AGV_HOME_STATIONS'])delete env[key];
    const key=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[key]='C:/msys64/ucrt64/bin'+path.delimiter+env[key];
    if(name==='bad-port')env.AGV_CORE_WS_PORT='bad';if(name==='bad-token')env.AGV_CORE_TOKEN='short';
    let output='';const child=spawn(process.env.AGV_CORE_BIN||path.join(root,'.qa/core-hardening-20261025/agv_core.exe'),['--mode','active-both'],{cwd:root,env,windowsHide:true});
    child.stdout.on('data',d=>output+=d);child.stderr.on('data',d=>output+=d);
    const timer=setTimeout(()=>child.kill(),10000);const [code,signal]=await once(child,'exit');clearTimeout(timer);
    assert.equal(signal,null,output);assert.equal(code,1,output);assert.match(output,/\[STARTUP\]/,output);
    assert.doesNotMatch(output,/terminate called|MASTER SERVER DANG CHAY/);
    if(file)assert.equal(fs.readFileSync(path.join(folder,file),'utf8'),content);
    // Listener guard must have relinquished the port after the failed startup.
    const probe=net.createServer();probe.listen(port,'127.0.0.1');await once(probe,'listening');await new Promise(r=>probe.close(r));
  }
  console.log('PASS startup: ten corrupt file/config/restore/migration cases exit 1, explicit errors, input preserved, listener closed');
  console.log('Artifacts: '+dir);
}
main().catch(e=>{console.error(e);process.exitCode=1;});
