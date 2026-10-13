const {randomUUID}=require('node:crypto');
const {shortestRoute}=require('./order-planner');
const SERVICE_MS=5000;
const finished=action=>['completed','interrupted'].includes(action.status);

// Temporary warehouse tests. These actions do not create, carry or reserve orders.
function createWarehouseActions({getState,getSession,send,publish}) {
  const actions=[];let running=null,closed=false;
  const changed=()=>publish();
  function interrupt(action,reason){action.status='interrupted';action.reason=reason;changed();}
  function at(state,action,node){const robot=state.fleet.agvs.find(robot=>robot.id===action.robot);
    return robot&&robot.online!==false&&robot.position_valid!==false&&!robot.error&&robot.state==='IDLE'&&!robot.task_id&&robot.node===node;}
  function complete(action){action.status='completed';action.task_id=null;action.completed_at=new Date().toISOString();changed();}
  async function dispatch(action,target,exit){
    action.status=exit?'dispatching_exit':'dispatching';action.task_id=null;changed();
    try {
      const result=await send({cmd:'direct_goto',agv_id:action.robot,target,priority:0});
      if(!result.task_id)throw Error('Core không trả task_id');
      action.task_id=result.task_id;action.dispatched_at=Date.now();action.status=exit?'to_gate':'to_warehouse';changed();
    } catch(error){interrupt(action,`Không xác định kết quả lệnh: ${error.message}`);throw error;}
  }
  async function create(robotId,body){
    const state=getState();
    if(!state||state.shift?.test_mode!=='robot01'||state.shift.phase!=='RUNNING'||state.mqtt_connected===false||state.fleet.accepting_tasks===false)
      throw Object.assign(Error('Cần Core test xe01 sẵn sàng và ca RUNNING'),{status:409});
    if(robotId!=='01')throw Object.assign(Error('Chế độ này thử riêng xe01'),{status:409});
    if(!['pickup','dropoff'].includes(body?.action))throw Object.assign(Error('Chọn lấy hàng hoặc hạ hàng'),{status:400});
    const warehouse=body.warehouse;
    if(typeof warehouse!=='string'||!/^K[1-6]$/.test(warehouse)||!state.map.nodes.some(node=>node.id===warehouse&&node.type==='station'))
      throw Object.assign(Error('Kho không hợp lệ'),{status:400});
    const gate=warehouse.replace('K','G'),robot=state.fleet.agvs.find(robot=>robot.id===robotId);
    if(!robot||robot.state!=='IDLE'||robot.error||robot.online===false||robot.position_valid===false||robot.task_id||robot.target||robot.path?.length)
      throw Object.assign(Error('Xe01 cần IDLE, có RFID hợp lệ và không đang chạy tuyến'),{status:409});
    if(actions.some(action=>!finished(action)))throw Object.assign(Error('Đang thực hiện thao tác kho hiện tại'),{status:409});
    if(state.fleet.pending_tasks?.length)throw Object.assign(Error('Core đang có task chờ; xử lý trước khi thử kho'),{status:409});
    if(!state.map.edges.some(edge=>edge.from===warehouse&&edge.to===gate||edge.two_way&&edge.to===warehouse&&edge.from===gate))
      throw Object.assign(Error('Thiếu đường lùi từ kho ra gate'),{status:400});
    const route=shortestRoute(state.map,robot.node,warehouse),exit=shortestRoute(state.map,warehouse,gate);
    if(!Number.isFinite(route.cost)||!Number.isFinite(exit.cost))throw Object.assign(Error('Không có đường tới kho hoặc ra gate'),{status:409});
    const action={id:randomUUID(),robot:robotId,action:body.action,warehouse,gate,status:'dispatching',task_id:null,
      core_session_id:getSession(),route:route.path,exit_route:exit.path,reason:'',created:new Date().toISOString()};
    actions.push(action);if(actions.length>20)actions.shift();
    if(robot.node===warehouse){action.status='waiting';action.service_until=new Date(Date.now()+SERVICE_MS).toISOString();changed();}
    else await dispatch(action,warehouse,false);
    return action;
  }
  async function advance(){
    const action=actions.find(action=>!finished(action));if(!action||action.status.startsWith('dispatching'))return;
    const state=getState();
    if(!state||getSession()!==action.core_session_id||state.shift?.phase!=='RUNNING'){interrupt(action,'Mất dữ liệu Core hoặc ca đã thay đổi; không phát lại thao tác');return;}
    const robot=state.fleet.agvs.find(robot=>robot.id===action.robot);
    if(!robot||robot.online===false||robot.position_valid===false||robot.state==='ERROR'||robot.error){interrupt(action,'Xe mất vị trí tin cậy hoặc đang lỗi');return;}
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
    if(action.status==='to_gate')complete(action);
    else{action.status='waiting';action.service_until=new Date(Date.now()+SERVICE_MS).toISOString();changed();}
  }
  function tick(){if(closed)return Promise.resolve();if(running)return running;
    running=advance().catch(error=>{const action=actions.find(action=>!finished(action));if(action)interrupt(action,`Không thể tiếp tục thao tác kho: ${error.message}`);}).finally(()=>running=null);return running;}
  const timer=setInterval(()=>{void tick();},100);timer.unref();
  return {create,tick,list:()=>actions.slice().reverse(),close:async()=>{closed=true;clearInterval(timer);if(running)await running;}};
}
module.exports={createWarehouseActions,SERVICE_MS};
