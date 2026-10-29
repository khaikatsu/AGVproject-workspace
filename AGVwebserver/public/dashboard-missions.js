'use strict';

const missionExpanded = new Set();
let missionRenderKey = '';
let missionFilter = 'all';
const missionOrderNames = {active: 'Đang điều phối', completed: 'Đã hoàn thành', needs_attention: 'Cần đối soát', closed: 'Đã đóng sau đối soát'};
const missionPhases = {queued: 'Chờ xe', dispatching: 'Đang cấp việc lấy kệ', to_pickup: 'Đi tới kệ nguồn',
  waiting_pickup_lift: 'Chờ nâng kệ', dispatching_pickup_lift: 'Đang gửi lệnh nâng', pickup_lift: 'Đang nâng kệ',
  ready_dropoff: 'Chuẩn bị giao kệ', dispatching_dropoff: 'Đang cấp tuyến giao', to_dropoff: 'Đưa kệ tới vị trí đích',
  waiting_dropoff_lift: 'Chờ hạ kệ', dispatching_dropoff_lift: 'Đang gửi lệnh hạ', dropoff_lift: 'Đang hạ kệ',
  ready_exit: 'Chờ ra cửa kho', dispatching_exit: 'Đang cấp lệnh ra cửa', to_exit: 'Đang ra cửa kho',
  delivered: 'Đã đặt kệ', interrupted: 'Gián đoạn'};
const missionLegNames = {pickup: 'Tới kho lấy', dropoff: 'Giao kệ', exit: 'Ra cửa kho'};

function setMissionFilter(filter) {
  if (!['all', 'active', 'done', 'attention'].includes(filter)) return;
  missionFilter = filter;
  renderMissions(lastState);
}

function missionCell(row, label, className) {
  const cell = document.createElement('td');
  cell.dataset.label = label;
  cell.className = className || '';
  row.append(cell);
  return cell;
}

function missionText(parent, text, tag = 'span', className = '') {
  const element = document.createElement(tag);
  element.textContent = text;
  element.className = className;
  parent.append(element);
  return element;
}

function missionBadge(parent, text, tone) {
  const badge = missionText(parent, text, 'span', 'mission-badge');
  badge.dataset.tone = tone;
  return badge;
}

function missionTaskCanCancel(task) {
  return task.core_task_id && ['pending', 'dispatched'].includes(task.status) && task.core_session === lastState?.core_session_id;
}

function missionTaskActions(parent, task) {
  if(task.resume_available && !task.order_id)parent.append(action('Tiếp tục',button=>resumeStandaloneTask(task,button)));
  if (missionTaskCanCancel(task)) {
    const cancel = action('Hủy', button => cancelTask({task_id: task.core_task_id, target: task.target_node}, button));
    cancel.title = `Hủy chặng ${task.core_task_id}`;
    parent.append(cancel);
  }
}

function renderMissionTaskDetail(parent, task) {
  const block = missionText(parent, '', 'div', 'mission-task-detail');
  missionText(block, task.order_phase ? missionLegNames[task.order_phase] || task.order_phase : 'Chi tiết di chuyển', 'strong');
  missionText(block, `${labels[task.status] || task.status} · Xe ${task.assigned_agv || '—'} · Đích ${missionLocationText(task.target_node,task.location_names)}`, 'p');
  if (task.reason) missionText(block, task.reason, 'p', 'mission-warning');
  missionText(block, `Task: ${task.core_task_id || `Bản ghi cũ #${task.id}`}`, 'code');
  missionText(block, `Phiên: ${task.core_session || 'Dữ liệu cũ'} · ${fmtDate(task.created_at)}`, 'small');
  const actions = missionText(block, '', 'div', 'mission-actions');
  missionTaskActions(actions, task);
  if (task.id != null) actions.append(recordDeleteButton('history', task.id));
}

function renderMissionOrderDetail(parent, row) {
  const order = row.order;
  const intro = missionText(parent, '', 'div', 'mission-detail-intro');
  missionText(intro, `${row.delivered}/${row.total} kệ · ${missionOrderNames[order.status] || order.status}`, 'strong');
  missionText(intro, `Tạo lúc ${fmtDate(order.created)}`, 'small');
  if (order.reason) missionText(parent, order.reason, 'p', 'mission-warning');
  const items = missionText(parent, '', 'div', 'mission-item-grid');
  for (const item of order.items || []) {
    const block = missionText(items, '', 'article', 'mission-item');
    missionText(block, `${missionLocationText(item.pickup,order.location_names)} → ${missionLocationText(item.dropoff,order.location_names)}`, 'strong', 'mission-item-route');
    missionText(block, `Lượt ${item.number} · ${item.robot ? `Xe ${item.robot}` : 'Chờ phân xe'}${item.rack_id ? ` · Kệ ${item.rack_id}` : ''}`, 'p');
    missionText(block, missionPhases[item.status] || item.status, 'p', 'mission-item-phase');
    const steps = missionText(block, '', 'ol', 'mission-steps');
    const stage = transferStage(item.status);
    for (const [index, name] of ['Tới lấy', 'Nâng', 'Chuyển', 'Hạ', 'Ra cửa'].entries()) {
      const step = missionText(steps, name, 'li');
      step.dataset.stage = stage > index ? 'done' : stage === index && order.status === 'active' && controlsReady() ? 'current' : 'waiting';
      if (step.dataset.stage === 'current') step.setAttribute('aria-current', 'step');
    }
    if (item.waiting_reason || item.reason) missionText(block, item.waiting_reason || item.reason, 'p', 'mission-warning');
    if(!order.closed_by_shift&&item.status==='interrupted'&&['to_pickup','to_dropoff','to_exit'].includes(item.suspended_status))
      block.append(action(`Tiếp tục lượt ${item.number}`,button=>resumeWarehouseItem(order,item,button)));
    const tasks = row.tasks.filter(task => task.order_item === item.number);
    if (tasks.length) {
      const technical = document.createElement('details');
      technical.className = 'mission-technical';
      missionText(technical, 'Các chặng và mã nhiệm vụ', 'summary');
      for (const task of tasks) renderMissionTaskDetail(technical, task);
      block.append(technical);
    }
    if (item.route_plan) {
      const routes = document.createElement('details');
      routes.className = 'mission-technical';missionText(routes, 'Chi tiết đường đi', 'summary');
      for (const [phase, name] of [['pickup', 'Tuyến lấy dự kiến'], ['delivery', 'Tuyến giao dự kiến'], ['exit', 'Lùi sau khi giao']]) {
        const route = item.route_plan[phase];
        missionText(routes, route?.path?.length ? `${name}: ${route.path.map(node=>missionLocationText(node,order.location_names)).join(' → ')} · trọng số ${route.cost}` : `${name}: chưa có đường`, 'p');
      }
      block.append(routes);
    }
  }
  missionText(parent, `Mã đơn: ${order.id}`, 'code', 'mission-order-code');
}

function renderMissionRow(tbody, row) {
  const tr = document.createElement('tr');
  tr.className = 'mission-row';tr.dataset.missionId = row.key;tr.dataset.category = row.category;
  if (row.type === 'order') tr.dataset.orderId = row.order.id;
  else {tr.dataset.historyId = row.task.id;tr.title = row.task.core_task_id || `Bản ghi cũ #${row.task.id}`;}
  const expanded = missionExpanded.has(row.key);
  const identity = missionCell(tr, 'Công việc', 'mission-identity');
  const toggle = document.createElement('button');toggle.type = 'button';toggle.className = 'mission-toggle';
  toggle.setAttribute('aria-expanded', String(expanded));
  toggle.setAttribute('aria-label', `${expanded ? 'Thu gọn' : 'Chi tiết'} ${row.type === 'order' ? row.order.id : row.task.core_task_id || row.task.id}`);
  missionText(toggle, expanded ? '▾' : '▸', 'span', 'mission-chevron');
  const title = missionText(toggle, '', 'span');
  missionText(title, row.type === 'order' ? 'Chuyển kệ' : 'Đi tới node', 'strong');
  missionText(title, row.type === 'order' ? `${row.total} kệ · ${row.order.items.length} lượt` : row.task.core_task_id?.slice(0, 12) || `Bản ghi #${row.task.id}`, 'small');
  identity.append(toggle);
  const vehicles = missionCell(tr, 'Xe', 'mission-vehicles');
  if (!row.vehicles.length) missionText(vehicles, 'Chờ phân xe', 'span', 'mission-muted');
  for (const id of row.vehicles) {const tag = missionText(vehicles, `Xe ${id}`, 'span', 'mission-vehicle');tag.dataset.vehicle = id;}
  const route = missionCell(tr, 'Nguồn → Đích', 'mission-route-cell');
  missionText(route, row.routes[0] || '—', 'strong');
  if (row.routes.length > 1) missionText(route, `+ ${row.routes.length - 1} tuyến khác`, 'small');
  missionText(route, fmtDate(row.created), 'small');
  const status = missionCell(tr, 'Trạng thái / Tiến độ', 'mission-status');
  missionBadge(status, row.type === 'order' ? missionOrderNames[row.status] || row.status : labels[row.status] || row.status, row.category);
  if (row.type === 'order') {
    const progress = document.createElement('progress');progress.max = row.total || 1;progress.value = row.delivered;
    progress.setAttribute('aria-label', `Hoàn thành ${row.delivered}/${row.total} kệ`);status.append(progress);
    const current = row.order.items.find(item => item.status !== 'delivered');
    missionText(status, `${row.delivered}/${row.total} kệ${current ? ` · ${missionPhases[current.status] || current.status}` : ' · Đã đặt kệ'}`, 'small');
    if (row.order.reason) missionText(status, row.order.reason, 'small', 'mission-warning');
    else if (current?.waiting_reason) missionText(status, current.waiting_reason, 'small', 'mission-muted');
  } else if (row.task.reason) missionText(status, row.task.reason, 'small', 'mission-warning');
  const actions = missionCell(tr, 'Thao tác', 'mission-actions-cell');
  const group = missionText(actions, '', 'div', 'mission-actions');
  const detailButton = action(expanded ? 'Thu gọn' : 'Chi tiết', () => toggle.click(), null);
  detailButton.type = 'button';detailButton.setAttribute('aria-expanded', String(expanded));group.append(detailButton);
  if (row.type === 'order') {
    if (row.status === 'needs_attention') group.append(action(warehouseRollbackLabel(row.order), () => resolveWarehouseOrder(row.order.id), null));
    group.append(recordDeleteButton('order', row.order.id));
  } else {missionTaskActions(group, row.task);group.append(recordDeleteButton('history', row.task.id));}
  const detail = document.createElement('tr');detail.className = 'mission-detail-row';detail.hidden = !expanded;
  const detailId = `mission-detail-${tbody.children.length}`;
  detail.id = detailId;toggle.setAttribute('aria-controls', detailId);
  detailButton.setAttribute('aria-controls', detailId);
  const cell = document.createElement('td');cell.colSpan = 5;detail.append(cell);
  const content = missionText(cell, '', 'div', 'mission-detail');
  if (expanded) {
    if (row.type === 'order') renderMissionOrderDetail(content, row);
    else renderMissionTaskDetail(content, row.task);
  }
  toggle.onclick = () => {
    if (missionExpanded.has(row.key)) missionExpanded.delete(row.key);else missionExpanded.add(row.key);
    renderMissions(lastState);
  };
  tbody.append(tr, detail);
}

function renderMissions(state) {
  const includeClosed = $('orderShowResolved').checked;
  const result = buildMissionRows(state, {filter: missionFilter, includeClosed, query: $('missionSearch').value});
  const resolved = (state?.orders || []).filter(order => !order.deleted_at && order.status === 'closed').length;
  $('orderResolvedFilter').hidden = !resolved;
  $('orderResolvedFilterLabel').textContent = `Hiện đơn đã đối soát (${resolved})`;
  for (const button of document.querySelectorAll('[data-mission-filter]')) {
    const filter = button.dataset.missionFilter;
    button.setAttribute('aria-pressed', String(filter === missionFilter));
    button.querySelector('span').textContent = result.counts[filter];
  }
  $('missionCount').textContent = `${result.rows.length} công việc`;
  const key = JSON.stringify([result.rows, state?.warehouse?.revision, state?.location_names?.revision,
    missionFilter, includeClosed, [...missionExpanded], dataFresh(), controlsReady()]);
  if (key === missionRenderKey) return;
  missionRenderKey = key;
  const tbody = $('missionTable').tBodies[0];tbody.replaceChildren();
  for (const row of result.rows) renderMissionRow(tbody, row);
  if (!result.rows.length) {
    const tr = document.createElement('tr'), cell = document.createElement('td');cell.colSpan = 5;
    cell.className = 'mission-empty';
    missionText(cell, 'Chưa có công việc phù hợp', 'strong');
    missionText(cell, state ? 'Thử đổi bộ lọc hoặc từ khóa tìm kiếm.' : 'Đang chờ dữ liệu từ Core.', 'p');
    tr.append(cell);tbody.append(tr);
  }
  document.querySelectorAll('#missionTable [data-control]').forEach(button => button.disabled = !controlsReady() || button.dataset.busy === 'true');
}
