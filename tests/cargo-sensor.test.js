// Server-only: private in-memory SQLite and a fake Core, no firmware/robot.
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createDatabase}=require('../AGVwebserver/db');
const {createWarehouseInventory}=require('../AGVwebserver/warehouse-inventory');
const {createOrderService}=require('../AGVwebserver/orders');
const {createWarehouseActions}=require('../AGVwebserver/warehouse-actions');
function fixture(t,id='01',scope=false){
  const db=createDatabase(':memory:'),commands=[];
  const robot={id,node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null,path:[],error:'',
    virtual_cargo_known:false,virtual_cargo:false,
    lift:{supported:true,ready:true,fresh:true,observed_node:'K1',active:false,state:'IDLE',command_id:0},
    cargo_sensor:{required:true,supported:true,fresh:true,stable:true,present:true,stopped_verified:true,observed_node:'K1',sample_seq:1}};
  const state={mqtt_connected:true,shift:{phase:'CLOSED',...(scope?{operation_profile:'scoped',active_robot_ids:[id]}:{})},
    map:{nodes:['K1','K2','G1','G2'].map(id=>({id,type:id[0]==='K'?'station':'waypoint'})),
      edges:[{from:'K1',to:'G1',weight:1,two_way:true},{from:'G1',to:'G2',weight:1,two_way:true},{from:'G2',to:'K2',weight:1,two_way:true}]},
    fleet:{agvs:[robot],tasks:[],pending_tasks:[],accepting_tasks:true}};
  const inventory=createWarehouseInventory({db});
  inventory.configure({slots:[{node:'K1',rack_id:'R1'},{node:'K2',rack_id:null}],physically_verified:true,expected_revision:0},state);
  state.shift.phase='RUNNING';inventory.observeShift(state,'cargo-test');
  const send=async payload=>{commands.push(payload);
    if(payload.cmd==='warehouse_lift'){
      robot.state=payload.action==='pickup'?'LIFTING':'LOWERING';robot.lift.active=true;
      return {command_id:commands.length};
    }
    const task={task_id:`task-${commands.length}`,status:'dispatched',target:payload.target,assigned_agv:id};
    state.fleet.tasks.push(task);robot.state='WORKING';robot.task_id=task.task_id;return {task_id:task.task_id};};
  const service=scope?createWarehouseActions({inventory,getState:()=>state,getSession:()=> 'cargo-test',send,publish:()=>{}}):
    createOrderService({db,inventory,getState:()=>state,getSession:()=> 'cargo-test',send,publish:()=>{}});
  t.after(async()=>{await service.close();db.close();});
  const create=()=>scope?service.create(id,{action:'pickup',warehouse:'K1'}):
    service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});
  const tick=async()=>{await service.tick();await service.tick();};
  function arrive(node){const task=state.fleet.tasks.at(-1);if(task)task.status='completed';
    robot.node=node;robot.state='IDLE';robot.task_id=null;robot.path=[];
    robot.lift.observed_node=node;robot.lift.active=false;Object.assign(robot.cargo_sensor,{observed_node:node,stopped_verified:true});}
  function done(command,up){robot.state='IDLE';robot.task_id=null;robot.virtual_cargo_known=true;robot.virtual_cargo=up;
    Object.assign(robot.lift,{state:up?'DONE_UP':'DONE_DOWN',active:false,command_id:command});}
  return {db,state,robot,inventory,service,commands,create,tick,arrive,done};
}
for(const id of ['01','02'])for(const scope of [false,true]){
  test(`LM393 ${id} ${scope?'action':'order'} empty pickup never actuates; ledger preserves missing identity`,async t=>{
    const f=fixture(t,id,scope);f.robot.cargo_sensor.present=false;
    const work=await f.create();await f.tick();
    assert.equal(f.commands.length,0);assert.equal(work.status,scope?'interrupted':'needs_attention');
    const s=f.inventory.snapshot();assert.equal(s.slots[0].rack_id,null);assert.equal(s.racks[0].id,'R1');
    assert.deepEqual(s.racks[0].location,{type:'unknown',id:'K1'});assert.equal(s.racks[0].status,'needs_attention');
    assert.equal(f.inventory.history().filter(e=>e.type==='missing').length,1);
    assert.equal(s.shift.initial_slots[0].rack_id,'R1');await f.tick();
    assert.equal(f.inventory.history().filter(e=>e.type==='missing').length,1);
    if(!scope){await f.service.resolve(work.id,true,s.revision);assert.equal(f.inventory.snapshot().slots[0].rack_id,null,'closing never recreates a missing rack');}
  });
}
test('order sensor and servo must agree on both pickup and dropoff before inventory or next route',async t=>{
  const f=fixture(t);const order=await f.create();await f.tick();
  const item=order.items[0];assert.equal(f.commands.at(-1).cmd,'warehouse_lift');
  f.done(item.lift_command_id,true);f.robot.cargo_sensor.present=false;
  await f.tick();assert.equal(f.inventory.snapshot().slots[0].rack_id,'R1');assert.equal(f.commands.length,1);
  f.robot.cargo_sensor.present=true;await f.tick();assert.equal(f.inventory.snapshot().slots[0].rack_id,null);
  assert.equal(f.commands.at(-1).target,'K2');f.arrive('K2');await f.tick();
  assert.equal(f.commands.at(-1).action,'dropoff');const count=f.commands.length;
  f.done(item.lift_command_id,false);await f.tick();assert.equal(f.commands.length,count);
  assert.equal(f.inventory.snapshot().slots[1].rack_id,null);
  f.robot.cargo_sensor.present=false;await f.tick();assert.equal(f.inventory.snapshot().slots[1].rack_id,'R1');
  assert.equal(f.commands.at(-1).target,'G2');
  assert.equal(f.inventory.history().filter(e=>e.type==='dropoff').length,1);
});
for(const bad of ['unsupported','stale','unstable','wrong_node','moving'])test(`LM393 ${bad} cannot infer an empty warehouse`,async t=>{
  const f=fixture(t);const sensor=f.robot.cargo_sensor;sensor.present=false;
  if(bad==='unsupported')sensor.supported=false;
  if(bad==='stale')sensor.fresh=false;
  if(bad==='unstable'){sensor.stable=false;sensor.present=null;}
  if(bad==='wrong_node')sensor.observed_node='K2';
  if(bad==='moving')sensor.stopped_verified=false;
  await f.create();await f.tick();assert.equal(f.commands.length,0);
  assert.equal(f.inventory.snapshot().slots[0].rack_id,'R1');assert.equal(f.inventory.history().some(e=>e.type==='missing'),false);
});
test('missing-stock transaction rolls back on failed audit write, and rejects a different robot or active servo',t=>{
  const f=fixture(t),key='transaction';f.inventory.reserve({key,pickup:'K1',dropoff:'K2',robot:'01'});
  f.robot.cargo_sensor.present=false;const before=f.inventory.snapshot();
  assert.throws(()=>f.inventory.confirmMissing(key,{...f.robot,id:'02'},'cargo-test'),/bằng chứng/);
  f.robot.lift.active=true;assert.throws(()=>f.inventory.confirmMissing(key,f.robot,'cargo-test'),/bằng chứng/);f.robot.lift.active=false;
  f.db.db.exec("CREATE TRIGGER missing_fail BEFORE INSERT ON warehouse_events WHEN json_extract(NEW.event_json,'$.type')='missing' BEGIN SELECT RAISE(ABORT,'disk-test'); END");
  assert.throws(()=>f.inventory.confirmMissing(key,f.robot,'cargo-test'),/disk-test/);assert.deepEqual(f.inventory.snapshot(),before);
  f.db.db.exec('DROP TRIGGER missing_fail');f.inventory.confirmMissing(key,f.robot,'cargo-test');
  const revision=f.inventory.snapshot().revision;f.inventory.confirmMissing(key,f.robot,'cargo-test');assert.equal(f.inventory.snapshot().revision,revision);
});
