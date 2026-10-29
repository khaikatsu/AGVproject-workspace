const {randomUUID} = require('crypto');
const {planOrder,shortestCost,shortestRoute} = require('./order-planner');
const {cargoReading,cargoResult,loweredForExit,dropoffDoneAtGate}=require('./cargo-proof');

const SERVICE_MS = 5000;
const LIFT_READY_TIMEOUT_MS = 5000;
const LIFT_RESULT_TIMEOUT_MS = 15000;
const activeItem = item=>!['queued','delivered','interrupted'].includes(item.status);

function createOrderService({db,getState,getSession,send,publish,inventory,getLocationNames,cargoScanTimeoutMs = 0}) {
  const orders = db.getOrders();
  let tickPromise = null,stopped = false,endingShift=false;
  const resolving=new Set();
  const save = order=>{order.updated = new Date().toISOString();db.saveOrder(order);publish();};
  const moveKey=(order,item)=>`order:${order.id}:${item.number}`;
  // A later explicit full warehouse reconciliation supersedes an ended order's
  // cargo hold. Keep the stored audit record; never infer this from deletion.
  const reconciliationFor=order=>inventory?.reconciliationFor?.(order)||null;
  const needsAttention=order=>order.status==='needs_attention'&&!reconciliationFor(order);
  const holdsRobot=(order,item)=>order.status==='active'&&activeItem(item)||
    needsAttention(order)&&item.robot&&!['queued','delivered'].includes(item.status);
  function orderView(order) {
    const proof=reconciliationFor(order);
    return proof?{...order,status:'closed',resolution:'warehouse_reconciled',closed_at:proof.created_at,
      warehouse_reconciliation:proof,reason:'Kho và xe đã được xác nhận lại sau khi kết ca; giữ lịch sử đơn cũ'}:order;
  }
  // A restart loses the in-memory command handshake and cargo observation.
  for (const order of orders) if (order.status === 'active') {
    order.status = 'needs_attention';order.reason = 'Gateway khởi động lại; đối soát vị trí xe và kiện hàng trước khi tạo đơn mới';
    save(order);
  }
  function interrupt(order,item,reason) {
    if(inventory)for(const candidate of order.items) {
      const candidateReason = (!item || candidate === item)
        ? reason
        : `Tạm dừng do lượt Xe ${item.robot || 'khác'} (${item.pickup}→${item.dropoff}) gặp sự cố: ${reason}`;
      inventory.interrupt(moveKey(order,candidate),candidateReason);
    }
    for (const candidate of order.items.filter(activeItem)) {
      if(candidate.status!=='interrupted') {
        candidate.suspended_status=candidate.status;
        const state=getState(),task=state?.fleet.tasks?.find(task=>task.task_id===candidate.task_id);
        candidate.resume_node=task?.resume_node||state?.fleet.agvs.find(robot=>robot.id===candidate.robot)?.node||null;
      }
      candidate.status='interrupted';
      if (!item || candidate === item) {
        candidate.reason = reason;
      } else {
        const otherRef = item.robot ? `Xe ${item.robot}` : 'Lượt khác';
        const locRef = item.pickup ? ` tại ${item.pickup}` : '';
        candidate.reason = `Tạm dừng do ${otherRef}${locRef} gặp sự cố: ${reason}`;
      }
    }
    order.status = 'needs_attention';order.reason = reason;save(order);
  }
  function interruptAll(reason) {
    for (const order of orders) if (order.status === 'active') interrupt(order,order.items.find(activeItem),reason);
  }
  function endShift() {
    for(const order of orders) if(['active','needs_attention'].includes(order.status)&&!order.closed_by_shift) {
      const hasPhysicalMove=order.items.some(item=>inventory?.snapshot().moves.some(move=>
        move.key===moveKey(order,item)&&['reserved','carrying','needs_attention'].includes(move.status)));
      interrupt(order,null,'Ca đã kết thúc; nhiệm vụ cũ không được chạy trong ca tiếp theo');
      order.closed_by_shift=true;
      order.shift_closed_at=new Date().toISOString();
      if(!hasPhysicalMove) {order.status='closed';order.resolution='shift_cancelled';order.closed_at=new Date().toISOString();}
      save(order);
    }
  }
  async function resume(orderId,itemNumber,physicallyClear,expectedRevision) {
    if(physicallyClear!==true)throw Object.assign(Error('Xác nhận xe ở RFID cũ và kệ đúng vị trí trước khi tiếp tục'),{status:400});
    const order=orders.find(order=>order.id===orderId),state=getState();
    if(!order)throw Object.assign(Error('Không tìm thấy đơn hàng'),{status:404});
    if(endingShift||!state||state.shift?.phase!=='RUNNING'||order.closed_by_shift||order.core_session_id!==getSession())
      throw Object.assign(Error('Chỉ tiếp tục nhiệm vụ trong đúng ca và phiên Core cũ'),{status:409});
    if(resolving.has(orderId))throw Object.assign(Error('Đang xử lý đơn này'),{status:409});
    const item=order.items.find(item=>item.number===itemNumber);
    const travel=['to_pickup','to_dropoff','to_exit'];
    if(!item||item.status!=='interrupted'||!travel.includes(item.suspended_status))
      throw Object.assign(Error('Chặng này cần đối soát; không tự phát lại thao tác nâng/hạ'),{status:409});
    const robot=state.fleet.agvs.find(robot=>robot.id===item.robot);
    if(!robot||robot.node!==item.resume_node||robot.state!=='IDLE'||robot.error||robot.online!==true||
       robot.position_valid!==true||robot.task_id||!robot.recovery_node_verified)
      throw Object.assign(Error('Phục hồi xe và xác minh dừng tại RFID cũ trước khi tiếp tục'),{status:409});
    if(robot.heading_confirmation_required||robot.heading_confirmation_pending||robot.recovery_reset_pending)
      throw Object.assign(Error('Hoàn tất khôi phục và xác nhận hướng trước khi tiếp tục'),{status:409});
    resolving.add(orderId);
    try {
      inventory?.resumeMove(moveKey(order,item),robot,expectedRevision);
      await send({cmd:'resume_task',task_id:item.task_id,expected_node:item.resume_node});
      // A concurrent close wins. The Core cancels the resumed task on end_shift.
      if(endingShift||order.closed_by_shift||getState()?.shift?.phase!=='RUNNING')return order;
      item.status=item.suspended_status;item.reason='';item.dispatched_at=new Date().toISOString();
      order.status='active';order.reason='';save(order);return order;
    } catch(error) {
      inventory?.interrupt(moveKey(order,item),`Tiếp tục chưa được xác nhận: ${error.message}`);
      item.reason=order.reason=`Tiếp tục chưa được xác nhận: ${error.message}`;save(order);
      // If the command result was lost, cancel the exact task rather than
      // allowing an unobserved resumed route to continue in the background.
      if(error.unknown)try{await send({cmd:'cancel_task',task_id:item.task_id});}catch{}
      throw error;
    } finally {resolving.delete(orderId);}
  }
  async function resolve(orderId,physicallyClear,expectedRevision) {
    const order = orders.find(candidate=>candidate.id === orderId);
    if (!order) throw Object.assign(new Error('Không tìm thấy đơn hàng'),{status:404});
    if(reconciliationFor(order))return orderView(order);
    if(order.status==='closed'&&order.resolution==='rollback_to_pickup')return order;
    const pendingKeys=new Set((inventory?.snapshot().moves||[]).filter(move=>move.status==='needs_attention').map(move=>move.key));
    const legacyClosed=order.status==='closed'&&order.items.some(item=>pendingKeys.has(moveKey(order,item)));
    if (order.status !== 'needs_attention'&&!legacyClosed) throw Object.assign(new Error('Đơn chưa ở trạng thái cần đối soát'),{status:409});
    if (physicallyClear !== true) throw Object.assign(new Error('Cần xác nhận kệ đã về kho lấy và xe đã trống'),{status:400});
    if(resolving.has(orderId))throw Object.assign(new Error('Đang hoàn tác đơn này'),{status:409});
    resolving.add(orderId);
    try {
      if(inventory?.snapshot().initialized&&expectedRevision!==inventory.snapshot().revision)
        throw Object.assign(new Error('Trạng thái kho đã thay đổi; tải lại trước khi hoàn tác'),{status:409});
      const rollback=inventory?.rollbackPlan(order);
      const affected=inventory?.snapshot().initialized ? rollback :
        order.items.filter(item=>item.robot&&!['queued','delivered'].includes(item.status));
      const robots=[...new Set(affected.map(item=>item.robot).filter(Boolean))];
      if(orders.some(other=>other.id!==order.id&&(other.status==='active'||needsAttention(other))&&
          other.items.some(item=>robots.includes(item.robot)&&!['queued','delivered'].includes(item.status))))
        throw Object.assign(new Error('Xe đang gắn với lượt chuyển khác; xử lý lượt đó trước khi hoàn tác'),{status:409});
      if(robots.length) {
        const state=getState();
        if(!state||state.mqtt_connected===false)throw Object.assign(new Error('Chờ kết nối Core và xe trước khi hoàn tác'),{status:503});
        const checks=robots.map(id=>{
          const robot=state.fleet.agvs.find(robot=>robot.id===id);
          if(!robot||robot.online!==true||robot.position_valid!==true)
            throw Object.assign(new Error(`Kết nối lại xe ${id} và xác nhận vị trí RFID trước khi hoàn tác`),{status:409});
          if(!['IDLE','ERROR','CHARGING'].includes(robot.state)||
              robot.task_id||robot.target||robot.path?.length||robot.lift?.active||
              state.fleet.tasks?.some(task=>task.assigned_agv===id&&['pending','dispatched'].includes(task.status)))
            throw Object.assign(new Error(`Dừng xe ${id} và xử lý nhiệm vụ đang chạy trước khi hoàn tác`),{status:409});
          return {id,lift_command_id:robot.lift?.command_id||0};
        });
        await send({cmd:'reconcile_test_cargo',robots:checks,physically_clear:true});
      }
      const updated={...order,status:'closed',resolution:'rollback_to_pickup',closed_at:new Date().toISOString(),
        updated:new Date().toISOString(),reason:'Đã hoàn tác lượt dang dở về kho lấy; xác nhận xe trống'};
      if(inventory)inventory.rollbackOrder(order,expectedRevision,moves=>{updated.rolled_back=moves;db.saveOrder(updated);});
      else db.saveOrder(updated);
      Object.assign(order,updated);publish();return order;
    } finally {resolving.delete(orderId);}
  }
  function remove(orderId) {
    const order=orders.find(candidate=>candidate.id===orderId);
    if(!order)throw Object.assign(new Error('Không tìm thấy đơn hàng'),{status:404});
    if(!order.deleted_at) {
      const updated={...order,deleted_at:new Date().toISOString(),updated:new Date().toISOString(),
        ...(order.closed_by_shift&&!order.shift_closed_at?{shift_closed_at:order.updated}:{})};
      // Commit the display deletion before mutating the scheduler's live object.
      db.saveOrder(updated);Object.assign(order,updated);publish();
    }
    return {id:order.id,deleted_at:order.deleted_at};
  }
  function linkTaskHistory(rows) {
    const links=new Map(),durableLinks=new Map();
    const add=(map,key,value)=>{
      const previous=map.get(key);
      if(map.has(key)&&(previous===null||previous.order_id!==value.order_id||previous.order_item!==value.order_item))map.set(key,null);
      else map.set(key,value);
    };
    for(const order of orders)for(const item of order.items) {
      const known=[...(item.task_links||[])];
      if(item.task_id&&!known.some(link=>link.task_id===item.task_id&&link.core_session_id===order.core_session_id))
        known.push({task_id:item.task_id,core_session_id:order.core_session_id,phase:null});
      for(const link of known)if(link.task_id&&link.core_session_id) {
        const key=JSON.stringify([link.core_session_id,link.task_id]);
        const value={order_id:order.id,order_item:item.number,order_phase:link.phase,order_deleted_at:order.deleted_at||null};
        // Ambiguous legacy data remains ungrouped; never assign by robot or target.
        add(links,key,value);
        if(Number.isSafeInteger(link.history_id))add(durableLinks,JSON.stringify([link.history_id,link.task_id]),value);
      }
    }
    return rows.map(row=>{
      const key=JSON.stringify([row.id,row.core_task_id]);
      return {...row,...(durableLinks.has(key)?durableLinks.get(key):links.get(JSON.stringify([row.core_session,row.core_task_id])))};
    });
  }
  function robotAt(state,item,node) {
    const robot = state.fleet.agvs.find(candidate=>candidate.id === item.robot);
    return robot && robot.online !== false && robot.position_valid !== false && robot.state === 'IDLE' && robot.node === node && !robot.task_id;
  }
  function liftReady(robot,node) {
    return !robot?.heading_confirmation_required&&!robot?.heading_confirmation_pending&&!robot?.recovery_reset_pending&&
      robot?.node===node && robot.lift?.supported===true && robot.lift.ready===true &&
      robot.lift.fresh===true && robot.lift.observed_node===node;
  }
  function beginLiftWait(order,item,pickup) {
    item.task_id=null;item.lift_command_id=null;item.lift_sent_at=null;
    item.lift_wait_started_at=Date.now();delete item.service_until;
    item.status=pickup?'waiting_pickup_lift':'waiting_dropoff_lift';save(order);
  }
  function delivered(order,item) {
    item.status='delivered';item.completed_at=new Date().toISOString();item.task_id=null;
    if (order.items.every(candidate=>candidate.status==='delivered')) {
      order.status='completed';order.completed_at=new Date().toISOString();
    }
    save(order);
  }
  function warehouseGate(state,node) {
    const gate=node.replace(/^K/,'G');
    if (gate===node || !state.map.nodes.some(candidate=>candidate.id===gate) ||
        !state.map.edges.some(edge=>edge.from===node&&edge.to===gate || edge.two_way&&edge.to===node&&edge.from===gate))
      throw new Error(`Thiếu đường lùi từ ${node} ra gate ${gate}`);
    return gate;
  }
  function processItem(order,item,state) {
    if (['dispatching','dispatching_dropoff','dispatching_exit'].includes(item.status)) return;
    const robot = state.fleet.agvs.find(candidate=>candidate.id === item.robot);
    if (!robot || robot.online === false || robot.position_valid === false || ['ERROR','OFFLINE','UNLOCALIZED'].includes(robot.state)) {
      interrupt(order,item,`Xe ${item.robot} mất trạng thái tin cậy trong chuyến hàng`);return;
    }
    if (['to_pickup','to_dropoff','to_exit'].includes(item.status)) {
      const task = state.fleet.tasks.find(candidate=>candidate.task_id === item.task_id);
      if (!task) {
        if (Date.now() - Date.parse(item.dispatched_at) > 10000)
          interrupt(order,item,`Không còn thấy task ${item.task_id} trong Core`);
        return;
      }
      const link=item.task_links?.find(candidate=>candidate.task_id===task.task_id&&candidate.core_session_id===order.core_session_id);
      if(link&&(link.status!==task.status||link.reason!==(task.reason||''))) {
        link.status=task.status;link.reason=task.reason||'';
        if(['completed','cancelled','interrupted','rejected'].includes(task.status))link.completed_at=new Date().toISOString();
        save(order);
      }
      item.task_seen = true;
      if (task.assigned_agv && task.assigned_agv !== item.robot || ['cancelled','interrupted','rejected'].includes(task.status)) {
        interrupt(order,item,`Task ${item.task_id} gián đoạn hoặc đổi xe`);return;
      }
      if (task.status !== 'completed') return;
      const pickup = item.status === 'to_pickup',exiting=item.status==='to_exit';
      const node = pickup ? item.pickup : exiting ? item.exit_gate : item.dropoff;
      if (!robotAt(state,item,node)) {interrupt(order,item,`Task hoàn thành nhưng xe chưa xác nhận đứng ở ${node}`);return;}
      if (exiting) {
        if(item.dropoff_exit_pending) {
          if(!dropoffDoneAtGate(robot,item.dropoff,item.lift_command_id)) {
            item.exit_arrived_at??=Date.now();
            if(Date.now()-item.exit_arrived_at>5000)interrupt(order,item,'Đã tới gate nhưng chưa xác nhận kệ rời xe sau hạ');
            return;
          }
          inventory?.confirmLift(moveKey(order,item),'dropoff',robot,item.lift_command_id,order.core_session_id);
          item.dropoff_exit_pending=false;
        }
        delivered(order,item);return;
      }
      beginLiftWait(order,item,pickup);
      return;
    }
    if (item.status==='waiting_pickup_lift' || item.status==='waiting_dropoff_lift') {
      const pickup=item.status==='waiting_pickup_lift',node=pickup?item.pickup:item.dropoff;
      if (!robotAt(state,item,node)) {interrupt(order,item,`Xe rời ${node} trước khi servo sẵn sàng`);return;}
      if (Date.now()-item.lift_wait_started_at>LIFT_READY_TIMEOUT_MS)
        interrupt(order,item,`Servo xe ${item.robot} chưa sẵn sàng tại ${node}`);
      return;
    }
    if (item.status!=='pickup_lift' && item.status!=='dropoff_lift') return;
    const pickup=item.status==='pickup_lift',node=pickup?item.pickup:item.dropoff,expected=pickup;
    if (robot.node!==node) {interrupt(order,item,`Xe rời ${node} khi đang nâng/hạ kệ`);return;}
    const lift=robot.lift;
    if (lift?.command_id===item.lift_command_id && lift.state==='INTERRUPTED') {
      interrupt(order,item,`Servo xe ${item.robot} bị gián đoạn`);return;
    }
    if(!pickup&&loweredForExit(robot,node,item.lift_command_id)&&robotAt(state,item,node)) {
      item.lift_completed_at=new Date().toISOString();item.dropoff_exit_pending=true;
      item.status='ready_exit';item.exit_gate=warehouseGate(state,item.dropoff);save(order);return;
    }
    if (lift?.fresh===true && lift.command_id===item.lift_command_id &&
        lift.state===(pickup?'DONE_UP':'DONE_DOWN') && lift.active===false &&
        (pickup||robot.lift.dropoff_exit_supported!==true&&robot.cargo_sensor?.required!==true) &&
        robot.virtual_cargo_known===true && robot.virtual_cargo===expected && cargoResult(robot,node,expected) && robotAt(state,item,node)) {
      inventory?.confirmLift(moveKey(order,item),pickup?'pickup':'dropoff',robot,item.lift_command_id,order.core_session_id);
      item.lift_completed_at=new Date().toISOString();
      if (pickup) item.status='ready_dropoff';
      else {item.status='ready_exit';item.exit_gate=warehouseGate(state,item.dropoff);}
      save(order);return;
    }
    if (Date.now()-item.lift_sent_at>LIFT_RESULT_TIMEOUT_MS)
      interrupt(order,item,`Không nhận hoàn tất servo đúng lệnh tại ${node}; không cho xe chạy tiếp`);
  }
  function eligible(state,used) {
    const trialRobot = selectedRobot(state);
    return state.fleet.agvs.filter(robot=>robot.participating !== false && robot.state === 'IDLE' && !robot.error && robot.online !== false && robot.position_valid !== false &&
      !robot.heading_confirmation_required&&!robot.heading_confirmation_pending&&!robot.recovery_reset_pending&&
      (!trialRobot || robot.id === trialRobot) && !robot.task_id && !used.has(robot.id) && robot.lift?.supported===true &&
      (!inventory || inventory.robotAvailable(robot.id)) &&
      !(robot.virtual_cargo_known===true && robot.virtual_cargo===true));
  }
  function selectedRobot(state) {
    if (state.shift?.test_mode === 'robot01') return '01';
    if (state.shift?.test_mode === 'robot02') return '02';
    return state.shift?.trial_robot_id || state.fleet.trial_robot_id || null;
  }
  function standaloneScope(state) {
    return ['robot01','robot02'].includes(state?.shift?.test_mode) || state?.shift?.operation_profile === 'scoped' &&
      Array.isArray(state.shift.active_robot_ids) && state.shift.active_robot_ids.length === 1;
  }
  function preview(state,item,robot) {
    const route=(from,to)=>{const value=shortestRoute(state.map,from,to);return {path:value.path,cost:Number.isFinite(value.cost)?value.cost:null};};
    return {robot_start:robot?.position_valid !== false ? robot?.node || null : null,
      pickup:route(robot?.position_valid !== false ? robot?.node : null,item.pickup),delivery:route(item.pickup,item.dropoff),
      exit:route(item.dropoff,warehouseGate(state,item.dropoff))};
  }
  function waitingReason(state,robot,used,item) {
    if (!robot) return `Chưa thấy xe ${selectedRobot(state)} kết nối Core`;
    if (robot.online === false) return `Xe ${robot.id} đang mất kết nối`;
    if (robot.error || robot.state === 'ERROR') return `Xe ${robot.id} đang lỗi ${robot.error || 'ERROR'}; cần xử lý trước khi lấy hàng`;
    if (robot.position_valid === false) return `Xe ${robot.id} chưa xác nhận vị trí RFID`;
    if(robot.heading_confirmation_required||robot.heading_confirmation_pending||robot.recovery_reset_pending)
      return `Xe ${robot.id} đang khôi phục hoặc cần xác nhận hướng đầu xe`;
    if (robot.lift?.supported!==true) return `Xe ${robot.id} chưa báo capability nâng/hạ servo`;
    if (robot.virtual_cargo_known===true && robot.virtual_cargo===true) return `Xe ${robot.id} đang được ghi nhận có kệ trên xe`;
    if (robot.state !== 'IDLE' || robot.task_id) return `Xe ${robot.id} đang thực hiện task ${robot.task_id || ''}`;
    if (used.has(robot.id)) {
      const held=orders.find(order=>needsAttention(order)&&order.items.some(candidate=>candidate.robot===robot.id&&!['queued','delivered'].includes(candidate.status)));
      return held ? `Xe ${robot.id} chờ đối soát đơn ${held.id}` : `Xe ${robot.id} đang phục vụ kiện khác`;
    }
    if(inventory&&!inventory.robotAvailable(robot.id))return `Xe ${robot.id} còn kệ theo sổ kho; cần hạ hoặc đối soát`;
    if (!item.route_plan?.pickup.path.length || !item.route_plan.delivery.path.length) return 'Không có đường có hướng tới kho lấy hoặc kho giao';
    if (state.fleet.agvs.some(other=>other.id!==robot.id && [item.pickup,item.dropoff].includes(other.node))) return 'Kho lấy hoặc kho giao đang có xe khác';
    return 'Đang chờ Core cấp tuyến';
  }
  function planQueued(order,item,state,used) {
    const selected=selectedRobot(state);
    const policy=selected ? `fixed:${selected}` : 'nearest';
    const before=JSON.stringify([item.robot,item.route_plan,item.waiting_reason,order.selection_policy]);
    order.selection_policy=policy;
    if (selected) item.robot=selected;
    const robot=state.fleet.agvs.find(candidate=>candidate.id===item.robot);
    item.route_plan=preview(state,item,robot);
    const participating=state.fleet.agvs.filter(candidate=>candidate.participating!==false);
    item.waiting_reason=selected ? waitingReason(state,robot,used,item) :
      participating.length && participating.every(candidate=>candidate.lift?.supported!==true) ?
        'Chưa có xe tham gia báo capability nâng/hạ servo' : !participating.length?'Chưa có xe tham gia kết nối Core':
        [...new Set(participating.map(candidate=>waitingReason(state,candidate,used,
          {...item,route_plan:preview(state,item,candidate)})))].join(' · ');
    if (before!==JSON.stringify([item.robot,item.route_plan,item.waiting_reason,order.selection_policy])) save(order);
  }
  async function dispatch(order,item,robot,target,phase,state) {
    if(robot.heading_confirmation_required||robot.heading_confirmation_pending||robot.recovery_reset_pending) {
      item.waiting_reason=`Xe ${robot.id} đang khôi phục hoặc cần xác nhận hướng đầu xe`;save(order);return;
    }
    if(phase==='pickup'&&inventory) {
      try {
        const move=inventory.reserve({key:moveKey(order,item),pickup:item.pickup,dropoff:item.dropoff,robot:robot.id});
        item.rack_id=move.rack_id;
      } catch(error) {item.waiting_reason=error.message;save(order);return;}
    }
    item.robot = robot.id;item.status = phase === 'pickup' ? 'dispatching' : phase === 'exit' ? 'dispatching_exit' : 'dispatching_dropoff';
    item.waiting_reason='';
    if (phase === 'pickup') item.route_plan=preview(state,item,robot);
    item.task_id = null;save(order);
    if (robot.node === target) {
      if (phase === 'exit') {delivered(order,item);return;}
      beginLiftWait(order,item,phase==='pickup');return;
    }
    try {
      const result = await send({cmd:'direct_goto',agv_id:robot.id,target,priority:0});
      if(endingShift||order.closed_by_shift)return;
      if (!result.task_id) throw new Error('Core không trả task_id');
      item.task_id = result.task_id;item.task_seen = false;
      item.dispatched_at = new Date().toISOString();
      if(!Array.isArray(item.task_links))item.task_links=[];
      if(!item.task_links.some(link=>link.task_id===result.task_id&&link.core_session_id===order.core_session_id))
        item.task_links.push({task_id:result.task_id,core_session_id:order.core_session_id,phase,
          history_id:db.getTaskReference?.(result.task_id,order.core_session_id)?.id||null,
          from:robot.node,target,robot:robot.id,status:'dispatched',reason:'',created_at:item.dispatched_at});
      item.status = phase === 'pickup' ? 'to_pickup' : phase === 'exit' ? 'to_exit' : 'to_dropoff';save(order);
    } catch (error) {
      interrupt(order,item,`Không xác định được chặng ${phase} của kiện ${item.number}: ${error.message}`);
    }
  }
  async function dispatchLift(order,item,robot,pickup) {
    const node=pickup?item.pickup:item.dropoff;
    item.status=pickup?'dispatching_pickup_lift':'dispatching_dropoff_lift';save(order);
    try {
      const result=await send({cmd:'warehouse_lift',agv_id:robot.id,node,action:pickup?'pickup':'dropoff'});
      if(endingShift||order.closed_by_shift)return;
      if(!Number.isSafeInteger(result.command_id)||result.command_id<=0)throw new Error('Core không trả command_id servo');
      item.lift_command_id=result.command_id;item.lift_sent_at=Date.now();
      item.status=pickup?'pickup_lift':'dropoff_lift';save(order);
    } catch(error) {
      if ((error.message.includes('Cho ') || error.message.includes('chua san sang')) &&
          (Date.now() - (item.lift_wait_started_at || 0) < LIFT_READY_TIMEOUT_MS)) {
        item.status = pickup ? 'waiting_pickup_lift' : 'waiting_dropoff_lift';
        save(order);
        return;
      }
      interrupt(order,item,`Không xác định được thao tác ${pickup?'nâng':'hạ'} kệ: ${error.message}`);
    }
  }
  function tick() {
    if (stopped) return Promise.resolve();
    if (tickPromise) return tickPromise;
    tickPromise = runTick().catch(error=>{
      interruptAll(`Lỗi điều phối đơn hàng: ${error.message}`);
      console.error('[AGV-ORDERS]',error);
    }).finally(()=>{tickPromise = null;});
    return tickPromise;
  }
  async function runTick() {
      const state = getState();
      if (!state) return;
      // Xe01 is currently a warehouse-action test, not an order/cargo system.
      if (endingShift) return;
      if(state.shift?.phase!=='RUNNING') {endShift();return;}
      if (standaloneScope(state)) return;
      for (const order of orders) {
        if (order.status !== 'active') continue;
        if (order.core_session_id !== getSession()) {interrupt(order,order.items.find(activeItem),'Phiên Core đã thay đổi');continue;}
        if (state.shift?.phase !== 'RUNNING') {interrupt(order,order.items.find(activeItem),'Ca không còn RUNNING');continue;}
        for (const item of order.items) if (order.status === 'active' && activeItem(item)) processItem(order,item,state);
      }
      if (state.mqtt_connected === false || state.fleet.accepting_tasks === false || state.shift?.phase !== 'RUNNING') return;
      const used = new Set(orders.flatMap(order=>order.items
        .filter(item=>holdsRobot(order,item))
        .map(item=>item.robot).filter(Boolean)));
      for (const order of orders.slice().reverse()) {
        if (order.status !== 'active') continue;
        for (const item of order.items) {
          if (order.status !== 'active') break;
          if (item.status === 'ready_exit') {
            const robot=state.fleet.agvs.find(candidate=>candidate.id===item.robot);
            if (!robot || !robotAt(state,item,item.dropoff)) {interrupt(order,item,'Xe không còn ở kho giao trước khi lùi ra gate');break;}
            await dispatch(order,item,robot,item.exit_gate,'exit',state);
          } else if (item.status==='waiting_pickup_lift' || item.status==='waiting_dropoff_lift') {
            const pickup=item.status==='waiting_pickup_lift',node=pickup?item.pickup:item.dropoff;
            const robot=state.fleet.agvs.find(candidate=>candidate.id===item.robot);
            if (!robot || !robotAt(state,item,node)) {interrupt(order,item,`Xe không còn đứng tại ${node} trước khi nâng/hạ`);break;}
            const cargo=cargoReading(robot,node);
            if(cargo.required&&!cargo.known)continue;
            if(cargo.required&&!cargo.present) {
              if(pickup && (Date.now() - (item.lift_wait_started_at || 0) < cargoScanTimeoutMs)) continue;
              if(pickup)inventory?.confirmMissing(moveKey(order,item),robot,order.core_session_id);
              interrupt(order,item,pickup?`${node} thực tế không có hàng; không nâng kệ`:`Xe ${robot.id} không còn hàng trước khi hạ`);break;
            }
            if (pickup && robot.virtual_cargo_known===true && robot.virtual_cargo===true) {
              interrupt(order,item,`Xe ${robot.id} đã có kệ trước lệnh nâng`);break;
            }
            if (!pickup && (robot.virtual_cargo_known!==true || robot.virtual_cargo!==true)) {
              interrupt(order,item,`Không xác nhận được kệ đang ở trên xe ${robot.id} trước lệnh hạ`);break;
            }
            if (liftReady(robot,node)) await dispatchLift(order,item,robot,pickup);
          } else if (item.status === 'ready_dropoff') {
            const robot = state.fleet.agvs.find(candidate=>candidate.id === item.robot);
            if (!robot || !robotAt(state,item,item.pickup)) {interrupt(order,item,'Xe không còn ở kho lấy trước khi giao');break;}
            await dispatch(order,item,robot,item.dropoff,'dropoff',state);
          } else if (item.status === 'queued') {
            planQueued(order,item,state,used);
            const owner = order.destinationOwners[item.dropoff];
            const robots = eligible(state,used).filter(robot=>(!owner || robot.id === owner) &&
              state.fleet.agvs.every(other=>other.id === robot.id ||
                other.node !== item.dropoff && other.node !== item.pickup));
            const ranked = robots.map(robot=>({robot,cost:shortestCost(state.map,robot.node,item.pickup)+shortestCost(state.map,item.pickup,item.dropoff)}))
              .filter(choice=>Number.isFinite(choice.cost))
              .sort((a,b)=>a.cost-b.cost || a.robot.id.localeCompare(b.robot.id));
            if (!ranked.length) continue;
            const robot = ranked[0].robot;used.add(robot.id);
            order.destinationOwners[item.dropoff] = robot.id;
            await dispatch(order,item,robot,item.pickup,'pickup',state);
          }
        }
      }
  }
  function create(body) {
    const state = getState();
    if (standaloneScope(state)) throw Object.assign(new Error('Phạm vi một xe dùng nút Nâng kệ / Hạ kệ, chưa xử lý đơn hàng'),{status:409});
    if (endingShift || !state || state.mqtt_connected === false || state.fleet.accepting_tasks === false || state.shift?.phase !== 'RUNNING')
      throw Object.assign(new Error('Cần Core/MQTT sẵn sàng và ca RUNNING để tạo đơn'),{status:409});
    let plan;
    try {
      for(const lines of [body?.pickups,body?.deliveries]) if(Array.isArray(lines)&&lines.some(line=>line?.quantity!==1))
        throw Error('Mỗi vị trí K chứa một kệ; số lượng mỗi vị trí phải bằng 1');
      plan = planOrder(state.map,body);
      for (const item of plan.items) {warehouseGate(state,item.pickup);warehouseGate(state,item.dropoff);}
    } catch (error) {error.status = 400;throw error;}
    inventory?.assertPlan(plan);
      const order = {id:`order-${Date.now()}-${randomUUID().slice(0,8)}`,...plan,destinationOwners:{},status:'active',reason:'',
      core_session_id:getSession(),created:new Date().toISOString(),...(getLocationNames ? {location_names:getLocationNames()} : {})};
    for (const item of order.items) planQueued(order,item,state,new Set());
    orders.push(order);save(order);void tick();return order;
  }
  const timer = setInterval(()=>{void tick();},500);timer.unref();
  return {create,resolve,resume,remove,linkTaskHistory,endShift,
    pauseForShiftEnd:async()=>{endingShift=true;if(tickPromise)await tickPromise;},unpause:()=>{endingShift=false;},
    list:({includeDeleted=false}={})=>orders.filter(order=>includeDeleted||!order.deleted_at)
    .sort((a,b)=>Date.parse(b.created)-Date.parse(a.created)).map(orderView),tick,interruptAll,close:async()=>{
    stopped = true;clearInterval(timer);if (tickPromise) await tickPromise;
  }};
}

module.exports = {createOrderService,SERVICE_MS,LIFT_READY_TIMEOUT_MS,LIFT_RESULT_TIMEOUT_MS};
