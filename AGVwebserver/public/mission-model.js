'use strict';

function missionLocationText(id,config) {
  const name=config?.labels?.[id] || id || '—';
  return name===id || !id ? name : `${name} (vị trí ${id})`;
}

function missionCategory(status, type) {
  if (type === 'order') return status === 'active' ? 'active' :
    ['completed', 'closed'].includes(status) ? 'done' : 'attention';
  return ['pending', 'dispatched', 'submitting'].includes(status) ? 'active' :
    ['completed', 'cancelled'].includes(status) ? 'done' : 'attention';
}

function missionTime(value) {
  const source = /^\d{4}-\d\d-\d\d \d\d:/.test(value || '') ? value.replace(' ', 'T') + 'Z' : value;
  return Date.parse(source) || 0;
}

function buildMissionRows(state, options = {}) {
  const orders = Array.isArray(state?.orders) ? state.orders : [];
  const history = Array.isArray(state?.task_history) ? state.task_history : [];
  const parents = new Map(orders.map(order => [order.id, order]));
  const links = new Map();
  const identity = (session, task) => JSON.stringify([session, task]);
  for (const order of orders) for (const item of order.items || []) {
    const known = [...(item.task_links || [])];
    if (item.task_id && order.core_session_id && !known.some(link => link.task_id === item.task_id && link.core_session_id === order.core_session_id))
      known.push({task_id: item.task_id, core_session_id: order.core_session_id});
    for (const link of known) {
      if (!link.task_id || !link.core_session_id) continue;
      const key = identity(link.core_session_id, link.task_id);
      const previous = links.get(key);
      if (links.has(key) && (previous === null || previous.order_id !== order.id || previous.order_item !== item.number)) links.set(key, null);
      else links.set(key, {order_id: order.id, order_item: item.number, order_phase: link.phase});
    }
  }
  const tasksByOrder = new Map(), standalone = [];
  for (const task of history) {
    if (task.deleted_at) continue;
    const link = task.order_id ? task : links.get(identity(task.core_session, task.core_task_id));
    const orderId = link?.order_id;
    if (orderId && (parents.has(orderId) || task.order_deleted_at)) {
      if (!tasksByOrder.has(orderId)) tasksByOrder.set(orderId, []);
      tasksByOrder.get(orderId).push({...task, order_item: link.order_item, order_phase: link.order_phase});
    } else standalone.push(task);
  }
  const rows = [];
  for (const order of orders) {
    if (order.deleted_at || order.status === 'closed' && !options.includeClosed) continue;
    const items = order.items || [];
    const routes = [...new Set(items.map(item => `${missionLocationText(item.pickup,order.location_names)} → ${missionLocationText(item.dropoff,order.location_names)}`))];
    const vehicles = [...new Set(items.map(item => item.robot).filter(Boolean))];
    rows.push({key: `order:${order.id}`, type: 'order', order, tasks: tasksByOrder.get(order.id) || [],
      routes, vehicles, status: order.status, category: missionCategory(order.status, 'order'),
      created: order.created, delivered: items.filter(item => item.status === 'delivered').length,
      total: Number.isInteger(order.total) ? order.total : items.length});
  }
  for (const task of standalone) rows.push({key: `history:${task.id}`, type: 'task', task,
    routes: [`→ ${missionLocationText(task.target_node,task.location_names)}`], vehicles: task.assigned_agv ? [task.assigned_agv] : [],
    status: task.status, category: missionCategory(task.status, 'task'), created: task.created_at});
  const counts = {all: rows.length, active: 0, done: 0, attention: 0};
  for (const row of rows) counts[row.category]++;
  const query = String(options.query || '').trim().toLocaleLowerCase('vi');
  const rank = {active: 0, attention: 1, done: 2};
  const visible = rows.filter(row => (!options.filter || options.filter === 'all' || row.category === options.filter) &&
    (!query || [row.order?.id, row.task?.core_task_id, row.task?.reason, row.order?.reason,
      ...row.routes, ...row.vehicles].join(' ').toLocaleLowerCase('vi').includes(query)));
  visible.sort((a, b) => rank[a.category] - rank[b.category] || missionTime(b.created) - missionTime(a.created) || a.key.localeCompare(b.key));
  return {rows: visible, counts};
}

if (typeof module !== 'undefined' && module.exports) module.exports = {buildMissionRows, missionCategory};
