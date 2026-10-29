// Server/web only: private SQLite files and fake Core callbacks, no firmware.
const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {createDatabase}=require('../AGVwebserver/db');
const {createOrderService}=require('../AGVwebserver/orders');
const {createWarehouseInventory}=require('../AGVwebserver/warehouse-inventory');
const {createShiftReview}=require('../AGVwebserver/shift-review');
function fixture(t) {
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.resolve(__dirname,'../.qa/shift-web-'));
  const db=createDatabase(path.join(dir,'test.db'));
  const state={core_session_id:'shift-test',mqtt_connected:true,shift:{phase:'CLOSED'},mutex:{},
    map:{nodes:['S1','G1','G2','K1','K2'].map(id=>({id,type:id[0]==='K'?'station':'waypoint'})),edges:[
      {from:'S1',to:'G1',weight:1,two_way:true},{from:'G1',to:'K1',weight:1,two_way:true},
      {from:'G1',to:'G2',weight:1,two_way:true},{from:'G2',to:'K2',weight:1,two_way:true}]},
    fleet:{accepting_tasks:true,tasks:[],pending_tasks:[],agvs:[{id:'01',node:'G1',state:'IDLE',online:true,
      position_valid:true,recovery_node_verified:true,path:[],task_id:null,
      lift:{supported:true,ready:true,fresh:true,active:false,observed_node:'G1'},virtual_cargo:false,virtual_cargo_known:true}]}};
  const inventory=createWarehouseInventory({db});
  inventory.configure({slots:[{node:'K1',rack_id:'R01'},{node:'K2',rack_id:null}],expected_revision:0,physically_verified:true},state);
  const commands=[];
  const orders=createOrderService({db,getState:()=>state,getSession:()=>state.core_session_id,inventory,publish:()=>{},
    send:async payload=>{commands.push(payload);
      if(payload.cmd==='resume_task'){const task=state.fleet.tasks.find(task=>task.task_id===payload.task_id);task.status='dispatched';return {task_id:task.task_id};}
      if(payload.cmd==='warehouse_lift')return {command_id:500};
      const task={task_id:`task-${commands.length}`,target:payload.target,status:'dispatched',assigned_agv:'01'};
      state.fleet.tasks.push(task);return {task_id:task.task_id};
    }});
  t.after(async()=>{await orders.close();db.close();});
  return {db,state,inventory,orders,commands,robot:state.fleet.agvs[0]};
}
test('ending a shift closes queued orders and forbids replay in the next shift',async t=>{
  const f=fixture(t);f.state.shift.phase='RUNNING';f.robot.state='ERROR';
  const order=f.orders.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});
  await f.orders.tick();await f.orders.pauseForShiftEnd();f.orders.endShift();f.orders.unpause();
  assert.equal(order.closed_by_shift,true);assert.equal(order.status,'closed');
  f.robot.state='IDLE';await f.orders.tick();assert.equal(f.commands.length,0);
  await assert.rejects(f.orders.resume(order.id,1,true,f.inventory.snapshot().revision),/đúng ca/);
});
test('resume the same transport task only after RFID and cargo checks; never replay a lift',async t=>{
  const f=fixture(t);f.state.shift.phase='RUNNING';
  const order=f.orders.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});
  await f.orders.tick();const item=order.items[0],task=f.state.fleet.tasks[0];
  assert.equal(item.status,'to_pickup');task.status='interrupted';task.resume_node='G1';
  f.robot.state='ERROR';await f.orders.tick();assert.equal(item.suspended_status,'to_pickup');
  f.robot.state='IDLE';f.robot.recovery_node_verified=false;
  await assert.rejects(f.orders.resume(order.id,1,true,f.inventory.snapshot().revision),/Phục hồi xe/);
  f.robot.recovery_node_verified=true;
  const revisionBeforeHeading=f.inventory.snapshot().revision;
  f.robot.heading_confirmation_required=true;
  await assert.rejects(f.orders.resume(order.id,1,true,revisionBeforeHeading),/xác nhận hướng/);
  assert.equal(f.inventory.snapshot().revision,revisionBeforeHeading);
  f.robot.heading_confirmation_required=false;
  f.robot.heading_confirmation_pending=true;
  await assert.rejects(f.orders.resume(order.id,1,true,revisionBeforeHeading),/xác nhận hướng/);
  f.robot.heading_confirmation_pending=false;
  await assert.rejects(f.orders.resume(order.id,1,true,-1),/Kho đã thay đổi/);
  await f.orders.resume(order.id,1,true,f.inventory.snapshot().revision);
  assert.equal(item.status,'to_pickup');assert.equal(f.commands.at(-1).cmd,'resume_task');
  assert.equal(f.commands.at(-1).task_id,task.task_id);assert.equal(f.inventory.snapshot().moves[0].status,'reserved');
  assert.equal(f.commands.some(command=>command.cmd==='warehouse_lift'),false);
  item.status='pickup_lift';f.robot.state='ERROR';await f.orders.tick();f.robot.state='IDLE';
  const before=f.commands.length;await assert.rejects(f.orders.resume(order.id,1,true,f.inventory.snapshot().revision),/nâng\/hạ/);
  assert.equal(f.commands.length,before);
});

test('operator recovery automatically resumes a managed task with the live inventory revision',async t=>{
  const f=fixture(t);f.state.shift.phase='RUNNING';
  const order=f.orders.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});
  await f.orders.tick();const task=f.state.fleet.tasks[0];task.status='interrupted';task.resume_node='G1';
  f.robot.state='ERROR';await f.orders.tick();
  f.robot.state='IDLE';f.robot.recovery_auto_resume=true;f.robot.heading_confirmation_required=true;
  const before=f.commands.length;await f.orders.tick();assert.equal(f.commands.length,before);
  f.robot.heading_confirmation_required=false;await f.orders.tick();
  assert.equal(f.commands.at(-1).cmd,'resume_task');assert.equal(f.commands.at(-1).task_id,task.task_id);
  assert.equal(order.items[0].status,'to_pickup');assert.equal(f.inventory.snapshot().moves[0].status,'reserved');
  assert.equal(f.commands.filter(c=>c.cmd==='resume_task').length,1);
});
test('binary inventory rejects quantities above one before any Core dispatch',async t=>{
  const f=fixture(t);f.state.shift.phase='RUNNING';
  assert.throws(()=>f.orders.create({pickups:[{pickup:'K1',quantity:2}],deliveries:[{dropoff:'K2',quantity:2}]}),/phải bằng 1/);
  assert.equal(f.commands.length,0);
});
test('shift review keeps audit, rejects new incidents/stale tokens and retains unresolved physical blockers',t=>{
  const f=fixture(t),review=createShiftReview({...f,getState:()=>f.state,getSession:()=>f.state.core_session_id,
    actions:{list:()=>[]}});
  f.state.shift.phase='RUNNING';f.inventory.observeShift(f.state,f.state.core_session_id);review.observe();
  f.db.insertSystemLog(1,'','AGV_01 BAO LOI: LINE_LOST',f.state.core_session_id);
  f.state.shift.phase='CLOSED';const first=review.snapshot();assert.equal(first.incidents.length,1);
  f.db.insertSystemLog(2,'','AGV_02 MAT KET NOI',f.state.core_session_id);
  assert.throws(()=>review.reconcile({review_token:first.review_token,physically_verified:true}),/đã thay đổi/);
  let next=review.snapshot();f.state.mutex.M1={owner:'02'};
  next=review.snapshot();assert.throws(()=>review.reconcile({review_token:next.review_token,physically_verified:true}),/quyền vùng/);
  f.state.mutex={};next=review.snapshot();const logs=f.db.getRecentSystemLogs().length;
  assert.equal(review.reconcile({review_token:next.review_token,physically_verified:true}).incidents.length,0);
  assert.equal(f.db.getRecentSystemLogs().length,logs);assert.equal(f.db.db.prepare('SELECT COUNT(*) n FROM shift_error_reviews').get().n,1);
  f.db.insertSystemLog(3,'','AGV_01 BAO LOI: NEW_FAULT',f.state.core_session_id);
  assert.equal(review.snapshot().incidents.length,1);
});
