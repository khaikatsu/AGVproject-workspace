const fs = require('fs');
const path = require('path');
const net = require('net');
const { spawn } = require('child_process');
const { once } = require('events');
const express = require('../AGVwebserver/node_modules/express');
const { createGateway } = require('../AGVwebserver/gateway');
const { createSecurity } = require('../AGVwebserver/security');
const { SimulationRuntime } = require('./runtime');
const root = path.resolve(__dirname, '..');
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
async function freePort() {
  const socket = net.createServer(); socket.listen(0, '127.0.0.1'); await once(socket, 'listening');
  const port = socket.address().port; await new Promise(resolve => socket.close(resolve)); return port;
}
function listening(port) { return new Promise(resolve => { const s = net.connect(port, '127.0.0.1'); s.once('connect', () => { s.destroy(); resolve(true); }); s.once('error', () => resolve(false)); }); }
async function createSimulation(options = {}) {
  // This process is only a loopback lab. Production security/env is not inherited by its children.
  if (process.env.AGV_SECURITY_REQUIRED || process.env.AGV_HTTPS_CERT || process.env.AGV_HTTPS_KEY)
    throw new Error('Dùng scripts/run-simulation.ps1 để mở môi trường mô phỏng riêng khỏi cấu hình vận hành');
  const port = options.port ?? Number(process.env.SIM_WEB_PORT || 3100);
  if (!Number.isInteger(port) || port < 0 || port > 65535 || [1883, 3000, 8080].includes(port)) throw new Error('Cổng mô phỏng không được trùng 1883/3000/8080');
  const binary = options.coreBinary || process.env.AGV_CORE_BIN || path.join(root, 'build', process.platform === 'win32' ? 'agv_core.exe' : 'agv_core');
  if (!fs.existsSync(binary)) throw new Error('Thiếu Core: chạy ./scripts/build-core.ps1 trước');
  const parent = options.runParent || path.join(__dirname, 'runs'); fs.mkdirSync(parent, { recursive: true });
  const directory = fs.mkdtempSync(path.join(parent, 'run-'));
  const mapPath = path.join(directory, 'map_layout.json'); fs.copyFileSync(options.mapPath || path.join(root, 'data/map_layout.json'), mapPath);
  fs.copyFileSync(mapPath, path.join(directory, 'initial-map.json'));
  const map = JSON.parse(fs.readFileSync(mapPath, 'utf8'));
  if (!['S1', 'S2'].every(id => map.nodes.some(n => n.id === id && n.type === 'charging'))) throw new Error('Bản mô phỏng hai xe cần trạm S1/S2');
  const mqttPort = await freePort(), corePort = await freePort();
  const config = path.join(directory, 'broker.conf'); fs.writeFileSync(config, `listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const bin = options.mosquittoBin || process.env.AGV_MOSQUITTO_BIN || (process.platform === 'win32' ? 'C:/msys64/ucrt64/bin' : '/usr/bin');
  const env = Object.fromEntries(Object.entries(process.env).filter(([key]) => !key.startsWith('AGV_')));
  Object.assign(env, {
    AGV_MQTT_HOST: '127.0.0.1', AGV_MQTT_PORT: String(mqttPort), AGV_CORE_WS_HOST: '127.0.0.1', AGV_CORE_WS_PORT: String(corePort),
    AGV_MAP_PATH: mapPath, AGV_COMMAND_ID_PATH: path.join(directory, 'command_sequence.json'), AGV_TASK_ID_PATH: path.join(directory, 'task_sequence.json'),
    AGV_RUNTIME_STATE_PATH: path.join(directory, 'runtime_state.json'), AGV_SHIFT_STATE_PATH: path.join(directory, 'shift_state.json'), AGV_HOME_STATIONS: '{"01":"S1","02":"S2"}'
  });
  if (options.twoRobotTrial === true) env.AGV_TWO_ROBOT_TRIAL = '1';
  const pathKey = Object.keys(env).find(key => key.toLowerCase() === 'path') || 'PATH'; env[pathKey] = bin + path.delimiter + (env[pathKey] || '');
  let runtime, gateway, closing = false; const children = [], streams = [];
  const viewers = new Map();
  let idleTimer;
  const idleTimeoutMs = options.idleTimeoutMs || 0;
  const viewerTimeoutMs = options.viewerTimeoutMs || 12000;
  let lastViewerAt = Date.now();
  async function close() {
    if (closing) return; closing = true;
    clearInterval(idleTimer);
    if (runtime) await runtime.close(); if (gateway) await gateway.close();
    for (const child of children) if (child.exitCode == null) {
      child.kill(); await Promise.race([once(child, 'exit').catch(() => { }), pause(1500)]);
      child.stdout.unpipe(); child.stderr.unpipe();
    }
    for (const stream of streams) await new Promise(resolve => stream.end(resolve));
  }
  async function child(command, args, name, port) {
    const log = fs.createWriteStream(path.join(directory, name + '.log'), { flags: 'a' }); streams.push(log);
    const process = spawn(command, args, { env, cwd: directory, windowsHide: true }); process.simulationName = name; children.push(process);
    process.stdout.pipe(log, { end: false }); process.stderr.pipe(log, { end: false }); let error;
    process.on('error', e => { error = e; });
    process.on('exit', (code) => { if (!closing && runtime) { runtime.health = `${name} đã dừng (${code})`; runtime.event('process_exit', { name, code }); } });
    const deadline = Date.now() + 10000;
    while (Date.now() < deadline) {
      if (error || process.exitCode != null) throw error || new Error(name + ' không khởi động; xem ' + directory);
      if (await listening(port)) return process; await pause(80);
    }
    throw new Error(name + ' chưa mở cổng; xem ' + directory);
  }
  try {
    await child(path.join(bin, process.platform === 'win32' ? 'mosquitto.exe' : 'mosquitto'), ['-c', config], 'broker', mqttPort);
    await child(binary, [], 'core', corePort);
    gateway = createGateway({ dbPath: path.join(directory, 'simulation.db'), coreUrl: `ws://127.0.0.1:${corePort}`, coreToken: '', authToken: '', allowedOrigins: '', reconnectMs: 200 });
    runtime = new SimulationRuntime({ directory, map, mqttPort });
    gateway.app.use('/sim-assets', express.static(path.join(__dirname, 'public')));
    gateway.app.get('/simulation', (req, res) => {
      const html = fs.readFileSync(path.join(root, 'AGVwebserver/index.html'), 'utf8')
        .replace('</head>', '<link rel="stylesheet" href="/sim-assets/simulation.css"></head>')
        .replace('Trung tâm điều phối', 'Phòng mô phỏng AGV').replace('Kho AGV · Hai xe tự hành', 'MÔ PHỎNG · Hai xe ảo · Core C++ thật')
        .replace('<title>', '<title>Mô phỏng · ')
        .replace('<aside id="right-panel" aria-label="Điều khiển và cấu hình">', '<aside id="right-panel" aria-label="Điều khiển và cấu hình">' + fs.readFileSync(path.join(__dirname, 'public/panel.html'), 'utf8'))
        .replace('</body>', '<script src="/sim-assets/simulation.js"></script></body>');
      res.set('Cache-Control', 'no-store').type('html').send(html);
    });
    gateway.app.use('/sim-api', createSecurity({ token: '', origins: '' }).middleware);
    const route = fn => (req, res) => Promise.resolve().then(() => fn(req, res)).catch(error => res.status(error.status || 500).json({ ok: false, error: error.message }));
    gateway.app.get('/sim-api/state', (req, res) => res.json(runtime.snapshot()));
    gateway.app.post('/sim-api/viewer', route((req, res) => {
      const id = req.body?.id;
      if (typeof id !== 'string' || !/^[a-zA-Z0-9-]{16,80}$/.test(id))
        throw Object.assign(new Error('ID tab không hợp lệ'), { status: 400 });
      const origin = req.get('origin');
      if (origin && origin !== `${req.protocol}://${req.get('host')}`)
        throw Object.assign(new Error('Origin không hợp lệ'), { status: 403 });
      if (req.body?.leaving === true) viewers.delete(id);
      else { viewers.set(id, Date.now()); lastViewerAt = Date.now(); }
      res.json({ ok: true });
    }));
    gateway.app.post('/sim-api/settings', route((req, res) => res.json({ ok: true, settings: runtime.configure(req.body) })));
    gateway.app.post('/sim-api/robots/:id/action', route(async (req, res) => {
      const robot = await runtime.action(req.params.id, req.body);
      res.json({ ok: true, robot, ...runtime.actionMessage(req.params.id, req.body) });
    }));
    gateway.app.post('/sim-api/robots/:id/goto', route(async (req, res) => res.json(await runtime.goto(req.params.id, req.body.target))));
    gateway.app.post('/sim-api/jobs', route(async (req, res) => res.json({ ok: true, job: await runtime.cargoJob(req.body.robot, req.body.pickup, req.body.dropoff) })));
    gateway.app.post('/sim-api/orders', route((req, res) => res.json({ ok: true, order: runtime.createOrder(req.body) })));
    gateway.app.post('/sim-api/stop', route((req, res) => {
      if (req.body.confirm !== true) throw Object.assign(new Error('Cần confirm=true để đóng phiên mô phỏng'), { status: 400 });
      res.json({ ok: true, message: 'Đang lưu báo cáo và đóng phiên mô phỏng' });
      setTimeout(() => close().then(() => options.onClose?.()).catch(error => console.error(error.message)), 50);
    }));
    gateway.app.get('/sim-api/report', (req, res) => { res.set('Content-Disposition', 'attachment; filename="agv-simulation-report.json"').json(runtime.report()); });
    gateway.app.get('/sim-api/events', (req, res) => res.download(path.join(directory, 'events.jsonl'), 'agv-simulation-events.jsonl'));
    const address = await gateway.start(port, '127.0.0.1'), base = `http://127.0.0.1:${address.port}`;
    runtime.ports = { web: address.port, core: corePort, mqtt: mqttPort }; runtime.start(base);
    lastViewerAt = Date.now();
    if (idleTimeoutMs) idleTimer = setInterval(() => {
      const now = Date.now();
      for (const [id, seen] of viewers) if (now - seen > viewerTimeoutMs) viewers.delete(id);
      if (!viewers.size && now - lastViewerAt >= idleTimeoutMs && !closing)
        close().then(() => options.onClose?.()).catch(error => console.error(error.message));
    }, 1000);
    fs.writeFileSync(path.join(directory, 'run.json'), JSON.stringify({ pid: process.pid, corePid: children[1].pid, brokerPid: children[0].pid, url: base + '/simulation', ports: runtime.ports, settings: runtime.settings }, null, 2));
    // Fixture controls only: no HTTP route exposes process restart to the dashboard.
    async function restartProcess(name) {
      if (closing || !['core', 'broker'].includes(name)) throw new Error('Không thể restart fixture');
      const process = children.filter(p => p.simulationName === name).at(-1);
      if (process && process.exitCode == null) { process.kill(); await once(process, 'exit'); }
      return name === 'core' ? child(binary, [], 'core', corePort) :
        child(path.join(bin, process.platform === 'win32' ? 'mosquitto.exe' : 'mosquitto'), ['-c', config], 'broker', mqttPort);
    }
    return { runtime, gateway, directory, base, url: base + '/simulation', close, restartProcess };
  } catch (error) { await close(); throw error; }
}
if (require.main === module) {
  createSimulation({ onClose: () => process.exit(0) }).then(sim => {
    console.log('MÔ PHỎNG HAI XE ẢO: ' + sim.url + '\nDữ liệu riêng: ' + sim.directory + '\nCtrl+C để dừng và lưu báo cáo.');
    const stop = () => sim.close().then(() => process.exit(0)); process.once('SIGINT', stop); process.once('SIGTERM', stop);
  }).catch(error => { console.error(error.message); process.exitCode = 1; });
}
module.exports = { createSimulation };
