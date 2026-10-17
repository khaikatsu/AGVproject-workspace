// Menu/F5 va loc xe: broker, map, counter, cong rieng; khong dieu khien xe that.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const {spawn} = require('node:child_process');
const {once} = require('node:events');
const {randomUUID} = require('node:crypto');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {MqttFixture} = require('./helpers/mqtt-client');
const root = path.resolve(__dirname,'..');
const executable = process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe');
const bin = process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
const delay = ms => new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout=8000) {
  const end=Date.now()+timeout;
  while(Date.now()<end) {if(await fn()) return;await delay(25);}
  throw Error('Core mode condition timed out');
}
async function freePort() {
  const server=net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
  const port=server.address().port;await new Promise(resolve=>server.close(resolve));return port;
}
async function main() {
  fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.join(root,'.qa/core-mode-test-'));
  const mqttPort=await freePort();
  const config=path.join(directory,'broker.conf');
  fs.writeFileSync(config,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const baseEnv={...process.env,AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_HOST:'127.0.0.1',
    AGV_ONE_ROBOT_TRIAL:'1',AGV_TWO_ROBOT_TRIAL:'1',AGV_MONITOR_ONLY_02:'1'};
  delete baseEnv.AGV_CORE_TOKEN;delete baseEnv.AGV_TEST_MODE;
  const pathKey=Object.keys(baseEnv).find(key=>key.toLowerCase()==='path') || 'PATH';
  baseEnv[pathKey]=bin+path.delimiter+baseEnv[pathKey];
  const children=[];let output='',peer,mqtt,heartbeat;
  function child(file,args,env=baseEnv) {
    const proc=spawn(file,args,{env,cwd:root,windowsHide:true});children.push(proc);proc.captured='';
    const capture=data=>{proc.captured+=data;output+=data;};
    proc.stdout.on('data',capture);proc.stderr.on('data',capture);proc.on('error',error=>output+=error.message);
    return proc;
  }
  function caseEnv(name,port) {
    const folder=path.join(directory,name);fs.mkdirSync(folder);
    fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(folder,'map.json'));
    return {...baseEnv,AGV_CORE_WS_PORT:String(port),AGV_HOME_STATIONS:'{"01":"S1","02":"S2"}',
      AGV_MAP_PATH:path.join(folder,'map.json'),AGV_COMMAND_ID_PATH:path.join(folder,'counter.json'),
      AGV_TASK_ID_PATH:path.join(folder,'task.json'),AGV_RUNTIME_STATE_PATH:path.join(folder,'runtime.json'),
      AGV_SHIFT_STATE_PATH:path.join(folder,'shift.json')};
  }
  async function stop(proc) {
    if(proc.exitCode===null && proc.signalCode===null) proc.kill();
    await until(()=>proc.exitCode!==null || proc.signalCode!==null);
  }
  try {
    const broker=child(path.join(bin,process.platform==='win32'?'mosquitto.exe':'mosquitto'),['-c',config,'-v']);
    await until(()=>broker.captured.includes('running'));
    // Cancel/EOF must not touch counters or connect to MQTT.
    for(const [name,input] of [['cancel','0\n'],['eof','']]) {
      const env=caseEnv(name,await freePort());const core=child(executable,['--select-mode'],env);
      core.stdin.end(input);await until(()=>core.exitCode!==null);
      assert.equal(core.exitCode,0);assert.equal(fs.existsSync(env.AGV_COMMAND_ID_PATH),false);
      assert.equal(fs.existsSync(env.AGV_SHIFT_STATE_PATH),false);
    }
    for(const [choice,mode,id,active] of [['1','active01','01',['01']],['2','active02','02',['02']],['3','active-both','',['01','02']]]) {
      const port=await freePort(),env=caseEnv(mode,port),core=child(executable,['--select-mode'],env);
      await until(()=>core.captured.includes('Nhap lua chon'));
      assert.equal(fs.existsSync(env.AGV_COMMAND_ID_PATH),false);
      core.stdin.write('wrong\n');await until(()=>core.captured.includes('Lua chon khong hop le'));
      assert.equal(fs.existsSync(env.AGV_COMMAND_ID_PATH),false);
      core.stdin.end(` ${choice} \n`);
      let state;const results=new Map();
      peer=new WebSocket(`ws://127.0.0.1:${port}`);peer.on('error',()=>{});
      peer.on('message',raw=>{
        const message=JSON.parse(raw);
        if(message.type==='command_result') {results.set(message.request_id,message);state=message.state;}
        else state=message.type==='state_delta'?{...state,...message.changes,state_seq:message.state_seq}:message;
      });
      // Retry connecting while Core reserves files and starts MQTT.
      await until(()=> {
        if(peer.readyState===WebSocket.CLOSED) {
          peer=new WebSocket(`ws://127.0.0.1:${port}`);peer.on('error',()=>{});
          peer.on('message',raw=>{const m=JSON.parse(raw);if(m.type==='command_result'){results.set(m.request_id,m);state=m.state;}
            else state=m.type==='state_delta'?{...state,...m.changes,state_seq:m.state_seq}:m;});
        }
        return state?.mqtt_connected;
      });
      assert.equal(state.shift.test_mode,'normal');assert.equal(state.shift.trial_robot_id,'');
      assert.equal(state.map.nodes.length,22);
      assert.equal(state.map.nodes.some(n=>/^C[1-4]$/.test(n.id)),false);
      assert.equal(state.map.edges.filter(e=>e.geometry && e.weight===135).length,4);
      assert.equal(state.shift.operation_profile,'scoped');assert.deepEqual(state.shift.active_robot_ids,active);
      assert.equal(state.shift.supervised_trial,false);assert.equal(state.shift.single_robot_trial,false);
      assert.equal(state.shift.two_robot_trial,false);assert.equal(state.shift.can_start,false);
      assert.equal(state.fleet.battery_gate_enabled,false);
      assert.equal(state.fleet.automatic_charging_enabled,false);
      async function command(body) {
        const request_id=randomUUID();peer.send(JSON.stringify({...body,request_id}));
        await until(()=>results.has(request_id));return results.get(request_id);
      }
      for(const target of ['C1','C2','C3','C4'])
        assert.equal((await command({cmd:'add_task',target})).ok,false);
      const commands=[];mqtt=new MqttFixture(mqttPort);mqtt.on('error',()=>{});
      await until(()=>mqtt.connected);mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
      mqtt.on('publish',(topic,payload)=>{
        if(!topic.endsWith('/command')) return;
        const c=JSON.parse(payload),robot=topic.split('/')[1];commands.push({robot,...c});
        mqtt.publish(`agv/${robot}/ack`,String(c.cmd_id));
      });
      const nodes={'01':'S1','02':'S2'};
      if(id) nodes[id==='01'?'02':'01']='UNKNOWN';
      let battery='?';
      const telemetry=()=>{for(const robot of ['01','02']) mqtt.publish(`agv/${robot}/telemetry`,`${nodes[robot]}|${battery}|0`);};
      telemetry();heartbeat=setInterval(telemetry,150);
      await until(()=>state?.fleet?.agvs?.length===2);
      if(id) {
        await delay(300);assert.equal(state.shift.can_start,false);
        nodes[id==='01'?'02':'01']=id==='01'?'S2':'S1';telemetry();
      }
      await until(()=>state.shift.can_start);
      assert.equal(state.fleet.agvs.every(a=>!a.battery_measured),true,'unknown battery does not block opening shift');
      battery='0';telemetry();
      await until(()=>state.fleet.agvs.every(a=>a.battery===0) && state.shift.can_start);
      assert.equal((await command({cmd:'start_shift'})).ok,true);
      // Both upgraded firmwares bind independent streams, even with identical boot/session/seq.
      const samples=new Map();
      for(const robot of active){
        mqtt.publish(`agv/${robot}/diagnostic`,JSON.stringify({version:1,agvId:robot,batteryCalibrated:true,batteryPercentage:0,
          encoderSupported:true,encoderCoreSessionId:state.core_session_id,encoderBootId:1234,session:1,uptimeMs:1000,
          node:nodes[robot],state:'AGV_IDLE',error:'NONE',motor:'STOP',taskActive:false,wifi:true,mqtt:true,rfidAgeMs:0,leftDuty:0,rightDuty:0}));
        const sample={schema_version:1,robot_id:robot,core_session_id:state.core_session_id,boot_id:1234,connection_session:1,
          seq:1,uptime_ms:1000,left_ticks:10,right_ticks:10,pulses_per_100cm:161,route_cmd_id:0,path_index:0,
          from_node:nodes[robot],to_node:'',tracking:false,translating:false,reverse:false,distance_cm:0,speed_cm_s:0};
        samples.set(robot,sample);mqtt.publish(`agv/${robot}/odometry`,JSON.stringify(sample));
      }
      await until(()=>active.every(id=>state.fleet.agvs.find(a=>a.id===id)?.odometry?.seq===1)).catch(error=>{
        console.error(JSON.stringify({mode,session:state.core_session_id,robots:state.fleet.agvs,samples:[...samples]}));
        console.error(output.split('\n').filter(line=>line.includes('[LOI]')||line.includes('Encoder')).slice(-20).join('\n'));throw error;
      });
      const target=active[0],spoof={...samples.get(target),robot_id:target==='01'?'02':'01',seq:2};
      mqtt.publish(`agv/${target}/odometry`,JSON.stringify(spoof));await delay(150);
      assert.equal(state.fleet.agvs.find(a=>a.id===target).odometry.seq,1,'topic/robot identity mismatch rejected');
      if(id) {
        const other=id==='01'?'02':'01';
        assert.equal((await command({cmd:'direct_goto',agv_id:other,target:other==='01'?'GS1':'M3'})).ok,false);
        // An unselected robot cannot acquire a new mutex.
        mqtt.publish(`agv/${other}/mutex_request`,'M2');await delay(300);
        assert.equal(state.mutex.M2.owner,'');
        const task=await command({cmd:'add_task',target:id==='01'?'GS1':'M3'});assert.equal(task.ok,true);
        await until(()=>commands.some(c=>c.robot===id && c.cmd==='GOTO'));
        assert.equal(state.fleet.tasks.find(t=>t.task_id===task.data.task_id).assigned_agv,id);
        assert.equal(commands.some(c=>c.robot!==id && ['GOTO','CHARGE_START'].includes(c.cmd)),false);
      } else {
        assert.equal((await command({cmd:'direct_goto',agv_id:'01',target:'GS1'})).ok,true);
        assert.equal((await command({cmd:'direct_goto',agv_id:'02',target:'M3'})).ok,true);
        await until(()=>['01','02'].every(robot=>commands.some(c=>c.robot===robot && c.cmd==='GOTO')));
        mqtt.publish('agv/01/mutex_request','M2');await until(()=>state.mutex.M2.owner==='01');
        mqtt.publish('agv/02/mutex_request','M2');await until(()=>state.mutex.M2.waiting.includes('02'));
        assert.equal(state.mutex.M2.owner,'01');
      }
      assert.equal(commands.some(c=>c.cmd==='CHARGE_START'),false,'manual charging never emits CHARGE_START');
      clearInterval(heartbeat);heartbeat=null;mqtt.close();mqtt=null;peer.terminate();peer=null;
      const counter=JSON.parse(fs.readFileSync(env.AGV_COMMAND_ID_PATH,'utf8')).reserved_through;
      await stop(core);
      // Noninteractive argument mode reuses the counter instead of rewinding it.
      const restarted=child(executable,['--mode',mode],env);
      await until(()=>fs.existsSync(env.AGV_COMMAND_ID_PATH) && JSON.parse(fs.readFileSync(env.AGV_COMMAND_ID_PATH,'utf8')).reserved_through>counter);
      await stop(restarted);
    }
    const env=caseEnv('normal',await freePort()),normal=child(executable,['--mode','normal'],env);
    let normalState;
    await until(()=>{
      if(!peer || peer.readyState===WebSocket.CLOSED) {
        peer=new WebSocket(`ws://127.0.0.1:${env.AGV_CORE_WS_PORT}`);peer.on('error',()=>{});
        peer.on('message',raw=>{const m=JSON.parse(raw);normalState=m.type==='state_delta'?{...normalState,...m.changes}:m;});
      }
      return normalState?.mqtt_connected;
    });
    assert.equal(normalState.shift.test_mode,'normal');assert.equal(normalState.shift.supervised_trial,false);
    assert.equal(normalState.shift.single_robot_trial,false);assert.equal(normalState.shift.two_robot_trial,false);
    assert.equal(normalState.fleet.battery_gate_enabled,false);assert.equal(normalState.fleet.automatic_charging_enabled,false);
    peer.terminate();peer=null;await stop(normal);
    console.log('PASS unified Core menu: scopes 01/02/both, temporary unknown/0% battery bypass, manual charging, dual mutex, modes and counter continuation');
    console.log('Artifacts: '+directory);
  } catch(error) {console.error(output.slice(-6500));throw error;}
  finally {
    clearInterval(heartbeat);if(mqtt) mqtt.close();if(peer) peer.terminate();
    for(const proc of children) if(proc.exitCode===null && proc.signalCode===null) await stop(proc);
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
