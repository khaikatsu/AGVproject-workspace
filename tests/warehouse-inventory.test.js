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

for(const id of ['01','02'])test(`verified warehouse reconciliation releases a hidden ended order's hold on ${id}`,async t=>{
  const {db,inventory,core}=fixture(t),commands=[];
  const old={id:`legacy-${id}`,status:'needs_attention',closed_by_shift:true,core_session_id:'old-core',
    created:'2026-01-01T00:00:00.000Z',updated:'2026-01-02T00:00:00.000Z',deleted_at:'2026-01-02T00:00:00.000Z',
    items:[{number:1,pickup:'K2',dropoff:'K3',rack_id:'R01',robot:id,status:'interrupted',task_id:'old-task'}]};
  db.saveOrder(old);const saved=structuredClone(db.getOrders()),before=inventory.snapshot();
  core.shift.phase='RUNNING';core.fleet.agvs=[robotAt(id,'G2')];
  const service=createOrderService({db,inventory,getState:()=>core,getSession:()=> 'new-core',publish:()=>{},
    send:async payload=>{commands.push(payload);return {task_id:'new-task'};}});
  t.after(()=>service.close());
  const projected=service.list({includeDeleted:true}).find(order=>order.id===old.id);
  assert.equal(projected.status,'closed');assert.equal(projected.resolution,'warehouse_reconciled');
  assert.ok(projected.warehouse_reconciliation.event_id);assert.equal(projected.deleted_at,old.deleted_at);
  assert.deepEqual(db.getOrders(),saved,'read projection preserves original audit records');
  assert.deepEqual(inventory.snapshot(),before);assert.equal(commands.length,0);
  await service.resolve(old.id,true,before.revision);
  assert.equal(commands.length,0,'already verified closure sends no reset, servo or cargo command');
  const order=service.create({pickups:[{pickup:'K2',quantity:1}],deliveries:[{dropoff:'K1',quantity:1}]});
  await service.tick();
  assert.equal(order.items[0].robot,id);assert.equal(order.items[0].status,'to_pickup');
  assert.equal(commands[0].cmd,'direct_goto');assert.equal(commands[0].agv_id,id);
  assert.equal(commands[0].target,'K2');assert.equal(commands.length,1);
  assert.equal(db.getOrders().find(order=>order.id===old.id).status,'needs_attention');
});

test('warehouse proof does not release current, unverified or physically outstanding orders',t=>{
  const {inventory,core}=fixture(t);
  const ended={id:'held',status:'needs_attention',closed_by_shift:true,
    updated:'2026-01-01T00:00:00.000Z',items:[{number:1,pickup:'K2',dropoff:'K1',robot:'01',status:'interrupted'}]};
  const proof=inventory.reconciliationFor(ended);assert.ok(proof);
  assert.equal(inventory.reconciliationFor({...ended,closed_by_shift:false}),null);
  assert.equal(inventory.reconciliationFor({...ended,status:'active'}),null);
  assert.equal(inventory.reconciliationFor({...ended,updated:'invalid'}),null);
  assert.equal(inventory.reconciliationFor({...ended,updated:proof.created_at}),null,'same millisecond is ambiguous');
  assert.equal(inventory.reconciliationFor({...ended,updated:'2099-01-01T00:00:00.000Z'}),null);
  inventory.reserve({key:'order:held:1',pickup:'K2',dropoff:'K1',robot:'01'});
  inventory.interrupt('order:held:1','not yet reconciled');
  assert.equal(inventory.reconciliationFor(ended),null,'open physical move retains its hold');
  assert.throws(()=>inventory.configure({...layout,expected_revision:inventory.snapshot().revision,physically_verified:false},core),/xác nhận/);
  assert.equal(inventory.reconciliationFor(ended),null);
  inventory.configure({...layout,expected_revision:inventory.snapshot().revision},core);
  assert.ok(inventory.reconciliationFor(ended),'a later successful full reconciliation updates cached proof');
});

test('reconciled order projection survives dashboard deletion and reload without rewriting old order status',async t=>{
  const {db,inventory,core}=fixture(t);
  const old={id:'old-hidden',status:'needs_attention',closed_by_shift:true,created:'2026-01-01T00:00:00.000Z',
    updated:'2026-01-02T00:00:00.000Z',items:[{number:1,robot:'02',status:'interrupted'}]};
  db.saveOrder(old);
  const make=ledger=>createOrderService({db,inventory:ledger,getState:()=>core,getSession:()=> 's',publish:()=>{},
    send:async()=>{throw Error('must not issue robot commands');}});
  const service=make(inventory);service.remove(old.id);await service.close();
  const reloaded=make(createWarehouseInventory({db}));t.after(()=>reloaded.close());
  const view=reloaded.list({includeDeleted:true})[0];
  assert.equal(view.status,'closed');assert.ok(view.deleted_at);assert.ok(view.warehouse_reconciliation);
  assert.equal(db.getOrders()[0].status,'needs_attention');
});
test('test reconciliation restores both interrupted racks, clears reservations, persists and permits a new pickup',async t=>{
  const {db,inventory,core}=fixture(t),commands=[];
  const order={id:'rollback-two',status:'needs_attention',created:new Date().toISOString(),items:[
    {number:1,pickup:'K2',dropoff:'K1',robot:'01',rack_id:'R01',status:'interrupted'},
    {number:2,pickup:'K3',dropoff:'K6',robot:'02',rack_id:'R02',status:'interrupted'}]};
  const robots=[robotAt('01','K2'),robotAt('02','K3')];core.fleet.agvs=robots;
  for(const item of order.items)inventory.reserve({key:`order:${order.id}:${item.number}`,...item});
  done(robots[0],77,true);inventory.confirmLift('order:rollback-two:1','pickup',robots[0],77,'s');
  for(const item of order.items)inventory.interrupt(`order:${order.id}:${item.number}`,'test interruption');
  db.saveOrder(order);const before=inventory.snapshot();
  const service=createOrderService({db,inventory,getState:()=>core,getSession:()=> 's',publish:()=>{},send:async payload=>{
    commands.push(payload);
    for(const entry of payload.robots||[]) {const robot=robots.find(robot=>robot.id===entry.id);robot.virtual_cargo=false;robot.virtual_cargo_known=true;}
    return {task_id:'next'};
  }});t.after(()=>service.close());
  await assert.rejects(()=>service.resolve(order.id,false,before.revision),/Cần xác nhận/);
  await assert.rejects(()=>service.resolve(order.id,true,before.revision-1),/đã thay đổi/);
  robots[1].state='WORKING';await assert.rejects(()=>service.resolve(order.id,true,before.revision),/Dừng xe 02/);
  assert.equal(commands.length,0);assert.deepEqual(inventory.snapshot(),before);robots[1].state='IDLE';
  db.db.exec("CREATE TRIGGER rollback_fail BEFORE INSERT ON warehouse_events WHEN json_extract(NEW.event_json,'$.type')='rollback' BEGIN SELECT RAISE(ABORT,'disk-test'); END");
  await assert.rejects(()=>service.resolve(order.id,true,before.revision),/disk-test/);
  assert.deepEqual(inventory.snapshot(),before);assert.equal(db.getOrders()[0].status,'needs_attention');
  db.db.exec('DROP TRIGGER rollback_fail');
  db.db.exec("CREATE TRIGGER order_save_fail BEFORE UPDATE ON warehouse_orders WHEN json_extract(NEW.order_json,'$.status')='closed' BEGIN SELECT RAISE(ABORT,'order-disk-test'); END");
  await assert.rejects(()=>service.resolve(order.id,true,before.revision),/order-disk-test/);
  assert.deepEqual(inventory.snapshot(),before);assert.equal(db.getOrders()[0].status,'needs_attention');
  assert.equal(inventory.history().filter(event=>event.type==='rollback').length,0);
  db.db.exec('DROP TRIGGER order_save_fail');
  const closed=await service.resolve(order.id,true,before.revision);
  assert.equal(closed.status,'closed');assert.equal(closed.rolled_back.length,2);
  assert.deepEqual(inventory.snapshot().slots,before.slots.map(slot=>({...slot,rack_id:slot.node==='K2'?'R01':slot.rack_id})));
  assert.equal(inventory.snapshot().moves.length,0);assert.ok(inventory.robotAvailable('01')&&inventory.robotAvailable('02'));
  assert.equal(inventory.history().filter(event=>event.type==='rollback').length,2);
  const count=commands.length;await service.resolve(order.id,true,before.revision);assert.equal(commands.length,count,'retry is idempotent');
  assert.deepEqual(commands[0],{cmd:'reconcile_test_cargo',physically_clear:true,robots:[{id:'01',lift_command_id:77},{id:'02',lift_command_id:0}]});
  const restarted=createWarehouseInventory({db});assert.equal(restarted.snapshot().moves.length,0);
  assert.equal(restarted.snapshot().racks.find(rack=>rack.id==='R01').location.id,'K2');
  core.shift.phase='RUNNING';
  const next=service.create({pickups:[{pickup:'K2',quantity:1}],deliveries:[{dropoff:'K1',quantity:1}]});
  await service.tick();assert.equal(next.items[0].status,'waiting_pickup_lift');
});

test('legacy closed/deleted order can rollback only its remaining interrupted move, never without explicit confirmation',async t=>{
  const {db,inventory,core}=fixture(t),robot=robotAt('01','K2'),commands=[];
  const order={id:'legacy-closed',status:'closed',deleted_at:new Date().toISOString(),created:new Date(0).toISOString(),items:[
    {number:1,pickup:'K2',dropoff:'K1',robot:'01',rack_id:'R01',status:'interrupted'}]};
  inventory.reserve({key:'order:legacy-closed:1',pickup:'K2',dropoff:'K1',robot:'01'});
  done(robot,19,true);inventory.confirmLift('order:legacy-closed:1','pickup',robot,19,'s');
  inventory.interrupt('order:legacy-closed:1','legacy interruption');db.saveOrder(order);
  db.saveOrder({id:'closed-without-move',status:'closed',created:new Date(0).toISOString(),items:[]});
  core.fleet.agvs=[];
  const before=inventory.snapshot();
  const service=createOrderService({db,inventory,getState:()=>core,getSession:()=> 's',publish:()=>{},send:async payload=>{commands.push(payload);return {};}});
  t.after(()=>service.close());
  assert.deepEqual(inventory.snapshot(),before,'loading legacy records never migrates their cargo automatically');
  await assert.rejects(()=>service.resolve(order.id,false,before.revision),/Cần xác nhận/);
  await assert.rejects(()=>service.resolve('closed-without-move',true,before.revision),/chưa ở trạng thái/);
  await assert.rejects(()=>service.resolve(order.id,true,before.revision),/Kết nối lại xe 01/);
  assert.equal(commands.length,0);assert.deepEqual(inventory.snapshot(),before);
  core.fleet.agvs=[robot];
  const result=await service.resolve(order.id,true,before.revision);
  assert.equal(result.resolution,'rollback_to_pickup');assert.equal(result.deleted_at,order.deleted_at);
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K2').rack_id,'R01');
  assert.equal(inventory.snapshot().moves.length,0);assert.equal(commands.length,1);
  await service.resolve(order.id,true,before.revision);assert.equal(commands.length,1);
});

test('rollback preserves a confirmed dropoff, rejects occupied source, and leaves saved reports immutable',async t=>{
  const {db,inventory,core}=fixture(t),robot=robotAt('01','K2');
  const order={id:'placed',status:'needs_attention',created:new Date().toISOString(),items:[
    {number:1,pickup:'K2',dropoff:'K1',robot:'01',rack_id:'R01',status:'interrupted'}]};
  inventory.observeShift(core,'s');core.shift.phase='RUNNING';inventory.observeShift(core,'s');
  inventory.reserve({key:'order:placed:1',pickup:'K2',dropoff:'K1',robot:'01'});
  done(robot,1,true);inventory.confirmLift('order:placed:1','pickup',robot,1,'s');
  inventory.interrupt('order:placed:1','test');
  const row=JSON.parse(db.db.prepare('SELECT state_json FROM warehouse_inventory').get().state_json);
  row.slots.find(slot=>slot.node==='K2').rack_id='OTHER';
  db.db.prepare('UPDATE warehouse_inventory SET state_json=?').run(JSON.stringify(row));
  assert.throws(()=>inventory.rollbackPlan(order),/kệ khác/);
  row.slots.find(slot=>slot.node==='K2').rack_id=null;row.moves[0].status='carrying';
  db.db.prepare('UPDATE warehouse_inventory SET state_json=?').run(JSON.stringify(row));
  robot.node='K1';done(robot,2,false);inventory.confirmLift('order:placed:1','dropoff',robot,2,'s');
  core.shift.phase='CLOSED';inventory.observeShift(core,'s');
  const report=inventory.reports()[0],before=inventory.snapshot();
  assert.deepEqual(inventory.rollbackPlan(order),[]);
  inventory.rollbackOrder(order,before.revision,()=>db.saveOrder({...order,status:'closed'}));
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K1').rack_id,'R01');
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K2').rack_id,null);
  assert.deepEqual(inventory.reportById(report.id),report);
  db.saveOrder(order);const commands=[];
  // This old exit-leg failure must not reset a newer cargo observation on the same robot.
  core.fleet.agvs=[{...robot,virtual_cargo:true,virtual_cargo_known:true}];
  const service=createOrderService({db,inventory,getState:()=>core,getSession:()=> 's',publish:()=>{},
    send:async payload=>{commands.push(payload);}});t.after(()=>service.close());
  await service.resolve(order.id,true,inventory.snapshot().revision);
  assert.equal(commands.length,0);assert.equal(core.fleet.agvs[0].virtual_cargo,true);
  assert.equal(inventory.snapshot().slots.find(slot=>slot.node==='K1').rack_id,'R01');
});
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
  const reserved=inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'});
  assert.equal(inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'}).rack_id,reserved.rack_id);
  assert.equal(inventory.snapshot().moves.length,1,'an exact retry stays idempotent');
  assert.throws(()=>inventory.reserve({key:'a',pickup:'K3',dropoff:'K6',robot:'02'}),/đã được dùng/);
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
    assert.throws(()=>inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'}),/cần đối soát/);
    assert.throws(()=>inventory.reserve({key:'b',pickup:'K2',dropoff:'K6',robot:'02'}),/sẵn sàng/);
    inventory.observeShift(core,'s2');
    assert.equal(inventory.reports()[0].reason,'core_session_changed');
    assert.equal(inventory.reports()[0].observation_gap,true);
    assert.equal(inventory.history().filter(event=>event.type==='pickup').length,0);
  } finally {db.close();}
});
test('gateway restart finalizes an observed shift from the first CLOSED snapshot in the same Core session',()=>{
  const dir=fs.mkdtempSync(path.join(os.tmpdir(),'agv-warehouse-closed-')),filename=path.join(dir,'test.db');
  let db=createDatabase(filename),inventory=createWarehouseInventory({db});const core=coreState();
  try {
    inventory.configure(layout,core);core.shift.phase='RUNNING';inventory.observeShift(core,'same-session');
    inventory.reserve({key:'a',pickup:'K2',dropoff:'K1',robot:'01'});
    db.close();db=createDatabase(filename);inventory=createWarehouseInventory({db});
    core.shift.phase='CLOSED';inventory.observeShift(core,'same-session');
    assert.equal(inventory.reports().length,1);
    assert.equal(inventory.reports()[0].reason,'shift_closed');
    assert.equal(inventory.reports()[0].observation_gap,true);
    inventory.observeShift(core,'same-session');assert.equal(inventory.reports().length,1);
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
