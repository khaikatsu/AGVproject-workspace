// Supervised, wheels-lifted motor command probe. Do not run on the floor.
const {MqttFixture} = require('../tests/helpers/mqtt-client');

const base = 'http://127.0.0.1:3000';
const robotId = '01';
const target = 'GS1';
const durationMs = 2000;
const samples = [];
let taskId = '';
let gotoAttempted = false;
let cancelDone = false;

async function request(path, method = 'GET', body) {
  const response = await fetch(base + path, {
    method,
    headers: {'content-type': 'application/json'},
    body: body === undefined ? undefined : JSON.stringify(body),
    signal: AbortSignal.timeout(4000),
  });
  const result = await response.json();
  if (!response.ok || result.ok === false) throw new Error(`${path}: ${response.status} ${JSON.stringify(result)}`);
  return result;
}

async function cancel() {
  if (cancelDone || !gotoAttempted) return;
  const result = await request('/api/tasks/cancel', 'POST', taskId ? {task_id: taskId} : {target});
  cancelDone = true;
  console.log(`CANCEL ${JSON.stringify(result)}`);
}

async function main() {
  const mqtt = new MqttFixture(1883);
  mqtt.on('connected', () => mqtt.subscribe('agv/01/diagnostic'));
  mqtt.on('publish', (_topic, payload) => {
    let data;
    try { data = JSON.parse(payload); } catch { return; }
    samples.push({time: Date.now(), state: data.state, error: data.error,
      node: data.node, motor: data.motor, leftDuty: data.leftDuty,
      rightDuty: data.rightDuty, wifi: data.wifi, mqtt: data.mqtt});
  });
  try {
    await new Promise((resolve, reject) => {
      mqtt.once('subscribed', resolve);
      mqtt.once('error', reject);
      setTimeout(() => reject(new Error('MQTT subscribe timeout')), 3000);
    });
    let state = await request('/api/state');
    const robot = state.fleet.agvs.find(agv => agv.id === robotId);
    if (!state.core_ready || !state.mqtt_connected || state.shift.phase !== 'RUNNING' ||
        state.fleet.agvs.length !== 1 || !robot || !robot.online ||
        !robot.position_valid || robot.node !== 'S1' || robot.task_id ||
        state.fleet.pending_tasks.length) throw new Error('Preflight state is not ready');
    if (robot.state === 'ERROR') {
      await request(`/api/agvs/${robotId}/clear-error`, 'POST', {});
      await new Promise(resolve => setTimeout(resolve, 500));
    }
    state = await request('/api/state');
    if (state.fleet.agvs.find(agv => agv.id === robotId)?.state !== 'IDLE')
      throw new Error('Robot is not IDLE after clear-error');
    const started = Date.now();
    gotoAttempted = true;
    const watchdog = setTimeout(() => cancel().catch(error => console.error(`WATCHDOG CANCEL FAILED: ${error.message}`)), durationMs + 500);
    const result = await request(`/api/agvs/${robotId}/goto`, 'POST', {target});
    taskId = result.task_id;
    if (!taskId) throw new Error('GOTO returned no task_id');
    console.log(`GOTO ${taskId} at ${new Date(started).toISOString()}`);
    await new Promise(resolve => setTimeout(resolve, durationMs));
    await cancel();
    clearTimeout(watchdog);
    await new Promise(resolve => setTimeout(resolve, 500));
    console.log(JSON.stringify(samples.filter(sample => sample.time >= started), null, 2));
  } finally {
    try { await cancel(); } catch (error) { console.error(`CANCEL FAILED: ${error.message}`); }
    mqtt.close();
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
