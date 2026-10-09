// Broker/Core/gateway va hai robot MQTT gia lap tren port + du lieu rieng.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), net = require('node:net');
const {spawn} = require('node:child_process'), {once} = require('node:events');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');
const pause = ms => new Promise(resolve => setTimeout(resolve,ms));
async function until(check, timeout = 8000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {if (await check()) return;await pause(40);}
  throw new Error('Recovery condition timed out');
}
async function freePort() {
  const server = net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
  const port = server.address().port;await new Promise(resolve => server.close(resolve));return port;
}
async function available(port) {
  return new Promise(resolve => {const socket=net.connect(port,'127.0.0.1');
    socket.once('connect',()=>{socket.destroy();resolve(true);});socket.once('error',()=>resolve(false));});
}
async function main() {
  const root=path.resolve(__dirname,'..'),baseDir=path.join(root,'build/recovery-check');
  fs.mkdirSync(baseDir,{recursive:true});const directory=fs.mkdtempSync(path.join(baseDir,'mqtt-'));
  const mqttPort=await freePort(),corePort=await freePort();
  const mapPath=path.join(directory,'map.json');fs.copyFileSync(path.join(root,'data/map_layout.json'),mapPath);
  const brokerConfig=path.join(directory,'broker.conf');
  fs.writeFileSync(brokerConfig,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin=process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env={...process.env,AGV_TWO_ROBOT_TRIAL:'1',AGV_HOME_STATIONS:JSON.stringify({'01':'S1','02':'S2'}),
    AGV_MAP_PATH:mapPath,AGV_COMMAND_ID_PATH:path.join(directory,'counter.json'),
    AGV_SHIFT_STATE_PATH:path.join(directory,'shift.json'),AGV_RUNTIME_STATE_PATH:path.join(directory,'runtime.json'),
    AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path') || 'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const db=createDatabase(path.join(directory,'test.db')),children=[],commands=[];
  let output='',gateway,mqtt;
  function child(command,args=[]) {
    const process=spawn(command,args,{env,cwd:directory,windowsHide:true});children.push(process);
    process.stdout.on('data',data=>output+=data);process.stderr.on('data',data=>output+=data);
    process.on('error',error=>output+=error.message);return process;
  }
  try {
    child(path.join(bin,process.platform==='win32'?'mosquitto.exe':'mosquitto'),['-c',brokerConfig]);
    await until(()=>available(mqttPort));
    child(process.env.AGV_CORE_BIN || path.join(baseDir,'agv_core.exe'));
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:100});
    const address=await gateway.start(0,'127.0.0.1');
    const state=async()=>(await fetch(`http://127.0.0.1:${address.port}/api/state`)).json();
    const robot=async id=>(await state()).fleet.agvs.find(item=>item.id===id);
    await until(async()=>{const s=await state();return s.core_ready&&s.mqtt_connected;});
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',error=>output+=error.message);
    await until(()=>mqtt.connected);
    mqtt.on('publish',(topic,payload)=>{
      const id=topic.split('/')[1];
      if (topic.endsWith('/command')) {
        const command=JSON.parse(payload);commands.push({id,...command});
        mqtt.publish(`agv/${id}/ack`,String(command.cmd_id));
      }
      if (topic.endsWith('/mutex_grant')) mqtt.publish(`agv/${id}/mutex_ack`,payload);
    });
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed);
    const telemetry=(id,node)=>mqtt.publish(`agv/${id}/telemetry`,`${node}|?|0`);
    const diagnostic=(id,node,stateName,error,motor='STOP',taskActive=false)=>
      mqtt.publish(`agv/${id}/diagnostic`,JSON.stringify({version:1,agvId:id,session:1,uptimeMs:100,
        batteryCalibrated:true,batteryPercentage:50,node,rfidAgeMs:0,state:stateName,error,motor,taskActive,
        leftDuty:0,rightDuty:0,wifi:true,mqtt:true}));
    telemetry('01','S1');telemetry('02','S2');
    await until(async()=>{const s=await state();return s.fleet.agvs.length===2&&s.shift.phase==='CLOSED';});
    mqtt.publish('agv/01/mutex_request','M2');
    await until(async()=>(await state()).mutex.M2.owner==='01');
    mqtt.publish('agv/01/status','offline');
    await until(async()=>(await robot('01')).state==='OFFLINE');
    assert.equal((await state()).mutex.M2.owner,'01','LWT retains occupied zone');
    telemetry('01','M2');diagnostic('01','M2','AGV_SAFE_STOP','NETWORK_UNAVAILABLE');
    await until(async()=>(await robot('01')).state==='ERROR');
    assert.equal((await state()).mutex.M2.owner,'01','tag inside zone cannot release');
    telemetry('01','S1');
    await until(async()=>(await robot('01')).node==='S1');
    assert.equal((await state()).mutex.M2.owner,'01','telemetry alone cannot release');
    diagnostic('01','S1','AGV_SAFE_STOP','NETWORK_UNAVAILABLE');
    await until(async()=>(await state()).mutex.M2.owner==='');
    await pause(350);
    diagnostic('01','S1','AGV_SAFE_STOP','NETWORK_UNAVAILABLE');
    await until(()=>commands.some(command=>command.id==='01'&&command.cmd==='RESET_FAULT'));
    assert.equal((await robot('01')).state,'ERROR','MCU fault still latched');
    diagnostic('01','S1','AGV_IDLE','NONE');
    await until(async()=>(await robot('01')).state==='IDLE');
    await until(async()=>(await state()).shift.phase==='CLOSED');
    assert.equal((await state()).shift.can_start,true);
    mqtt.publish('agv/02/mutex_request','M3');await until(async()=>(await state()).mutex.M3.owner==='02');
    mqtt.publish('agv/02/error','LINE_LOST');await until(async()=>(await robot('02')).state==='ERROR');
    diagnostic('02','S2','AGV_SAFE_STOP','LINE_LOST');
    await until(async()=>(await state()).mutex.M3.owner==='');
    assert.equal((await robot('02')).state,'ERROR');
    assert.equal(commands.filter(command=>command.id==='02'&&command.cmd==='RESET_FAULT').length,0);
    console.log('PASS recovery: LWT retains zone, station telemetry + stopped diagnostic release it, network reset once, healthy diagnostic clears Core; line fault stays latched');
    console.log('Evidence: '+directory);
  } catch(error) {fs.writeFileSync(path.join(directory,'failure.log'),output);throw error;}
  finally {
    mqtt?.close();if(gateway) await gateway.close();db.close();
    for(const process of children) if(process.exitCode===null) process.kill();
    fs.writeFileSync(path.join(directory,'runtime.log'),output);
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
