const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createWarehouseActions,SERVICE_MS}=require('../AGVwebserver/warehouse-actions');
function fixture(){
  const robot={id:'01',node:'S1',state:'IDLE',online:true,position_valid:true,error:'',path:[],task_id:null};
  const state={mqtt_connected:true,shift:{phase:'RUNNING',test_mode:'robot01'},map:{
    nodes:['S1','G1','G2','K1','K2'].map(id=>({id,type:id.startsWith('K')?'station':'waypoint'})),
    edges:[{from:'S1',to:'G1',weight:10},{from:'G1',to:'K1',weight:20,two_way:true},{from:'G1',to:'G2',weight:50},{from:'G2',to:'K2',weight:20,two_way:true}],obstacles:[]},
    fleet:{agvs:[robot],tasks:[],pending_tasks:[],accepting_tasks:true}};
  let session='session-1';const commands=[];
  const service=createWarehouseActions({getState:()=>state,getSession:()=>session,publish:()=>{},send:async payload=>{
    commands.push(payload);const task={task_id:'task-'+commands.length,status:'dispatched',target:payload.target,assigned_agv:payload.agv_id};
    state.fleet.tasks.push(task);robot.state='WORKING';robot.task_id=task.task_id;return {task_id:task.task_id};}});
  return {state,robot,service,commands,session:value=>session=value,arrive(node){state.fleet.tasks.at(-1).status='completed';robot.node=node;robot.state='IDLE';robot.task_id=null;}};
}
test('pickup then dropoff are standalone: RFID, five seconds, reverse gate, next command',async()=>{
  const f=fixture();try {
    assert.equal(SERVICE_MS,5000);
    for(const [action,warehouse,gate] of [['pickup','K1','G1'],['dropoff','K2','G2']]){
      const started=await f.service.create('01',{action,warehouse});assert.equal(f.commands.at(-1).target,warehouse);
      await f.service.tick();assert.equal(started.status,'to_warehouse');
      f.arrive(warehouse);await f.service.tick();assert.equal(started.status,'waiting');
      assert.ok(Date.parse(started.service_until)-Date.now()>4800);const count=f.commands.length;
      await f.service.tick();assert.equal(f.commands.length,count);
      await assert.rejects(f.service.create('01',{action,warehouse}),/thao tác kho hiện tại/);
      started.service_until=new Date(Date.now()-1).toISOString();await f.service.tick();
      assert.equal(f.commands.at(-1).target,gate);assert.equal(started.status,'to_gate');
      f.arrive(gate);await f.service.tick();assert.equal(started.status,'completed');
      assert.equal(f.robot.node,gate);assert.equal('order_id' in started,false);
    }
  } finally {await f.service.close();}
});
test('warehouse tests reject moving/faulted robots and never continue after session loss',async()=>{
  const f=fixture();try {
    f.robot.error='LINE_LOST';await assert.rejects(f.service.create('01',{action:'pickup',warehouse:'K1'}),/IDLE/);assert.equal(f.commands.length,0);
    f.robot.error='';const started=await f.service.create('01',{action:'pickup',warehouse:'K1'});f.arrive('K1');await f.service.tick();
    f.session('session-2');started.service_until=new Date(0).toISOString();await f.service.tick();
    assert.equal(started.status,'interrupted');assert.equal(f.commands.length,1,'session loss does not issue gate exit');
  } finally {await f.service.close();}
});
test('completed Core task at wrong RFID does not start service timer',async()=>{
  const f=fixture();try {const started=await f.service.create('01',{action:'dropoff',warehouse:'K2'});f.arrive('G2');await f.service.tick();
    assert.equal(started.status,'interrupted');assert.equal(started.service_until,undefined);assert.equal(f.commands.length,1);
  } finally {await f.service.close();}
});
