// S1: Core/broker rieng, ACK/telemetry/grant nhu MCU; khong dieu khien xe that.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {randomUUID}=require('node:crypto');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {MqttFixture}=require('./helpers/mqtt-client');
const root=path.resolve(__dirname,'..');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,timeout=7000){const end=Date.now()+timeout;while(Date.now()<end){if(await fn())return;await delay(25);}throw Error('GS2 audit timeout');}
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function main(){
  fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.join(root,'.qa/server-gs2-test-'));
  const bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  const mqttPort=await freePort(),children=[],events=[];
  const env={...process.env,AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_HOST:'127.0.0.1'};
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  delete env.AGV_CORE_TOKEN;delete env.AGV_ONE_ROBOT_TRIAL;delete env.AGV_TWO_ROBOT_TRIAL;delete env.AGV_MONITOR_ONLY_02;
  let output='',mqtt,peer,heartbeat;
  function child(file,args,childEnv=env){const p=spawn(file,args,{env:childEnv,cwd:root,windowsHide:true});children.push(p);p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;}
  async function stop(p){if(p.exitCode===null&&p.signalCode===null)p.kill();await until(()=>p.exitCode!==null||p.signalCode!==null);}
  try{
    const config=path.join(directory,'broker.conf');fs.writeFileSync(config,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
    child(path.join(bin,'mosquitto.exe'),['-c',config,'-v']);
    await until(()=>new Promise(resolve=>{const s=net.connect(mqttPort,'127.0.0.1');s.once('connect',()=>{s.destroy();resolve(true);});s.once('error',()=>resolve(false));}));
    for(const mode of ['robot02','both']){
      const folder=path.join(directory,mode);fs.mkdirSync(folder);fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(folder,'map.json'));
      const corePort=await freePort();
      const core=child(process.env.AGV_CORE_BIN||path.join(root,'build/agv_core.exe'),['--mode',mode],{...env,
        AGV_CORE_WS_PORT:String(corePort),AGV_HOME_STATIONS:'{"01":"S1","02":"S2"}',AGV_MAP_PATH:path.join(folder,'map.json'),
        AGV_COMMAND_ID_PATH:path.join(folder,'counter.json'),AGV_TASK_ID_PATH:path.join(folder,'task.json'),
        AGV_RUNTIME_STATE_PATH:path.join(folder,'runtime.json'),AGV_SHIFT_STATE_PATH:path.join(folder,'shift.json')});
      let state;const results=new Map(),commands=[],grants=[];
      function connect(){peer=new WebSocket(`ws://127.0.0.1:${corePort}`);peer.on('error',()=>{});peer.on('message',raw=>{const m=JSON.parse(raw);
        if(m.type==='command_result'){results.set(m.request_id,m);state=m.state;}
        else state=m.type==='state_delta'?{...state,...m.changes,state_seq:m.state_seq}:m;});}
      connect();await until(()=>{if(peer.readyState===WebSocket.CLOSED)connect();return state?.mqtt_connected;});
      const command=async body=>{const request_id=randomUUID();peer.send(JSON.stringify({...body,request_id}));await until(()=>results.has(request_id));return results.get(request_id);};
      mqtt=new MqttFixture(mqttPort);mqtt.on('error',()=>{});await until(()=>mqtt.connected);mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
      mqtt.on('publish',(topic,payload)=>{
        if(topic.endsWith('/command')){const c=JSON.parse(payload),robot=topic.split('/')[1];commands.push({robot,...c});events.push({mode,topic,payload:c});mqtt.publish(`agv/${robot}/ack`,String(c.cmd_id));}
        if(topic.endsWith('/mutex_grant')){const g=JSON.parse(payload),robot=topic.split('/')[1];grants.push({robot,...g});events.push({mode,topic,payload:g});mqtt.publish(`agv/${robot}/mutex_ack`,payload);
          // Robot 01 is held at S1 in this fixture and returns its unused M3 grant.
          if(robot==='01'&&g.zone==='M3')mqtt.publish('agv/01/mutex_release',payload);}
      });
      const nodes={'01':'S1','02':'S2'};
      const telemetry=()=>{for(const robot of ['01','02'])mqtt.publish(`agv/${robot}/telemetry`,`${nodes[robot]}|?|0`);};
      heartbeat=setInterval(telemetry,150);telemetry();await until(()=>state.shift.can_start);
      assert.equal((await command({cmd:'start_shift'})).ok,true);
      if(mode==='both')assert.equal((await command({cmd:'direct_goto',agv_id:'01',target:'GS1'})).ok,true);
      const first=await command({cmd:'direct_goto',agv_id:'02',target:'M3'});assert.equal(first.ok,true);
      await until(()=>commands.some(c=>c.robot==='02'&&c.cmd==='GOTO'));
      const route=commands.find(c=>c.robot==='02'&&c.cmd==='GOTO');assert.deepEqual(route.path,['GS2','M3']);
      const baseline=commands.filter(c=>c.robot==='02').length;
      nodes['02']='GS2';telemetry();await until(()=>state.fleet.agvs.find(a=>a.id==='02')?.node==='GS2');
      mqtt.publish('agv/02/mutex_request','M3');await until(()=>state.mutex.M3.owner==='02'&&state.mutex.M3.grant_acked);
      const grantId=state.mutex.M3.grant_id;
      mqtt.publish('agv/02/mutex_request','M3');await until(()=>grants.filter(g=>g.robot==='02'&&g.zone==='M3').length>=2);
      assert.equal(grants.filter(g=>g.robot==='02'&&g.zone==='M3').at(-1).grant_id,grantId);
      mqtt.publish('agv/01/mutex_request','M3');await delay(250);assert.equal(state.mutex.M3.owner,'02');
      const queued=await command({cmd:'add_task',target:'K1'});assert.equal(queued.ok,true);
      await delay(3400); // Exceeds command retry timeout: ACK prevents duplicate GOTO.
      assert.equal(commands.filter(c=>c.robot==='02').length,baseline,'No unsolicited STOP/WAIT or duplicate GOTO at GS2');
      assert.deepEqual(state.fleet.agvs.find(a=>a.id==='02').path,['M3']);
      assert.equal(state.fleet.agvs.find(a=>a.id==='02').state,'WORKING');
      assert.equal(state.fleet.tasks.find(t=>t.task_id===queued.data.task_id).status,'pending');
      clearInterval(heartbeat);heartbeat=null;await delay(250);
      mqtt.publish('agv/02/status','offline');await until(()=>state.fleet.agvs.find(a=>a.id==='02').state==='OFFLINE');
      heartbeat=setInterval(telemetry,150);telemetry();await until(()=>state.fleet.agvs.find(a=>a.id==='02').state==='ERROR');
      assert.equal(state.mutex.M3.owner,'02');
      const beforeClear=commands.length;
      const rejected=await command({cmd:'clear_error',id:'02'});assert.equal(rejected.ok,false);assert.match(rejected.error,/tram rieng/);
      await delay(300);assert.equal(commands.length,beforeClear);
      nodes['02']='S2';telemetry();await until(()=>state.fleet.agvs.find(a=>a.id==='02').node==='S2');
      assert.equal((await command({cmd:'clear_error',id:'02'})).ok,true);
      await until(()=>commands.some((c,i)=>i>=beforeClear&&c.robot==='02'&&c.cmd==='RESET_FAULT'));
      await delay(300);assert.equal(state.fleet.agvs.find(a=>a.id==='02').state,'ERROR');
      assert.equal(commands.slice(beforeClear).some(c=>c.cmd==='GOTO'),false,'ACK must not authorize motion');
      const diagnostic={version:1,agvId:'02',batteryCalibrated:false,node:'S2',motor:'STOP',leftDuty:0,rightDuty:0,rfidAgeMs:0,
        wifi:true,mqtt:true,taskActive:false,state:'AGV_SAFE_STOP',error:'NETWORK_UNAVAILABLE'};
      mqtt.publish('agv/02/diagnostic',JSON.stringify(diagnostic));await delay(450);
      assert.equal(commands.slice(beforeClear).some(c=>c.cmd==='GOTO'),false);
      mqtt.publish('agv/02/diagnostic',JSON.stringify({...diagnostic,state:'AGV_IDLE',error:'NONE'}));
      await until(()=>state.fleet.tasks.find(t=>t.task_id===queued.data.task_id).status==='dispatched');
      assert.equal(state.fleet.tasks.find(t=>t.task_id===queued.data.task_id).assigned_agv,'02');
      assert.equal(commands.at(-1).cmd,'GOTO');assert.equal(commands.at(-1).path[0],'GS2');
      assert.equal(state.fleet.tasks.find(t=>t.task_id===first.data.task_id).status,'interrupted');
      fs.writeFileSync(path.join(folder,'final-state.json'),JSON.stringify(state,null,2));
      clearInterval(heartbeat);heartbeat=null;mqtt.close();mqtt=null;peer.terminate();peer=null;await stop(core);
      console.log(`PASS ${mode}: S2-GS2-M3 route/grant/ACK, no spurious stop or duplicate GOTO, offline hold, reset confirmation`);
    }
    fs.writeFileSync(path.join(directory,'events.json'),JSON.stringify(events,null,2));console.log('Artifacts: '+directory);
  }catch(e){console.error(output.slice(-4500));throw e;}
  finally{clearInterval(heartbeat);if(mqtt)mqtt.close();if(peer)peer.terminate();for(const p of children)await stop(p);}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
