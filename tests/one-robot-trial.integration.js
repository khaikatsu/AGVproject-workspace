// Isolated broker/Core/gateway trial: one unmeasured robot follows directed shortest paths.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const {spawn} = require('node:child_process');
const {once} = require('node:events');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const {MqttFixture} = require('./helpers/mqtt-client');

const root = path.resolve(__dirname, '..');
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(predicate, timeout = 8000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    if (await predicate()) return;
    await pause(40);
  }
  throw new Error('One-robot trial timed out');
}
async function freePort() {
  const server = net.createServer();
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const port = server.address().port;
  await new Promise(resolve => server.close(resolve));
  return port;
}
async function available(port) {
  return new Promise(resolve => {
    const socket = net.connect(port, '127.0.0.1');
    socket.once('connect', () => { socket.destroy(); resolve(true); });
    socket.once('error', () => resolve(false));
  });
}
async function main() {
  const directory = fs.mkdtempSync(path.join(root, '.qa/trial-test-'));
  const mqttPort = await freePort(), corePort = await freePort();
  const mapPath = path.join(directory, 'map.json');
  fs.copyFileSync(path.join(root, 'data/map_layout.json'), mapPath);
  const brokerPath = path.join(directory, 'broker.conf');
  fs.writeFileSync(brokerPath, `listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin = process.env.AGV_MOSQUITTO_BIN || 'C:/msys64/ucrt64/bin';
  const env = {...process.env, AGV_HOME_STATIONS: '{}', AGV_ONE_ROBOT_TRIAL: '1',
    AGV_MQTT_HOST: '127.0.0.1', AGV_MQTT_PORT: String(mqttPort),
    AGV_CORE_WS_HOST: '127.0.0.1', AGV_CORE_WS_PORT: String(corePort),
    AGV_MAP_PATH: mapPath, AGV_COMMAND_ID_PATH: path.join(directory, 'counter.json')};
  const pathKey = Object.keys(env).find(key => key.toLowerCase() === 'path') || 'PATH';
  env[pathKey] = bin + path.delimiter + env[pathKey];
  const children = [], commands = [];
  const db = createDatabase(path.join(directory, 'test.db'));
  let gateway, mqtt, output = '';
  function child(command, args = []) {
    const process = spawn(command, args, {env, cwd: directory, windowsHide: true});
    children.push(process);
    process.stderr.on('data', data => { output += data; });
    process.on('error', error => { output += error.message; });
    return process;
  }
  try {
    child(path.join(bin, 'mosquitto.exe'), ['-c', brokerPath]);
    await until(() => available(mqttPort));
    child(process.env.AGV_CORE_BIN || path.join(root, 'build/agv_core.exe'));
    gateway = createGateway({db, coreUrl: `ws://127.0.0.1:${corePort}`, reconnectMs: 100});
    const address = await gateway.start(0, '127.0.0.1');
    const base = `http://127.0.0.1:${address.port}`;
    const state = async () => (await fetch(base + '/api/state')).json();
    async function goto(target) {
      const response = await fetch(base + '/api/agvs/01/goto', {method: 'POST',
        headers: {'Content-Type': 'application/json'}, body: JSON.stringify({target})});
      return {status: response.status, data: await response.json()};
    }
    await until(async () => { const s = await state(); return s.core_ready && s.mqtt_connected; });
    mqtt = new MqttFixture(mqttPort);
    mqtt.on('error', error => { output += error.message; });
    await once(mqtt, 'connected');
    mqtt.subscribe('agv/01/command');
    await once(mqtt, 'subscribed');
    mqtt.on('publish', (topic, payload) => {
      if (topic === 'agv/01/command') commands.push(JSON.parse(payload));
    });
    mqtt.publish('agv/01/telemetry', 'S1|?|0');
    await until(async () => (await state()).fleet.agvs.some(robot =>
      robot.id === '01' && robot.node === 'S1' && robot.position_valid && !robot.battery_measured));
    const automatic = await fetch(base + '/api/tasks', {method: 'POST',
      headers: {'Content-Type': 'application/json'}, body: JSON.stringify({target: 'M2'})});
    assert.notEqual(automatic.status, 200);
    const trial = await goto('M2');
    assert.equal(trial.status, 200);
    await until(() => commands.some(command => command.cmd === 'GOTO'));
    const command = commands.find(item => item.cmd === 'GOTO');
    assert.deepEqual(command.path, ['GS1', 'M2']);
    assert.equal(command.target, 'M2');
    mqtt.publish('agv/01/ack', String(command.cmd_id));
    mqtt.publish('agv/01/telemetry', 'GS1|?|0');
    await until(async () => (await state()).fleet.agvs.some(robot => robot.id === '01' && robot.node === 'GS1'));
    mqtt.publish('agv/01/telemetry', 'M2|?|0');
    await until(async () => {
      const s = await state();
      return s.fleet.agvs.some(robot => robot.id === '01' && robot.node === 'M2' && !robot.battery_measured) &&
        s.fleet.tasks.some(task => task.task_id === trial.data.task_id && task.status === 'completed');
    });
    const next = await goto('C3');
    assert.equal(next.status, 200);
    await until(() => commands.filter(item => item.cmd === 'GOTO').length === 2);
    assert.equal(commands.filter(item => item.cmd === 'GOTO')[1].target, 'C3');
    mqtt.publish('agv/02/telemetry', 'S2|80|0');
    await until(async () => {
      const s = await state();
      return s.fleet.tasks.some(task => task.task_id === next.data.task_id && task.status === 'interrupted') &&
        s.fleet.agvs.some(robot => robot.id === '01' && robot.state === 'ERROR');
    });
    assert.notEqual((await goto('C3')).status, 200);
    console.log('PASS solo route: S1->GS1->M2, unmeasured battery, automatic task denied, second robot stops trial');
    console.log('Artifacts: ' + directory);
  } catch (error) { console.error(output.slice(-4000)); throw error; }
  finally {
    mqtt?.close();
    if (gateway) await gateway.close();
    for (const process of children) if (process.exitCode === null && process.signalCode === null) process.kill();
    db.close();
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
