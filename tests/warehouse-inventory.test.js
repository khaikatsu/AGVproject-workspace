const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const os=require('node:os');
const path=require('node:path');
const {createDatabase}=require('../AGVwebserver/db');
const {createWarehouseInventory}=require('../AGVwebserver/warehouse-inventory');
const {createOrderService}=require('../AGVwebserver/orders');
const {createWarehouseActions}=require('../AGVwebserver/warehouse-actions');

const layout={slots:[1,2,3,4,5,6].map(n=>({node:`K${n}`,rack_id:n>=2&&n<=5?`R0${n-1}`:null})),physically_verified:true,expected_revision:0};
function coreState() {
  const nodes=[1,2,3,4,5,6].flatMap(n=>[{id:`K${n}`,type:'station'},{id:`G${n}`,type:'waypoint'}]);
  const edges=[1,2,3,4,5,6].map(n=>({from:`K${n}`,to:`G${n}`,weight:1,two_way:true}));
  for(const a of nodes)for(const b of nodes)if(a.id!==b.id)edges.push({from:a.id,to:b.id,weight:2});
  return {map:{nodes,edges,obstacles:[]},fleet:{agvs:[],tasks:[],pending_tasks:[],accepting_tasks:true},shift:{phase:'CLOSED'},mqtt_connected:true};
}
function robotAt(id,node) {
  return {id,node,state:'IDLE',online:true,position_valid:true,task_id:null,
    lift:{supported:true,ready:true,fresh:true,observed_node:node,active:false,state:'IDLE',command_id:0},
    virtual_cargo_known:false,virtual_cargo:false};
}
function done(robot,id,up) {
  robot.state='IDLE';robot.task_id=null;robot.virtual_cargo_known=true;robot.virtual_cargo=up;
  Object.assign(robot.lift,{command_id:id,state:up?'DONE_UP':'DONE_DOWN',active:false,fresh:true,observed_node:robot.node});
}
function fixture(t,filename=':memory:') {
  const db=createDatabase(filename);t.after(()=>db.close());
  const inventory=createWarehouseInventory({db}),core=coreState();inventory.configure(layout,core);
  return {db,inventory,core};
}
test('operator layout validates unique rack/slot IDs, verification, revision and closed shift',t=>{
  const {inventory,core}=fixture(t),revision=inventory.snapshot().revision;
  assert.equal(inventory.snapshot().racks.length,4);
  assert.throws(()=>inventory.configure({...layout,expected_revision:revision,physically_verified:false},core),/xác nhận/);
  assert.throws(()=>inventory.configure(layout,core),/đã thay đổi/);
  assert.throws(()=>inventory.configure({...layout,slots:layout.slots.map(slot=>({...slot,rack_id:'duplicate'}))},core),/trùng/);
  core.shift.phase='RUNNING';assert.throws(()=>inventory.configure(layout,core),/ca đã đóng/);
  assert.equal(inventory.snapshot().revision,revision);
  assert.equal(inventory.history().length,1);
});
test('two robots reserve different racks and empty slots; duplicate or occupied targets are blocked',t=>{
  const {inventory}=fixture(t);
  inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'});
  assert.throws(()=>inventory.reserve({key:'b',pickup:'K3',dropoff:'K1',robot:'02'}),/dành/);
  assert.throws(()=>inventory.reserve({key:'b',pickup:'K2',dropoff:'K6',robot:'02'}),/dành/);
  assert.throws(()=>inventory.reserve({key:'b',pickup:'K3',dropoff:'K4',robot:'02'}),/chưa trống/);
  assert.throws(()=>inventory.assertPlan({items:[{pickup:'K2',dropoff:'K1'},{pickup:'K2',dropoff:'K6'}]}));
  inventory.reserve({key:'b',pickup:'K3',dropoff:'K6',robot:'02'});
  assert.equal(inventory.snapshot().moves.length,2);
});
test('matching DONE moves the same rack slot → robot → slot once; rejected evidence and DB failure roll back',t=>{
  const {inventory,db}=fixture(t),robot=robotAt('01','K2');
  inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'});
  assert.throws(()=>inventory.confirmLift('a','pickup',robot,10,'s'),/Chưa có kết quả/);
  done(robot,10,true);
  db.db.exec("CREATE TRIGGER fail_inventory_event BEFORE INSERT ON warehouse_events WHEN NEW.event_key LIKE 'lift:%' BEGIN SELECT RAISE(ABORT,'disk-test'); END");
  assert.throws(()=>inventory.confirmLift('a','pickup',robot,10,'s'),/disk-test/);
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K2').rack_id,'R01');
  db.db.exec('DROP TRIGGER fail_inventory_event');
  inventory.confirmLift('a','pickup',robot,10,'s');inventory.confirmLift('a','pickup',robot,10,'s');
  assert.equal(inventory.history().filter(event=>event.type==='pickup').length,1);
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K2').rack_id,null);
  assert.deepEqual(inventory.snapshot().racks.find(rack=>rack.id==='R01').location,{type:'robot',id:'01'});
  robot.node='K1';done(robot,11,false);inventory.confirmLift('a','dropoff',robot,11,'s');
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K1').rack_id,'R01');
  assert.equal(inventory.snapshot().moves.length,0);
});
test('end-of-shift report waits for CLOSED, is immutable, and keeps carrying racks needing attention',t=>{
  const {inventory,core}=fixture(t);inventory.observeShift(core,'s');core.shift.phase='RUNNING';inventory.observeShift(core,'s');
  inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'});
  const robot=robotAt('01','K2');done(robot,1,true);inventory.confirmLift('a','pickup',robot,1,'s');
  core.shift.phase='CLOSING';inventory.observeShift(core,'s');assert.equal(inventory.reports().length,0);
  core.shift.phase='CLOSED';inventory.observeShift(core,'s',{orders:[{id:'unfinished',status:'needs_attention'}]});
  inventory.observeShift(core,'s');assert.equal(inventory.reports().length,1);
  const report=inventory.reports()[0];assert.equal(report.reason,'shift_closed');assert.equal(report.observation_gap,false);
  assert.deepEqual(report.summary,{pickups:1,dropoffs:0,on_robots:1,needs_attention:1});
  assert.equal(report.unfinished_orders.length,1);
  assert.equal(report.initial_state.slots.find(slot=>slot.node==='K2').rack_id,'R01');
  assert.equal(report.final_state.racks.find(rack=>rack.id==='R01').status,'needs_attention');
  inventory.configure({...layout,expected_revision:inventory.snapshot().revision},core);
  assert.deepEqual(inventory.reportById(report.id),report,'later reconciliation does not rewrite the saved report');
});
test('persistent inventory survives restart without replaying; Core session change archives an incomplete report',t=>{
  const dir=fs.mkdtempSync(path.join(os.tmpdir(),'agv-warehouse-')),filename=path.join(dir,'test.db');
  let db=createDatabase(filename),inventory=createWarehouseInventory({db});const core=coreState();
  try {
    inventory.configure(layout,core);core.shift.phase='RUNNING';inventory.observeShift(core,'s1');
    inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'});
    db.close();db=createDatabase(filename);inventory=createWarehouseInventory({db});
    assert.equal(inventory.snapshot().moves[0].status,'needs_attention');
    assert.throws(()=>inventory.reserve({key:'b',pickup:'K2',dropoff:'K6',robot:'02'}),/sẵn sàng/);
    inventory.observeShift(core,'s2');
    assert.equal(inventory.reports()[0].reason,'core_session_changed');
    assert.equal(inventory.reports()[0].observation_gap,true);
    assert.equal(inventory.history().filter(event=>event.type==='pickup').length,0);
  } finally {db.close();}
});
test('automatic order records servo pickup/dropoff before movement and reports final rack position',async t=>{
  const {inventory,core,db}=fixture(t),robot=robotAt('01','K2'),commands=[];
  core.fleet.agvs=[robot];inventory.observeShift(core,'s');core.shift.phase='RUNNING';inventory.observeShift(core,'s');
  const send=async payload=>{
    commands.push(payload);
    if(payload.cmd==='warehouse_lift')return {command_id:commands.length};
    const task={task_id:`task-${commands.length}`,assigned_agv:robot.id,target:payload.target,status:'dispatched'};
    core.fleet.tasks.push(task);return {task_id:task.task_id};
  };
  const service=createOrderService({db,inventory,getState:()=>core,getSession:()=> 's',send,publish:()=>{}});
  try {
    assert.throws(()=>service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K6',quantity:1}]}),/không có kệ/);
    const order=service.create({pickups:[{pickup:'K2',quantity:1}],deliveries:[{dropoff:'K1',quantity:1}]});
    await service.tick();await service.tick();assert.equal(order.items[0].status,'pickup_lift');
    assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K2').rack_id,'R01');
    done(robot,order.items[0].lift_command_id,true);await service.tick();
    assert.equal(order.items[0].status,'to_dropoff');assert.equal(order.items[0].rack_id,'R01');
    assert.equal(inventory.history()[0].type,'pickup');
    core.fleet.tasks.at(-1).status='completed';robot.node='K1';robot.lift.observed_node='K1';
    await service.tick();assert.equal(order.items[0].status,'dropoff_lift');
    done(robot,order.items[0].lift_command_id,false);await service.tick();
    assert.equal(order.items[0].status,'to_exit');assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K1').rack_id,'R01');
    core.fleet.tasks.at(-1).status='completed';robot.node='G1';await service.tick();assert.equal(order.status,'completed');
    core.shift.phase='CLOSED';inventory.observeShift(core,'s',{orders:service.list()});
    assert.deepEqual(inventory.reports()[0].summary,{pickups:1,dropoffs:1,on_robots:0,needs_attention:0});
  } finally {await service.close();}
});
test('standalone scope also updates the configured ledger, but never from the simulated timer',async t=>{
  const {inventory,core}=fixture(t),robot=robotAt('02','K2'),commands=[];
  core.fleet.agvs=[robot];core.shift={phase:'RUNNING',operation_profile:'scoped',active_robot_ids:['02']};
  const actions=createWarehouseActions({inventory,getState:()=>core,getSession:()=> 's',publish:()=>{},send:async payload=>{
    commands.push(payload);
    if(payload.cmd==='warehouse_lift')return {command_id:commands.length};
    const task={task_id:`task-${commands.length}`,target:payload.target,assigned_agv:'02',status:'dispatched'};
    core.fleet.tasks.push(task);return {task_id:task.task_id};
  }});
  try {
    const action=await actions.create('02',{warehouse:'K2',action:'pickup'});
    assert.equal(inventory.history().filter(event=>event.type==='pickup').length,0);
    done(robot,action.lift_command_id,true);await actions.tick();
    assert.equal(inventory.snapshot().racks.find(rack=>rack.id==='R01').location.id,'02');
    assert.equal(inventory.history().filter(event=>event.type==='pickup').length,1);
    core.fleet.tasks.at(-1).status='completed';robot.node='G2';await actions.tick();
    assert.equal(action.status,'completed');robot.node='K1';robot.lift.observed_node='K1';
    assert.equal(inventory.robotAvailable('02'),false);
    await assert.rejects(()=>actions.create('02',{warehouse:'K3',action:'pickup'}),/đang có kệ/);
    const down=await actions.create('02',{warehouse:'K1',action:'dropoff'});
    done(robot,down.lift_command_id,false);await actions.tick();
    assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K1').rack_id,'R01');
    assert.equal(inventory.history().filter(event=>event.type==='dropoff').length,1);
    assert.equal(inventory.robotAvailable('02'),true);
  } finally {await actions.close();}
});
