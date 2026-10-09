const assert = require('assert/strict');
const fs = require('fs');
const path = require('path');
const {createHash} = require('crypto');
const {createSimulation} = require('../simulation/server');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,label,timeout = 15000) {
  const end = Date.now()+timeout;
  while (Date.now() < end) {if (await fn()) return;await pause(75);}
  throw new Error('Timeout: '+label);
}
async function main() {
  const root = path.resolve(__dirname,'..'),qa = path.join(root,'.qa');fs.mkdirSync(qa,{recursive:true});
  const protectedFiles = ['data/map_layout.json','data/command_sequence.json','data/task_sequence.json','data/runtime_state.json','data/shift_state.json',
    'AGVwebserver/index.html','AGVwebserver/public/dashboard.js','src/FLeetmanager.hpp','src/Mutexarbiter.hpp','src/MapGraph.hpp'];
  const hash = file=>fs.existsSync(file) ? createHash('sha256').update(fs.readFileSync(file)).digest('hex') : null;
  const before = protectedFiles.map(file=>hash(path.join(root,file)));
  const sim = await createSimulation({port:0,runParent:qa});
  const {runtime:r} = sim,one = r.robot('01'),two = r.robot('02');
  try {
    assert.ok(fs.existsSync(path.join(sim.directory,'simulation.db')));
    assert.notEqual(sim.directory,path.join(root,'AGVwebserver'),'DB fixture is separate; a running production gateway may independently write agv.db');
    await until(()=>r.health === 'Sẵn sàng' && r.coreState.shift?.can_start,'both homes ready');
    assert.equal(one.node,'S1');assert.equal(two.node,'S2');
    const html = await (await fetch(sim.url)).text();assert.match(html,/MÔ PHỎNG/);assert.match(html,/simControlPanel/);
    const wrongOrigin = await fetch(sim.base+'/sim-api/settings',{method:'POST',headers:{'Content-Type':'application/json',Origin:'https://foreign.test'},body:'{"timeScale":5}'});
    assert.equal(wrongOrigin.status,403);
    assert.equal((await fetch(sim.base+'/sim-api/robots/03/action',{method:'POST',headers:{'Content-Type':'application/json'},body:'{"action":"pause"}'})).status,400);
    r.configure({timeScale:10,speedCmS:100,drainPerCm:0,chargeRate:10,serviceSeconds:0.2});
    await r.api('POST','/api/shift/start',{});
    await until(()=>r.coreState.fleet.agvs.every(a=>a.state === 'IDLE'),'shift running');
    one.dropAcks = 1;
    const task = await r.api('POST','/api/agvs/01/goto',{target:'K2'});
    const initial = r.tasks.get(task.task_id);assert.notEqual(initial?.status,'completed');
    await until(()=>r.tasks.get(task.task_id)?.status === 'completed','real RFID completion');
    assert.equal(one.node,'K2');assert.ok(one.distanceCm > 0);assert.ok(one.routeCost > 0);
    assert.ok(r.events.some(e=>e.type === 'grant_ack'));
    // The real retry (3 wall seconds) must not repeat travel. Keep 01 stationary at destination.
    const cost = one.routeCost;await pause(3400);assert.equal(one.routeCost,cost);
    assert.ok(r.events.some(e=>e.type === 'command_duplicate'));
    const sameNode = await r.goto('01','K2');await until(()=>r.tasks.get(sameNode.task_id)?.status === 'completed','empty path already at RFID');
    assert.equal(one.routeCost,cost);assert.equal(one.fault,'');
    const jobOne = await r.cargoJob('01','K2','K3'),jobTwo = await r.cargoJob('02','K5','K4');
    await until(()=>jobOne.ended && jobTwo.ended,'two real cargo sequences',30000);
    assert.equal(jobOne.phase,'completed');assert.equal(jobTwo.phase,'completed');
    assert.equal(one.cargo,false);assert.equal(two.cargo,false);
    assert.equal(jobOne.taskIds.length,1);assert.equal(jobTwo.taskIds.length,2);
    for (const id of [...jobOne.taskIds,...jobTwo.taskIds]) assert.equal(r.tasks.get(id)?.status,'completed');
    one.batteryChange(15);
    await until(()=>one.node === 'S1' && one.charging,'Core sends low battery home',20000);
    await until(()=>r.coreState.fleet.agvs.find(a=>a.id === '01').state === 'IDLE','charging_done restores readiness');
    // Empty battery does not masquerade as successful arrival.
    r.configure({timeScale:1,speedCmS:50});
    const empty = await r.api('POST','/api/agvs/01/goto',{target:'K6'});
    await until(()=>one.edge?.progress > 0,'moving before empty injection');one.batteryChange(0);
    const distance = one.distanceCm;await pause(350);assert.equal(one.distanceCm,distance);assert.ok(one.fault);
    await until(()=>r.coreState.fleet.agvs.find(a=>a.id === '01').state === 'ERROR','zero battery local error');
    assert.notEqual(r.tasks.get(empty.task_id)?.status,'completed','ACK/empty battery never implies completion');
    // Core may requeue work before processing the error; keep its real policy.
    await r.api('POST','/api/tasks/cancel',{task_id:empty.task_id});
    one.batteryChange(85);await r.action('01',{action:'recover'});
    await until(()=>r.coreState.fleet.agvs.find(a=>a.id === '01').state === 'IDLE','manual recovery');
    // Offline in a zone preserves occupancy. Stop at M5 until its next edge starts.
    r.configure({timeScale:1,speedCmS:50});
    await r.api('POST','/api/agvs/01/goto',{target:'M5'});
    await until(()=>one.node === 'M5' && one.grants.has('M5'),'owns M5',25000);
    await r.action('01',{action:'offline'});
    await until(()=>r.coreState.fleet.agvs.find(a=>a.id === '01').state === 'OFFLINE','LWT offline');
    assert.equal(r.coreState.mutex.M5.owner,'01');
    await r.action('01',{action:'online'});await until(()=>one.connected,'reconnect');await r.action('01',{action:'recover'});
    r.configure({timeScale:10,speedCmS:100});
    await r.api('POST','/api/agvs/01/goto',{target:'K3'});
    await until(()=>one.node === 'K3','leave recovered zone');
    await until(()=>r.coreState.mutex.M5.owner !== '01','release verified exit');
    await r.api('POST','/api/shift/end',{});
    await until(()=>r.coreState.shift?.phase === 'CLOSED','finish shift return fixed homes',30000);
    assert.equal(one.node,'S1');assert.equal(two.node,'S2');
    assert.deepEqual(protectedFiles.map(file=>hash(path.join(root,file))),before,'operational code/data unchanged');
    console.log('PASS simulator: exact real Core/gateway; homes/shift, ACK dedup, two cargo jobs, weighted distance, low battery/charging, empty failsafe, offline mutex retention/recovery, isolated data and Origin validation');
    console.log('Artifacts: '+sim.directory);
  } catch (error) {
    fs.writeFileSync(path.join(sim.directory,'failure.json'),JSON.stringify(r.snapshot(),null,2));
    console.error('Failure artifacts: '+sim.directory);console.error(r.robots.map(robot=>robot.snapshot()));throw error;
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
