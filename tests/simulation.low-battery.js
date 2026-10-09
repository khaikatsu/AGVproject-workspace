const assert = require('assert/strict');
const fs = require('fs');
const path = require('path');
const {createSimulation} = require('../simulation/server');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,label,timeout = 15000) {
  const deadline = Date.now()+timeout;
  while (Date.now() < deadline) {if (fn()) return;await pause(50);}
  throw new Error('Timeout: '+label);
}
async function main() {
  const qa = path.resolve(__dirname,'../.qa');fs.mkdirSync(qa,{recursive:true});
  const sim = await createSimulation({port:0,runParent:qa}),r = sim.runtime,robot = r.robot('01');
  const state = ()=>r.coreState?.fleet?.agvs?.find(v=>v.id === '01');
  async function action(action,value) {
    const response = await fetch(sim.base+'/sim-api/robots/01/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,...(value == null ? {} : {value})})});
    const result = await response.json();assert.equal(response.status,200,result.error);return result;
  }
  async function toK6() {
    await until(()=>state()?.state === 'IDLE','ready to dispatch');
    await r.goto('01','K6');await until(()=>robot.node === 'K6' && state()?.state === 'IDLE','K6');
  }
  async function homeAndCharged() {
    await until(()=>robot.node === 'S1' && robot.charging,'RFID S1 and charge start');
    await until(()=>state()?.state === 'IDLE' && state().battery >= 90,'charging_done');
  }
  try {
    r.configure({speedCmS:100,timeScale:10,drainPerCm:0,chargeRate:10});
    await until(()=>r.coreState?.shift?.can_start,'homes');await r.api('POST','/api/shift/start',{});
    await toK6();await action('battery',15);await homeAndCharged();
    // Repeat charging at the same station: a previous charging_done must not latch forever.
    await action('battery',15);await until(()=>state()?.state === 'CHARGING','repeat charge cycle');await homeAndCharged();
    await toK6();await action('battery',0);await until(()=>state()?.state === 'ERROR','zero battery error');
    const distance = robot.distanceCm,result = await action('battery',15);
    assert.equal(result.warning,true);assert.match(result.message,/khóa lỗi/);await pause(300);assert.equal(robot.distanceCm,distance,'battery alone cannot clear failsafe');
    await action('recover');await homeAndCharged();
    await toK6();await action('pause');const pausedDistance = robot.distanceCm;
    const paused = await action('battery',15);assert.equal(paused.warning,true);assert.match(paused.message,/tạm dừng/);
    await until(()=>robot.path.at(-1) === 'S1','Core orders return while device paused');await pause(150);assert.equal(robot.distanceCm,pausedDistance);
    await action('resume');await homeAndCharged();
    await toK6();await action('offline');await until(()=>!robot.connected && state()?.state === 'OFFLINE','LWT');
    const offline = await action('battery',15);assert.equal(offline.warning,true);assert.match(offline.message,/offline/);
    await action('recover');assert.equal(robot.connected,true);assert.equal(robot.paused,false);assert.equal(robot.fault,'');
    await homeAndCharged();
    console.log('PASS low battery: K6→S1 at 15%, repeated charging_done, 0→15 stays stopped until explicit recovery, manual pause/resume, offline→15→RFID recovery/reconnect; real Core unchanged');
    console.log('Artifacts: '+sim.directory);
  } catch (error) {
    fs.writeFileSync(path.join(sim.directory,'failure.json'),JSON.stringify({sim:r.report(),core:r.coreState},null,2));
    console.error('Failure: '+sim.directory);throw error;
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
