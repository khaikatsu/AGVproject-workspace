'use strict';
let warehouseSlotsRenderKey='';
let deletedOrderAttentionKey='';
const transferPhases={queued:'Chờ phân xe',dispatching:'Đang giao việc lấy kệ',to_pickup:'Đang đi lấy',
  waiting_pickup_lift:'Chờ nâng kệ',dispatching_pickup_lift:'Đang gửi lệnh nâng',pickup_lift:'Đang nâng kệ',pickup_service:'Đang lấy kệ (lịch sử)',
  ready_dropoff:'Chuẩn bị giao',dispatching_dropoff:'Đang cấp tuyến giao',to_dropoff:'Đang giao',
  waiting_dropoff_lift:'Chờ hạ kệ',dispatching_dropoff_lift:'Đang gửi lệnh hạ',dropoff_lift:'Đang hạ kệ',dropoff_service:'Đang hạ kệ (lịch sử)',
  ready_exit:'Đã hạ · chờ ra cửa kho',dispatching_exit:'Đã hạ · chuẩn bị ra cửa kho',to_exit:'Đã hạ · đang ra cửa kho',
  delivered:'Đã hoàn tất lượt',interrupted:'Lượt bị gián đoạn'};
function operationText(parent,text,tag='div',className='') {return warehouseText(parent,text,tag,className);}
function operationBadge(parent,text,tone='') {
  const badge=operationText(parent,text,'span','operation-badge');badge.dataset.tone=tone;return badge;
}
function transferRoute(parent,pickup,dropoff,current='') {
  const route=document.createElement('div');route.className='transfer-route';
  for(const [leg,label,node] of [['pickup','LẤY TỪ',pickup],['dropoff','GIAO ĐẾN',dropoff]]) {
    if(leg==='dropoff')operationText(route,'→','span','transfer-route-arrow');
    const end=document.createElement('div');end.className='transfer-route-end';end.dataset.current=String(leg===current);
    operationText(end,label,'span');operationText(end,locationText(node),'strong');route.append(end);
  }
  parent.append(route);return route;
}
function transferStage(status) {
  if(['queued','dispatching','to_pickup'].includes(status))return 0;
  if(['waiting_pickup_lift','dispatching_pickup_lift','pickup_lift','pickup_service'].includes(status))return 1;
  if(['ready_dropoff','dispatching_dropoff','to_dropoff'].includes(status))return 2;
  if(['waiting_dropoff_lift','dispatching_dropoff_lift','dropoff_lift','dropoff_service'].includes(status))return 3;
  if(['ready_exit','dispatching_exit','to_exit'].includes(status))return 4;
  return status==='delivered'?5:-1;
}
function robotTransfer(state,robot) {
  const candidates=(state?.orders||[]).flatMap(order=>(order.items||[]).filter(item=>item.robot===robot.id&&
    !['queued','delivered'].includes(item.status)&&['active','needs_attention'].includes(order.status)).map(item=>({order,item})));
  const matching=candidates.find(({item})=>robot.task_id&&item.task_id===robot.task_id);
  if(matching)return matching;
  if(robot.task_id)return null; // A different Core task must not be relabelled as an old delivery.
  return candidates.find(({order})=>order.status==='active')||candidates.find(({order,item})=>
    state.warehouse?.moves?.some(move=>move.key===`order:${order.id}:${item.number}`))||null;
}
function renderRobotTransfer(state,robot,card,stale) {
  const assignment=robotTransfer(state,robot),item=assignment?.item;
  const action=!assignment?(state.warehouse_actions||[]).find(action=>action.robot===robot.id&&!['completed','interrupted'].includes(action.status)&&
    (!robot.task_id||action.task_id===robot.task_id)):null;
  const offline=robot.online!==true||['OFFLINE','UNKNOWN'].includes(robot.state),fault=!!robot.error||['ERROR','UNLOCALIZED'].includes(robot.state);
  const attention=assignment?.order.status==='needs_attention'||fault;
  card.dataset.tone=offline||attention?'bad':'';
  const header=operationText(card,'','div','transfer-card-heading');operationText(header,`Xe ${robot.id}`,'strong');
  const status=robot.cancel_waiting_for_node?'Đã hủy task · chờ xác minh dừng':
    robot.replan_waiting_for_node?'Chờ RFID mới để đổi tuyến':
    assignment?.order.status==='needs_attention'?'Cần đối soát':
    robot.lift?.active&&robot.lift.state==='PENDING'?'Đang gửi lệnh servo':labels[robot.state]||'Chờ trạng thái';
  operationBadge(header,stale?'Dữ liệu cũ':offline?'Mất kết nối':status,
    stale?'warn':offline||attention?'bad':robot.state==='IDLE'?'good':'');
  renderVehicleTelemetry(card,robot,state);
  const stage=item?transferStage(item.status):-1;
  let phase=item ? `${transferPhases[item.status]||'Theo dõi lượt'}${stage===0&&item.status==='to_pickup'?` tại ${locationText(item.pickup)}`:
    stage===2?` đến ${locationText(item.dropoff)}`:stage===1?` tại ${locationText(item.pickup)}`:stage===3?` tại ${locationText(item.dropoff)}`:''}` :
    action?`${action.action==='pickup'?'Lấy kệ':'Hạ kệ'} tại ${locationText(action.warehouse)}`:
    robot.target?`Đi tới ${locationText(robot.target)}`:robot.state==='CHARGING'?'Đang ở trạm sạc':'Chưa có lượt lấy–giao';
  if(offline)phase='Mất liên lạc · giữ thông tin lần cuối';
  else if(attention)phase='Cần kiểm tra xe và kệ trước khi tiếp tục';
  operationText(card,phase,'p','transfer-phase');
  if(item)transferRoute(card,item.pickup,item.dropoff,stage<=1&&stage>=0?'pickup':stage<=3&&stage>=2?'dropoff':'');
  else if(action)operationText(card,`Thao tác riêng · ${action.action==='pickup'?'Lấy từ':'Hạ tại'} ${locationText(action.warehouse)}`,'p','transfer-metadata');
  const currentTask=(state.fleet.tasks||[]).find(task=>task.task_id===robot.task_id);
  const target=robot.target||currentTask?.target||(item&&assignment.order.status==='active'?(stage<=1?item.pickup:stage<=3?item.dropoff:item.exit_gate):null);
  operationText(card,`RFID gần nhất: ${locationText(robot.node)}${robot.position_valid===false?' (Chưa xác minh)':''}${target?` · Đích lệnh: ${locationText(target)}`:''}`,'p','transfer-metadata');
  const rack=state.warehouse?.racks?.find(rack=>rack.location.type==='robot'&&rack.location.id===robot.id);
  operationText(card,rack?`Trên xe: ${rack.id}${rack.status==='needs_attention'?' · cần đối soát':''}`:
    robot.virtual_cargo===true?'Có hàng ảo trên xe · chưa xác định mã kệ':item?.rack_id?`Kệ thực hiện: ${item.rack_id}`:'Chưa ghi nhận kệ trên xe','p','transfer-metadata');
  if(item) {
    const steps=document.createElement('ol');steps.className='transfer-stage-list';steps.setAttribute('aria-label','Các bước của lượt chuyển kệ');
    ['Tới lấy','Nâng','Chuyển','Hạ','Ra cửa'].forEach((label,index)=>{
      const step=operationText(steps,label,'li');step.dataset.stage=stage>index?'done':stage===index&&!attention&&!offline?'current':'waiting';
      if(stale)step.dataset.stage=stage>index?'done':'waiting';
      if(step.dataset.stage==='current')step.setAttribute('aria-current','step');
    });card.append(steps);
  }
  if(attention)operationText(card,robot.error||assignment?.order.reason||'Đối soát vị trí thực tế trước khi giao việc mới.','p','transfer-alert');
  renderVehicleActions(card,robot.id,robot);
}
function renderDeletedOrderAttention(state) {
  const orders=(state?.orders||[]).filter(order=>order.deleted_at&&order.status==='needs_attention');
  const key=JSON.stringify([orders.map(order=>[order.id,order.reason]),state?.warehouse?.revision,state?.location_names?.revision]);
  if(key===deletedOrderAttentionKey)return;deletedOrderAttentionKey=key;
  const box=$('deletedOrderAttention');box.hidden=!orders.length;box.replaceChildren();
  for(const order of orders) {
    const line=operationText(box,'','div','deleted-order-attention');
    operationText(line,`Đơn đã xóa cần đối soát: ${order.reason||order.id}`,'span');
    line.dataset.deletedOrderId=order.id;
    const resolve=action(warehouseRollbackLabel(order),()=>resolveWarehouseOrder(order.id),null);line.append(resolve);
  }
}
function renderWarehouseSlot(state,node,parent) {
  const inventory=state.warehouse,slot=inventory?.slots?.find(slot=>slot.node===node),known=inventory?.initialized&&!!slot;
  const rack=inventory?.racks?.find(rack=>rack.id===slot?.rack_id);
  const move=inventory?.moves?.find(move=>['reserved','carrying','needs_attention'].includes(move.status)&&[move.pickup,move.dropoff].includes(node));
  const attention=rack?.status==='needs_attention'||move?.status==='needs_attention';
  const event=inventory?.events?.find(event=>['pickup','dropoff'].includes(event.type)&&(event.source===node||event.target===node)&&
    (!inventory.shift||event.shift_id===inventory.shift.id));
  const card=document.createElement('article');card.className='warehouse-slot-card';card.dataset.warehouseSlot=node;
  card.dataset.tone=attention?'bad':move?'warn':known&&slot.rack_id?'good':'';
  const header=operationText(card,'','div','warehouse-slot-header');operationText(header,locationText(node),'strong');
  operationText(header,!known?'Chưa nhập':slot.rack_id?'Có kệ':'Trống','span');
  operationText(card,!known?'Chưa xác định':slot.rack_id||(attention?'Chờ xác minh vị trí':'Ô trống'),'p','warehouse-slot-rack');
  let detail='Sẵn sàng nhận kệ';
  if(!known)detail='Nhập vị trí ban đầu';
  else if(attention)detail=move?`Xe ${move.robot} · cần kiểm tra thực tế`:'Cần kiểm tra thực tế';
  else if(move)detail=move.pickup===node?(move.status==='carrying'?`Đã lấy · xe ${move.robot}`:`Xe ${move.robot} sẽ lấy ${move.rack_id}`):
    `${move.status==='carrying'?'Đang giao':'Chờ nhận'} ${move.rack_id} · xe ${move.robot}`;
  else if(event)detail=event.type==='pickup'?`Đã lấy ${event.rack_id} · xe ${event.robot}`:`Đã nhận ${event.rack_id} · xe ${event.robot}`;
  else if(slot.rack_id)detail='Sẵn sàng cho lấy kệ';
  operationText(card,detail,'p','warehouse-slot-detail');
  const initial=inventory?.shift?.initial_slots?.find(slot=>slot.node===node);
  if(initial)operationText(card,`Đầu ca: ${initial.rack_id||'Trống'}`,'p','warehouse-slot-baseline');
  parent.append(card);
}
function renderWarehouseOperations(state) {
  const stale=!dataFresh()||state?.mqtt_connected===false||state?.storage_healthy===false;
  $('warehouseOperationsBoard').dataset.stale=String(stale);
  const fresh=$('operationsFreshness');fresh.textContent=!state?'Chờ dữ liệu':stale?'Dữ liệu cũ · chờ kết nối':'Đang cập nhật';fresh.dataset.tone=stale?'warn':'good';
  const phase=state?.shift?.phase;
  $('operationsShiftLabel').textContent=({RUNNING:'Ca đang chạy',CLOSING:'Ca đang kết thúc',CLOSED:'Ca đã đóng'})[phase]||'Chờ ca';
  renderVehicleCards(state,stale);
  renderDeletedOrderAttention(state);
  renderWarehouseInventory(state);
}
function renderWarehouseInventory(state) {
  const inventory=state?.warehouse;
  const nodes=warehouseNodes(state?.map);
  const rollbackOrders=(state?.orders||[]).filter(order=>['needs_attention','closed'].includes(order.status)&&
    order.items?.some(item=>inventory?.moves?.some(move=>move.status==='needs_attention'&&move.key===`order:${order.id}:${item.number}`)));
  const key=JSON.stringify([nodes,inventory,state?.location_names?.revision,rollbackOrders.map(order=>[order.id,order.status])]);
  if(key===warehouseSlotsRenderKey)return;warehouseSlotsRenderKey=key;
  const racks=inventory?.racks||[],moves=(inventory?.moves||[]).filter(move=>['reserved','carrying','needs_attention'].includes(move.status));
  const carried=racks.filter(rack=>rack.location.type==='robot');
  const available=(inventory?.slots||[]).filter(slot=>!slot.rack_id&&!moves.some(move=>[move.pickup,move.dropoff].includes(slot.node))).length;
  const uncertain=(inventory?.slots||[]).filter(slot=>racks.some(rack=>rack.id===slot.rack_id&&rack.status==='needs_attention')||
    moves.some(move=>move.status==='needs_attention'&&[move.pickup,move.dropoff].includes(slot.node))).length;
  const missing=racks.filter(rack=>rack.location.type==='unknown').length;
  $('warehouseShiftSummary').textContent=inventory?.initialized?
    `Toàn kho: ${racks.length-missing} kệ đã xác định · ${available} ô sẵn sàng · ${carried.length} kệ trên xe${missing?` · ${missing} kệ cần tìm`:''}${uncertain?` · ${uncertain} vị trí cần đối soát`:''}`:'Chưa nhập kho · Bấm Mở ca để nhập và xác nhận vị trí kệ';
  const slots=$('warehouseSlotCards');slots.replaceChildren();
  for(const node of nodes)renderWarehouseSlot(state,node,slots);
  if(!nodes.length)operationText(slots,'Chờ danh sách kho K1–K6.','p','transfer-metadata');
  const cargo=$('warehouseCarriedRacks');cargo.replaceChildren();
  operationText(cargo,'Kệ trên xe','strong');
  if(!inventory?.initialized)operationText(cargo,'Chưa có dữ liệu kho','span','warehouse-cargo-empty');
  else if(!carried.length)operationText(cargo,'Không có kệ trên xe','span','warehouse-cargo-empty');
  for(const rack of racks.filter(rack=>rack.location.type==='unknown'))
    operationText(cargo,`Thiếu kệ ${rack.id} · không thấy tại ${locationText(rack.location.id)} · cần đối soát`,'span','transfer-alert');
  for(const rack of carried) {
    const tag=operationText(cargo,`Xe ${rack.location.id} · Kệ ${rack.id} · ${rackStateNames[rack.status]||rack.status}`,'span','warehouse-cargo-tag');
    tag.dataset.rackId=rack.id;tag.dataset.tone=rack.status==='needs_attention'?'bad':'';
  }
  for(const order of rollbackOrders) {
    const button=action(warehouseRollbackLabel(order),()=>resolveWarehouseOrder(order.id),null);
    button.type='button';button.dataset.warehouseRollbackOrder=order.id;cargo.append(button);
  }
}
function orderWarehouseAvailability(node,kind) {
  const inventory=lastState?.warehouse,slot=inventory?.slots?.find(slot=>slot.node===node);
  if(!inventory?.initialized||!slot)return {valid:false,label:'chưa nhập',reason:'Cần nhập trạng thái kệ ban đầu khi ca đã đóng.'};
  const rack=inventory.racks.find(rack=>rack.id===slot.rack_id),move=inventory.moves?.find(move=>[move.pickup,move.dropoff].includes(node));
  if(move||rack?.status==='needs_attention')return {valid:false,label:move?.status==='needs_attention'||rack?.status==='needs_attention'?'cần đối soát':'đã đặt chỗ',reason:`${locationText(node)} đang được đặt chỗ hoặc cần đối soát.`};
  if(kind==='pickup')return {valid:!!slot.rack_id,label:slot.rack_id||'trống',reason:`${locationText(node)} đang trống, không có kệ để lấy.`};
  return {valid:!slot.rack_id,label:slot.rack_id?`có ${slot.rack_id}`:'trống',reason:`${locationText(node)} đang có kệ, cần chọn ô giao trống.`};
}
function refreshOrderWarehouseOptions() {
  for(const kind of ['pickup','dropoff'])for(const select of $(kind==='pickup'?'orderPickups':'orderDeliveries').querySelectorAll('select')) {
    for(const option of select.options) {
      const info=orderWarehouseAvailability(option.value,kind);
      const text=`${locationText(option.value)} · ${info.label}`;
      if(option.text!==text)option.text=text;
      option.disabled=!info.valid;
    }
    // Choose an initial valid default once, never silently replace a user's selection after a state change.
    if(select.dataset.initialized!=='true'&&[...select.options].some(option=>!option.disabled)) {
      if(!select.selectedOptions[0]||select.selectedOptions[0].disabled)select.value=[...select.options].find(option=>!option.disabled).value;
      select.dataset.initialized='true';
    }
  }
}
function validateTransferForm() {
  const pickups=orderLines('pickup'),deliveries=orderLines('dropoff');
  if(!lastState?.warehouse?.initialized)return 'Nhập trạng thái kệ ban đầu trước khi tạo lượt chuyển.';
  if(!pickups.length||!deliveries.length)return 'Chọn ít nhất một kho lấy và một kho giao.';
  const used=new Set();
  for(const [kind,lines] of [['pickup',pickups],['dropoff',deliveries]])for(const line of lines) {
    const node=line[kind];if(used.has(node))return `${node} bị lặp: mỗi vị trí chỉ tham gia một lượt trong đơn.`;
    used.add(node);const available=orderWarehouseAvailability(node,kind);if(!available.valid)return available.reason;
  }
  if(pickups.length!==deliveries.length)return 'Số kho lấy và kho giao cần bằng nhau (mỗi vị trí một kệ).';
  if(!dataFresh())return 'Đang chờ dữ liệu mới từ Core.';
  if(lastState?.mqtt_connected===false)return 'MQTT đang mất kết nối; chờ kết nối lại.';
  if(lastState?.shift?.phase!=='RUNNING')return 'Bắt đầu ca để giao lượt chuyển kệ.';
  if(!tasksReady())return 'Core chưa sẵn sàng nhận việc; xem trạng thái kết nối và ca.';
  return '';
}
function renderTransferForm() {
  refreshOrderWarehouseOptions();
  const pickups=orderLines('pickup'),deliveries=orderLines('dropoff'),preview=$('orderTransferPreview');
  const key=JSON.stringify([pickups,deliveries,lastState?.location_names?.revision]);
  if(preview.dataset.routes!==key) {
    preview.dataset.routes=key;preview.replaceChildren();
    if(pickups.length===1&&deliveries.length===1)transferRoute(preview,pickups[0].pickup,deliveries[0].dropoff);
    else operationText(preview,`${pickups.map(line=>locationText(line.pickup)).join(', ')||'Chưa chọn nguồn'} → ${deliveries.map(line=>locationText(line.dropoff)).join(', ')||'Chưa chọn đích'}. Server sẽ ghép các tuyến sau khi tạo đơn.`,'p');
  }
  const reason=validateTransferForm();$('orderFormHint').textContent=reason;
  $('submitOrderBtn').disabled=!!reason||$('submitOrderBtn').dataset.busy==='true';
  $('submitOrderBtn').textContent=pickups.length>1?`Tạo đơn chuyển ${pickups.length} kệ`:'Giao chuyển 1 kệ';
}
