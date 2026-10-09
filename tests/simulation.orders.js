const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createSimulation} = require('../simulation/server');
const {shortestCost,normalizeOrder,chooseRobot} = require('../simulation/order-planner');

const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,label,timeout = 60000) {
  const deadline = Date.now()+timeout;
  while (Date.now() < deadline) {if (await check()) return;await pause(100);}
  throw new Error('Timeout: '+label);
}

async function main() {
  const map = JSON.parse(fs.readFileSync(path.resolve(__dirname,'../data/map_layout.json'),'utf8'));
  assert.equal(shortestCost({nodes:[{id:'A'},{id:'B'}],edges:[{from:'A',to:'B',weight:7,two_way:false}],obstacles:[]},'A','B'),7);
  assert.equal(shortestCost({nodes:[{id:'A'},{id:'B'}],edges:[{from:'A',to:'B',weight:7,two_way:false}],obstacles:[]},'B','A'),Infinity);
  assert.equal(normalizeOrder(map,{pickup:'K1',deliveries:[{dropoff:'K2',quantity:2},{dropoff:'K4',quantity:3}]}).total,5);
  assert.throws(()=>normalizeOrder(map,{pickup:'K1',deliveries:[{dropoff:'K2',quantity:21}]}),/20/);
  assert.throws(()=>normalizeOrder(map,{pickup:'K1',deliveries:[{dropoff:'K1',quantity:1}]}),/khác kho/);
  const choice = chooseRobot(map,[{id:'02',node:'S2'},{id:'01',node:'S1'}],'K1','K2');
  assert.equal(choice.robot.id,'02'); // Directed map: S2 → K1 costs 210, S1 → K1 costs 550.

  const qa = path.resolve(__dirname,'../.qa/order-tests');fs.mkdirSync(qa,{recursive:true});
  const sim = await createSimulation({port:0,runParent:qa,twoRobotTrial:true});
  const runtime = sim.runtime;
  try {
    await until(()=>runtime.health === 'Sẵn sàng' && runtime.coreState?.shift?.can_start,'ready');
    assert.equal(runtime.coreState.shift.two_robot_trial,true);
    runtime.configure({timeScale:3,speedCmS:50,drainPerCm:0,serviceSeconds:0.2});
    await runtime.api('POST','/api/shift/start',{});
    await until(()=>runtime.coreState?.shift?.phase === 'RUNNING' &&
      runtime.coreState.fleet.accepting_tasks === true &&
      runtime.coreState.fleet.agvs.every(agv=>agv.state === 'IDLE'),'shift running');

    const invalid = await fetch(sim.base+'/sim-api/orders',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({pickup:'K1',deliveries:[{dropoff:'K2',quantity:21}]})});
    assert.equal(invalid.status,400);
    const response = await fetch(sim.base+'/sim-api/orders',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({pickup:'K1',deliveries:[{dropoff:'K2',quantity:2},{dropoff:'K4',quantity:3}]})});
    assert.equal(response.status,200);const order = (await response.json()).order;
    assert.equal(order.total,5);
    let parallelJobs = false,parallelMotion = false;
    await until(()=>{
      const active = runtime.jobs.filter(job=>job.orderId === order.id && !job.ended && job.taskIds.length);
      if (new Set(active.map(job=>job.robot)).size === 2) parallelJobs = true;
      if (runtime.robots.filter(robot=>robot.edge && active.some(job=>job.robot === robot.id)).length === 2) parallelMotion = true;
      return runtime.orders[0].status === 'completed';
    },'five delivered items',120000);
    assert.ok(parallelJobs,'two robots must hold Core tasks for one order at the same time');
    assert.ok(parallelMotion,'two robots must move at the same time for one order');
    const jobs = runtime.jobs.filter(job=>job.orderId === order.id && job.phase === 'completed');
    assert.equal(jobs.length,5);assert.equal(new Set(jobs.map(job=>job.robot)).size,2);
    assert.deepEqual(runtime.orders[0].items.map(item=>item.dropoff),['K2','K2','K4','K4','K4']);
    assert.ok(runtime.orders[0].items.every(item=>item.status === 'delivered'));
    for (const job of jobs) {
      assert.equal(job.phase,'completed');
      for (const taskId of job.taskIds) assert.equal(runtime.tasks.get(taskId)?.status,'completed');
    }
    assert.ok(runtime.robots.every(robot=>!robot.cargo));
    const report = await (await fetch(sim.base+'/sim-api/report')).json();
    assert.equal(report.orders[0].items.filter(item=>item.status === 'delivered').length,5);
    const otherA = runtime.createOrder({pickup:'K2',deliveries:[{dropoff:'K3',quantity:1}]});
    const otherB = runtime.createOrder({pickup:'K5',deliveries:[{dropoff:'K4',quantity:1}]});
    await until(()=>otherA.status === 'completed' && otherB.status === 'completed','two independent orders',60000);
    const independent = [otherA,otherB].map(order=>runtime.jobs.find(job=>job.orderId === order.id && job.phase === 'completed'));
    assert.equal(new Set(independent.map(job=>job.robot)).size,2);
    assert.ok(independent[0].started < independent[1].ended && independent[1].started < independent[0].ended,
      'independent orders overlap when Core permits');
    const faultOrder = runtime.createOrder({pickup:'K1',deliveries:[{dropoff:'K6',quantity:2}]});
    await until(()=>faultOrder.items.some(item=>item.status === 'in_progress'),'fault order dispatched',20000);
    const activeItem = faultOrder.items.find(item=>item.status === 'in_progress');
    runtime.robot(activeItem.robot).batteryChange(0);
    await until(()=>faultOrder.status === 'needs_attention','fault order needs attention',20000);
    assert.ok(faultOrder.items.some(item=>item.status === 'interrupted'));
    assert.ok(faultOrder.items.filter(item=>item.status === 'delivered').length < faultOrder.total,
      'fault does not count unconfirmed cargo as delivered');
    const jobCount = runtime.jobs.length;await pause(500);
    assert.equal(runtime.jobs.length,jobCount,'no new order item dispatched after failure');
    console.log('PASS order: 5 kiện K1 → K2/K4, hai xe cùng chuyển động, mỗi kiện đối chiếu task_id Core và RFID');
    console.log('PASS parallel: hai đơn ở kho khác nhau chạy chồng thời gian trên hai xe');
    console.log('PASS fault: xe hết pin làm đơn cần đối soát, không tự tính kiện chưa giao là hoàn thành');
    console.log('Artifacts: '+sim.directory);
  } catch (error) {
    fs.writeFileSync(path.join(sim.directory,'failure.json'),JSON.stringify(runtime.report(),null,2));
    throw error;
  } finally {await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
