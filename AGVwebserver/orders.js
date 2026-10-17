const {randomUUID} = require('crypto');
const {planOrder,shortestCost,shortestRoute} = require('./order-planner');

const SERVICE_MS = 5000;
const activeItem = item=>!['queued','delivered','interrupted'].includes(item.status);

function createOrderService({db,getState,getSession,send,publish}) {
  const orders = db.getOrders();
  let tickPromise = null,stopped = false;
  const save = order=>{order.updated = new Date().toISOString();db.saveOrder(order);publish();};
  // A restart loses the in-memory command handshake and cargo observation.
  for (const order of orders) if (order.status === 'active' && !order.items.every(item=>item.status === 'queued' && !item.task_id)) {
    order.status = 'needs_attention';order.reason = 'Gateway khởi động lại; đối soát vị trí xe và kiện hàng trước khi tạo đơn mới';
    save(order);
  }
  function interrupt(order,item,reason) {
    if (item) {item.status = 'interrupted';item.reason = reason;}
    order.status = 'needs_attention';order.reason = reason;save(order);
  }
  function interruptAll(reason) {
    for (const order of orders) if (order.status === 'active') interrupt(order,order.items.find(activeItem),reason);
  }
  function resolve(orderId,physicallyClear) {
    const order = orders.find(candidate=>candidate.id === orderId);
    if (!order) throw Object.assign(new Error('Không tìm thấy đơn hàng'),{status:404});
    if (order.status !== 'needs_attention') throw Object.assign(new Error('Đơn chưa ở trạng thái cần đối soát'),{status:409});
    if (physicallyClear !== true) throw Object.assign(new Error('Cần xác nhận đã kiểm tra xe và kiện hàng'),{status:400});
    order.status = 'closed';order.reason = 'Đã đối soát thủ công; các kiện chưa giao không được tự chạy lại';
    order.closed_at = new Date().toISOString();save(order);return order;
  }
  function robotAt(state,item,node) {
    const robot = state.fleet.agvs.find(candidate=>candidate.id === item.robot);
    return robot && robot.online !== false && robot.position_valid !== false && robot.state === 'IDLE' && robot.node === node && !robot.task_id;
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
      item.task_seen = true;
      if (task.assigned_agv && task.assigned_agv !== item.robot || ['cancelled','interrupted','rejected'].includes(task.status)) {
        interrupt(order,item,`Task ${item.task_id} gián đoạn hoặc đổi xe`);return;
      }
      if (task.status !== 'completed') return;
      const pickup = item.status === 'to_pickup',exiting=item.status==='to_exit';
      const node = pickup ? item.pickup : exiting ? item.exit_gate : item.dropoff;
      if (!robotAt(state,item,node)) {interrupt(order,item,`Task hoàn thành nhưng xe chưa xác nhận đứng ở ${node}`);return;}
      if (exiting) {delivered(order,item);return;}
      item.status = pickup ? 'pickup_service' : 'dropoff_service';
      item.service_until = new Date(Date.now()+SERVICE_MS).toISOString();save(order);
      return;
    }
    if (item.status !== 'pickup_service' && item.status !== 'dropoff_service') return;
    const pickup = item.status === 'pickup_service',node = pickup ? item.pickup : item.dropoff;
    if (!robotAt(state,item,node)) {interrupt(order,item,`Xe rời ${node} trong lúc mô phỏng bốc/dỡ`);return;}
    if (Date.now() < Date.parse(item.service_until)) return;
    if (pickup) item.status = 'ready_dropoff';
    else {item.status='ready_exit';item.exit_gate=warehouseGate(state,item.dropoff);}
    save(order);
  }
  function eligible(state,used) {
    const trialRobot = selectedRobot(state);
    return state.fleet.agvs.filter(robot=>robot.participating !== false && robot.state === 'IDLE' && !robot.error && robot.online !== false && robot.position_valid !== false &&
      (!trialRobot || robot.id === trialRobot) && !robot.task_id && !used.has(robot.id));
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
    if (robot.state !== 'IDLE' || robot.task_id) return `Xe ${robot.id} đang thực hiện task ${robot.task_id || ''}`;
    if (used.has(robot.id)) {
      const held=orders.find(order=>order.status==='needs_attention'&&order.items.some(candidate=>candidate.robot===robot.id&&!['queued','delivered'].includes(candidate.status)));
      return held ? `Xe ${robot.id} chờ đối soát đơn ${held.id}` : `Xe ${robot.id} đang phục vụ kiện khác`;
    }
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
    item.waiting_reason=selected ? waitingReason(state,robot,used,item) : 'Chờ xe sẵn sàng gần kho lấy nhất';
    if (before!==JSON.stringify([item.robot,item.route_plan,item.waiting_reason,order.selection_policy])) save(order);
  }
  async function dispatch(order,item,robot,target,phase,state) {
    item.robot = robot.id;item.status = phase === 'pickup' ? 'dispatching' : phase === 'exit' ? 'dispatching_exit' : 'dispatching_dropoff';
    item.waiting_reason='';
    if (phase === 'pickup') item.route_plan=preview(state,item,robot);
    item.task_id = null;save(order);
    if (robot.node === target) {
      if (phase === 'exit') {delivered(order,item);return;}
      item.status = phase === 'pickup' ? 'pickup_service' : 'dropoff_service';
      item.service_until = new Date(Date.now()+SERVICE_MS).toISOString();save(order);return;
    }
    try {
      const result = await send({cmd:'direct_goto',agv_id:robot.id,target,priority:0});
      if (!result.task_id) throw new Error('Core không trả task_id');
      item.task_id = result.task_id;item.task_seen = false;
      item.dispatched_at = new Date().toISOString();
      item.status = phase === 'pickup' ? 'to_pickup' : phase === 'exit' ? 'to_exit' : 'to_dropoff';save(order);
    } catch (error) {
      interrupt(order,item,`Không xác định được chặng ${phase} của kiện ${item.number}: ${error.message}`);
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
      if (standaloneScope(state)) return;
      for (const order of orders) {
        if (order.status !== 'active') continue;
        if (order.core_session_id !== getSession()) {interrupt(order,order.items.find(activeItem),'Phiên Core đã thay đổi');continue;}
        if (state.shift?.phase !== 'RUNNING') {interrupt(order,order.items.find(activeItem),'Ca không còn RUNNING');continue;}
        for (const item of order.items) if (order.status === 'active' && activeItem(item)) processItem(order,item,state);
      }
      if (state.mqtt_connected === false || state.fleet.accepting_tasks === false || state.shift?.phase !== 'RUNNING') return;
      const used = new Set(orders.flatMap(order=>order.items
        .filter(item=>order.status==='active'&&activeItem(item) || order.status==='needs_attention'&&item.robot&&!['queued','delivered'].includes(item.status))
        .map(item=>item.robot).filter(Boolean)));
      for (const order of orders.slice().reverse()) {
        if (order.status !== 'active') continue;
        for (const item of order.items) {
          if (order.status !== 'active') break;
          if (item.status === 'ready_exit') {
            const robot=state.fleet.agvs.find(candidate=>candidate.id===item.robot);
            if (!robot || !robotAt(state,item,item.dropoff)) {interrupt(order,item,'Xe không còn ở kho giao trước khi lùi ra gate');break;}
            await dispatch(order,item,robot,item.exit_gate,'exit',state);
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
    if (standaloneScope(state)) throw Object.assign(new Error('Phạm vi một xe dùng nút Lấy hàng / Hạ hàng, chưa xử lý đơn hàng'),{status:409});
    if (!state || state.mqtt_connected === false || state.fleet.accepting_tasks === false || state.shift?.phase !== 'RUNNING')
      throw Object.assign(new Error('Cần Core/MQTT sẵn sàng và ca RUNNING để tạo đơn'),{status:409});
    let plan;
    try {
      plan = planOrder(state.map,body);
      for (const item of plan.items) {warehouseGate(state,item.pickup);warehouseGate(state,item.dropoff);}
    } catch (error) {error.status = 400;throw error;}
      const order = {id:`order-${Date.now()}-${randomUUID().slice(0,8)}`,...plan,destinationOwners:{},status:'active',reason:'',
      core_session_id:getSession(),created:new Date().toISOString()};
    for (const item of order.items) planQueued(order,item,state,new Set());
    orders.push(order);save(order);void tick();return order;
  }
  const timer = setInterval(()=>{void tick();},500);timer.unref();
  return {create,resolve,list:()=>orders.slice().sort((a,b)=>Date.parse(b.created)-Date.parse(a.created)),tick,interruptAll,close:async()=>{
    stopped = true;clearInterval(timer);if (tickPromise) await tickPromise;
  }};
}

module.exports = {createOrderService,SERVICE_MS};
