// Read an old checkpoint; all runs use separate broker/map/counter/runtime paths.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const net=require('node:net');
const {spawn}=require('node:child_process');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {MqttFixture}=require('./helpers/mqtt-client');
const root=path.resolve(__dirname,'..');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn){const end=Date.now()+10000;while(Date.now()<end){if(fn())return;await pause(30);}throw Error('Migration startup timed out');}
async function port(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function main(){
  assert.ok(process.argv[2],'Pass a v3 checkpoint fixture path');
  const source=fs.readFileSync(process.argv[2]);const saved=JSON.parse(source);
  const dir=fs.mkdtempSync(path.join(root,'.qa/runtime-migration-integration-'));
  const bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  const env={...process.env};const pk=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pk]=bin+path.delimiter+env[pk];
  for(const key of ['AGV_CORE_TOKEN','AGV_TEST_MODE','AGV_ONE_ROBOT_TRIAL','AGV_TWO_ROBOT_TRIAL','AGV_MONITOR_ONLY_02','AGV_HOME_STATIONS']) delete env[key];
  const mqttPort=await port(),config=path.join(dir,'broker.conf');
  fs.writeFileSync(config,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const children=[];let mqtt,peer,log='';
  function child(file,args,childEnv=env){const p=spawn(file,args,{env:childEnv,cwd:root,windowsHide:true});children.push(p);p.captured='';p.on('error',e=>log+=e);for(const stream of [p.stdout,p.stderr])stream.on('data',d=>{p.captured+=d;log+=d;});return p;}
  async function stop(p){if(p.exitCode===null&&p.signalCode===null)p.kill();await until(()=>p.exitCode!==null||p.signalCode!==null);}
  try{
    const broker=child(path.join(bin,'mosquitto.exe'),['-c',config,'-v']);await until(()=>broker.captured.includes('running'));
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',()=>{});await until(()=>mqtt.connected);mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    const commands=[];mqtt.on('publish',(topic,payload)=>{if(topic.endsWith('/command')||topic.endsWith('/mutex_grant')) commands.push({topic,payload});});
    for(const mode of ['active01','active02','active-both']){
      const folder=path.join(dir,mode);fs.mkdirSync(folder);const runtime=path.join(folder,'runtime.json');fs.writeFileSync(runtime,source);
      const mapPath=path.join(folder,'map.json');fs.copyFileSync(path.join(root,'data/map_layout.json'),mapPath);
      const wsPort=await port();const childEnv={...env,AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(wsPort),
        AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_MAP_PATH:mapPath,AGV_RUNTIME_STATE_PATH:runtime,
        AGV_COMMAND_ID_PATH:path.join(folder,'command.json'),AGV_TASK_ID_PATH:path.join(folder,'task.json'),AGV_SHIFT_STATE_PATH:path.join(folder,'shift.json')};
      for(let restart=0;restart<2;restart++){
        const p=child(process.env.AGV_CORE_BIN||path.join(root,'build/agv_core.exe'),['--mode',mode],childEnv);let state;
        await until(()=>{
          if(!peer||peer.readyState===WebSocket.CLOSED){peer=new WebSocket(`ws://127.0.0.1:${wsPort}`);peer.on('error',()=>{});peer.on('message',raw=>{const m=JSON.parse(raw);state=m.type==='state_delta'?{...state,...m.changes,state_seq:m.state_seq}:m;});}
          return state?.mqtt_connected===true;
        });
        assert.equal(state.map.nodes.length,22);assert.equal(state.map.edges.length,35);
        assert.equal(state.map.nodes.some(n=>/^C[1-4]$/.test(n.id)),false);
        assert.equal(state.map.edges.filter(e=>e.geometry).length,4);
        assert.equal(state.fleet.tasks.length,saved.fleet.tasks.length);
        assert.equal(state.fleet.pending_tasks.length,0);assert.equal(state.shift.can_start,false);
        for(const [zone,claim] of Object.entries(saved.mutex)){
          assert.equal(state.mutex[zone].owner,claim.owner);
          if(claim.owner){assert.equal(state.mutex[zone].grant_id,claim.grant_id);assert.equal(state.mutex[zone].recovery_required,true);}
        }
        await pause(300);assert.deepEqual(commands,[]);
        peer.terminate();peer=null;await stop(p);
      }
      const backups=fs.readdirSync(folder).filter(n=>n.startsWith('runtime.json.before-rounded-map-v4'));
      assert.equal(backups.length,1);assert.deepEqual(fs.readFileSync(path.join(folder,backups[0])),source);
    }
    assert.deepEqual(fs.readFileSync(process.argv[2]),source);
    console.log('PASS migration startup + restart for active01/active02/active-both; 22 nodes; history/owner/token/backup preserved; no commands');
    console.log('Artifacts: '+dir);
  }catch(e){console.error(log.slice(-4000));throw e;}
  finally{peer?.terminate();if(mqtt)mqtt.socket.destroy();for(const p of children)if(p.exitCode===null&&p.signalCode===null)p.kill();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
