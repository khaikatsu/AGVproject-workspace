'use strict';
let ws = null, lastState = null, backendOnline = false, reconnectTimer = null, generation = 0;
let API_BASE = /^https?:$/.test(location.protocol) ? location.origin : 'http://localhost:3000';
let selectedAgvId = null, previewMapId = null, currentKpiId = null, kpiHistoryData = [];
let logSession = null;
let orderWarehouses = [],orderWarehouseKey = '';
let orderProgressOptionsKey = '';
const pendingControls = new Set();
const seenLogs = new Set(), recentLogs = [];
const views = new Set(['overview','map','reports']);
function snapshotAge() {
  const at = Date.parse(lastState?.state_received_at || '');
  return Number.isFinite(at) ? Math.max(0,(Date.now()-at)/1000) : Infinity;
}
function dataFresh() {return backendOnline && !!lastState?.core_ready && snapshotAge()*1000 <= (lastState.stale_after_ms || 5000);}
function controlsReady() {return dataFresh() && !!lastState?.protocol_supported && lastState.mqtt_connected !== false && lastState.storage_healthy !== false;}
function tasksReady() {return controlsReady() && lastState?.fleet.accepting_tasks !== false;}
function canEditMap() {return controlsReady() && !lastState.fleet.agvs.length && !lastState.fleet.pending_tasks.length;}
function setTaskMode(mode) {
  for (const [name,form,button] of [['order','orderTaskForm','orderModeBtn'],['node','nodeTaskForm','nodeModeBtn'],['warehouse','warehouseTaskForm','warehouseModeBtn']]) {
    $(form).hidden=mode!==name;$(button).setAttribute('aria-pressed',String(mode===name));
  }
}
function showView(view) {
  if (!views.has(view)) view = 'overview';
  document.body.dataset.view = view;
  document.querySelectorAll('[data-view-target]').forEach(button=>{
    if (button.dataset.viewTarget === view) button.setAttribute('aria-current','page');
    else button.removeAttribute('aria-current');
  });
  if (view === 'map' && backendOnline) loadMapList();
  if (view === 'reports' && backendOnline) {loadLatestKpi();loadKpiHistory();loadWarehouseReports();}
  $('left-panel').scrollTop = 0;$('right-panel').scrollTop = 0;
  closeMapPreview();
  scheduleMapDocking();
}
function toggleMapExpanded() {
  const expanded = document.body.dataset.mapExpanded !== 'true';
  document.body.dataset.mapExpanded = String(expanded);
  $('mapExpandBtn').textContent = expanded ? 'Thu gọn' : 'Mở rộng';
  $('mapExpandBtn').setAttribute('aria-pressed',String(expanded));
  scheduleMapDocking();
}
function toggleMapZoom() {
  const viewport = $('mapViewport'),zoomed = viewport.dataset.zoom !== 'zoomed';
  viewport.dataset.zoom = zoomed ? 'zoomed' : 'fit';
  $('mapZoomBtn').textContent = zoomed ? 'Vừa màn hình' : 'Phóng to';
  $('mapZoomBtn').setAttribute('aria-pressed',String(zoomed));
  if (!zoomed) {viewport.scrollLeft = 0;viewport.scrollTop = 0;}
  scheduleMapDocking();
}
function renderSummary(state) {
  const agvs = state?.fleet?.agvs || [],pending = state?.fleet?.pending_tasks || [];
  const expected = Object.keys(state?.shift?.home_stations || {}).length || 2;
  const located = agvs.filter(a=>a.online !== false && a.position_valid !== false).length;
  const active = (state?.fleet?.tasks || []).filter(t=>t.status === 'dispatched').length;
  const zones = Object.values(state?.mutex || {}),occupied = zones.filter(z=>z?.owner).length;
  const waiting = zones.reduce((sum,z)=>sum+(z?.waiting?.length || 0),0);
  const phases = {RUNNING:'Đang làm việc',CLOSING:'Đang kết thúc ca',CLOSED:'Đã tập kết'};
  $('summaryFleet').textContent = `${located} / ${expected}`;
  $('summaryFleetDetail').textContent = dataFresh() ? 'Xe có vị trí hợp lệ' : 'Chờ dữ liệu mới';
  $('summaryTasks').textContent = String(pending.length + active);
  $('summaryTasksDetail').textContent = `${active} đang chạy · ${pending.length} chờ`;
  $('summaryZones').textContent = `${occupied} / ${zones.length}`;
  $('summaryZonesDetail').textContent = waiting ? `${waiting} xe đang chờ quyền` : 'Không có xe chờ quyền';
  $('summaryShift').textContent = phases[state?.shift?.phase] || 'Đang chờ';
  $('summaryShiftDetail').textContent = state?.shift?.supported ? 'Theo xác nhận của core' : 'Chờ core xác nhận';
}
function populateNodeOptions(map) {
  const list = $('mapNodeOptions');list.replaceChildren();
  (map?.nodes || []).forEach(node=>{const option = document.createElement('option');option.value = node.id;list.append(option);});
  const tasks=$('taskNodeOptions');tasks.replaceChildren();
  (map?.nodes || []).forEach(node=>{const option=document.createElement('option');option.value=locationName(node.id);option.label=locationText(node.id);tasks.append(option);});
}
function renderObstacleAction() {
  const node = $('obstacle_node').value.trim(),known = lastState?.map?.nodes?.some(n=>n.id === node);
  const blocked = known && (lastState.map.obstacles || []).includes(node);
  $('obstacleNodeState').textContent = !node ? 'Chọn node để xem trạng thái.' : !known ? 'Node chưa có trong bản đồ đang chạy.' : blocked ? `${node} đang bị khóa.` : `${node} đang mở.`;
  const button = $('blockNodeBtn');button.textContent = blocked ? 'Mở node' : 'Khóa node';
  button.className = blocked ? 'btn btn-outline full-width' : 'btn btn-danger full-width';
  button.disabled = !controlsReady() || !known || button.dataset.busy === 'true';
}
function updateConnection() {
  $('status').textContent = backendOnline ? 'Web: Đã kết nối' : 'Web: Mất kết nối';
  $('status').className = 'status-indicator ' + (backendOnline ? 'ok' : 'bad');
  let text = 'Core: Mất kết nối';
  if (backendOnline && lastState?.agv_connected) {
    text = !lastState.protocol_supported ? 'Core: Cần cập nhật phiên bản' : dataFresh() ? 'Core: Có dữ liệu mới' : 'Core: Đang chờ / dữ liệu cũ';
  }
  $('agvLinkStatus').textContent = text;
  $('agvLinkStatus').className = 'status-indicator ' + (dataFresh() && lastState?.protocol_supported ? 'ok' : 'bad');
  const mqtt = $('mqttStatus');
  mqtt.textContent = !dataFresh() ? 'MQTT: Chưa rõ' : lastState.mqtt_connected === true ? 'MQTT: Đã kết nối' : lastState.mqtt_connected === false ? 'MQTT: Mất kết nối' : 'MQTT: Chưa rõ';
  mqtt.className = 'status-indicator ' + (!dataFresh() || lastState.mqtt_connected == null ? 'warn' : lastState.mqtt_connected ? 'ok' : 'bad');
  if (controlsReady()) $('kpiAlertBar').style.display = 'none';
  const reason = !dataFresh() ? ' • Dữ liệu cũ, tạm khóa điều khiển' : !lastState?.protocol_supported ? ' • Cần cập nhật core để điều khiển' : lastState.mqtt_connected === false ? ' • MQTT mất liên kết, tạm khóa điều khiển' : '';
  $('dataFreshness').textContent = lastState?.state_received_at
    ? `Nhận từ core: ${fmtDate(lastState.state_received_at)}${reason}`
    : 'Đang chờ bản đồ và telemetry từ core';
  document.querySelectorAll('[data-control]').forEach(button=>button.disabled = !controlsReady() || button.dataset.busy === 'true');
  document.querySelectorAll('[data-task-control]').forEach(button=>button.disabled = !tasksReady() || button.dataset.busy === 'true');
  const shift = lastState?.shift;
  $('startShiftBtn').disabled = !controlsReady() || !shift?.supported || !shift.can_start || $('startShiftBtn').dataset.busy === 'true';
  $('endShiftBtn').disabled = !dataFresh() || !lastState?.protocol_supported || !shift?.supported || shift.phase !== 'RUNNING' || $('endShiftBtn').dataset.busy === 'true';
  $('endShiftBtn').textContent = shift?.phase==='CLOSED'?'Ca đã đóng':shift?.phase==='CLOSING'?'Đang kết ca':'Kết thúc ca';
  $('endShiftBtn').title = !dataFresh()?'Chờ Core có dữ liệu mới':shift?.phase==='CLOSED'?'Ca này đã kết thúc':
    shift?.phase==='CLOSING'?'Đã nhận kết ca. Xem lý do từng xe trong bảng Ca làm việc.':'';
  updateWarehouseControls();
  updateWarehouseInventoryControls();
  renderWarehouseOperations(lastState);renderTransferForm();
  document.querySelectorAll('[data-map-edit]').forEach(button=>button.disabled = !canEditMap() || button.dataset.busy === 'true');
  document.querySelectorAll('[data-record-delete]').forEach(button=>{
    button.disabled=!backendOnline||button.dataset.busy==='true'||pendingControls.has('DELETE'+button.dataset.recordDelete);
  });
  renderObstacleAction();renderSummary(lastState);
  renderLocationNames();
  updateVehicleRecovery();
}
function connect(automatic = false) {
  clearTimeout(reconnectTimer);
  const token = ++generation;
  if (ws) {ws.onclose = ws.onmessage = ws.onopen = ws.onerror = null;ws.close();}
  let url;
  try {
    url = new URL($('wsUrl').value);
    if (!['ws:','wss:'].includes(url.protocol) || url.pathname !== '/ws' || url.username || url.password || url.search || url.hash) throw new Error();
  } catch {toast('Địa chỉ kết nối phải có dạng ws://máy-chủ:3000/ws','error');return;}
  const nextApi = url.origin.replace(/^ws/,'http');
  if (nextApi !== API_BASE) {
    lastState = null;recentLogs.length = 0;seenLogs.clear();logSession = null;
    kpiHistoryData=[];closeMapPreview();renderEmpty();renderSystemLog();renderKpi(null);
    warehouseLatestReport=null;warehouseReportSelection=null;selectedWarehouseReport=null;
    warehouseReportData=[];$('warehouseReportList').replaceChildren(new Option('Chưa có báo cáo kho',''));$('warehouseReportDetail').replaceChildren();
    $('mapListBox').replaceChildren();$('kpiHistoryBox').replaceChildren();$('robotHistoryPanel').hidden = true;
  }
  API_BASE = nextApi;backendOnline = false;updateConnection();
  url.searchParams.set('delta','1');
  ws = new WebSocket(url.href);
  ws.onopen = () => {
    if (token !== generation) return;
    backendOnline = true;updateConnection();
    loadMapList();loadLatestKpi();loadKpiHistory();loadStoredLogs();
    if (automatic) toast('Đã kết nối lại web','ok');
  };
  ws.onclose = () => {
    if (token !== generation) return;
    backendOnline = false;updateConnection();cancelSelection();
    reconnectTimer = setTimeout(async()=>{if (await checkAuthentication()) connect(true);},3000);
  };
  ws.onerror = () => {backendOnline = false;updateConnection();};
  ws.onmessage = event => {
    if (token !== generation) return;
    try {
      let state = JSON.parse(event.data);
      if (!state || typeof state !== 'object') return;
      if (state.type === 'state_delta') {
        if (!lastState || lastState.state_seq !== state.base_state_seq) {ws.close();return;}
        state = {...lastState,...state.changes,state_seq:state.state_seq};
      }
      lastState = {...state,map:{...(state.map || {}),nodes:state.map?.nodes || [],edges:state.map?.edges || [],obstacles:state.map?.obstacles || []},
        fleet:{...(state.fleet || {}),agvs:state.fleet?.agvs || [],pending_tasks:state.fleet?.pending_tasks || []},mutex:state.mutex || {}};
      if (logSession !== state.core_session_id) {logSession = state.core_session_id;seenLogs.clear();}
      render(lastState);updateConnection();processServerLogs(state.logs);
      if (state.kpi_alert) showKpiAlert(state.kpi_alert);
    } catch (error) {console.error('Snapshot không hợp lệ',error);toast('Không đọc được dữ liệu trạng thái','error');}
  };
}
function toast(message,type = 'ok') {
  const item = document.createElement('div');item.className = 'toast ' + type;
  const text = document.createElement('div');text.textContent = message;
  const time = document.createElement('div');time.className = 'toast-time';time.textContent = new Date().toLocaleTimeString('vi-VN');
  item.append(text,time);item.onclick = ()=>item.remove();$('toastContainer').append(item);
  while ($('toastContainer').children.length > 2) $('toastContainer').firstChild.remove();
  setTimeout(()=>item.remove(),4500);
}
async function api(method,route,body,quiet404 = false) {
  try {
    body=locationRequest(method,route,body);
    const response = await fetch(API_BASE + route,{method,credentials:'include',headers:body ? {'Content-Type':'application/json'} : undefined,body:body ? JSON.stringify(body) : undefined});
    const raw = await response.text();
    let data;
    try {data=JSON.parse(raw);} catch {
      const missingDelete=method==='DELETE'&&response.status===404&&raw.includes('Cannot DELETE');
      toast(missingDelete?'Gateway đang chạy bản cũ. Khởi động lại gateway Node.js rồi tải lại web.':`Máy chủ trả phản hồi không hợp lệ (HTTP ${response.status}).`,'error');
      return null;
    }
    if (!response.ok) {
      if (response.status === 401) checkAuthentication();
      if (!(quiet404 && response.status === 404)) toast(data.error || `HTTP ${response.status}`,data.unknown ? 'warn' : 'error');
      return null;
    }
    // Preserve array responses: snapshots, history and logs are arrays, not objects.
    return data;
  } catch (error) {console.error(error);toast('Không kết nối được web server','error');return null;}
}
async function control(method,route,body,button) {
  const endingShift=route==='/api/shift/end'&&method==='POST'&&dataFresh()&&lastState?.protocol_supported;
  if (!controlsReady()&&!endingShift) {toast('Chờ dữ liệu mới từ core trước khi điều khiển','warn');return null;}
  const key = method + route;
  if (button?.dataset.busy === 'true' || pendingControls.has(key)) return null;
  pendingControls.add(key);
  if (button) {button.dataset.busy = 'true';button.disabled = true;}
  try {return await api(method,route,body);}
  finally {pendingControls.delete(key);if (button) button.dataset.busy = 'false';updateConnection();}
}
function appendRow(table,cells) {
  const row = document.createElement('tr');
  cells.forEach(cell=>{const td = document.createElement('td');if (cell instanceof Node) td.append(cell);else td.textContent = cell ?? '—';row.append(td);});
  table.append(row);return row;
}
function action(text,handler,kind = 'control') {
  const button = document.createElement('button');button.className = 'btn-mini btn-outline';button.textContent = text;
  if (kind) button.setAttribute(kind === 'map' ? 'data-map-edit' : 'data-control','');
  button.onclick = ()=>handler(button);return button;
}
function emptyTable(tbody,cols,text) {const row = appendRow(tbody,[text]);row.firstChild.colSpan = cols;}
function recordDeleteButton(kind,id) {
  const button=action('Xóa',button=>deleteDashboardRecord(kind,id,button),null);
  button.type='button';button.classList.add('record-delete');
  button.dataset.recordDelete=kind==='order'?`/api/orders/${encodeURIComponent(id)}`:`/api/tasks/history/${id}`;
  button.setAttribute('aria-label',`Xóa ${kind==='order'?'đơn':'lịch sử'} ${id}`);
  button.title='Xóa mục này khỏi danh sách';
  button.onclick=event=>{event.preventDefault();event.stopPropagation();void deleteDashboardRecord(kind,id,button);};
  button.disabled=!backendOnline;return button;
}
async function deleteDashboardRecord(kind,id,button) {
  const route=button.dataset.recordDelete,key='DELETE'+route;
  if(!backendOnline||pendingControls.has(key))return;
  if(!confirm(`Xóa ${kind==='order'?'đơn lấy–giao':'dòng lịch sử'} này khỏi danh sách? Mục sẽ không hiện lại khi tải trang. Nhiệm vụ đang chạy vẫn tiếp tục; sổ kho và báo cáo được giữ.`))return;
  pendingControls.add(key);button.dataset.busy='true';button.disabled=true;
  try {
    const result=await api('DELETE',route);
    if(!result)return;
    if(lastState) {
      if(kind==='order') {
        for(const order of lastState.orders||[])if(order.id===id)order.deleted_at=result.deleted_at;
        missionExpanded.delete(`order:${id}`);renderOrders(lastState.orders);renderWarehouseOperations(lastState);
      } else {
        lastState.task_history=(lastState.task_history||[]).filter(row=>row.id!==id);
        missionExpanded.delete(`history:${id}`);renderMissions(lastState);
      }
    }
    toast('Đã xóa mục khỏi danh sách');
  } finally {pendingControls.delete(key);button.dataset.busy='false';updateConnection();}
}
function warehouseRollbackLabel(order) {
  const sources=[...new Set((order?.items||[]).filter(item=>item.status!=='delivered'&&item.pickup)
    .map(item=>locationText(item.pickup)))];
  return sources.length?`Hoàn tác về ${sources.join(', ')}`:'Hoàn tác';
}
async function resolveWarehouseOrder(orderId) {
  const key=`resolve:${orderId}`;
  if(pendingControls.has(key))return;
  const order=lastState?.orders?.find(order=>order.id===orderId);
  if(!order)return;
  const revision=lastState?.warehouse?.revision;
  pendingControls.add(key);
  try {
    const result=await api('POST',`/api/orders/${encodeURIComponent(orderId)}/resolve`,{expected_revision:revision});
    if(result)toast('Đã hoàn tác hàng dang dở về kho trên sổ kho');
  } finally {pendingControls.delete(key);}
}
function updateRobotAges() {
  document.querySelectorAll('[data-robot-age]').forEach(el=>{
    const robot = lastState?.fleet.agvs.find(a=>a.id === el.dataset.robotAge);
    if (!robot || robot.last_seen_s == null) {el.textContent = 'Chưa có tuổi telemetry';return;}
    const seconds = finite(robot.last_seen_s)+snapshotAge();
    const text = `${Number.isFinite(seconds) ? Math.floor(seconds) + 's' : '?'} từ telemetry${dataFresh() ? '' : ' • dữ liệu cũ'}`;
    if(el.textContent!==text)el.textContent=text;
  });
  document.querySelectorAll('[data-robot-wait]').forEach(el=>{
    const robot=lastState?.fleet.agvs.find(a=>a.id===el.dataset.robotWait);
    const text=`Đã chờ ${Math.floor((robot?.node_proof_wait_ms||0)/1000)} giây. Đưa xe về thẻ RFID và xác minh dừng.`;
    if(el.textContent!==text)el.textContent=text;
  });
}
function renderTasks(tasks) {
  const tbody = document.querySelector('#taskTable tbody');tbody.replaceChildren();
  if (!tasks.length) return emptyTable(tbody,3,'Hàng đợi trống');
  tasks.forEach(t=>appendRow(tbody,[`${locationText(t.target)} · ${t.task_id ? t.task_id.slice(0,8) : 'không có ID'}`,t.priority,
    action('Hủy',button=>cancelTask(t,button))]));
}
function addOrderLine(kind,node = '') {
  const box = $(kind === 'pickup' ? 'orderPickups' : 'orderDeliveries');
  const row = document.createElement('div');row.className = 'order-line';
  const select = document.createElement('select');select.setAttribute('aria-label',kind === 'pickup' ? 'Kho lấy' : 'Kho giao');
  for (const warehouse of orderWarehouses) select.add(new Option(warehouse,warehouse));
  if (node && orderWarehouses.includes(node)) select.value = node;
  else if (kind === 'dropoff' && orderWarehouses.length > 1) select.selectedIndex = 1;
  const remove = document.createElement('button');remove.type = 'button';remove.className = 'btn-outline';remove.textContent = '×';remove.title = 'Xóa kho';
  remove.setAttribute('aria-label','Xóa kho');remove.onclick = ()=>{row.remove();updateOrderTotals();};
  select.onchange = ()=>{select.dataset.initialized='true';updateOrderTotals();};
  row.append(select,remove);box.append(row);updateOrderTotals();
}
function populateOrderWarehouses(map) {
  const warehouses = warehouseNodes(map);
  const key = JSON.stringify(warehouses);
  if (key === orderWarehouseKey) return;
  orderWarehouseKey = key;orderWarehouses = warehouses;
  for (const box of [$('orderPickups'),$('orderDeliveries')]) for (const select of box.querySelectorAll('select')) {
    const previous = select.value;select.replaceChildren();
    for (const warehouse of warehouses) select.add(new Option(warehouse,warehouse));
    if (warehouses.includes(previous)) select.value = previous;
  }
  if (!$('orderPickups').children.length) addOrderLine('pickup');
  if (!$('orderDeliveries').children.length) addOrderLine('dropoff');
  updateOrderTotals();
}
function orderLines(kind) {
  return [...$(kind === 'pickup' ? 'orderPickups' : 'orderDeliveries').querySelectorAll('.order-line')]
    .map(row=>({[kind]:row.querySelector('select').value,quantity:1}));
}
function updateOrderTotals() {
  const pickups = orderLines('pickup'),deliveries = orderLines('dropoff');
  const taken = pickups.reduce((sum,line)=>sum+(Number.isInteger(line.quantity) ? line.quantity : 0),0);
  const given = deliveries.reduce((sum,line)=>sum+(Number.isInteger(line.quantity) ? line.quantity : 0),0);
  $('orderTotals').textContent = `Lấy ${taken} · giao ${given} kệ${taken !== given ? ' · chưa khớp' : ''}`;
  renderTransferForm();
}
async function submitOrder() {
  if (!tasksReady()) return toast('Chờ ca RUNNING và Core sẵn sàng','warn');
  const reason=validateTransferForm();if(reason)return toast(reason,'warn');
  const pickups = orderLines('pickup'),deliveries = orderLines('dropoff');
  if (!pickups.length || !deliveries.length) return toast('Cần ít nhất một kho lấy và một kho giao','warn');
  const result = await control('POST','/api/orders',{pickups,deliveries},$('submitOrderBtn'));
  if (result) {toast(`Đã tạo ${result.order.total} lượt chuyển kệ. Theo dõi trong Nhiệm vụ và kết quả bên dưới bản đồ.`);showView('overview');}
}
function singleWarehouseRobot() {
  const shift=lastState?.shift;
  if(shift?.operation_profile==='scoped'&&shift.active_robot_ids?.length===1)return shift.active_robot_ids[0];
  return shift?.test_mode==='robot01'?'01':shift?.test_mode==='robot02'?'02':null;
}
function updateWarehouseControls() {
  const robotId=singleWarehouseRobot(),robot=lastState?.fleet.agvs.find(robot=>robot.id===robotId);
  const active=lastState?.warehouse_actions?.find(action=>!['completed','interrupted'].includes(action.status));
  const ready=tasksReady()&&Array.isArray(lastState?.warehouse_actions)&&lastState.shift?.phase==='RUNNING'&&robotId&&
    robot?.state==='IDLE'&&robot.online!==false&&robot.position_valid!==false&&!robot.error&&!robot.task_id&&!active&&!!$('warehouseTarget').value;
  for(const id of ['pickupWarehouseBtn','dropoffWarehouseBtn']) $(id).disabled=!ready||$(id).dataset.busy==='true';
}
async function warehouseAction(action,button) {
  const robotId=singleWarehouseRobot();if(!robotId)return;
  const result=await control('POST',`/api/agvs/${robotId}/warehouse-action`,{action,warehouse:$('warehouseTarget').value},button);
  if(result)toast(`Xe ${robotId}: ${action==='pickup'?'lấy':'hạ'} tại ${locationText(result.action.warehouse)}, ${result.action.servo?'servo 5s':'chờ mô phỏng 5s (chưa có servo)'} rồi lùi ra ${result.action.gate}`);
}
function renderWarehouseActionStatus() {
  const robotId=singleWarehouseRobot(),test=!!robotId;
  $('warehouseModeBtn').hidden=!test;$('orderModeBtn').hidden=test;
  if(test&&!$('orderTaskForm').hidden)setTaskMode('warehouse');
  if(!test&&!$('warehouseTaskForm').hidden)setTaskMode('order');
  const warehouses=warehouseNodes(lastState?.map);
  const select=$('warehouseTarget'),key=JSON.stringify([warehouses,lastState?.location_names?.revision]);
  if(select.dataset.nodes!==key){const prior=select.value;select.replaceChildren();for(const node of warehouses)select.add(new Option(locationText(node),node));if(warehouses.includes(prior))select.value=prior;select.dataset.nodes=key;}
  const action=lastState?.warehouse_actions?.find(action=>action.robot===robotId),robot=lastState?.fleet.agvs.find(robot=>robot.id===robotId);
  const names={dispatching:'Đang cấp tuyến tới kho',to_warehouse:'Đang đi tới kho',waiting_servo:'Chờ nhúng xác nhận dừng tại kho',dispatching_lift:'Đang gửi lệnh servo',handling:robot?.lift?.state==='PENDING'?'Chờ nhúng nhận lệnh':action?.action==='pickup'?'Đang nâng kệ (servo 5s)':'Đang hạ kệ (servo 5s)',dispatching_exit:'Đang cấp lệnh lùi',to_gate:'Đang lùi ra gate',completed:'Đã ra gate; chờ lệnh tiếp theo',interrupted:'Thao tác gián đoạn'};
  $('warehouseActionStatus').textContent=test&&!Array.isArray(lastState?.warehouse_actions)?'Cần khởi động lại gateway để nạp chức năng Nâng / hạ kệ.':action ? action.status==='waiting' ?
    `Dừng tại ${locationText(action.warehouse)}: còn ${Math.max(0,Math.ceil((Date.parse(action.service_until)-Date.now())/1000))}s (mô phỏng, chưa điều khiển servo)` :
    `${names[action.status]||action.status} · ${locationText(action.warehouse)} → ${action.gate}${action.reason?' · '+action.reason:''}` :
    robot?.state==='IDLE'?`Xe ${robotId} sẵn sàng. Chọn kho rồi bấm Nâng kệ hoặc Hạ kệ.`:`Xe ${robotId||'—'}: ${robot?.error||robot?.state||'chưa có dữ liệu'}`;
  updateWarehouseControls();
}
function updateOrderTimers() {
  document.querySelectorAll('[data-order-service-until]').forEach(node=>{
    const seconds = Math.max(0,Math.ceil((Date.parse(node.dataset.orderServiceUntil)-Date.now())/1000));
    node.textContent = `Mô phỏng ${node.dataset.orderServiceKind}: còn ${seconds}s`;
  });
  renderWarehouseActionStatus();
}
function renderOrderProgress(orders) {
  const rows = (Array.isArray(orders) ? orders : []).filter(order=>!order.deleted_at&&(order.status!=='closed'||$('orderShowResolved').checked));
  const select = $('orderProgressSelect');
  const optionsKey = JSON.stringify(rows.map(order=>[order.id,order.status,order.items.map(item=>[item.pickup,item.dropoff,item.robot])]));
  if (optionsKey !== orderProgressOptionsKey) {
    const previous = rows.find(order=>order.id === select.value);
    const selected = previous?.status === 'active' ? previous : rows.find(order=>order.status === 'active') || previous || rows[0];
    select.replaceChildren();
    if (!rows.length) select.add(new Option('Chưa có đơn hàng',''));
    else for (const order of rows) {
      const route=order.items.length===1?`${missionLocationText(order.items[0].pickup,order.location_names)} → ${missionLocationText(order.items[0].dropoff,order.location_names)}`:`${order.total} lượt chuyển kệ`;
      const robots=[...new Set(order.items.map(item=>item.robot).filter(Boolean))];
      select.add(new Option(`${route} · ${robots.length?`Xe ${robots.join(' + ')}`:'Chờ phân xe'}`,order.id));
    }
    select.value = selected?.id || '';
    select.disabled = rows.length < 2;
    orderProgressOptionsKey = optionsKey;
  }
  const order = rows.find(candidate=>candidate.id === select.value);
  const total = Number.isInteger(order?.total) && order.total > 0 ? order.total : 0;
  const delivered = order?.items?.filter(item=>item.status === 'delivered').length || 0;
  const progress = $('orderProgressBar');
  progress.max = total || 1;
  progress.value = Math.min(delivered,total);
  progress.setAttribute('aria-valuetext',`${delivered}/${total} kệ`);
  $('orderProgressCount').textContent = `${delivered}/${total} kệ`;
}
function renderOrders(orders) {
  orders=Array.isArray(orders)?orders:[];
  const selected=lastState?.shift?.test_mode === 'robot01' ? '01' : lastState?.shift?.test_mode === 'robot02' ? '02' :
    lastState?.shift?.trial_robot_id || lastState?.fleet?.trial_robot_id;
  if ($('orderDispatchHint')) $('orderDispatchHint').textContent=selected ?
    `Phạm vi hiện tại: xe ${selected}.` :
    'Tự chọn xe sẵn sàng gần kho lấy nhất. Theo dõi xe và tuyến đã giao ở bảng phía trên.';
  renderOrderProgress(orders);
  renderMissions(lastState?{...lastState,orders}:null);
  updateOrderTimers();
}
function renderMutex(mutex) {
  const tbody = document.querySelector('#mutexTable tbody');tbody.replaceChildren();
  if (!Object.keys(mutex).length) return emptyTable(tbody,3,'Không có vùng chung');
  Object.entries(mutex).forEach(([zone,z])=>{
    const owner = lastState.fleet.agvs.find(a=>a.id === z.owner);
    const parkedOwner = owner?.state === 'IDLE' && owner.online && owner.position_valid &&
      !owner.task_id && !owner.target && !(owner.path || []).length &&
      !(owner.uncertain_path || []).length && lastState.shift?.two_robot_trial === true &&
      (lastState.shift.robots || []).some(robot=>robot.id === z.owner && robot.at_home);
    const cell = document.createElement('div');
    cell.textContent = z.owner ? `${z.owner}${z.recovery_required ? ' · cần xác minh' : !z.grant_acked ? ' · chờ ACK quyền' : ''}` : 'Trống';
    if (z.owner && (!owner || ['ERROR','OFFLINE'].includes(owner.state) || parkedOwner) && z.grant_id != null)
      cell.append(action('Xác minh vùng trống',async button=>{
        if (!confirm(`Chỉ tiếp tục sau khi đã dừng xe ${z.owner} bằng biện pháp tại xe và xác minh toàn bộ thân xe đã ra khỏi vùng ${zone}. Bạn xác nhận vùng trống?`)) return;
        const result = await control('POST',`/api/mutex/${encodeURIComponent(zone)}/confirm-clear`,{owner:z.owner,grant_id:z.grant_id,physically_clear:true},button);
        if (result) toast(`Core đã xử lý quyền vùng ${zone}`);
      }));
    appendRow(tbody,[zone,cell,(z.waiting || []).join(', ') || '—']);
  });
}
function populateTaskAgvSelect(agvs) {
  const select = $('task_agv'),previous = select.value,trial = lastState?.shift?.single_robot_trial === true;
  const trialRobot = lastState?.shift?.trial_robot_id || '';
  const twoRobotTrial = lastState?.shift?.two_robot_trial === true;
  const active=lastState?.shift?.operation_profile==='scoped' ? lastState.shift.active_robot_ids||[] : [];
  const automatic = new Option(trial ? 'Chỉ xe 01 trong lần thử' : trialRobot ? `Tự động giao cho xe ${trialRobot}` : active.length ?
    `Tự động chọn trong xe ${active.join(' + ')}` : 'Tự động chọn xe gần nhất','');
  automatic.disabled = trial;
  select.replaceChildren(automatic);
  agvs.forEach(a=>{
    const option = new Option(`${a.id} — ${labels[a.state] || a.state}`,a.id);
    option.disabled = a.participating === false || (trial && a.id !== '01') || (trialRobot && a.id !== trialRobot) || (twoRobotTrial && !['01','02'].includes(a.id)) ||
      !vehicleCanSelect(a);
    select.add(option);
  });
  if (trial) {
    const robot01 = [...select.options].find(option=>option.value === '01' && !option.disabled);
    select.value = robot01 ? '01' : '';
    $('taskBatteryHint').textContent = robot01
      ? 'Lần thử một xe dùng GOTO trực tiếp 01. % pin đang chỉ hiển thị, không áp dụng ngưỡng 20%/40%.'
      : 'Chờ xe 01 có telemetry/RFID hợp lệ để thử GOTO trực tiếp; % pin đang chỉ hiển thị.';
    return;
  }
  if (Array.from(select.options).some(option=>option.value === previous && !option.disabled)) select.value = previous;
  if (lastState?.fleet?.battery_gate_enabled === false) {
    $('taskBatteryHint').textContent = 'Đang tạm bỏ chốt pin để test: pin thấp hoặc chưa có số đo không chặn nhận việc/mở ca. Sạc thủ công; RFID, MQTT và mutex vẫn được kiểm tra.';
    return;
  }
  if (trialRobot) {
    $('taskBatteryHint').textContent = `Thử riêng xe ${trialRobot}: chỉ xe này nhận nhiệm vụ; mutex vẫn bật. Core nhận pin diagnostic, nhưng chế độ thử không chặn theo ngưỡng.`;
    return;
  }
  if (twoRobotTrial) {
    $('taskBatteryHint').textContent = 'Thử hai xe 01/02: Core nhận pin diagnostic, nhưng chế độ thử không chặn theo ngưỡng. Core giữ mutex; vùng chỉ được nhả khi đã xác minh toàn bộ xe rời vùng.';
    return;
  }
  if (active.length) {
    $('taskBatteryHint').textContent = `Core thống nhất chỉ giao việc cho xe ${active.join(' + ')}; vẫn áp dụng pin 20%/40%, RFID, MQTT và mutex như vận hành thật.`;
    return;
  }
  const diagnosticBattery = agvs.some(agv=>agv.battery_control_source === 'diagnostic');
  $('taskBatteryHint').textContent = diagnosticBattery
    ? 'Core đang dùng phần trăm pin diagnostic đã lọc cho ngưỡng thấp 20% và nhận việc lại từ 40%.'
    : '';
}
function render(state) {
  renderLocationNames();
  renderWarehouse(state.warehouse);
  renderWarehouseOperations(state);
  renderShift(state.shift);
  if (state.fleet.accepting_tasks === false && selectedAgvId) cancelSelection();
  if (selectedAgvId && !vehicleCanSelect(state.fleet.agvs.find(a=>a.id===selectedAgvId))) cancelSelection();
  renderMap(state.map,state.fleet.agvs);renderTasks(state.fleet.pending_tasks);
  renderMutex(state.mutex);populateTaskAgvSelect(state.fleet.agvs);
  populateNodeOptions(state.map);populateOrderWarehouses(state.map);renderOrders(state.orders || []);renderObstacleAction();renderSummary(state);
  if ($('mapTitle')) $('mapTitle').textContent = '';
  updateVehicleRecovery();
}
function renderEmpty() {
  clearMapRenderCache();
  renderWarehouse(null);
  renderWarehouseOperations(null);
  renderShift(null);
  renderTasks([]);renderOrders([]);$('nodesG').replaceChildren();$('edgesG').replaceChildren();$('agvsG').replaceChildren();$('routesG').replaceChildren();$('sonarObstaclesG').replaceChildren();
  populateNodeOptions(null);renderSummary(null);renderObstacleAction();
}
function setSelectionText(text,picking) {$('selectionText').textContent = text;$('selectionBar').className = picking ? 'picking' : 'idle';$('cancelSelectBtn').style.display = picking ? 'inline-block' : 'none';}
function selectAgv(robotId) {
  if (!tasksReady()) return toast('Chờ bắt đầu ca và dữ liệu mới từ core','warn');
  const robot = lastState.fleet.agvs.find(a=>a.id === robotId);
  if (!vehicleCanSelect(robot)) return;
  selectedAgvId = robotId;setTaskMode('node');setSelectionText(`Đã chọn ${robotId}. Bấm node đích để giao nhiệm vụ cho xe này.`,true);renderMap(lastState.map,lastState.fleet.agvs);
}
function cancelSelection() {
  selectedAgvId = null;setSelectionText('Bấm node để Core chọn xe gần nhất; bấm robot trước nếu muốn chỉ định xe. Vị trí là node RFID gần nhất.',false);
  if (lastState) renderMap(lastState.map,lastState.fleet.agvs);
}
async function onNodeClick(nodeId) {
  $('task_target').value = locationName(nodeId);$('obstacle_node').value = nodeId;setTaskMode('node');
  renderObstacleAction();
  const robotId = selectedAgvId || (lastState?.shift?.single_robot_trial === true ? '01' : '');
  if (selectedAgvId) cancelSelection();
  if (!robotId) $('task_agv').value = '';
  if (!tasksReady()) return toast('Ca chưa mở nhận nhiệm vụ','warn');
  const route = robotId ? `/api/agvs/${encodeURIComponent(robotId)}/goto` : '/api/tasks';
  const result = await control('POST',route,{target:locationName(nodeId),priority:Number($('task_priority').value)});
  if (!result) return;
  const assigned = robotId || lastState?.fleet?.tasks?.find(task=>task.task_id === result.task_id)?.assigned_agv;
  toast(`Core đã nhận nhiệm vụ ${result.task_id.slice(0,8)}${assigned ? ` cho xe ${assigned}` : ' · chờ xe khả dụng'}`);
  setSelectionText(`Nhiệm vụ tới ${locationText(nodeId)} đã được Core nhận${assigned ? `; xe ${assigned} được chọn` : '; Core sẽ chọn xe gần nhất khi có xe khả dụng'}. Theo dõi tiến độ trong lịch sử.`,false);
}
async function addTask() {
  if (!tasksReady()) return toast('Ca chưa mở nhận nhiệm vụ','warn');
  const target = $('task_target').value.trim(),priority = Number($('task_priority').value),robot = $('task_agv').value;
  if (lastState?.shift?.single_robot_trial === true && robot !== '01')
    return toast('Chế độ thử một xe chỉ nhận GOTO trực tiếp xe 01','warn');
  if(robot&&!vehicleCanSelect(lastState?.fleet?.agvs?.find(candidate=>candidate.id===robot)))
    return toast('Xe chưa rảnh hoặc chưa khôi phục xong. Chờ tuyến hiện tại kết thúc trước khi giao lệnh mới.','warn');
  const route = robot ? `/api/agvs/${encodeURIComponent(robot)}/goto` : '/api/tasks';
  const result = await control('POST',route,{target,priority},$('submitTaskBtn'));
  if (result) toast(`Core đã nhận nhiệm vụ ${result.task_id.slice(0,8)}`);
}
function renderShift(shift) {
  const names = {RUNNING:'Đang làm việc',CLOSING:'Đang kết ca',CLOSED:'Ca đã đóng'};
  $('shiftStatus').textContent = shift?.supported ? names[shift.phase] || shift.phase : 'Chưa có quản lý ca trên core / bản đồ này';
  const diagnosticBattery = lastState?.fleet?.agvs?.some(robot=>robot.battery_control_source === 'diagnostic');
  const mode = $('coreModeStatus');
  mode.textContent = !shift?.supported ? 'Chưa xác định chế độ Core'
    : lastState?.fleet?.battery_gate_enabled === false ? `Core · xe ${(shift.active_robot_ids||['01','02']).join(' + ')} · tạm bỏ chốt pin · sạc thủ công · mutex giữ nguyên`
    : shift.operation_profile==='scoped' ? `Core thống nhất · đang vận hành xe ${(shift.active_robot_ids||[]).join(' + ')} · áp dụng pin/mutex như Core thật`
    : shift.trial_robot_id ? `Core: thử riêng xe ${shift.trial_robot_id} · mutex bật · trial không chặn ngưỡng pin`
    : shift.single_robot_trial ? 'Core: thử một xe 01 · trial không chặn ngưỡng pin'
    : shift.two_robot_trial ? 'Core: thử hai xe 01/02 · mutex bật · trial không chặn ngưỡng pin'
    : diagnosticBattery ? 'Core: chế độ thường · pin diagnostic dùng cho điều phối'
      : 'Core: chế độ thường';
  mode.className = 'field-hint';
  $('homeStationCaption').textContent = shift?.supported && shift.home_stations
    ? Object.entries(shift.home_stations).map(([robot,station])=>`${robot} → ${locationText(station)}`).join(' · ') || 'Theo cấu hình core'
    : 'Theo cấu hình core';
  const list = $('homeStationList');list.replaceChildren();
  const reasons = {'Chua tham gia ca':'Chưa tham gia ca','Cho telemetry / RFID':'Chờ telemetry / RFID','Mat telemetry':'Mất telemetry',
    'Cho RFID hop le':'Chờ RFID hợp lệ','Can xac minh xe / xoa loi':'Cần xác minh / xóa lỗi','Can doi soat ke tren xe':'Cần đối soát kệ trên xe',
    'Cho hoan thanh task hien tai':'Chờ hoàn thành task','Cho duong ve tram sac':'Chờ đường về trạm',
    'Cho xac nhan roi mutex zone':'Chờ xác nhận rời vùng mutex','Da xac minh tai tram':'Đã xác minh tại trạm'};
  for (const robot of shift?.robots || []) {
    const line = document.createElement('div');
    const vehicle=lastState?.fleet?.agvs?.find(entry=>entry.id===robot.id);
    let reason = reasons[robot.reason] || (robot.reason?.startsWith('Dang ve tram ') ? `Đang về ${locationText(robot.home_station)}` : robot.reason || '');
    if(robot.reason==='Can doi soat ke tren xe') {
      if(vehicle?.recovery_reset_pending)reason='Đang chờ xe xác nhận reset lỗi';
      else if(vehicle?.heading_confirmation_pending)reason='Đang chờ xe xác nhận hướng';
      else if(vehicle?.heading_confirmation_required)reason=vehicle.node===lastState?.shift?.home_stations?.[robot.id]
        ?'Chờ xe tự khôi phục tại trạm, không cần chọn hướng':'Cần xác nhận hướng trong Khôi phục xe';
    }
    line.textContent = `${robot.id} → ${locationText(robot.home_station)}: ${reason}${vehicle?.error?` • ${vehicle.error}`:''}${robot.actual_node ? ` • RFID gần nhất: ${locationText(robot.actual_node)}` : ''}`;
    line.style.color = robot.ready ? colors.IDLE : colors.BLOCKED;list.append(line);
  }
  if (shift?.phase === 'CLOSED' && !shift.can_start) {
    const note = document.createElement('small');
    note.textContent = lastState?.fleet?.battery_gate_enabled === false
      ? 'Chờ xe tham gia online tại trạm riêng, RFID mới và trạng thái an toàn; xe ngoài phạm vi phải an toàn tại trạm. Pin đang không chặn mở ca.'
      : shift.operation_profile==='scoped'
      ? `Chờ xe tham gia ${(shift.active_robot_ids||[]).join(' + ')} đủ pin, RFID và trạng thái an toàn. Xe ngoài phạm vi nếu đã xuất hiện phải đứng yên tại trạm, hết task/vùng.`
      : shift.trial_robot_id
      ? `Chờ xe ${shift.trial_robot_id} tại trạm riêng. Xe không chọn đã xuất hiện cần dừng/rảnh tại trạm và hết lỗi/vùng chưa xác minh.`
      : shift.single_robot_trial
      ? 'Chờ xe 01 online tại S1, RFID mới và xử lý lỗi/vùng chiếm. Pin không chặn ca thử một xe.'
      : shift.two_robot_trial
      ? 'Chờ cả xe 01 tại S1 và xe 02 tại S2 online, RFID mới và xử lý lỗi/vùng chiếm. Pin không chặn ca thử hai xe.'
      : 'Chờ ít nhất một xe online tại trạm riêng, RFID mới và xử lý lỗi/vùng chiếm. Pin đã đo cần ≥40%.';
    list.append(note);
  }
}
async function changeShift(mode,button) {
  if(mode==='start'){openShiftWarehouse();return;}
  if(!confirm('Kết thúc ca sẽ hủy toàn bộ nhiệm vụ đang chờ và đang thực hiện. Xe nhận lệnh dừng; kiểm tra kệ còn trên xe và đưa xe về trạm. Tiếp tục?'))return;
  const result = await control('POST',`/api/shift/${mode}`,{},button);
  if (result) toast(result.shift?.phase==='CLOSED'?'Ca đã đóng. Có thể kiểm tra và lưu đối soát kho.':
    'Core đã nhận kết ca. Xem lý do còn chờ của từng xe trong bảng Ca làm việc.');
}
async function cancelTask(task,button) {
  const payload = task.task_id ? {task_id:task.task_id} : {target:task.target};
  const result = await control('POST','/api/tasks/cancel',payload,button);
  if (result) toast(`Core đã hủy ${result.cancelled} nhiệm vụ; robot nhận lệnh chờ`);
}
async function removeAGV(robotId,button) {
  if (!confirm(`Đã đưa robot ${robotId} ra khỏi kho và xác minh vùng đường trống?`)) return;
  const session=lastState?.core_session_id;
  if (await control('DELETE',`/api/agvs/${encodeURIComponent(robotId)}`,undefined,button)) {
    markVehicleRemoved(robotId,session);
    renderWarehouseOperations(lastState);
    toast(`Đã gỡ xe ${robotId} khỏi Core; thẻ xe vẫn được giữ trên web`);
  }
}
async function setObstacle(blocked) {
  const result = await control('POST','/api/map/obstacle',{node:$('obstacle_node').value.trim(),blocked},$('blockNodeBtn'));
  if (result) toast(blocked ? 'Core đã khóa node và tính lại đường' : 'Core đã mở node');
}
function toggleObstacle() {
  const node = $('obstacle_node').value.trim();
  if (!lastState?.map?.nodes?.some(n=>n.id === node)) return toast('Chọn một node có trong bản đồ','warn');
  return setObstacle(!(lastState.map.obstacles || []).includes(node));
}
async function saveMap() {
  const result = await control('POST','/api/map/save',{},$('quickSaveBtn'));
  if (result) {toast(`Core đã lưu bản đồ: ${result.path}`);await loadMapList();}
}
async function saveNamedMap() {
  const result = await control('POST','/api/map/snapshots',{name:$('map_save_name').value.trim()},$('saveSnapshotBtn'));
  if (result) {$('map_save_name').value = '';toast(`Đã lưu snapshot ${result.name}`);await loadMapList();}
}
async function loadMapList() {
  const rows = await api('GET','/api/map/snapshots');if (!Array.isArray(rows)) return;
  const box = $('mapListBox');box.replaceChildren();
  rows.forEach(row=>{
    const item = document.createElement('div');item.className = 'log-item';
    const text = document.createElement('div');text.textContent = row.name || `Bản đồ #${row.id}`;
    const time = document.createElement('div');time.className = 'log-time';time.textContent = fmtDate(row.saved_at);text.append(time);
    item.append(text,action('Xem',()=>viewSavedMap(row.id),null));box.append(item);
  });if (!rows.length) box.textContent = 'Chưa có bản đồ đã lưu';
}
async function viewSavedMap(snapshotId) {
  const snap = await api('GET',`/api/map/snapshots/${snapshotId}`);if (!snap?.map) return;
  previewMapId = snapshotId;$('mapPreviewTitle').textContent = snap.name || `Bản đồ #${snapshotId}`;
  drawMap(snap.map,[],'preview');$('mapPreviewOverlay').style.display = 'flex';updateConnection();
}
function closeMapPreview() {$('mapPreviewOverlay').style.display = 'none';previewMapId = null;}
async function applyPreviewedMap() {
  if (!previewMapId) return;
  if (await control('POST',`/api/map/snapshots/${previewMapId}/apply`,{},$('applySnapshotBtn'))) {toast('Core đã xác nhận áp dụng bản đồ');closeMapPreview();}
}
async function deletePreviewedMap() {
  if (!previewMapId || !confirm('Xóa snapshot đã lưu này?')) return;
  if (await api('DELETE',`/api/map/snapshots/${previewMapId}`)) {closeMapPreview();loadMapList();}
}
function processServerLogs(logs,notify = true) {
  if (!Array.isArray(logs)) return;
  for (const entry of logs) {
    if (!entry || typeof entry.msg !== 'string') continue;
    const key = `${entry.core_session || logSession}:${entry.seq ?? entry.core_seq}`;
    if (seenLogs.has(key)) continue;seenLogs.add(key);
    recentLogs.unshift(entry);
    if (notify && recentLogs.length <= 5) toast(entry.msg,/\[LOI\]|BAO LOI|KHONG ACK/.test(entry.msg) ? 'error' : 'ok');
  }
  recentLogs.length = Math.min(50,recentLogs.length);renderSystemLog();
}
function renderSystemLog() {
  const box = $('systemLogBox');box.replaceChildren();
  recentLogs.forEach(log=>{
    const item = document.createElement('div');item.className = 'log-item';item.textContent = log.msg;
    const time = document.createElement('div');time.className = 'log-time';time.textContent = log.time || '';item.append(time);box.append(item);
  });if (!recentLogs.length) box.textContent = 'Đang chờ sự kiện';
}
async function loadStoredLogs() {
  const rows = await api('GET','/api/logs?limit=50');
  if (Array.isArray(rows)) processServerLogs([...rows].reverse().map(r=>({...r,msg:r.message,time:fmtDate(r.created_at)})),false);
}
function showKpiAlert(report) {
  $('kpiAlertBar').style.display = 'block';$('kpiAlertBar').textContent = `Mất liên kết core lúc ${fmtDate(report.created_at)}. Dữ liệu robot đang hiển thị là dữ liệu cũ. Đã lưu báo cáo #${report.id}.`;
  if(currentKpiId==null)renderKpi(report);loadKpiHistory();
}
async function loadRobotHistory(robotId) {
  const rows = await api('GET',`/api/agvs/${encodeURIComponent(robotId)}/history?limit=30`);if (!Array.isArray(rows)) return;
  $('robotHistoryTitle').textContent = `Lịch sử ${robotId}`;
  const tbody = document.querySelector('#robotHistoryTable tbody');tbody.replaceChildren();
  rows.forEach(r=>appendRow(tbody,[fmtDate(r.created_at),r.node,r.battery == null ? '—' : `${r.battery}%`,labels[r.state] || r.state]));
  if (!rows.length) emptyTable(tbody,4,'Chưa có dữ liệu');$('robotHistoryPanel').hidden = false;showView('overview');
}
document.addEventListener('keydown',event=>{if (event.key === 'Escape') {cancelSelection();closeMapPreview();}});
$('obstacle_node').addEventListener('input',renderObstacleAction);
document.querySelectorAll('[data-view-target]').forEach(button=>button.addEventListener('click',()=>showView(button.dataset.viewTarget)));
$('wsUrl').value = API_BASE.replace(/^http/,'ws') + '/ws';
async function checkAuthentication() {
  try {
    const response = await fetch(API_BASE+'/auth/status',{credentials:'include'}),auth = await response.json();
    $('logoutBtn').hidden = !auth.required || !auth.authenticated;
    if (auth.required && !auth.authenticated) {
      if (!$('loginDialog').open) $('loginDialog').showModal();return false;
    }
    return auth.authenticated;
  } catch {return true;}
}
async function logout() {
  await fetch(API_BASE+'/auth/logout',{method:'POST',credentials:'include'});
  clearTimeout(reconnectTimer);++generation;ws?.close();backendOnline = false;updateConnection();
  await checkAuthentication();
}
$('loginForm').addEventListener('submit',async event=>{
  event.preventDefault();
  try {
    const response = await fetch(API_BASE+'/auth/login',{method:'POST',credentials:'include',headers:{'Content-Type':'application/json'},body:JSON.stringify({token:$('loginToken').value})});
    const result = await response.json();
    if (!response.ok) {$('loginError').textContent = result.error;return;}
    $('loginToken').value = '';$('loginError').textContent = '';$('loginDialog').close();
    await checkAuthentication();connect();
  } catch {$('loginError').textContent = 'Không kết nối được máy chủ';}
});
$('loginDialog').addEventListener('cancel',event=>event.preventDefault());
new ResizeObserver(scheduleMapDocking).observe($('mapViewport'));
$('orderProgressSelect').addEventListener('change',()=>renderOrderProgress(lastState?.orders || []));
setTaskMode('order');showView('overview');renderEmpty();checkAuthentication().then(ready=>{if (ready) connect();});
setInterval(()=>{
  updateConnection();
  updateRobotAges();
  updateOrderTimers();
  if(lastState)renderWarehouseOperations(lastState);
  if(!robotMotionFrame)animateRobotMarkers();
},1000);


// PTIT / Standard Theme Switcher
function applyAppTheme(theme) {
  const isPtit = theme === 'ptit';
  document.documentElement.setAttribute('data-theme', theme);
  document.body.setAttribute('data-theme', theme);
  const brandMark = document.querySelector('.brand-mark');
  const brandSub = document.querySelector('.brand-subtitle');
  const themeToggleLabel = document.getElementById('themeToggleLabel');
  const themeBadge = document.getElementById('themeBadge');
  const brandLogo = document.getElementById('brandLogoImg');
  if (brandLogo) brandLogo.style.display = isPtit ? 'block' : 'none';
  if (brandMark) { brandMark.style.display = isPtit ? 'none' : 'grid'; brandMark.textContent = 'AGV'; }
  if (brandSub) brandSub.textContent = isPtit ? 'PTIT Smart Logistics · Học viện CN Bưu chính Viễn thông' : 'Kho AGV · Hai xe tự hành';
  if (themeToggleLabel) themeToggleLabel.textContent = isPtit ? 'Gu PTIT' : 'Chuẩn';
  if (themeBadge) themeBadge.textContent = isPtit ? 'PTIT' : 'CYBER';
  try { localStorage.setItem('agv_theme', theme); } catch(e) {}
}

function initThemeSwitcher() {
  let savedTheme = 'ptit';
  try {
    savedTheme = localStorage.getItem('agv_theme') || 'ptit';
  } catch(e) {}
  applyAppTheme(savedTheme);

  const toggleBtn = document.getElementById('themeToggleBtn');
  if (toggleBtn) {
    toggleBtn.addEventListener('click', () => {
      const current = document.body.getAttribute('data-theme') || 'ptit';
      const nextTheme = current === 'ptit' ? 'standard' : 'ptit';
      applyAppTheme(nextTheme);
    });
  }
}
initThemeSwitcher();

