const {test} = require('node:test');
const assert = require('node:assert/strict');
const {buildMissionRows} = require('../AGVwebserver/public/mission-model');

function order(id = 'order-a', status = 'active') {
  return {id, status, total: 1, core_session_id: 'session-a', created: '2026-10-26T03:00:00Z', items: [
    {number: 1, pickup: 'K2', dropoff: 'K5', robot: '01', status: 'to_exit', task_links: [
      {task_id: 'pickup', core_session_id: 'session-a', phase: 'pickup'},
      {task_id: 'delivery', core_session_id: 'session-a', phase: 'dropoff'},
      {task_id: 'exit', core_session_id: 'session-a', phase: 'exit'}]}]};
}
function history(id, task, session = 'session-a') {
  return {id, core_task_id: task, core_session: session, target_node: 'K5', assigned_agv: '01', status: 'completed', created_at: '2026-10-26 03:00:00'};
}
test('one order groups its exact tasks; same robot and target never absorb an independent task', () => {
  const result = buildMissionRows({orders: [order()], task_history: ['pickup', 'delivery', 'exit', 'independent'].map((task, i) => history(i + 1, task))});
  assert.equal(result.rows.length, 2);
  assert.equal(result.rows.find(row => row.type === 'order').tasks.length, 3);
  assert.equal(result.rows.find(row => row.type === 'task').task.core_task_id, 'independent');
  assert.equal(result.rows.find(row => row.type === 'order').category, 'active', 'completed child tasks never invent order completion');
  assert.equal(result.rows.find(row => row.type === 'order').delivered, 0);
});
test('task identity includes the Core session; legacy rows without proof remain separate', () => {
  const result = buildMissionRows({orders: [order()], task_history: [history(1, 'pickup', 'other-session'), history(2, null)]});
  assert.equal(result.rows.length, 3);
  assert.equal(result.rows[0].tasks.length, 0);
});
test('deleted or hidden closed parents never leak their linked children as independent missions', () => {
  const hidden = {...order(), deleted_at: '2026-10-26T04:00:00Z'};
  assert.equal(buildMissionRows({orders: [hidden], task_history: [history(1, 'pickup')]}).rows.length, 0);
  const closed = order('closed', 'closed');
  assert.equal(buildMissionRows({orders: [closed], task_history: [history(1, 'pickup')]}).rows.length, 0);
  assert.equal(buildMissionRows({orders: [closed], task_history: [history(1, 'pickup')]}, {includeClosed: true}).rows.length, 1);
  assert.equal(buildMissionRows({orders: [], task_history: [{...history(1, 'pickup'), order_id: 'deleted', order_deleted_at: 'date'}]}).rows.length, 0);
});
test('two robot orders keep each route; filters and search count parent missions rather than legs', () => {
  const multiple = order();multiple.total = 2;
  multiple.items.push({number: 2, pickup: 'K3', dropoff: 'K6', robot: '02', status: 'delivered'});
  const state = {orders: [multiple, {...order('attention', 'needs_attention'), items: []}],
    task_history: [history(1, 'independent')]};
  const result = buildMissionRows(state, {filter: 'active', query: 'K6'});
  assert.equal(result.rows.length, 1);assert.deepEqual(result.rows[0].vehicles, ['01', '02']);
  assert.deepEqual(result.counts, {all: 3, active: 1, attention: 1, done: 1});
  assert.equal(buildMissionRows(state, {query: '02'}).rows.length, 1);
});
test('ambiguous task links and deleted history rows are handled conservatively', () => {
  const result = buildMissionRows({orders: [order('a'), order('b')], task_history: [history(1, 'pickup'), {...history(2, 'other'), deleted_at: 'date'}]});
  assert.equal(result.rows.length, 3);assert.equal(result.rows.filter(row => row.type === 'task').length, 1);
});
test('server associations preserve the correct item even when only a subset of orders is loaded', () => {
  const parent = order();parent.items[0].task_links = [];
  const result = buildMissionRows({orders: [parent], task_history: [{...history(1, 'recorded-task', 'new-session'), order_id: parent.id, order_item: 1, order_phase: 'dropoff'}]});
  assert.equal(result.rows.length, 1);assert.equal(result.rows[0].tasks[0].order_phase, 'dropoff');
});
