// Broker/cổng/counter riêng: core chạy trùng không được chiếm MQTT hoặc sửa counter.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const {spawn} = require('node:child_process');
const {once} = require('node:events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const root = path.resolve(__dirname,'..');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout = 7000) {
  const end = Date.now()+timeout;
  while (Date.now()<end) {if (await fn()) return;await pause(40);}
  throw new Error('Startup condition timed out');
}
async function freePort() {
  const server = net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
  const port = server.address().port;await new Promise(resolve=>server.close(resolve));return port;
}
async function main() {
  fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const directory = fs.mkdtempSync(path.join(root,'.qa/startup-test-'));
  const cwd = path.join(directory,'nested');fs.mkdirSync(cwd);
  const mqttPort = await freePort(),corePort = await freePort();
  const mapPath = path.join(directory,'map.json'),counterPath = path.join(directory,'counter.json');
  fs.copyFileSync(path.join(root,'data/map_layout.json'),mapPath);
  const brokerPath = path.join(directory,'broker.conf');
  fs.writeFileSync(brokerPath,`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin = process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env = {...process.env,AGV_HOME_STATIONS:'{}',AGV_MAP_PATH:mapPath,AGV_COMMAND_ID_PATH:counterPath,
    AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  const pathKey = Object.keys(env).find(key=>key.toLowerCase()==='path') || 'PATH';
  env[pathKey] = bin+path.delimiter+env[pathKey];
  const children = [],sockets = [];let log = '',brokerLog = '';
  function child(command,args = [],childEnv = env) {
    const process = spawn(command,args,{env:childEnv,cwd,windowsHide:true});children.push(process);
    process.captured = '';
    const capture = data=>{process.captured+=data;log+=data;};
    process.stdout.on('data',capture);process.stderr.on('data',capture);
    process.on('error',error=>log+=error.message);return process;
  }
  async function ready() {
    let peer,latest,states = [];
    await until(async ()=>{
      if (!peer || peer.readyState === WebSocket.CLOSED) {
        peer = new WebSocket(`ws://127.0.0.1:${corePort}`);sockets.push(peer);
        peer.on('error',()=>{});
        peer.on('message',raw=>{const incoming=JSON.parse(raw);latest=incoming.type==='state_delta' ? {...latest,...incoming.changes,state_seq:incoming.state_seq} : incoming;states.push(latest);});
      }
      return latest?.mqtt_connected;
    });
    return {peer,states,get state(){return latest;}};
  }
  try {
    const broker = child(path.join(bin,'mosquitto.exe'),['-c',brokerPath,'-v']);
    broker.stderr.on('data',data=>brokerLog+=data);
    await until(()=>brokerLog.includes('running'));
    const core = child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'));
    const connection = await ready();
    const readyIndex = connection.states.length-1;
    assert.equal(connection.state.map.nodes.length,26);
    const counterBefore = fs.readFileSync(counterPath,'utf8');
    const sessionBefore = connection.state.core_session_id;
    // Chạy wrapper từ thư mục khác; nó phải về gốc và vẫn bị core đang chạy từ chối.
    const duplicate = child('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(root,'scripts/run-core.ps1')]);
    await until(()=>duplicate.exitCode !== null);
    assert.notEqual(duplicate.exitCode,0);
    assert.match(duplicate.captured,/Bind that bai/);
    assert.ok(duplicate.captured.includes('Working directory: '+root));
    assert.equal(fs.readFileSync(counterPath,'utf8'),counterBefore);
    const snapshotsBefore = connection.states.length;
    await until(()=>connection.states.length >= snapshotsBefore+4);
    assert.equal(connection.state.core_session_id,sessionBefore);
    assert.ok(connection.states.slice(readyIndex).every(state=>state.mqtt_connected));
    assert.equal((brokerLog.match(/New client connected[^\n]*AGV_MasterServer/g)||[]).length,1);
    connection.peer.terminate();core.kill();await until(()=>core.exitCode !== null || core.signalCode !== null);
    const restartedCore = child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'));
    const restarted = await ready();
    assert.notEqual(restarted.state.core_session_id,sessionBefore);
    assert.ok(JSON.parse(fs.readFileSync(counterPath,'utf8')).reserved_through > JSON.parse(counterBefore).reserved_through);
    restarted.peer.terminate();restartedCore.kill();
    await until(()=>restartedCore.exitCode !== null || restartedCore.signalCode !== null);
    // Workspace fixture có counter cũ trong src; mặc định mới phải tiếp tục file đó.
    fs.mkdirSync(path.join(cwd,'data'),{recursive:true});
    fs.mkdirSync(path.join(cwd,'src/data'),{recursive:true});
    fs.copyFileSync(mapPath,path.join(cwd,'data/map_layout.json'));
    const legacyCounter = path.join(cwd,'src/data/command_sequence.json');
    fs.writeFileSync(legacyCounter,JSON.stringify({reserved_through:8192}));
    const legacyEnv = {...env};delete legacyEnv.AGV_MAP_PATH;delete legacyEnv.AGV_COMMAND_ID_PATH;
    child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'),[],legacyEnv);await ready();
    assert.equal(JSON.parse(fs.readFileSync(legacyCounter,'utf8')).reserved_through,9216);
    assert.equal(fs.existsSync(path.join(cwd,'data/command_sequence.json')),false);
    console.log('PASS startup: duplicate core rejected before MQTT/counter; wrapper normalizes cwd; active MQTT/session stable; restart and legacy counter preserved');
    console.log('Artifacts: '+directory);
  } catch (error) {console.error(log.slice(-6000));throw error;}
  finally {
    for (const socket of sockets) socket.terminate();
    for (const process of children) if (process.exitCode === null && process.signalCode === null) {
      if (process.spawnfile === 'powershell.exe') {
        const cleanup = spawn('taskkill.exe',['/PID',String(process.pid),'/T','/F'],{windowsHide:true,stdio:'ignore'});
        await once(cleanup,'exit');
      } else process.kill();
    }
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
