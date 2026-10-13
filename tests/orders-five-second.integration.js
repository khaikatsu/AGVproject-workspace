const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {createSimulation}=require('../simulation/server');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,label,timeout=60000){const end=Date.now()+timeout;while(Date.now()<end){const value=await check();if(value)return value;await pause(80);}throw Error('Timed out: '+label);}
async function main(){
  const parent=fs.mkdtempSync(path.join(os.tmpdir(),'agv-warehouse-five-second-'));
  const evidence=fs.mkdtempSync(path.join(__dirname,'../.qa/warehouse-five-second-'));
  const sim=await createSimulation({port:0,runParent:parent,twoRobotTrial:true,coreBinary:process.env.AGV_CORE_BIN||path.resolve(__dirname,'../build/robot01-morning/agv_core.exe')});
  const post=async(route,body={})=>{const response=await fetch(sim.base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const value=await response.json();assert.ok(response.ok,JSON.stringify(value));return value;};
  const stages=[];
  try {
    sim.runtime.configure({speedCmS:200,timeScale:4,drainPerCm:0});
    await until(()=>sim.runtime.health==='Sẵn sàng'&&sim.runtime.coreState?.fleet.agvs.length===2,'initial RFID');
    await post('/api/shift/start');
    await until(()=>sim.runtime.coreState.shift.phase==='RUNNING'&&sim.runtime.coreState.fleet.agvs.every(robot=>robot.state==='IDLE'),'ready shift');
    const created=await post('/api/orders',{pickups:[{pickup:'K5',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    let last;
    const finished=await until(async()=>{
      const orders=await (await fetch(sim.base+'/api/orders')).json(),order=orders.find(order=>order.id===created.order.id),item=order.items[0];
      if(item.status!==last){stages.push({phase:item.status,time:Date.now(),robot:item.robot,task:item.task_id});last=item.status;}
      assert.equal(order.status==='needs_attention',false,order.reason);
      return order.status==='completed'&&order;
    },'pickup, five second dwell, delivery, five second dwell, reverse exit',90000);
    const times=Object.fromEntries(stages.map(stage=>[stage.phase,stage.time]));
    assert.ok(times.to_dropoff-times.pickup_service>=4900,'pickup service is five wall-clock seconds');
    assert.ok(times.to_exit-times.dropoff_service>=4900,'dropoff service is five wall-clock seconds');
    const robot=sim.runtime.robots.find(robot=>robot.id===finished.items[0].robot);
    assert.equal(robot.node,'G3');assert.equal(robot.path.length,0);assert.equal(robot.cargo,false);
    const events=fs.readFileSync(path.join(sim.directory,'events.jsonl'),'utf8').trim().split('\n').map(line=>JSON.parse(line));
    const plans=events.filter(event=>event.type==='route_plan');
    assert.ok(plans.some(event=>event.path?.[0]==='G5'&&event.path?.at(-1)==='K3'),'delivery starts by leaving K5 through G5');
    assert.ok(plans.some(event=>event.path?.length===1&&event.path[0]==='G3'),'dropoff returns to its gate');
    fs.writeFileSync(path.join(evidence,'verification.json'),JSON.stringify({simDirectory:sim.directory,stages,order:finished,plans,passed:true},null,2));
    console.log('PASS real Core cargo flow: pickup 5s, K5→G5→K3, dropoff 5s, exit G3; '+evidence);
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
