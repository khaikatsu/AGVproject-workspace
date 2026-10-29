const {randomUUID}=require('node:crypto');
const {shortestRoute}=require('./order-planner');
const {isWarehouseNode}=require('./public/warehouse-rules');
const {cargoReading,cargoResult,loweredForExit,dropoffDoneAtGate}=require('./cargo-proof');
const SERVICE_MS=5000;
const finished=action=>['completed','interrupted'].includes(action.status);

// Temporary warehouse tests; the servo cargo marker is not an order or a load sensor.
function createWarehouseActions({getState,getSession,send,publish,inventory,cargoScanTimeoutMs = 0}) {
  const actions=[],resuming=new Set();let running=null,closed=false,endingShift=false;
  const scopedRobot=state=>state?.shift?.trial_robot_id ||
    (state?.shift?.operation_profile==='scoped'&&state.shift.active_robot_ids?.length===1 ? state.shift.active_robot_ids[0] :
      state?.shift?.test_mode==='robot01'?'01':state?.shift?.test_mode==='robot02'?'02':null);
  const changed=()=>publish();
  function interrupt(action,reason){
    if(action.status!=='interrupted') {
      action.suspended_status=action.status;
      const state=getState(),task=state?.fleet.tasks?.find(task=>task.task_id===action.task_id);
      action.resume_node=task?.resume_node||state?.fleet.agvs.find(robot=>robot.id===action.robot)?.node||null;
    }
    if(action.inventory_tracked)inventory.interrupt(`action:${action.id}`,reason);
    action.status='interrupted';action.reason=reason;changed();
  }
  function endShift(){for(const action of actions)if(!finished(action)||action.status==='interrupted') {
    if(!finished(action))interrupt(action,'Kết thúc ca; thao tác kho cũ không được chạy lại');
    action.closed_by_shift=true;
  }}
  async function resume(actionId,physicallyClear,expectedRevision){
    const action=actions.find(action=>action.id===actionId),state=getState();
    if(resuming.has(actionId))throw Object.assign(Error('Đang tiếp tục thao tác kho này'),{status:409});
    if(physicallyClear!==true)throw Object.assign(Error('Cần xác nhận xe và kệ tại RFID cũ'),{status:400});
    if(endingShift||!action||action.closed_by_shift||action.status!=='interrupted'||
      !['to_warehouse','to_gate'].includes(action.suspended_status)||state?.shift?.phase!=='RUNNING'||
      action.core_session_id!==getSession()||actions.some(other=>other!==action&&!finished(other)))
      throw Object.assign(Error('Chặng kho không thể tiếp tục trong ca này'),{status:409});
    const robot=state.fleet.agvs.find(robot=>robot.id===action.robot);
    if(!robot||robot.node!==action.resume_node||robot.state!=='IDLE'||!robot.recovery_node_verified)
      throw Object.assign(Error('Phục hồi xe tại RFID cũ trước khi tiếp tục'),{status:409});
    if(robot.heading_confirmation_required||robot.heading_confirmation_pending||robot.recovery_reset_pending)
      throw Object.assign(Error('Hoàn tất khôi phục và xác nhận hướng trước khi tiếp tục'),{status:409});
    resuming.add(actionId);
    let inventoryResumed=false;
    try {
      if(action.inventory_tracked){inventory.resumeMove(`action:${action.id}`,robot,expectedRevision);inventoryResumed=true;}
      await send({cmd:'resume_task',task_id:action.task_id,expected_node:action.resume_node});
      if(endingShift||action.closed_by_shift||getState()?.shift?.phase!=='RUNNING')return action;
      action.status=action.suspended_status;action.reason='';action.dispatched_at=Date.now();changed();return action;
    } catch(error){
      if(inventoryResumed)inventory.interrupt(`action:${action.id}`,`Tiếp tục chưa được xác nhận: ${error.message}`);
      if(error.unknown)try{await send({cmd:'cancel_task',task_id:action.task_id});}catch{}
      throw error;
    } finally {resuming.delete(actionId);}
  }
  function at(state,action,node){const robot=state.fleet.agvs.find(robot=>robot.id===action.robot);
    return robot&&robot.online!==false&&robot.position_valid!==false&&!robot.error&&robot.state==='IDLE'&&!robot.task_id&&robot.node===node;}
  function complete(action){action.status='completed';action.task_id=null;action.completed_at=new Date().toISOString();changed();}
  function servoReady(action){const robot=getState()?.fleet.agvs.find(robot=>robot.id===action.robot);
    return robot?.node===action.warehouse&&robot.lift?.ready===true&&robot.lift.fresh===true&&robot.lift.observed_node===action.warehouse&&
      cargoResult(robot,action.warehouse,true);}
  async function startServo(action){
    action.service_until=new Date(Date.now()+SERVICE_MS).toISOString();
    action.status='dispatching_lift';changed();
    try{
      const result=await send({cmd:'warehouse_lift',agv_id:action.robot,node:action.warehouse,action:action.action});
      if(endingShift||action.closed_by_shift)return;
      if(!Number.isSafeInteger(result.command_id)||result.command_id<=0)throw Error('Core không trả command_id servo');
      action.lift_command_id=result.command_id;action.lift_sent_at=Date.now();action.status='handling';changed();
    }catch(error){interrupt(action,`Chưa xác định kết quả servo: ${error.message}`);throw error;}
  }
  async function beginService(action){
    action.task_id=null;action.service_until=new Date(Date.now()+SERVICE_MS).toISOString();
    if(!action.servo){action.status='waiting';changed();return;}
    action.status='waiting_servo';action.lift_wait_started_at=Date.now();changed();
    // Inspect an empty pickup before sending any servo command.
    await inspectAndStart(action);
  }
  async function inspectAndStart(action){
    const robot=getState()?.fleet.agvs.find(robot=>robot.id===action.robot),cargo=cargoReading(robot,action.warehouse);
    if(cargo.required&&cargo.known&&!cargo.present){
      if(action.action==='pickup' && (Date.now() - (action.lift_wait_started_at || 0) < cargoScanTimeoutMs)) return;
      if(action.action==='pickup'&&action.inventory_tracked)inventory.confirmMissing(`action:${action.id}`,robot,action.core_session_id);
      interrupt(action,action.action==='pickup'?`${action.warehouse} thực tế không có hàng; không nâng kệ`:'Xe không còn hàng trước khi hạ');return;
    }
    if(servoReady(action))await startServo(action);
  }
  async function dispatch(action,target,exit){
    action.status=exit?'dispatching_exit':'dispatching';action.task_id=null;changed();
    try {
      const result=await send({cmd:'direct_goto',agv_id:action.robot,target,priority:0});
      if(endingShift||action.closed_by_shift)return;
      if(!result.task_id)throw Error('Core không trả task_id');
      action.task_id=result.task_id;action.dispatched_at=Date.now();action.status=exit?'to_gate':'to_warehouse';changed();
    } catch(error){interrupt(action,`Không xác định kết quả lệnh: ${error.message}`);throw error;}
  }
  async function create(robotId,body){
    const state=getState();
    if(endingShift||!state||!['01','02'].includes(scopedRobot(state))||state.shift.phase!=='RUNNING'||state.mqtt_connected===false||state.fleet.accepting_tasks===false)
      throw Object.assign(Error('Cần Core phạm vi một xe sẵn sàng và ca RUNNING'),{status:409});
    if(robotId!==scopedRobot(state))throw Object.assign(Error('Xe ngoài phạm vi vận hành'),{status:409});
    if(!['pickup','dropoff'].includes(body?.action))throw Object.assign(Error('Chọn lấy hàng hoặc hạ hàng'),{status:400});
    const warehouse=body.warehouse;
    if(typeof warehouse!=='string'||!state.map.nodes.some(node=>node.id===warehouse&&isWarehouseNode(node)))
      throw Object.assign(Error('Kho không hợp lệ'),{status:400});
    const gate=warehouse.replace('K','G'),robot=state.fleet.agvs.find(robot=>robot.id===robotId);
    if(robot?.heading_confirmation_required||robot?.heading_confirmation_pending||robot?.recovery_reset_pending)
      throw Object.assign(Error('Hoàn tất khôi phục và xác nhận hướng trước khi thao tác kho'),{status:409});
    if(!robot||robot.state!=='IDLE'||robot.error||robot.online===false||robot.position_valid===false||robot.task_id||robot.target||robot.path?.length)
      throw Object.assign(Error(`Xe ${robotId} cần IDLE, có RFID hợp lệ và không đang chạy tuyến`),{status:409});
    if(actions.some(action=>!finished(action)))throw Object.assign(Error('Đang thực hiện thao tác kho hiện tại'),{status:409});
    if(state.fleet.pending_tasks?.length)throw Object.assign(Error('Core đang có task chờ; xử lý trước khi thử kho'),{status:409});
    if(!state.map.edges.some(edge=>edge.from===warehouse&&edge.to===gate||edge.two_way&&edge.to===warehouse&&edge.from===gate))
      throw Object.assign(Error('Thiếu đường lùi từ kho ra gate'),{status:400});
    const route=shortestRoute(state.map,robot.node,warehouse),exit=shortestRoute(state.map,warehouse,gate);
    if(!Number.isFinite(route.cost)||!Number.isFinite(exit.cost))throw Object.assign(Error('Không có đường tới kho hoặc ra gate'),{status:409});
    const action={id:randomUUID(),robot:robotId,action:body.action,warehouse,gate,status:'dispatching',task_id:null,
      core_session_id:getSession(),servo:robot.lift?.supported===true,route:route.path,exit_route:exit.path,reason:'',created:new Date().toISOString()};
    if(inventory?.snapshot().initialized) {
      if(!action.servo)throw Object.assign(Error('Kho đã quản lý kệ: cần capability servo để cập nhật vị trí'),{status:409});
      const move=inventory.reserve({key:`action:${action.id}`,robot:robotId,standalone:true,
        pickup:body.action==='pickup'?warehouse:null,dropoff:body.action==='dropoff'?warehouse:null});
      action.inventory_tracked=true;action.rack_id=move.rack_id;
    }
    actions.push(action);if(actions.length>20)actions.shift();
    if(robot.node===warehouse)await beginService(action);
    else await dispatch(action,warehouse,false);
    return action;
  }
  async function advance(){
    if(endingShift)return;
    const action=actions.find(action=>!finished(action));if(!action||action.status.startsWith('dispatching'))return;
    const state=getState();
    if(!state||getSession()!==action.core_session_id||state.shift?.phase!=='RUNNING'){interrupt(action,'Mất dữ liệu Core hoặc ca đã thay đổi; không phát lại thao tác');return;}
    const robot=state.fleet.agvs.find(robot=>robot.id===action.robot);
    if(!robot||robot.online===false||robot.position_valid===false||robot.state==='ERROR'||robot.error){interrupt(action,'Xe mất vị trí tin cậy hoặc đang lỗi');return;}
    if(robot.heading_confirmation_required||robot.heading_confirmation_pending||robot.recovery_reset_pending)return;
    if(action.status==='waiting_servo'){
      if(!at(state,action,action.warehouse)){interrupt(action,'Xe không còn đứng chờ tại kho');return;}
      await inspectAndStart(action);
      if(action.status==='waiting_servo'&&Date.now()-action.lift_wait_started_at>5000)interrupt(action,'Chưa có diagnostic servo/cảm biến hàng sẵn sàng tại kho');
      return;
    }
    if(action.status==='handling'){
      if(robot.node!==action.warehouse){interrupt(action,'Xe rời kho khi đang nâng/hạ');return;}
      const lift=robot.lift,up=action.action==='pickup';
      if(lift?.command_id===action.lift_command_id&&lift.state==='INTERRUPTED'){interrupt(action,'Servo bị gián đoạn');return;}
      if(!up&&loweredForExit(robot,action.warehouse,action.lift_command_id)&&at(state,action,action.warehouse)) {
        action.dropoff_exit_pending=true;await dispatch(action,action.gate,true);return;
      }
      if(lift?.fresh===true&&lift.command_id===action.lift_command_id&&lift.state===(up?'DONE_UP':'DONE_DOWN')&&
        (up||lift.dropoff_exit_supported!==true&&robot.cargo_sensor?.required!==true)&&
        lift.active===false&&robot.virtual_cargo_known===true&&robot.virtual_cargo===up&&cargoResult(robot,action.warehouse,up)&&at(state,action,action.warehouse)){
        if(action.inventory_tracked)inventory.confirmLift(`action:${action.id}`,action.action,robot,action.lift_command_id,action.core_session_id);
        await dispatch(action,action.gate,true);return;
      }
      if(Date.now()-action.lift_sent_at>15000)interrupt(action,'Không nhận hoàn tất servo đúng lệnh; không tự lùi');
      return;
    }
    if(action.status==='waiting'){
      if(!at(state,action,action.warehouse)){interrupt(action,'Xe rời kho trong lúc chờ5s');return;}
      if(Date.now()>=Date.parse(action.service_until))await dispatch(action,action.gate,true);
      return;
    }
    const task=state.fleet.tasks?.find(task=>task.task_id===action.task_id);
    if(!task){if(Date.now()-action.dispatched_at>10000)interrupt(action,'Không còn thấy task trong Core');return;}
    if(task.assigned_agv&&task.assigned_agv!==action.robot||['interrupted','cancelled','rejected'].includes(task.status)){interrupt(action,'Task bị gián đoạn hoặc đổi xe');return;}
    if(task.status!=='completed')return;
    const node=action.status==='to_gate'?action.gate:action.warehouse;
    if(!at(state,action,node)){interrupt(action,`Task hoàn thành nhưng chưa xác nhận RFID tại ${node}`);return;}
    if(action.status==='to_gate') {
      if(action.dropoff_exit_pending) {
        if(!dropoffDoneAtGate(robot,action.warehouse,action.lift_command_id)) {
          action.exit_arrived_at??=Date.now();
          if(Date.now()-action.exit_arrived_at>5000)interrupt(action,'Đã tới gate nhưng chưa xác nhận kệ rời xe sau hạ');
          return;
        }
        if(action.inventory_tracked)inventory.confirmLift(`action:${action.id}`,'dropoff',robot,action.lift_command_id,action.core_session_id);
        action.dropoff_exit_pending=false;
      }
      complete(action);
    }
    else await beginService(action);
  }
  function tick(){if(closed)return Promise.resolve();if(running)return running;
    running=advance().catch(error=>{const action=actions.find(action=>!finished(action));if(action)interrupt(action,`Không thể tiếp tục thao tác kho: ${error.message}`);}).finally(()=>running=null);return running;}
  const timer=setInterval(()=>{void tick();},100);timer.unref();
  return {create,resume,tick,endShift,pauseForShiftEnd:async()=>{endingShift=true;if(running)await running;},
    unpause:()=>{endingShift=false;},list:()=>actions.slice().reverse(),close:async()=>{closed=true;clearInterval(timer);if(running)await running;}};
}
module.exports={createWarehouseActions,SERVICE_MS};
