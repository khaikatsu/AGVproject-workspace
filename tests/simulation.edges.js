const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createSimulation}=require('../simulation/server');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,label,timeout=30000){const end=Date.now()+timeout;while(Date.now()<end){if(await fn())return;await pause(60);}throw Error('Timeout '+label);}
async function main(){
 const parent=path.resolve(__dirname,'../.qa/edge-obstacles');fs.mkdirSync(parent,{recursive:true});
 const sim=await createSimulation({port:0,runParent:parent});const r=sim.runtime;
 const post=async body=>{const x=await fetch(sim.base+'/sim-api/edge-obstacles',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return {status:x.status,data:await x.json()};};
 try{
  await until(()=>r.health==='Sẵn sàng'&&r.coreState?.shift?.can_start,'ready');
  assert.equal((await post({from:'S1',to:'K1',blocked:true})).status,400);
  assert.equal((await post({from:'S1',to:'GS1',blocked:'yes'})).status,400);
  const edges=[...new Map(r.map.edges.map(e=>[[e.from,e.to].sort().join('|'),e])).values()];
  for(const e of edges){assert.equal((await post({...e,blocked:true})).status,200);await until(()=>r.coreState.map.blocked_edges.some(x=>[x.from,x.to].sort().join('|')===[e.from,e.to].sort().join('|')),'blocked snapshot');assert.equal((await post({...e,blocked:false})).status,200);await until(()=>!r.coreState.map.blocked_edges.length,'clear snapshot');}
  r.configure({speedCmS:100,timeScale:5,drainPerCm:0});await r.api('POST','/api/shift/start',{});
  // Blocking the only station exit produces Core BLOCKED, then clearing resumes the same task.
  r.robot('01').paused=true;const task=await r.goto('01','K2');
  await post({from:'S1',to:'GS1',blocked:true});await until(()=>r.coreState.fleet.agvs.find(a=>a.id==='01').state==='BLOCKED','no path');assert.equal(r.robot('01').node,'S1');
  await post({from:'GS1',to:'S1',blocked:false});r.robot('01').paused=false;await until(()=>r.tasks.get(task.task_id)?.status==='completed','resume same task');
  // G2 -> K6 uses M5 -> M6 normally; blocking it forces the outer directed loop.
  await post({from:'M5',to:'M6',blocked:true});await until(()=>r.coreState.map.blocked_edges.length,'shortcut blocked');
  const reroute=await r.goto('01','K6');await until(()=>r.tasks.get(reroute.task_id)?.status==='completed','rerouted arrival');
  const plan=r.events.filter(e=>e.type==='route_plan'&&e.robot==='01').at(-1);assert.ok(plan.path.includes('C3'));assert.ok(!plan.path.some((n,i)=>n==='M6'&&plan.path[i-1]==='M5'));
  await post({from:'M5',to:'M6',blocked:false});await until(()=>!r.coreState.map.blocked_edges.length,'shortcut open');
  // An obstacle on the occupied edge never teleports or reverses a robot from its last RFID.
  r.configure({speedCmS:25,timeScale:1});const moving=await r.goto('01','K1');
  await until(()=>r.robot('01').edge?.progress>.05,'mid-edge');const edge={...r.robot('01').edge};
  await post({from:edge.from,to:edge.to,blocked:true});const progress=r.robot('01').edge.progress;
  await pause(450);assert.equal(r.robot('01').edge.progress,progress);assert.equal(r.robot('01').fault,'');
  await post({from:edge.from,to:edge.to,blocked:false});r.configure({speedCmS:100,timeScale:5});
  await until(()=>r.tasks.get(moving.task_id)?.status==='completed','mid-edge resumed',45000);assert.equal(r.robot('01').fault,'');
  // A future blocked shortcut is replaced only after the current edge reaches RFID.
  r.configure({speedCmS:25,timeScale:1});const future=await r.goto('01','K6');
  await until(()=>r.robot('01').edge?.progress>.05,'future test mid-edge');
  const before={...r.robot('01').edge};assert.notEqual([before.from,before.to].sort().join('|'),'M5|M6');
  await post({from:'M5',to:'M6',blocked:true});assert.equal(r.robot('01').edge.from,before.from);assert.equal(r.robot('01').edge.to,before.to);
  assert.ok(r.pendingReplans.has('01'));r.configure({speedCmS:100,timeScale:5});
  await until(()=>r.tasks.get(future.task_id)?.status==='completed','future edge reroute',45000);
  assert.equal(r.pendingReplans.size,0);assert.equal(r.robot('01').fault,'');
  const futurePlan=r.events.filter(e=>e.type==='route_plan'&&e.robot==='01').at(-1);assert.ok(futurePlan.path.includes('C1'));assert.ok(!futurePlan.path.some((n,i)=>n==='M6'&&futurePlan.path[i-1]==='M5'));
  fs.writeFileSync(path.join(sim.directory,'verification.json'),JSON.stringify({pass:true,edges:edges.length,reroutePlan:plan},null,2));
  console.log('PASS '+edges.length+' segments: block/clear, Core alternate route, no-path resume same task, mid-edge hold/resume, isolated broker and DB; '+sim.directory);
 }finally{await sim.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
