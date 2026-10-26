'use strict';

const dashboardVehicleIds = ['01', '02'];
const vehicleCards = new Map();
const seenVehicleIds = new Set();
const removedVehicleIds = new Set();
let vehicleCoreSession = null;

function vehicleParticipates(robot, state = lastState) {
  const shift = state?.shift;
  return robot.participating !== false &&
    !(shift?.single_robot_trial && robot.id !== '01') &&
    !(shift?.trial_robot_id && robot.id !== shift.trial_robot_id) &&
    !(shift?.operation_profile === 'scoped' && !(shift.active_robot_ids || []).includes(robot.id));
}

function vehicleCanSelect(robot) {
  return !!robot && vehicleParticipates(robot) && robot.online === true && robot.position_valid !== false &&
    !robot.cancel_waiting_for_node && !robot.replan_waiting_for_node &&
    ['IDLE', 'WORKING', 'BLOCKED'].includes(robot.state);
}

function vehicleBattery(robot) {
  const value = robot.battery_display_available === true ? robot.battery_display_percent : robot.battery;
  const known = (robot.battery_display_available === true || robot.battery_measured === true) &&
    value != null && Number.isFinite(Number(value)) && Number(value) >= 0 && Number(value) <= 100;
  return {text: known ? `${Math.round(Number(value))}%` : 'Chưa đo', low: known && Number(value) <= 20};
}

function vehicleCardKey(state, robot, id, stale) {
  const assignment = robot && robotTransfer(state, robot);
  const item = assignment?.item;
  const action = robot && !assignment ? (state.warehouse_actions || []).find(candidate =>
    candidate.robot === id && !['completed', 'interrupted'].includes(candidate.status) &&
    (!robot.task_id || candidate.task_id === robot.task_id)) : null;
  const rack = state?.warehouse?.racks?.find(candidate => candidate.location.type === 'robot' && candidate.location.id === id);
  // Fast encoder values and ages are updated in place, without replacing card content.
  const view = robot ? {
    id, node: robot.node, state: robot.state, error: robot.error, online: robot.online,
    position_valid: robot.position_valid, participating: vehicleParticipates(robot, state),
    task_id: robot.task_id, target: robot.target, path: robot.path,
    battery: vehicleBattery(robot), battery_control_source: robot.battery_control_source,
    lift_pending: robot.lift?.active && robot.lift.state === 'PENDING',
    virtual_cargo: robot.virtual_cargo, virtual_cargo_known: robot.virtual_cargo_known,
    cancel_waiting_for_node: robot.cancel_waiting_for_node, replan_waiting_for_node: robot.replan_waiting_for_node,
    encoder_session_limit_reached: robot.encoder_session_limit_reached
  } : null;
  return JSON.stringify([view, stale, state?.core_ready, state?.shift?.home_stations?.[id],state?.location_names?.revision,
    assignment && [assignment.order.status, assignment.order.reason, assignment.order.deleted_at, item],
    action && [action.action, action.warehouse, action.status],
    (state?.fleet?.tasks || []).find(task => robot?.task_id && task.task_id === robot.task_id)?.target,
    rack && [rack.id, rack.status], seenVehicleIds.has(id), removedVehicleIds.has(id)]);
}

function renderVehicleTelemetry(card, robot, state) {
  const metadata = operationText(card, '', 'div', 'vehicle-telemetry');
  const battery = vehicleBattery(robot);
  const batteryLabel = operationText(metadata, `Pin: ${battery.text}`, 'span', 'vehicle-battery');
  batteryLabel.dataset.low = String(battery.low);
  if (robot.battery_control_source === 'diagnostic') batteryLabel.title = 'Phần trăm pin từ diagnostic đã lọc';
  const speed = operationText(metadata, robotSpeedText(robot), 'span');
  speed.dataset.robotSpeed = robot.id;
  const home = state.shift?.home_stations?.[robot.id];
  if (home) operationText(card, `Trạm riêng: ${locationText(home)}`, 'p', 'transfer-metadata');
  const age = operationText(card, '', 'p', 'transfer-metadata');
  age.dataset.robotAge = robot.id;
  if (robot.cancel_waiting_for_node || robot.replan_waiting_for_node) {
    const wait = operationText(card, '', 'p', 'transfer-alert');
    wait.dataset.robotWait = robot.id;
  }
  if (robot.encoder_session_limit_reached) {
    operationText(card, 'Đã vượt giới hạn phiên encoder. Đối soát xe rồi khởi động lại Core.', 'p', 'transfer-alert');
  }
  if (!vehicleParticipates(robot, state)) operationText(card, 'Ngoài phạm vi vận hành', 'p', 'transfer-metadata');
}

function renderVehicleActions(card, id, robot) {
  const actions = operationText(card, '', 'div', 'vehicle-actions');
  const history = action('Lịch sử', () => loadRobotHistory(id), null);
  history.title = `Xem lịch sử xe ${id}`;
  actions.append(history);
  if (!robot) return;
  for (const [name, text, handler] of [
    ['select', 'Chọn xe', () => selectAgv(id)],
    ['clear', 'Xóa lỗi', button => clearAgvError(id, button)],
    ['remove', 'Gỡ khỏi Core', button => removeAGV(id, button)]
  ]) {
    if (name === 'select' && !['IDLE', 'WORKING', 'BLOCKED'].includes(robot.state)) continue;
    if (name === 'clear' && robot.state !== 'ERROR') continue;
    if (name === 'remove' && robot.state !== 'OFFLINE') continue;
    const button = action(text, handler);
    button.dataset.vehicleAction = name;
    button.dataset.vehicleId = id;
    if (name === 'select') button.setAttribute('data-task-control', '');
    actions.append(button);
  }
}

function updateVehicleCardControls() {
  for (const {element} of vehicleCards.values()) {
    for (const button of element.querySelectorAll('[data-vehicle-action]')) {
      const robot = lastState?.fleet?.agvs?.find(candidate => candidate.id === button.dataset.vehicleId);
      const kind = button.dataset.vehicleAction;
      const allowed = kind === 'select' ? tasksReady() && vehicleCanSelect(robot) :
        controlsReady() && !!robot && (kind === 'clear' ? robot.state === 'ERROR' : robot.state === 'OFFLINE');
      button.disabled = !allowed || button.dataset.busy === 'true' ||
        pendingControls.has(`${kind === 'remove' ? 'DELETE' : 'POST'}/api/agvs/${button.dataset.vehicleId}${kind === 'clear' ? '/clear-error' : ''}`);
    }
  }
}

function renderVehiclePlaceholder(card, id, state, stale) {
  const header = operationText(card, '', 'div', 'transfer-card-heading');
  operationText(header, `Xe ${id}`, 'strong');
  const status = !state || stale ? 'Chờ dữ liệu Core' : removedVehicleIds.has(id) ? 'Đã gỡ khỏi Core' :
    seenVehicleIds.has(id) ? 'Không có trong Core' : 'Chưa kết nối';
  operationBadge(header, status, stale ? 'warn' : '');
  operationText(card, 'RFID: — · Pin: —', 'p', 'transfer-metadata');
  operationText(card, 'Chờ telemetry mới từ xe.', 'p', 'transfer-metadata');
  const rack = state?.warehouse?.racks?.find(candidate => candidate.location.type === 'robot' && candidate.location.id === id);
  if (rack) operationText(card, `Sổ kho: ${rack.id} trên xe · cần đối soát`, 'p', 'transfer-alert');
  renderVehicleActions(card, id, null);
}

function markVehicleRemoved(id, session) {
  if (session === lastState?.core_session_id) removedVehicleIds.add(id);
}

function renderVehicleCards(state, stale) {
  if (!state || vehicleCoreSession !== state.core_session_id) {
    seenVehicleIds.clear();
    removedVehicleIds.clear();
    vehicleCoreSession = state?.core_session_id ?? null;
  }
  const parent = $('robotTransferCards');
  for (const id of dashboardVehicleIds) {
    const robot = state?.fleet?.agvs?.find(candidate => candidate.id === id);
    if (robot) {
      seenVehicleIds.add(id);
      if (robot.online === true) removedVehicleIds.delete(id);
    }
    let entry = vehicleCards.get(id);
    if (!entry) {
      const element = document.createElement('article');
      element.className = 'robot-transfer-card';
      element.dataset.robotTransfer = id;
      element.setAttribute('aria-label', `Xe ${id}`);
      parent.append(element);
      entry = {element, key: null};
      vehicleCards.set(id, entry);
    }
    const key = vehicleCardKey(state, robot, id, stale);
    if (entry.key === key) continue;
    entry.key = key;
    entry.element.replaceChildren();
    entry.element.dataset.tone = '';
    if (robot) renderRobotTransfer(state, robot, entry.element, stale);
    else renderVehiclePlaceholder(entry.element, id, state, stale);
  }
  updateRobotAges();
  updateRobotMotionLabels();
  updateVehicleCardControls();
}
