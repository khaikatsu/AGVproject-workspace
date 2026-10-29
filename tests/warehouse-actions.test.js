const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createWarehouseActions,SERVICE_MS}=require('../AGVwebserver/warehouse-actions');
function fixture(servo=false,scoped=false,onSend){
  const robot={id:'01',node:'S1',state:'IDLE',online:true,position_valid:true,error:'',path:[],task_id:null};
  if(servo)robot.lift={supported:true,fresh:true,ready:true,observed_node:'S1',active:false,state:'IDLE',command_id:0};
  const state={mqtt_connected:true,shift:scoped?{phase:'RUNNING',test_mode:'normal',operation_profile:'scoped',active_robot_ids:['01']}:{phase:'RUNNING',test_mode:'robot01'},map:{
    nodes:['S1','G1','G2','K1','K2'].map(id=>({id,type:id.startsWith('K')?'station':'waypoint'})),
    edges:[{from:'S1',to:'G1',weight:10},{from:'G1',to:'K1',weight:20,two_way:true},{from:'G1',to:'G2',weight:50},{from:'G2',to:'K2',weight:20,two_way:true}],obstacles:[]},
    fleet:{agvs:[robot],tasks:[],pending_tasks:[],accepting_tasks:true}};
  let session='session-1';const commands=[];
  const service=createWarehouseActions({getState:()=>state,getSession:()=>session,publish:()=>{},send:async payload=>{
    if(onSend)await onSend(payload);
    commands.push(payload);
    if(payload.cmd==='warehouse_lift'){
      robot.lift={...robot.lift,active:true,state:'PENDING',command_id:commands.length};robot.state=payload.action==='pickup'?'LIFTING':'LOWERING';
      return {command_id:commands.length};
    }
    const task={task_id:'task-'+commands.length,status:'dispatched',target:payload.target,assigned_agv:payload.agv_id};
    state.fleet.tasks.push(task);robot.state='WORKING';robot.task_id=task.task_id;return {task_id:task.task_id};}});
  return {state,robot,service,commands,session:value=>session=value,arrive(node){state.fleet.tasks.at(-1).status='completed';robot.node=node;robot.state='IDLE';robot.task_id=null;if(robot.lift){robot.lift.observed_node=node;robot.lift.ready=true;}}};
}
test('unified Core scope 01 keeps the standalone warehouse workflow',async()=>{
  const f=fixture(false,true);try{
    const action=await f.service.create('01',{action:'pickup',warehouse:'K1'});
    assert.equal(action.status,'to_warehouse');assert.equal(f.commands.at(-1).agv_id,'01');
  }finally{await f.service.close();}
});

test('heading lock rejects warehouse create and concurrent resumes send only one command',async()=>{
  let release;const paused=new Promise(resolve=>release=resolve);
  const f=fixture(false,false,command=>command.cmd==='resume_task'?paused:undefined);
  try {
    f.robot.heading_confirmation_required=true;
    await assert.rejects(f.service.create('01',{action:'pickup',warehouse:'K1'}),/xác nhận hướng/);
    assert.equal(f.commands.length,0);f.robot.heading_confirmation_required=false;
    const action=await f.service.create('01',{action:'pickup',warehouse:'K1'});
    f.state.fleet.tasks[0].status='interrupted';f.robot.state='ERROR';await f.service.tick();
    Object.assign(f.robot,{state:'IDLE',error:'',task_id:null,recovery_node_verified:true});
    f.robot.heading_confirmation_pending=true;
    await assert.rejects(f.service.resume(action.id,true),/xác nhận hướng/);
    f.robot.heading_confirmation_pending=false;
    const first=f.service.resume(action.id,true);
    await assert.rejects(f.service.resume(action.id,true),/Đang tiếp tục/);
    release();await first;
    assert.equal(f.commands.filter(command=>command.cmd==='resume_task').length,1);
  } finally {release();await f.service.close();}
});
test('scope 02 runs its servo cycle and cannot dispatch robot 01',async()=>{
  const f=fixture(true,true);try{
    f.robot.id='02';f.state.shift.active_robot_ids=['02'];
    await assert.rejects(f.service.create('01',{action:'pickup',warehouse:'K1'}),/ngoài phạm vi/);
    const action=await f.service.create('02',{action:'pickup',warehouse:'K1'});
    f.arrive('K1');await f.service.tick();
    assert.equal(f.commands.at(-1).cmd,'warehouse_lift');assert.equal(f.commands.at(-1).agv_id,'02');
    Object.assign(f.robot.lift,{active:false,state:'DONE_UP',command_id:action.lift_command_id});
    Object.assign(f.robot,{state:'IDLE',virtual_cargo:true,virtual_cargo_known:true});
    await f.service.tick();assert.equal(f.commands.at(-1).agv_id,'02');assert.equal(f.commands.at(-1).target,'G1');
  }finally{await f.service.close();}
});
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

test('servo flow waits for matching MCU completion, not gateway timer or ACK',async()=>{
  const f=fixture(true);try{
    for(const [kind,warehouse,gate]of [['pickup','K1','G1'],['dropoff','K2','G2']]){
      const action=await f.service.create('01',{action:kind,warehouse});f.arrive(warehouse);await f.service.tick();
      assert.equal(action.status,'handling');assert.equal(f.commands.at(-1).cmd,'warehouse_lift');
      const command=action.lift_command_id,count=f.commands.length;
      action.service_until=new Date(0).toISOString();await f.service.tick();assert.equal(f.commands.length,count,'elapsed gateway timer cannot exit');
      f.robot.lift.active=false;f.robot.lift.state=kind==='pickup'?'DONE_UP':'DONE_DOWN';f.robot.state='IDLE';
      f.robot.virtual_cargo_known=true;f.robot.virtual_cargo=kind==='pickup';
      f.robot.lift.command_id=command-1;await f.service.tick();assert.equal(f.commands.length,count);
      f.robot.lift.command_id=command;f.robot.lift.fresh=false;await f.service.tick();assert.equal(f.commands.length,count);
      f.robot.lift.fresh=true;await f.service.tick();assert.equal(action.status,'to_gate');assert.equal(f.commands.at(-1).target,gate);
      f.arrive(gate);await f.service.tick();assert.equal(action.status,'completed');
    }
  }finally{await f.service.close();}
});
test('servo interruption and session change never dispatch the exit leg',async()=>{
  for(const failure of ['interrupted','session','moved']){
    const f=fixture(true);try{
      const action=await f.service.create('01',{action:'pickup',warehouse:'K1'});f.arrive('K1');await f.service.tick();const count=f.commands.length;
      if(failure==='interrupted')f.robot.lift.state='INTERRUPTED';
      if(failure==='session')f.session('new-session');
      if(failure==='moved')f.robot.node='G1';
      await f.service.tick();assert.equal(action.status,'interrupted');assert.equal(f.commands.length,count);
    }finally{await f.service.close();}
  }
});
test('RFID completion waits for a stopped servo diagnostic at the new warehouse',async()=>{
  const f=fixture(true);try{
    const action=await f.service.create('01',{action:'pickup',warehouse:'K1'});f.arrive('K1');
    f.robot.lift.ready=false;f.robot.lift.observed_node='S1';await f.service.tick();
    assert.equal(action.status,'waiting_servo');assert.equal(f.commands.length,1);
    f.robot.lift.ready=true;await f.service.tick();assert.equal(f.commands.length,1,'old-node diagnostic cannot authorize servo');
    f.robot.lift.observed_node='K1';await f.service.tick();assert.equal(action.status,'handling');assert.equal(f.commands.length,2);
  }finally{await f.service.close();}
});
