const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createSimulation} = require('../simulation/server');

const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,label,timeout = 12000) {
  const deadline = Date.now()+timeout;
  while (Date.now() < deadline) {if (await check()) return;await pause(100);}
  throw new Error('Timeout: '+label);
}

async function main() {
  const qa = path.resolve(__dirname,'../.qa/mqtt-recovery');fs.mkdirSync(qa,{recursive:true});
  const sim = await createSimulation({port:0,runParent:qa}),runtime = sim.runtime;
  try {
    await until(()=>runtime.health === 'Sẵn sàng' && runtime.robots.every(robot=>robot.connected),'initial connection');
    for (const robot of runtime.robots) robot.mqtt.close();
    await until(()=>runtime.robots.every(robot=>robot.connected && !robot.paused) &&
      runtime.events.filter(event=>event.type === 'auto_recovery_completed').length === 2 &&
      runtime.coreState.fleet.agvs.every(agv=>agv.online && agv.position_valid),'automatic recovery at known home');
    assert.deepEqual(runtime.robots.map(robot=>robot.node),['S1','S2']);

    await runtime.action('01',{action:'offline'});
    await until(()=>!runtime.robot('01').connected,'intentional offline');
    await pause(1300);
    assert.equal(runtime.robot('01').connected,false,'manual offline must not auto recover');
    await runtime.action('01',{action:'recover'});
    await until(()=>runtime.robot('01').connected && !runtime.robot('01').paused &&
      runtime.coreState.fleet.agvs.find(agv=>agv.id === '01')?.online,'manual recovery');
    console.log('PASS MQTT recovery: both idle robots at S1/S2 reconnect after unexpected drop; intentional offline remains stopped');
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
