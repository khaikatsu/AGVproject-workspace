const assert = require('assert/strict');
const fs = require('fs');
const path = require('path');
const {createSimulation} = require('../simulation/server');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,label) {const end = Date.now()+10000;while (Date.now()<end) {if (fn()) return;await pause(25);}throw new Error('Timeout: '+label);}
async function main() {
  const qa = path.resolve(__dirname,'../.qa');fs.mkdirSync(qa,{recursive:true});const dir = fs.mkdtempSync(path.join(qa,'simulation-mutex-'));
  const nodes = [['S1',0,0,'charging'],['S2',0,40,'charging'],['A1',30,0,'waypoint'],['A2',30,40,'waypoint'],['M5',100,20,'mutex'],['K1',150,10,'station'],['K2',150,30,'station']].map(([id,x,y,type])=>({id,x,y,type}));
  const edges = [['S1','A1',7],['S2','A2',9],['A1','M5',11],['A2','M5',13],['M5','K1',17],['M5','K2',19],['K1','S1',23],['K2','S2',29]].map(([from,to,weight])=>({from,to,weight}));
  const mapPath = path.join(dir,'fixture.json');fs.writeFileSync(mapPath,JSON.stringify({map_name:'Isolated FIFO fixture',nodes,edges,obstacles:[]}));
  const sim = await createSimulation({port:0,mapPath,runParent:dir}),r = sim.runtime,one = r.robot('01'),two = r.robot('02');
  try {
    r.configure({speedCmS:100,timeScale:1,drainPerCm:0});await until(()=>r.coreState?.shift?.can_start,'homes');
    await r.api('POST','/api/shift/start',{});await until(()=>r.coreState.fleet.agvs.every(a=>a.state === 'IDLE'),'idle');
    // Deterministic dwell at M5 allows the other real MQTT client to queue there.
    const originalEvent = r.event.bind(r);r.event = (type,data)=>{originalEvent(type,data);if (type === 'rfid' && data.robot === '01' && data.node === 'M5') one.paused = true;};
    const first = await r.api('POST','/api/agvs/01/goto',{target:'K1'}),second = await r.api('POST','/api/agvs/02/goto',{target:'K2'});
    await until(()=>one.node === 'M5' && r.coreState.mutex.M5.waiting.includes('02'),'real mutex contention');
    assert.equal(r.coreState.mutex.M5.owner,'01');assert.equal(two.node,'A2');assert.equal(two.grants.has('M5'),false);
    await pause(500);assert.equal(two.node,'A2');one.paused = false;
    await until(()=>r.tasks.get(first.task_id)?.status === 'completed' && r.tasks.get(second.task_id)?.status === 'completed','both complete');
    assert.ok(Math.abs(one.routeCost-35) < 1e-6);assert.ok(Math.abs(two.routeCost-41) < 1e-6);
    assert.ok(one.distanceCm > one.routeCost);assert.ok(two.waitSeconds >= 0.5);
    const events = r.events,release = events.find(e=>e.type === 'zone_exit' && e.robot === '01'),grant = events.find(e=>e.type === 'grant_ack' && e.robot === '02');
    assert.ok(release.seq < grant.seq,'second enters only after first verified exit');
    console.log('PASS simulation real FIFO: two clients queue at M5, exclusive grant, no entry before release, both task results, distance differs from weighted cost');
    console.log('Artifacts: '+sim.directory);
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
