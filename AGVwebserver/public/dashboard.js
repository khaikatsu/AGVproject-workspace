'use strict';
let ws = null, lastState = null, backendOnline = false, reconnectTimer = null, generation = 0;
let API_BASE = /^https?:$/.test(location.protocol) ? location.origin : 'http://localhost:3000';
let selectedAgvId = null, previewMapId = null, currentKpiId = null, kpiHistoryData = [];
let logSession = null;
let orderWarehouses = [],orderWarehouseKey = '',orderRenderKey = '';
let orderProgressOptionsKey = '';
const headingLabels = {N:'↑ Lên',E:'→ Phải',S:'↓ Xuống',W:'← Trái'};
const headingAngles = {N:-90,E:0,S:90,W:180};
const orderExpanded = new Map();
const pendingControls = new Set();
const seenLogs = new Set(), recentLogs = [];
const $ = id => document.getElementById(id);
const views = new Set(['overview','tasks','map','reports']);
let mapDockFrame = 0;
const robotMarkerMotion = new Map();
let robotMotionFrame = 0;
function encoderFresh(robot) {
  return dataFresh() && lastState?.mqtt_connected !== false && lastState?.storage_healthy !== false &&
    robot?.online === true && robot.position_valid !== false && !['ERROR','OFFLINE','UNLOCALIZED'].includes(robot.state) &&
    robot.odometry?.valid === true && typeof robot.odometry.age_ms === 'number' &&
    robot.odometry.age_ms + snapshotAge()*1000 <= (robot.odometry.stale_after_ms || 1000);
}
function robotSpeedText(robot) {
  return encoderFresh(robot) && typeof robot.speed_cm_s === 'number' && Number.isFinite(robot.speed_cm_s)
    ? `${Math.max(0,robot.speed_cm_s).toFixed(1)} cm/s` : '— cm/s';
}
function updateRobotMotionLabels() {
  document.querySelectorAll('[data-robot-speed]').forEach(el=>{
    const robot = lastState?.fleet.agvs.find(a=>a.id === el.dataset.robotSpeed);
    el.textContent = robotSpeedText(robot);
  });
  const focus = singleWarehouseRobot() || selectedAgvId || '01';
  const robot = lastState?.fleet.agvs.find(a=>a.id === focus),estimate = robot?.estimated_position;
  $('robot01Speed').textContent = (lastState?.fleet.agvs||[]).filter(a=>['01','02'].includes(a.id))
    .map(a=>`Xe ${a.id}: ${robotSpeedText(a)}`).join(' · ') || 'Chờ dữ liệu tốc độ';
  $('robot01Speed').title = `Tốc độ ước tính từ hai encoder${robot?.odometry?.pulses_per_100cm ? `, hệ số thử ${robot.odometry.pulses_per_100cm} xung/100 cm` : ''}`;
  const live = encoderFresh(robot),held = robotMarkerMotion.get(focus)?.source === 'encoder_stale';
  $('robot01Position').textContent = live && estimate
    ? `Xe ${focus}: ${robotSpeedText(robot)} · ước tính ${estimate.from} → ${estimate.to} · ${finite(estimate.distance_cm).toFixed(1)} / ${finite(estimate.edge_length_cm).toFixed(1)} cm · RFID gần nhất: ${robot.node}`
    : held || robot?.encoder_supported && !live
      ? `Xe ${focus}: ${robotSpeedText(robot)} · ${live ? 'vị trí ước tính cuối, chờ chặng/RFID mới' : 'dữ liệu encoder cũ / chưa có, giữ vị trí hiển thị'} · RFID gần nhất: ${robot?.node || '—'}`
      : `Xe ${focus}: vị trí RFID ${robot?.node || '—'}${live ? ' · tốc độ encoder' : ' · chờ encoder'}`;
  if (robot?.obstacle_alert) {
    const alert = robot.obstacle_alert;
    $('robot01Position').textContent += ` · VẬT CẢN ${alert.direction === 'rear' ? 'PHÍA SAU' : 'PHÍA TRƯỚC'} ${finite(alert.range_cm).toFixed(1)} cm — đang dừng chờ`;
  }
}
function markerPoint(motion, now) {
  const t = motion.duration ? Math.min(1,Math.max(0,(now-motion.startedAt)/motion.duration)) : 1;
  return {x:motion.fromX+(motion.x-motion.fromX)*t,y:motion.fromY+(motion.y-motion.fromY)*t};
}
function robotMarkerPoint(robot, fallback, transform) {
  const now = performance.now(),estimate = robot.estimated_position;
  const context = `${lastState?.core_session_id}:${lastState?.map?.revision}:${robot.node}:${transform.scale}:${transform.ox}:${transform.oy}`;
  let motion = robotMarkerMotion.get(robot.id);
  if (motion?.context !== context) motion = null;
  const live = encoderFresh(robot) && estimate?.source === 'encoder' &&
    typeof estimate.x === 'number' && Number.isFinite(estimate.x) && typeof estimate.y === 'number' && Number.isFinite(estimate.y);
  if (live) {
    const x = estimate.x*transform.scale+transform.ox,y = estimate.y*transform.scale+transform.oy;
    const key = `${robot.odometry.boot_id}:${robot.odometry.connection_session}:${robot.odometry.seq}`;
    if (!motion || motion.key !== key) {
      const current = motion ? {x:motion.displayX,y:motion.displayY} : fallback;
      motion = {context,key,fromX:current.x,fromY:current.y,x,y,startedAt:now,
        duration:motion && motion.route === estimate.route_cmd_id ? 180 : 0,route:estimate.route_cmd_id,source:'encoder'};
    } else motion.source = 'encoder';
  } else if (motion && robot.encoder_supported && (motion.source.startsWith('encoder') || !encoderFresh(robot))) {
    motion.fromX = motion.x = motion.displayX;motion.fromY = motion.y = motion.displayY;
    motion.duration = 0;motion.source = motion.source.startsWith('encoder') ? 'encoder_stale' : 'rfid';
  } else motion = {context,fromX:fallback.x,fromY:fallback.y,x:fallback.x,y:fallback.y,startedAt:now,duration:0,source:'rfid'};
  const point = markerPoint(motion,now);motion.displayX = point.x;motion.displayY = point.y;
  robotMarkerMotion.set(robot.id,motion);
  return {...point,source:motion.source};
}
function animateRobotMarkers() {
  robotMotionFrame = 0;const now = performance.now();let active = false;
  for (const [id,motion] of robotMarkerMotion) {
    const group = Array.from(document.querySelectorAll('#agvsG [data-robot]')).find(el=>el.dataset.robot === id);
    const robot = lastState?.fleet.agvs.find(a=>a.id === id);
    if (!group || !robot) continue;
    if (!encoderFresh(robot) && motion.source.startsWith('encoder')) {
      motion.fromX = motion.x = motion.displayX;motion.fromY = motion.y = motion.displayY;
      motion.duration = 0;motion.source = 'encoder_stale';group.dataset.positionSource = motion.source;
    }
    const point = markerPoint(motion,now);motion.displayX = point.x;motion.displayY = point.y;
    group.setAttribute('transform',`translate(${point.x+finite(motion.offset)},${point.y})`);
    if (motion.duration && now-motion.startedAt < motion.duration && encoderFresh(robot)) active = true;
  }
  updateRobotMotionLabels();
  if (active) robotMotionFrame = requestAnimationFrame(animateRobotMarkers);
}
function scheduleMapDocking() {
  if (mapDockFrame) return;
  mapDockFrame = requestAnimationFrame(()=>{
    mapDockFrame = 0;
    const body = document.body,viewport = $('mapViewport'),main = $('main-container');
    const graph = $('nodesG').getBoundingClientRect(),canvas = viewport.getBoundingClientRect(),frame = main.getBoundingClientRect();
    const leftSpace = graph.left-frame.left-40,rightSpace = frame.right-graph.right-40;
    const docked = body.dataset.mapExpanded === 'true' && body.dataset.view !== 'reports' &&
      innerWidth > 1100 && viewport.dataset.zoom === 'fit' && graph.width > 0 && leftSpace >= 260 && rightSpace >= 260;
    body.dataset.mapDocked = String(docked);
    if (docked) {
      main.style.setProperty('--map-tools-top',`${canvas.top-frame.top}px`);
      main.style.setProperty('--map-tools-height',`${canvas.height}px`);
      main.style.setProperty('--map-left-width',`${Math.min(300,leftSpace)}px`);
      main.style.setProperty('--map-right-width',`${Math.min(280,rightSpace)}px`);
    }
  });
}
const finite = (value,fallback = 0) => typeof value === 'number' && Number.isFinite(value) ? value : fallback;
const labels = {IDLE:'Sẵn sàng',WORKING:'Đang chạy',LIFTING:'Đang nâng hàng',LOWERING:'Đang hạ hàng',CHARGING_EN_ROUTE:'Đi tới trạm sạc',CHARGING:'Ở trạm sạc',
  BLOCKED:'Chờ đường',ERROR:'Lỗi / cần xác minh',OFFLINE:'Mất telemetry',UNKNOWN:'Chưa xác định',UNLOCALIZED:'Chờ quét RFID',
  submitting:'Chờ core xác nhận',unknown:'Chưa xác định kết quả',pending:'Chờ phân công',dispatched:'Đã giao xe',
  completed:'Hoàn thành',cancelled:'Đã hủy',interrupted:'Gián đoạn',rejected:'Bị từ chối'};
const colors = {WORKING:'#3B82F6',LIFTING:'#F59E0B',LOWERING:'#F59E0B',IDLE:'#10B981',CHARGING:'#F59E0B',CHARGING_EN_ROUTE:'#F59E0B',
  ERROR:'#EF4444',BLOCKED:'#F97316',OFFLINE:'#64748B',UNKNOWN:'#64748B',UNLOCALIZED:'#F59E0B'};
function fmtDate(value) {
  if (!value) return '—';
  const date = new Date(/^\d{4}-\d\d-\d\d \d\d:/.test(value) ? value.replace(' ','T') + 'Z' : value);
  return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString('vi-VN');
}
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
  $('endShiftBtn').disabled = !controlsReady() || !shift?.supported || shift.phase !== 'RUNNING' || $('endShiftBtn').dataset.busy === 'true';
  updateWarehouseControls();
  updateWarehouseInventoryControls();
  renderWarehouseOperations(lastState);renderTransferForm();
  document.querySelectorAll('[data-map-edit]').forEach(button=>button.disabled = !canEditMap() || button.dataset.busy === 'true');
  $('mapEditHint').textContent = canEditMap() ? 'Có thể chỉnh cấu trúc bản đồ.' : 'Chỉnh cấu trúc khi core chưa có robot và nhiệm vụ. Vẫn có thể xem bản đồ đã lưu.';
  renderObstacleAction();renderSummary(lastState);
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
    closeMapPreview();renderEmpty();renderSystemLog();renderKpi(null);
    warehouseLatestReport=null;warehouseReportSelection=null;
    $('warehouseReportList').textContent='Chưa tải báo cáo kho.';$('warehouseReportDetail').replaceChildren();
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
    const response = await fetch(API_BASE + route,{method,credentials:'include',headers:body ? {'Content-Type':'application/json'} : undefined,body:body ? JSON.stringify(body) : undefined});
    const data = await response.json();
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
  if (!controlsReady()) {toast('Chờ dữ liệu mới từ core trước khi điều khiển','warn');return null;}
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
function renderFleet(agvs) {
  const tbody = document.querySelector('#fleetTable tbody');tbody.replaceChildren();
  if (!agvs.length) return emptyTable(tbody,5,'Chưa nhận telemetry từ robot');
  agvs.forEach(a=>{
    const name = action(a.id,()=>loadRobotHistory(a.id),null);name.title = 'Xem lịch sử robot';
    const node = document.createElement('div');node.textContent = a.position_valid === false ? `Chưa xác minh (${a.node || '—'})` : a.node || 'Chưa định vị';
    const destination = document.createElement('small');destination.textContent = a.target ? `Đích: ${a.target}` : 'Không có đích';node.append(document.createElement('br'),destination);
    const home = lastState?.shift?.home_stations?.[a.id];
    if (home) {const line = document.createElement('small');line.textContent = `Trạm riêng: ${home}`;node.append(document.createElement('br'),line);}
    node.title = ['Node đo gần nhất',...(a.path || [])].join(' → ');
    const battery = document.createElement('span');
    const batteryValue = a.battery_display_available === true ? a.battery_display_percent : a.battery;
    const batteryKnown = (a.battery_display_available === true || a.battery_measured === true) &&
      batteryValue != null && Number.isFinite(Number(batteryValue)) && Number(batteryValue) >= 0 && Number(batteryValue) <= 100;
    battery.textContent = a.last_seen_s == null ? 'Chưa có' : batteryKnown ? `${Math.round(Number(batteryValue))}%` : 'Chưa đo';
    battery.style.color = batteryKnown && Number(batteryValue) <= 20 ? colors.ERROR : colors.IDLE;
    if (a.battery_control_source === 'diagnostic') battery.title = 'Phần trăm diagnostic đã lọc; Core dùng cho ngưỡng pin điều phối';
    const state = document.createElement('div');state.textContent = labels[a.state] || a.state || 'Chưa xác định';state.style.color = colors[a.state] || colors.UNKNOWN;
    if(a.lift?.active&&a.lift.state==='PENDING')state.textContent='Đang gửi lệnh servo';
    if(a.virtual_cargo===true){const cargo=document.createElement('small');cargo.textContent=a.virtual_cargo_known?'1 kiện ảo':'1 kiện ảo (chưa xác minh lại)';state.append(document.createElement('br'),cargo);}
    const age = document.createElement('small');age.dataset.robotAge = a.id;state.append(document.createElement('br'),age);
    if (a.id === '01' || a.id === '02') {
      const speed = document.createElement('small');speed.dataset.robotSpeed = a.id;
      speed.textContent = robotSpeedText(a);state.append(document.createElement('br'),speed);
    }
    if (a.error) {const error = document.createElement('div');error.className = 'robot-error';error.textContent = a.error;state.append(error);}
    const actions = document.createElement('div');
    if (a.state === 'ERROR') actions.append(action('Xóa lỗi',button=>clearAgvError(a.id,button)));
    if (a.state === 'OFFLINE') actions.append(action('Gỡ xe',button=>removeAGV(a.id,button)));
    else if (['IDLE','WORKING','BLOCKED'].includes(a.state)) {const choose = action('Chọn',()=>selectAgv(a.id));choose.setAttribute('data-task-control','');actions.append(choose);}
    appendRow(tbody,[name,node,battery,state,actions]);
  });updateRobotAges();updateRobotMotionLabels();
}
function updateRobotAges() {
  document.querySelectorAll('[data-robot-age]').forEach(el=>{
    const robot = lastState?.fleet.agvs.find(a=>a.id === el.dataset.robotAge);
    if (!robot || robot.last_seen_s == null) {el.textContent = 'Chưa có tuổi telemetry';return;}
    const seconds = finite(robot.last_seen_s)+snapshotAge();
    el.textContent = `${Number.isFinite(seconds) ? Math.floor(seconds) + 's' : '?'} từ telemetry${dataFresh() ? '' : ' • dữ liệu cũ'}`;
  });
}
function renderTasks(tasks) {
  const tbody = document.querySelector('#taskTable tbody');tbody.replaceChildren();
  if (!tasks.length) return emptyTable(tbody,3,'Hàng đợi trống');
  tasks.forEach(t=>appendRow(tbody,[`${t.target} · ${t.task_id ? t.task_id.slice(0,8) : 'không có ID'}`,t.priority,
    action('Hủy',button=>cancelTask(t,button))]));
}
function renderTaskHistory(rows) {
  const tbody = document.querySelector('#taskHistoryTable tbody');tbody.replaceChildren();
  if (!rows.length) return emptyTable(tbody,4,'Chưa có nhiệm vụ');
  rows.forEach(t=>{
    const status = document.createElement('div');status.textContent = labels[t.status] || t.status;
    const details = document.createElement('small');details.textContent = t.reason || fmtDate(t.completed_at || t.created_at);status.append(document.createElement('br'),details);
    if (['pending','dispatched'].includes(t.status) && t.core_task_id)
      status.append(action('Hủy',button=>cancelTask({task_id:t.core_task_id,target:t.target_node},button)));
    const row = appendRow(tbody,[t.core_task_id ? t.core_task_id.slice(0,8) : `Cũ #${t.id}`,t.target_node,t.assigned_agv || '—',status]);
    row.title = t.core_task_id || 'Bản ghi cũ chưa có định danh core';
  });
}
function addOrderLine(kind,node = '',quantity = 1) {
  const box = $(kind === 'pickup' ? 'orderPickups' : 'orderDeliveries');
  const row = document.createElement('div');row.className = 'order-line';
  const select = document.createElement('select');select.setAttribute('aria-label',kind === 'pickup' ? 'Kho lấy' : 'Kho giao');
  for (const warehouse of orderWarehouses) select.add(new Option(warehouse,warehouse));
  if (node && orderWarehouses.includes(node)) select.value = node;
  else if (kind === 'dropoff' && orderWarehouses.length > 1) select.selectedIndex = 1;
  const count = document.createElement('input');count.type = 'hidden';count.value = '1';
  const remove = document.createElement('button');remove.type = 'button';remove.className = 'btn-outline';remove.textContent = '×';remove.title = 'Xóa kho';
  remove.setAttribute('aria-label','Xóa kho');remove.onclick = ()=>{row.remove();updateOrderTotals();};
  select.onchange = ()=>{select.dataset.initialized='true';updateOrderTotals();};
  row.append(select,count,remove);box.append(row);updateOrderTotals();
}
function populateOrderWarehouses(map) {
  const warehouses = (map?.nodes || []).filter(node=>node.type === 'station').map(node=>node.id);
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
    .map(row=>({[kind]:row.querySelector('select').value,quantity:Number(row.querySelector('input').value)}));
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
  if (result) {toast(`Đã tạo ${result.order.total} lượt chuyển kệ. Theo dõi tuyến của từng xe ở bảng phía trên.`);showView('tasks');}
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
  if(result)toast(`Xe ${robotId}: ${action==='pickup'?'lấy':'hạ'} tại ${result.action.warehouse}, ${result.action.servo?'servo 5s':'chờ mô phỏng 5s (chưa có servo)'} rồi lùi ra ${result.action.gate}`);
}
function renderWarehouseActionStatus() {
  const robotId=singleWarehouseRobot(),test=!!robotId;
  $('warehouseModeBtn').hidden=!test;$('orderModeBtn').hidden=test;
  if(test&&!$('orderTaskForm').hidden)setTaskMode('warehouse');
  if(!test&&!$('warehouseTaskForm').hidden)setTaskMode('order');
  const warehouses=(lastState?.map.nodes||[]).filter(node=>node.type==='station'&&/^K[1-6]$/.test(node.id)).map(node=>node.id);
  const select=$('warehouseTarget'),key=JSON.stringify(warehouses);
  if(select.dataset.nodes!==key){const prior=select.value;select.replaceChildren();for(const node of warehouses)select.add(new Option(node,node));if(warehouses.includes(prior))select.value=prior;select.dataset.nodes=key;}
  const action=lastState?.warehouse_actions?.find(action=>action.robot===robotId),robot=lastState?.fleet.agvs.find(robot=>robot.id===robotId);
  const names={dispatching:'Đang cấp tuyến tới kho',to_warehouse:'Đang đi tới kho',waiting_servo:'Chờ nhúng xác nhận dừng tại kho',dispatching_lift:'Đang gửi lệnh servo',handling:robot?.lift?.state==='PENDING'?'Chờ nhúng nhận lệnh':action?.action==='pickup'?'Đang nâng hàng (servo 5s)':'Đang hạ hàng (servo 5s)',dispatching_exit:'Đang cấp lệnh lùi',to_gate:'Đang lùi ra gate',completed:'Đã ra gate; chờ lệnh tiếp theo',interrupted:'Thao tác gián đoạn'};
  $('warehouseActionStatus').textContent=test&&!Array.isArray(lastState?.warehouse_actions)?'Cần khởi động lại gateway để nạp chức năng Lấy / hạ hàng.':action ? action.status==='waiting' ?
    `Dừng tại ${action.warehouse}: còn ${Math.max(0,Math.ceil((Date.parse(action.service_until)-Date.now())/1000))}s (mô phỏng, chưa điều khiển servo)` :
    `${names[action.status]||action.status} · ${action.warehouse} → ${action.gate}${action.reason?' · '+action.reason:''}` :
    robot?.state==='IDLE'?`Xe ${robotId} sẵn sàng. Chọn kho rồi bấm Lấy hàng hoặc Hạ hàng.`:`Xe ${robotId||'—'}: ${robot?.error||robot?.state||'chưa có dữ liệu'}`;
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
  const rows = (Array.isArray(orders) ? orders : []).filter(order=>order.status!=='closed'||$('orderShowResolved').checked);
  const select = $('orderProgressSelect');
  const optionsKey = JSON.stringify(rows.map(order=>[order.id,order.status,order.items.map(item=>[item.pickup,item.dropoff,item.robot])]));
  if (optionsKey !== orderProgressOptionsKey) {
    const previous = rows.find(order=>order.id === select.value);
    const selected = previous?.status === 'active' ? previous : rows.find(order=>order.status === 'active') || previous || rows[0];
    select.replaceChildren();
    if (!rows.length) select.add(new Option('Chưa có đơn hàng',''));
    else for (const order of rows) {
      const route=order.items.length===1?`${order.items[0].pickup} → ${order.items[0].dropoff}`:`${order.total} lượt chuyển kệ`;
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
  const robot01Scope=!!singleWarehouseRobot();
  $('orderList').closest('section').hidden=robot01Scope;
  const selected=lastState?.shift?.test_mode === 'robot01' ? '01' : lastState?.shift?.test_mode === 'robot02' ? '02' :
    lastState?.shift?.trial_robot_id || lastState?.fleet?.trial_robot_id;
  if ($('orderDispatchHint')) $('orderDispatchHint').textContent=selected ?
    `Phạm vi hiện tại: xe ${selected}.` :
    'Tự chọn xe sẵn sàng gần kho lấy nhất. Theo dõi xe và tuyến đã giao ở bảng phía trên.';
  renderOrderProgress(orders);
  const resolvedCount=orders.filter(order=>order.status==='closed').length;
  $('orderResolvedFilter').hidden=resolvedCount===0;
  $('orderResolvedFilterLabel').textContent=`Hiện đơn đã đối soát (${resolvedCount})`;
  const visibleOrders=orders.filter(order=>order.status!=='closed'||$('orderShowResolved').checked);
  const key = JSON.stringify([visibleOrders,resolvedCount]);
  if (key === orderRenderKey) return;
  orderRenderKey = key;
  const list = $('orderList');
  list.replaceChildren();
  if (!visibleOrders.length) {list.textContent = resolvedCount?'Không còn đơn đang xử lý. Các đơn đã đối soát được ẩn khỏi danh sách.':'Chưa có đơn hàng';return;}
  const names = {active:'Đang điều phối',completed:'Đã hoàn thành',needs_attention:'Cần đối soát',closed:'Đã đóng sau đối soát'};
  const phases = {queued:'Chờ xe',dispatching:'Đang cấp việc lấy',to_pickup:'Đi tới kệ nguồn',
    waiting_pickup_lift:'Chờ servo sẵn sàng để nâng',dispatching_pickup_lift:'Đang gửi lệnh nâng',pickup_lift:'Đang nâng kệ',
    ready_dropoff:'Chờ đưa kệ tới đích',dispatching_dropoff:'Đang cấp việc giao',to_dropoff:'Đưa kệ tới vị trí đích',
    waiting_dropoff_lift:'Chờ servo sẵn sàng để hạ',dispatching_dropoff_lift:'Đang gửi lệnh hạ',dropoff_lift:'Đang hạ kệ',
    ready_exit:'Chờ lùi ra gate',dispatching_exit:'Đang cấp lệnh lùi ra gate',to_exit:'Lùi ra gate',
    delivered:'Đã đặt kệ (chưa có cảm biến tải)',interrupted:'Gián đoạn'};
  for (const order of visibleOrders) {
    const entry = document.createElement('details');entry.className = 'order-entry';entry.dataset.orderId = order.id;
    entry.open = orderExpanded.has(order.id) ? orderExpanded.get(order.id) : order.status !== 'completed' && order.status !== 'closed';
    entry.addEventListener('toggle',()=>orderExpanded.set(order.id,entry.open));
    const delivered = order.items.filter(item=>item.status === 'delivered').length;
    const summary = document.createElement('summary');summary.textContent = `${delivered}/${order.total} kệ · ${names[order.status] || order.status}`;
    summary.title=`${fmtDate(order.created)} · ${order.id}`;
    const progress = document.createElement('progress');progress.max = order.total;progress.value = delivered;
    entry.append(summary,progress);
    if (order.reason) {const reason = document.createElement('div');reason.className = 'order-reason';reason.textContent = order.reason;entry.append(reason);}
    if (order.status === 'needs_attention') {
      const resolve = document.createElement('button');resolve.type = 'button';resolve.className = 'btn-outline';
      resolve.textContent = 'Đã đối soát';resolve.title = 'Chỉ xác nhận sau khi kiểm tra vị trí xe và kiện hàng';
      resolve.onclick = async()=>{
        if (!confirm(`Đã kiểm tra xe và kiện hàng của ${order.id}? Các kiện chưa giao sẽ không tự chạy lại.`)) return;
        const result = await api('POST',`/api/orders/${encodeURIComponent(order.id)}/resolve`,{physically_clear:true});
        if (result) toast('Đã đóng đơn sau đối soát');
      };
      entry.append(resolve);
    }
    for (const item of order.items) {
      const line = document.createElement('div');line.className = 'order-item';
      const heading=operationText(line,'','div','order-transfer-line');
      operationText(heading,`${item.pickup} → ${item.dropoff}`,'strong');
      operationText(heading,item.robot?`Xe ${item.robot}`:'Chờ phân xe','span');
      operationText(line,`${item.rack_id?`Kệ ${item.rack_id} · `:''}${phases[item.status] || item.status}`,'div','transfer-metadata');
      if (item.task_id) line.title = `Core task_id: ${item.task_id}`;
      if (item.waiting_reason) {
        const waiting=document.createElement('div');waiting.className='order-reason';waiting.textContent=item.waiting_reason;line.append(waiting);
      }
      const routeDetails=document.createElement('details');routeDetails.className='order-route-details';
      operationText(routeDetails,'Chi tiết đường đi','summary');
      if (item.route_plan) for (const [key,label] of [['pickup','Tuyến lấy dự kiến'],['delivery','Tuyến giao dự kiến'],['exit','Lùi sau khi giao']]) {
        const route=item.route_plan[key],detail=document.createElement('div');detail.className='order-service';
        detail.textContent=route?.path?.length ? `${label}: ${route.path.join(' → ')} · trọng số ${route.cost}` : `${label}: chưa có đường`;
        routeDetails.append(detail);
      }
      if(item.route_plan)line.append(routeDetails);
      if (item.status === 'pickup_service' || item.status === 'dropoff_service') {
        const timer = document.createElement('div');timer.className = 'order-service';
        timer.dataset.orderServiceUntil = item.service_until;timer.dataset.orderServiceKind = item.status === 'pickup_service' ? 'bốc hàng' : 'dỡ hàng';
        line.append(timer);
      }
      entry.append(line);
    }
    list.append(entry);
  }
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
      !['IDLE','WORKING','BLOCKED'].includes(a.state) || a.online === false || a.position_valid === false;
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
  renderWarehouse(state.warehouse);
  renderWarehouseOperations(state);
  renderShift(state.shift);
  if (state.fleet.accepting_tasks === false && selectedAgvId) cancelSelection();
  if (selectedAgvId && !state.fleet.agvs.some(a=>a.id === selectedAgvId && ['IDLE','WORKING','BLOCKED'].includes(a.state))) cancelSelection();
  renderMap(state.map,state.fleet.agvs);renderFleet(state.fleet.agvs);renderTasks(state.fleet.pending_tasks);
  renderMutex(state.mutex);renderTaskHistory(state.task_history || []);populateTaskAgvSelect(state.fleet.agvs);
  populateNodeOptions(state.map);populateOrderWarehouses(state.map);renderOrders(state.orders || []);renderObstacleAction();renderSummary(state);
  $('mapTitle').textContent = `${state.map.map_name || 'Bản đồ kho'} • phiên bản ${state.map.revision ?? '—'}`;
}
function renderEmpty() {
  renderWarehouse(null);
  renderWarehouseOperations(null);
  renderShift(null);
  renderFleet([]);renderTasks([]);renderTaskHistory([]);renderOrders([]);$('nodesG').replaceChildren();$('edgesG').replaceChildren();$('agvsG').replaceChildren();$('routesG').replaceChildren();$('sonarObstaclesG').replaceChildren();
  populateNodeOptions(null);renderSummary(null);renderObstacleAction();
}
function setSelectionText(text,picking) {$('selectionText').textContent = text;$('selectionBar').className = picking ? 'picking' : 'idle';$('cancelSelectBtn').style.display = picking ? 'inline-block' : 'none';}
function selectAgv(robotId) {
  if (!tasksReady()) return toast('Chờ bắt đầu ca và dữ liệu mới từ core','warn');
  const robot = lastState.fleet.agvs.find(a=>a.id === robotId);
  if (!robot || !['IDLE','WORKING','BLOCKED'].includes(robot.state) || robot.online === false || robot.position_valid === false) return;
  selectedAgvId = robotId;setTaskMode('node');setSelectionText(`Đã chọn ${robotId}. Bấm node đích để giao nhiệm vụ cho xe này.`,true);renderMap(lastState.map,lastState.fleet.agvs);
}
function cancelSelection() {
  selectedAgvId = null;setSelectionText('Bấm node để Core chọn xe gần nhất; bấm robot trước nếu muốn chỉ định xe. Vị trí là node RFID gần nhất.',false);
  if (lastState) renderMap(lastState.map,lastState.fleet.agvs);
}
async function onNodeClick(nodeId) {
  $('task_target').value = nodeId;$('obstacle_node').value = nodeId;setTaskMode('node');
  renderObstacleAction();
  const robotId = selectedAgvId || (lastState?.shift?.single_robot_trial === true ? '01' : '');
  if (selectedAgvId) cancelSelection();
  if (!robotId) $('task_agv').value = '';
  if (!tasksReady()) return toast('Ca chưa mở nhận nhiệm vụ','warn');
  const route = robotId ? `/api/agvs/${encodeURIComponent(robotId)}/goto` : '/api/tasks';
  const result = await control('POST',route,{target:nodeId,priority:Number($('task_priority').value)});
  if (!result) return;
  const assigned = robotId || lastState?.fleet?.tasks?.find(task=>task.task_id === result.task_id)?.assigned_agv;
  toast(`Core đã nhận nhiệm vụ ${result.task_id.slice(0,8)}${assigned ? ` cho xe ${assigned}` : ' · chờ xe khả dụng'}`);
  setSelectionText(`Nhiệm vụ tới ${nodeId} đã được Core nhận${assigned ? `; xe ${assigned} được chọn` : '; Core sẽ chọn xe gần nhất khi có xe khả dụng'}. Theo dõi tiến độ trong lịch sử.`,false);
}
async function addTask() {
  if (!tasksReady()) return toast('Ca chưa mở nhận nhiệm vụ','warn');
  const target = $('task_target').value.trim(),priority = Number($('task_priority').value),robot = $('task_agv').value;
  if (lastState?.shift?.single_robot_trial === true && robot !== '01')
    return toast('Chế độ thử một xe chỉ nhận GOTO trực tiếp xe 01','warn');
  const route = robot ? `/api/agvs/${encodeURIComponent(robot)}/goto` : '/api/tasks';
  const result = await control('POST',route,{target,priority},$('submitTaskBtn'));
  if (result) toast(`Core đã nhận nhiệm vụ ${result.task_id.slice(0,8)}`);
}
function renderShift(shift) {
  const names = {RUNNING:'Đang làm việc',CLOSING:'Đang hoàn tất task / đưa xe về trạm',CLOSED:'Đã tập kết tại trạm • chờ bắt đầu ca'};
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
    ? Object.entries(shift.home_stations).map(([robot,station])=>`${robot} → ${station}`).join(' · ') || 'Theo cấu hình core'
    : 'Theo cấu hình core';
  const list = $('homeStationList');list.replaceChildren();
  const reasons = {'Chua tham gia ca':'Chưa tham gia ca','Cho telemetry / RFID':'Chờ telemetry / RFID','Mat telemetry':'Mất telemetry',
    'Cho RFID hop le':'Chờ RFID hợp lệ','Can xac minh xe / xoa loi':'Cần xác minh / xóa lỗi',
    'Cho hoan thanh task hien tai':'Chờ hoàn thành task','Cho duong ve tram sac':'Chờ đường về trạm',
    'Cho xac nhan roi mutex zone':'Chờ xác nhận rời vùng mutex','Da xac minh tai tram':'Đã xác minh tại trạm'};
  for (const robot of shift?.robots || []) {
    const line = document.createElement('div');
    const reason = reasons[robot.reason] || (robot.reason?.startsWith('Dang ve tram ') ? `Đang về ${robot.home_station}` : robot.reason || '');
    line.textContent = `${robot.id} → ${robot.home_station}: ${reason}${robot.actual_node ? ` • RFID gần nhất: ${robot.actual_node}` : ''}`;
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
  const result = await control('POST',`/api/shift/${mode}`,{},button);
  if (result) toast(mode === 'start' ? 'Core đã mở ca làm việc' : 'Core đã nhận kết thúc ca; theo dõi xe về trạm');
}
async function cancelTask(task,button) {
  const payload = task.task_id ? {task_id:task.task_id} : {target:task.target};
  const result = await control('POST','/api/tasks/cancel',payload,button);
  if (result) toast(`Core đã hủy ${result.cancelled} nhiệm vụ; robot nhận lệnh chờ`);
}
async function clearAgvError(robotId,button) {
  if (!confirm('Chỉ xóa lỗi sau khi đã xác minh vị trí robot, đường đi và xử lý sự cố. Tiếp tục?')) return;
  if (await control('POST',`/api/agvs/${encodeURIComponent(robotId)}/clear-error`,{},button))
    toast(lastState?.shift?.supervised_trial ? `Đã gửi reset lỗi ${robotId}; chờ xe xác nhận khỏe tại trạm` : `Core đã xóa lỗi ${robotId}`);
}
async function removeAGV(robotId,button) {
  if (!confirm(`Đã đưa robot ${robotId} ra khỏi kho và xác minh vùng đường trống?`)) return;
  if (await control('DELETE',`/api/agvs/${encodeURIComponent(robotId)}`,undefined,button)) toast(`Đã gỡ ${robotId}`);
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
async function editNode(remove = false) {
  const node = $('edit_node_id').value.trim();
  const result = await control(remove ? 'DELETE' : 'POST',remove ? '/api/map/node/' + encodeURIComponent(node) : '/api/map/node',
    remove ? undefined : {id:node,x:Number($('edit_node_x').value),y:Number($('edit_node_y').value),type:$('edit_node_type').value},$(remove ? 'removeNodeBtn' : 'saveNodeBtn'));
  if (result) toast(remove ? 'Đã xóa node và các cạnh liên quan' : 'Đã cập nhật node');
}
async function editEdge(remove = false) {
  const body = {from:$('edit_edge_from').value.trim(),to:$('edit_edge_to').value.trim()};
  if (!remove) Object.assign(body,{weight:Number($('edit_edge_weight').value),two_way:$('edit_edge_two_way').checked,type:$('edit_edge_type').value,path_class:$('edit_edge_class').value.trim()});
  if (await control(remove ? 'DELETE' : 'POST','/api/map/edge',body,$(remove ? 'removeEdgeBtn' : 'saveEdgeBtn'))) toast(remove ? 'Đã xóa cạnh (cả hai hướng nếu là cạnh hai chiều)' : 'Đã cập nhật cạnh');
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
const SVG_NS = 'http://www.w3.org/2000/svg';
function svgElement(tag,attributes,text) {
  const element = document.createElementNS(SVG_NS,tag);
  Object.entries(attributes).forEach(([key,value])=>element.setAttribute(key,value));
  if (text != null) element.textContent = text;return element;
}
function computeMapTransform(nodes) {
  if (!nodes.length) return {scale:1,ox:80,oy:80};
  const xs = nodes.map(n=>finite(n.x)),ys = nodes.map(n=>finite(n.y));
  const minX = Math.min(...xs),minY = Math.min(...ys),width = Math.max(...xs)-minX || 1,height = Math.max(...ys)-minY || 1;
  const scale = Math.min(900/width,600/height);
  return {scale,ox:80+(900-width*scale)/2-minX*scale,oy:80+(600-height*scale)/2-minY*scale};
}
function roundedEdgePath(a,via,b,clearance = 0) {
  const first=Math.hypot(via.x-a.x,via.y-a.y),last=Math.hypot(b.x-via.x,b.y-via.y);
  const ux=(via.x-a.x)/first,uy=(via.y-a.y)/first,vx=(b.x-via.x)/last,vy=(b.y-via.y)/last;
  const radius=Math.min(30,first/4,last/4);
  return `M ${a.x} ${a.y} L ${via.x-ux*radius} ${via.y-uy*radius} Q ${via.x} ${via.y} ${via.x+vx*radius} ${via.y+vy*radius} L ${b.x-vx*clearance} ${b.y-vy*clearance}`;
}
function drawMap(map,agvs,prefix = '') {
  const nodesG = $(prefix ? 'previewNodesG' : 'nodesG'),edgesG = $(prefix ? 'previewEdgesG' : 'edgesG');
  nodesG.replaceChildren();edgesG.replaceChildren();
  if (!prefix) $('sonarObstaclesG').replaceChildren();
  const nodes = Array.isArray(map.nodes) ? map.nodes : [],edges = Array.isArray(map.edges) ? map.edges : [];
  const {scale,ox,oy} = computeMapTransform(nodes),position = new Map();
  nodes.forEach(n=>position.set(n.id,{x:finite(n.x)*scale+ox,y:finite(n.y)*scale+oy}));
  const detections = prefix ? [] : agvs.filter(robot=>robot.online && robot.obstacle_detection &&
    Number.isFinite(robot.obstacle_detection.x) && Number.isFinite(robot.obstacle_detection.y));
  const seenEdges = new Set();
  edges.forEach(e=>{
    const a = position.get(e.from),b = position.get(e.to);if (!a || !b) return;
    const key = e.two_way ? [e.from,e.to].sort().join('\n') : `${e.from}\n${e.to}`;
    if (seenEdges.has(key)) return;seenEdges.add(key);
    const blockedBySonar = detections.some(robot=>{
      const d = robot.obstacle_detection;
      return d.from === e.from && d.to === e.to || e.two_way && d.from === e.to && d.to === e.from;
    });
    const lineAttrs = {x1:a.x,y1:a.y,x2:b.x,y2:b.y,class:blockedBySonar ? 'edge sonar-obstacle-edge' : 'edge'};
    if (blockedBySonar) Object.assign(lineAttrs,{'data-sonar-obstacle-edge':`${e.from}:${e.to}`});
    const via=e.geometry?.via ? {x:e.geometry.via.x*scale+ox,y:e.geometry.via.y*scale+oy} : null;
    const line = via ? svgElement('path',{class:lineAttrs.class,fill:'none',d:roundedEdgePath(a,via,b,prefix ? 19 : 23),
      'data-rounded-edge':`${e.from}:${e.to}`}) : svgElement('line',lineAttrs);
    if (blockedBySonar) line.setAttribute('data-sonar-obstacle-edge',`${e.from}:${e.to}`);
    const length = Math.hypot(b.x-a.x,b.y-a.y) || 1,dx = (b.x-a.x)/length,dy = (b.y-a.y)/length;
    const clearance = prefix ? 19 : 23;
    if (!via) {line.setAttribute('x2',b.x-dx*clearance);line.setAttribute('y2',b.y-dy*clearance);}
    if (e.two_way) {line.setAttribute('x1',a.x+dx*clearance);line.setAttribute('y1',a.y+dy*clearance);}
    if (e.two_way) line.setAttribute('marker-start','url(#arrow)');
    line.append(svgElement('title',{},`${e.from} ${e.two_way ? '↔' : '→'} ${e.to}; trọng số ${e.weight}`));edgesG.append(line);
    const parts=via ? [[a,via,e.geometry.segment_lengths_cm[0]],[via,b,e.geometry.segment_lengths_cm[1]]] : [[a,b,e.weight]];
    parts.forEach(([start,end,label])=>{
      const horizontal=Math.abs(end.x-start.x)>Math.abs(end.y-start.y);
      edgesG.append(svgElement('text',{x:(start.x+end.x)/2+(horizontal ? 0 : 10),
        y:(start.y+end.y)/2+(horizontal ? -10 : 4),'text-anchor':horizontal ? 'middle' : 'start',class:'edge-label'},label));
    });
  });
  detections.forEach(robot=>{
    const d = robot.obstacle_detection,x=d.x*scale+ox,y=d.y*scale+oy;
    const dot = svgElement('circle',{cx:x,cy:y,r:8,fill:'#EF4444',stroke:'#FEE2E2','stroke-width':2,
      'data-sonar-obstacle':robot.id});
    dot.append(svgElement('title',{},`Xe ${robot.id}: vật cản ${d.direction === 'rear' ? 'phía sau' : 'phía trước'}, đo ${d.range_cm.toFixed(1)} cm; vị trí ước tính theo encoder`));
    $('sonarObstaclesG').append(dot);
  });
  const obstacles = new Set(map.obstacles || []);
  const nodeTypes = {mutex:'RFID / vùng chung',station:'Kho',charging:'Trạm sạc',waypoint:'Điểm đường / cửa'};
  nodes.forEach(n=>{
    const p = position.get(n.id),stroke = obstacles.has(n.id) ? '#EF4444' : ({mutex:'#F97316',charging:'#22C55E',station:'#38BDF8'}[n.type] || '#475569');
    const circle = svgElement('circle',{cx:p.x,cy:p.y,r:prefix ? 16 : 20,fill:obstacles.has(n.id) ? '#7F1D1D' : '#1E293B',stroke,'stroke-width':2,class:'node-clickable'});
    circle.setAttribute('data-node',n.id);
    circle.append(svgElement('title',{},`${n.id}; ${n.type}; ${nodeTypes[n.type] || n.type}; (${n.x}, ${n.y})`));
    if (!prefix) circle.onclick = ()=>onNodeClick(n.id);
    // Compact IDs fit inside their node, leaving branch arrows/weights unobstructed.
    const compact = String(n.id).length <= 4;
    nodesG.append(circle,svgElement('text',{x:compact ? p.x : p.x+25,y:p.y+5,
      'text-anchor':compact ? 'middle' : 'start',class:'node-label','data-node-label':n.id},n.id));
  });
  // Frame the rendered graph rather than a fixed canvas with large empty margins.
  // This changes only the camera; warehouse coordinates and edge weights stay intact.
  const svg = $(prefix ? 'mapPreviewSvg' : 'mapSvg');
  if (nodes.length) {
    const points = nodes.map(n=>({...position.get(n.id),label:String(n.id)}));
    const left = Math.min(...points.map(p=>p.x-16)),top = Math.min(...points.map(p=>p.y-16));
    const right = Math.max(...points.map(p=>p.x+(p.label.length<=4 ? 20 : 29+p.label.length*(prefix ? 8 : 10))));
    const bottom = Math.max(...points.map(p=>p.y+16));
    svg.setAttribute('viewBox',`${left-40} ${top-48} ${right-left+80} ${bottom-top+96}`);
  } else svg.setAttribute('viewBox','20 20 1020 720');
  if (prefix) return;
  const routesG = $('routesG'),agvsG = $('agvsG');routesG.replaceChildren();agvsG.replaceChildren();
  agvs.forEach((robot,index)=>{
    const color = selectedAgvId === robot.id ? '#FDE047' : index % 2 ? '#C084FC' : '#38BDF8';
    const path = [robot.node,...(robot.path || [])];
    for (let i = 1;i < path.length;i++) {
      const a = position.get(path[i-1]),b = position.get(path[i]);if (!a || !b) continue;
      const edge=edges.find(e=>e.from===path[i-1] && e.to===path[i]);
      const via=edge?.geometry?.via;
      const attrs={stroke:color,'stroke-width':5,opacity:0.7,'stroke-dasharray':'8 4','data-robot-route':robot.id};
      const segment = via ? svgElement('path',{...attrs,fill:'none',d:roundedEdgePath(a,{x:via.x*scale+ox,y:via.y*scale+oy},b)}) :
        svgElement('line',{...attrs,x1:a.x,y1:a.y,x2:b.x,y2:b.y});
      segment.append(svgElement('title',{},`Kế hoạch ${robot.id}: ${path[i-1]} → ${path[i]}`));routesG.append(segment);
    }
    const p = position.get(robot.node);if (!p || robot.position_valid === false) {robotMarkerMotion.delete(robot.id);return;}
    const display = robotMarkerPoint(robot,p,{scale,ox,oy});
    const siblings = agvs.filter(a=>a.node === robot.node),offset = siblings.length > 1 ? siblings.indexOf(robot)*20-10 : 0;
    robotMarkerMotion.get(robot.id).offset = offset;
    if (display.source.startsWith('encoder')) agvsG.append(svgElement('circle',{cx:p.x,cy:p.y,r:7,fill:'none',stroke:color,opacity:0.55,'data-rfid-anchor':robot.id,'pointer-events':'none'}));
    const group = svgElement('g',{transform:`translate(${display.x+offset},${display.y})`,class:'agv-group',
      'data-robot':robot.id,'data-position-source':display.source,'pointer-events':'bounding-box'});
    group.append(svgElement('circle',{cx:0,cy:0,r:14,fill:colors[robot.state] || '#64748B',stroke:color,
      'stroke-width':selectedAgvId === robot.id ? 4 : 2,'stroke-dasharray':display.source.startsWith('encoder') ? '4 2' : 'none'}),
      svgElement('text',{x:-14,y:-43,class:'agv-label'},robot.id),svgElement('title',{},
        `${robot.id}: ${labels[robot.state] || robot.state}; ${display.source.startsWith('encoder') ? 'vị trí ước tính encoder; ' : ''}RFID gần nhất ${robot.node}; ${robotSpeedText(robot)}; đầu xe ${robot.heading_supported && headingLabels[robot.heading] || 'chưa biết'}`));
    if(robot.virtual_cargo===true){
      const cargo=svgElement('g',{'data-virtual-cargo':robot.id,opacity:robot.virtual_cargo_known&&robot.online!==false?1:0.45,'pointer-events':'none'});
      cargo.append(svgElement('rect',{x:-7,y:-8,width:14,height:14,rx:1,fill:'#D99A46',stroke:'#5B3715','stroke-width':1.5}),
        svgElement('path',{d:'M-7,-4 L7,-4 M0,-8 L0,-4',stroke:'#FFE0A3','stroke-width':2,fill:'none'}),
        svgElement('title',{},'Kiện ảo theo chu kỳ servo; không phải cảm biến xác nhận hàng thật'));
      group.append(cargo);
    }
    if (robot.online && robot.heading_supported && headingLabels[robot.heading])
      group.append(svgElement('path',{d:'M18,-6 L29,-6 L29,-10 L40,0 L29,10 L29,6 L18,6 Z',
        transform:`rotate(${headingAngles[robot.heading]})`,fill:color,stroke:'#0B1220','stroke-width':1.5,
        'data-robot-heading':robot.id,'data-heading':robot.heading}));
    else group.append(svgElement('text',{x:21,y:5,fill:'#94A3B8','font-size':16,'data-heading-unknown':robot.id},'?'));
    group.onclick = event=>{event.stopPropagation();selectAgv(robot.id);};agvsG.append(group);
  });
  for (const id of robotMarkerMotion.keys()) if (!agvs.some(a=>a.id === id)) robotMarkerMotion.delete(id);
  if (robotMotionFrame) cancelAnimationFrame(robotMotionFrame);
  robotMotionFrame = requestAnimationFrame(animateRobotMarkers);
}
function renderMap(map,agvs) {drawMap(map,agvs);scheduleMapDocking();}
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
function fmtKpiDuration(seconds) {
  if (seconds == null) return '—';const n = Math.max(0,Math.round(finite(seconds)));
  return n < 60 ? `${n}s` : `${Math.floor(n/60)}p ${n%60}s`;
}
function renderKpi(report) {
  const box = $('kpiBox');currentKpiId = report?.id ?? null;
  if (!report) {box.textContent = 'Chưa có báo cáo. Bấm Tạo báo cáo khi cần.';return;}
  box.replaceChildren();
  const line = (tag,text,className='')=>{const item=document.createElement(tag);item.textContent=text;item.className=className;box.append(item);return item;};
  line('div',`Báo cáo #${finite(report.id)} · ${fmtDate(report.created_at)}`);
  line('div',`${fmtDate(report.period_start)} → ${fmtDate(report.period_end)}`,'log-time');
  line('p',`Hoàn thành: ${finite(report.completed_tasks)} / ${finite(report.total_tasks)} nhiệm vụ tạo trong ca`);
  line('p',`Thời gian hoàn thành TB: ${fmtKpiDuration(report.avg_completion_seconds)}`);
  line('div',`Chờ gán xe: ${fmtKpiDuration(report.avg_queue_seconds)} · Từ gán đến hoàn thành: ${fmtKpiDuration(report.avg_travel_seconds)}`);
  line('p',`Chờ: ${finite(report.pending_tasks)} · Đã giao: ${finite(report.dispatched_tasks)} · Đã hủy: ${finite(report.cancelled_tasks)}`);
  line('p',`Hoàn thành trong ca (kể cả nhiệm vụ ca trước): ${finite(report.completed_in_period)} · Gián đoạn: ${finite(report.interrupted_tasks)} · Bị từ chối: ${finite(report.rejected_tasks)}`);
  line('div',`Sự cố xe: ${finite(report.agv_error_events)} · Vật cản: ${finite(report.obstacle_events)}`);
  Object.entries(report.agv_utilization || {}).forEach(([robot,u])=>{
    const title = document.createElement('p');title.textContent = `${robot} • thời gian có dữ liệu: ${fmtKpiDuration(u.known_seconds)}`;box.append(title);
    const bar = document.createElement('div');bar.className = 'utilization-bar';
    (u.breakdown || []).forEach(b=>{
      const segment = document.createElement('div');segment.style.width = Math.max(0,Math.min(100,finite(b.percent))) + '%';segment.style.background = colors[b.state] || '#64748B';
      segment.title = `${labels[b.state] || b.state}: ${finite(b.percent)}%`;bar.append(segment);
    });box.append(bar);
  });
  document.querySelectorAll('[data-kpi-id]').forEach(el=>el.classList.toggle('active',Number(el.dataset.kpiId) === currentKpiId));
}
async function loadLatestKpi() {renderKpi(await api('GET','/api/kpi/latest',undefined,true));}
async function generateKpi() {const result = await api('POST','/api/kpi/generate',{});if (result?.report) renderKpi(result.report);loadKpiHistory();}
async function loadKpiHistory() {
  const rows = await api('GET','/api/kpi/history?limit=20');if (!Array.isArray(rows)) return;kpiHistoryData = rows;
  const box = $('kpiHistoryBox');box.replaceChildren();
  rows.forEach(report=>{
    const item = document.createElement('button');item.className = 'log-item kpi-row';item.dataset.kpiId = report.id;
    item.textContent = `#${report.id} · ${fmtDate(report.created_at)} · ${report.completed_tasks}/${report.total_tasks} hoàn thành`;
    item.onclick = ()=>renderKpi(report);box.append(item);
  });if (!rows.length) box.textContent = 'Chưa có báo cáo đã lưu';
}
function showKpiAlert(report) {
  $('kpiAlertBar').style.display = 'block';$('kpiAlertBar').textContent = `Mất liên kết core lúc ${fmtDate(report.created_at)}. Dữ liệu robot đang hiển thị là dữ liệu cũ. Đã lưu báo cáo #${report.id}.`;
  renderKpi(report);loadKpiHistory();
}
async function loadRobotHistory(robotId) {
  const rows = await api('GET',`/api/agvs/${encodeURIComponent(robotId)}/history?limit=30`);if (!Array.isArray(rows)) return;
  $('robotHistoryTitle').textContent = `Lịch sử ${robotId}`;
  const tbody = document.querySelector('#robotHistoryTable tbody');tbody.replaceChildren();
  rows.forEach(r=>appendRow(tbody,[fmtDate(r.created_at),r.node,r.battery == null ? '—' : `${r.battery}%`,labels[r.state] || r.state]));
  if (!rows.length) emptyTable(tbody,4,'Chưa có dữ liệu');$('robotHistoryPanel').hidden = false;showView('tasks');
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
setInterval(()=>{updateConnection();updateRobotAges();},1000);
setInterval(()=>{if (robotMotionFrame) cancelAnimationFrame(robotMotionFrame);animateRobotMarkers();},200);
setInterval(updateOrderTimers,1000);
