// Core/gateway/broker rieng; telemetry gia lap, khong dieu khien robot that.
const assert = require('assert/strict');
const fs = require('fs'), path = require('path'), net = require('net');
const {spawn} = require('child_process'), {once} = require('events');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');
const defaultMode = process.argv.includes('--default');
const robotId = process.argv.includes('--robot=02') ? '02' : '01';
const home = robotId === '01' ? 'S1' : 'S2';

const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(check, timeout = 6500) {
  const end = Date.now() + timeout;
  while (Date.now() < end) { if (await check()) return; await pause(40); }
  throw new Error('Solo shift condition timed out');
}
async function freePort() {
  const server = net.createServer();server.listen(0, '127.0.0.1');await once(server, 'listening');
  const port = server.address().port;await new Promise(resolve => server.close(resolve));return port;
}
async function available(port) {
  return new Promise(resolve => {
    const socket = net.connect(port, '127.0.0.1');
    socket.once('connect', () => {socket.destroy();resolve(true);});
    socket.once('error', () => resolve(false));
  });
}
async function main() {
  const root = path.resolve(__dirname, '..');fs.mkdirSync(path.join(root, '.qa'), {recursive:true});
  const directory = fs.mkdtempSync(path.join(root, '.qa/solo-shift-test-'));
  const mqttPort = await freePort(), corePort = await freePort();
  const mapPath = path.join(directory, 'map.json');
  fs.copyFileSync(path.join(root, 'data/map_layout.json'), mapPath);
  const brokerPath = path.join(directory, 'broker.conf');
  fs.writeFileSync(brokerPath, `listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const brokerBin = process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env = {...process.env, AGV_ONE_ROBOT_TRIAL:defaultMode ? '0' : '1',
    AGV_MAP_PATH:mapPath, AGV_COMMAND_ID_PATH:path.join(directory, 'counter.json'),
    AGV_SHIFT_STATE_PATH:path.join(directory, 'shift.json'),
    AGV_MQTT_HOST:'127.0.0.1', AGV_MQTT_PORT:String(mqttPort),
    AGV_CORE_WS_PORT:String(corePort), AGV_CORE_WS_HOST:'127.0.0.1'};
  const pathKey = Object.keys(env).find(key => key.toLowerCase() === 'path') || 'PATH';
  env[pathKey] = brokerBin + path.delimiter + env[pathKey];
  const db = createDatabase(path.join(directory, 'test.db'));
  const children = [], commands = [];
  let output = '', gateway, mqtt, telemetryTimer;
  function child(command, args = []) {
    const proc = spawn(command, args, {env, cwd:directory, windowsHide:true});children.push(proc);
    proc.stdout.on('data', data => output += data);
    proc.stderr.on('data', data => output += data);
    proc.on('error', error => output += error.message);
    return proc;
  }
  try {
    child(path.join(brokerBin, 'mosquitto.exe'), ['-c', brokerPath]);
    await until(() => available(mqttPort));
    child(process.env.AGV_CORE_BIN || path.join(root, 'build/agv_core.exe'));
    gateway = createGateway({db, coreUrl:`ws://127.0.0.1:${corePort}`, reconnectMs:100});
    const address = await gateway.start(0, '127.0.0.1');
    const base = `http://127.0.0.1:${address.port}`;
    const state = async () => (await fetch(base + '/api/state')).json();
    const api = async route => fetch(base + route, {method:'POST', headers:{'Content-Type':'application/json'}, body:'{}'});
    await until(async () => {const current = await state();return current.core_ready && current.mqtt_connected;});
    let current = await state();
    assert.deepEqual(current.shift.home_stations, defaultMode ? {'01':'S1','02':'S2'} : {'01':'S1'});
    assert.equal(current.shift.single_robot_trial, !defaultMode);
    assert.equal(current.shift.can_start, false);
    assert.equal((await api('/api/shift/start')).status, 409);
    mqtt = new MqttFixture(mqttPort);mqtt.on('error', error => output += error.message);
    await until(() => mqtt.connected);
    mqtt.on('publish', (topic, payload) => {
      if (!topic.endsWith('/command')) return;
      const command = JSON.parse(payload);commands.push(command);
      mqtt.publish(`agv/${robotId}/ack`, String(command.cmd_id));
    });
    mqtt.subscribe('agv/#');await until(() => mqtt.subscribed);
    let battery = '?';
    const telemetry = () => mqtt.publish(`agv/${robotId}/telemetry`, `${home}|${battery}|0`);
    telemetry();telemetryTimer = setInterval(telemetry, 350);
    await until(async () => (await state()).shift.can_start === true);
    current = await state();
    assert.equal(current.shift.phase, 'CLOSED');
    assert.equal(current.fleet.agvs.length, 1);
    assert.equal(current.shift.robots.find(robot => robot.id === robotId).participating, true);
    if (defaultMode) assert.equal(current.shift.robots.find(robot => robot.id !== robotId).participating, false);
    assert.equal(current.fleet.agvs[0].battery_measured, false);
    assert.equal(commands.some(command => command.cmd === 'CHARGE_START'), false);
    assert.equal((await api('/api/shift/start')).status, 200);
    await until(async () => (await state()).shift.phase === 'RUNNING');
    if (defaultMode) {
      assert.equal((await api('/api/shift/end')).status, 200);
      battery = '25';telemetry();
      await until(async () => {
        const current = await state();
        return current.shift.phase === 'CLOSED' && current.fleet.agvs[0]?.battery === 25;
      });
      assert.equal((await state()).shift.can_start, false);
      battery = '85';telemetry();
      await until(async () => (await state()).shift.can_start === true);
      assert.equal(commands.some(command => command.cmd === 'CHARGE_START'), false);
      assert.equal((await api('/api/shift/start')).status, 200);
    }
    assert.equal(commands.some(command => command.cmd === 'GOTO'), false);
    console.log(`PASS solo shift: ${robotId} in ${defaultMode ? 'default mode with unknown/25%/85% battery' : 'trial mode with unknown battery'}, start via HTTP, no charge/motion command`);
    console.log('Evidence: ' + directory);
  } catch (error) {
    fs.writeFileSync(path.join(directory, 'failure.log'), output);throw error;
  } finally {
    clearInterval(telemetryTimer);mqtt?.close();if (gateway) await gateway.close();db.close();
    for (const proc of children) if (proc.exitCode === null) proc.kill();
    fs.writeFileSync(path.join(directory, 'runtime.log'), output);
  }
}
main().catch(error => {console.error(error);process.exitCode = 1;});
