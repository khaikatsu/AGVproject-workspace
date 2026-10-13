const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {createSimulation}=require('../simulation/server');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,label,timeout=60000){const end=Date.now()+timeout;while(Date.now()<end){const value=await check();if(value)return value;await pause(80);}throw Error('Timed out: '+label);}
async function main(){
  const parent=fs.mkdtempSync(path.join(os.tmpdir(),'agv-warehouse-actions-'));
  const evidence=fs.mkdtempSync(path.join(__dirname,'../.qa/warehouse-actions-integration-'));
  const sim=await createSimulation({port:0,runParent:parent,coreArgs:['--mode','robot01'],coreBinary:process.env.AGV_CORE_BIN||path.resolve(__dirname,'../build/agv_core.exe')});
  const post=async(route,body={})=>{const response=await fetch(sim.base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const value=await response.json();assert.ok(response.ok,JSON.stringify(value));return value;};
  const stages=[];
  try {
    sim.runtime.configure({speedCmS:200,timeScale:4,drainPerCm:0});
    await until(()=>sim.runtime.health==='Sẵn sàng'&&sim.runtime.coreState?.fleet.agvs.some(robot=>robot.id==='01'&&robot.state==='IDLE'),'initial RFID');
    assert.equal(sim.runtime.coreState.shift.test_mode,'robot01');await post('/api/shift/start');
    await until(()=>sim.runtime.coreState.shift.phase==='RUNNING','shift');
    for(const [kind,warehouse,gate] of [['pickup','K5','G5'],['dropoff','K3','G3']]){
      const started=await post('/api/agvs/01/warehouse-action',{action:kind,warehouse});let previous;
      await until(async()=>{
        const state=await (await fetch(sim.base+'/api/state')).json(),action=state.warehouse_actions.find(action=>action.id===started.action.id);
        if(action.status!==previous){stages.push({kind,phase:action.status,time:Date.now(),serviceUntil:action.service_until});previous=action.status;}
        assert.notEqual(action.status,'interrupted',action.reason);return action.status==='completed';
      },kind);
      const times=Object.fromEntries(stages.filter(stage=>stage.kind===kind).map(stage=>[stage.phase,stage.time]));
      const waiting=stages.find(stage=>stage.kind===kind&&stage.phase==='waiting');
      assert.ok(times.to_gate-(Date.parse(waiting.serviceUntil)-5000)>=4900,'five wall-clock seconds at warehouse');
      const robot=sim.runtime.robots.find(robot=>robot.id==='01');assert.equal(robot.node,gate);assert.equal(robot.path.length,0);
    }
    assert.equal((await (await fetch(sim.base+'/api/orders')).json()).length,0);
    fs.writeFileSync(path.join(evidence,'verification.json'),JSON.stringify({simDirectory:sim.directory,stages,passed:true},null,2));
    console.log('PASS Core robot01: standalone K5/5s/G5 then K3/5s/G3, zero orders; '+evidence);
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
