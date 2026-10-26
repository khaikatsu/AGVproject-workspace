const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {once} = require('node:events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {createDatabase} = require('../AGVwebserver/db');
const {createGateway} = require('../AGVwebserver/gateway');

async function main() {
  const playwright = require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  const directory = fs.mkdtempSync(path.resolve(__dirname, '../.qa/missions-browser-'));
  const db = createDatabase(path.join(directory, 'test.db'));
  const session = 'missions-session', commands = [];
  const nodes = [1, 2, 3, 4, 5, 6].flatMap(n => [{id: `K${n}`, type: 'station', x: n * 80, y: n % 2 * 100},
    {id: `G${n}`, type: 'waypoint', x: n * 80, y: 50}]);
  const tasks = [
    ...['pickup-a', 'delivery-a', 'exit-a', 'multi-a', 'multi-b'].map((task_id, i) => ({task_id, target: i === 2 ? 'G5' : 'K5',
      assigned_agv: task_id === 'multi-b' ? '02' : '01', status: 'completed', priority: 0})),
    {task_id: 'attention-leg', target: 'K5', assigned_agv: '01', status: 'dispatched', priority: 0, reason: '<img src=x onerror="window.__xss=1">'},
    {task_id: 'independent', target: 'K5', assigned_agv: '01', status: 'dispatched', priority: 0}];
  db.syncTasks([{task_id: 'delivery-a', target: 'K5', assigned_agv: '01', status: 'completed'}], 'legacy-session');
  db.syncTasks(tasks, session);
  const link = (task_id, phase) => ({task_id, phase, core_session_id: session, history_id: db.getTaskReference(task_id, session).id,
    target: phase === 'exit' ? 'G5' : 'K5', status: task_id === 'attention-leg' ? 'dispatched' : 'completed'});
  const item = (number, pickup, dropoff, robot, status, task_links = []) => ({number, pickup, dropoff, robot, status, task_links});
  const orders = [
    {id: 'order-running', status: 'active', items: [item(1, 'K4', 'K1', null, 'queued')]},
    {id: 'order-finished', status: 'completed', items: [item(1, 'K2', 'K5', '01', 'delivered',
      [link('pickup-a', 'pickup'), link('delivery-a', 'dropoff'), link('exit-a', 'exit')])]},
    {id: 'order-multi', status: 'completed', items: [item(1, 'K1', 'K5', '01', 'delivered', [link('multi-a', 'dropoff')]),
      item(2, 'K3', 'K6', '02', 'delivered', [link('multi-b', 'dropoff')])]},
    {id: 'order-attention', status: 'needs_attention', reason: 'LINE_LOST · Cần kiểm tra vị trí xe và kệ',
      items: [item(1, 'K2', 'K5', '01', 'interrupted', [link('attention-leg', 'dropoff')])]},
    {id: 'order-closed', status: 'closed', items: [item(1, 'K5', 'K6', '02', 'interrupted')]}];
  orders.forEach((order, index) => db.saveOrder({...order, total: order.items.length,
    created: new Date(Date.UTC(2026, 9, 26, 3, index)).toISOString(), core_session_id: session, destinationOwners: {}}));
  const model = {protocol_version: 1, core_session_id: session, mqtt_connected: true,
    map: {map_name: 'Kho kiểm thử công việc', revision: 1, nodes, edges: nodes.flatMap(a => nodes.filter(b => b !== a).map(b => ({from: a.id, to: b.id, weight: 1}))), obstacles: []},
    fleet: {agvs: [{id: '01', node: 'K2', state: 'ERROR', error: 'LINE_LOST', online: true, position_valid: true, battery: 82, battery_measured: true, last_seen_s: 0},
      {id: '02', node: 'G3', state: 'WORKING', online: true, position_valid: true, battery: 76, battery_measured: true, last_seen_s: 0}],
      tasks, pending_tasks: [], accepting_tasks: true}, mutex: {}, logs: [],
    shift: {supported: true, phase: 'RUNNING', operation_profile: 'scoped', active_robot_ids: ['01', '02'], robots: []}};
  let peer, gateway, browser, pump, pushEnabled = true;
  const core = new WebSocket.Server({host: '127.0.0.1', port: 0});await once(core, 'listening');
  const push = () => {if (pushEnabled && peer?.readyState === WebSocket.OPEN) peer.send(JSON.stringify(model));};
  core.on('connection', socket => {peer = socket;push();socket.on('message', raw => {
    const request = JSON.parse(raw);commands.push(request);
    assert.equal(request.cmd, 'cancel_task');
    model.fleet.tasks.find(task => task.task_id === request.task_id).status = 'cancelled';
    socket.send(JSON.stringify({type: 'command_result', request_id: request.request_id, core_session_id: session,
      ok: true, data: {cancelled: 1}, state: model}));
  });});
  try {
    gateway = createGateway({db, coreUrl: `ws://127.0.0.1:${core.address().port}`, staleMs: 1200});
    const address = await gateway.start(0, '127.0.0.1');pump = setInterval(push, 200);
    browser = await playwright.chromium.launch({headless: true, channel: process.env.AGV_BROWSER_CHANNEL || 'msedge'});
    const page = await browser.newPage({viewport: {width: 1440, height: 1000}}), errors = [];
    page.on('pageerror', error => errors.push(error.message));page.on('dialog', dialog => dialog.accept());
    await page.goto(`http://127.0.0.1:${address.port}`);
    await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    const rows = page.locator('#missionTable .mission-row');
    await page.waitForFunction(() => document.querySelectorAll('#missionTable .mission-row').length === 6);
    assert.equal(await page.locator('#orderList, #taskHistoryTable').count(), 0, 'old duplicate lists are removed');
    assert.equal(await page.locator('[data-order-id="order-finished"]').count(), 1);
    assert.equal(await page.locator('#missionTable .mission-row[data-history-id]').count(), 2, 'only independent and unlinked legacy tasks are top-level');
    assert.equal(await page.evaluate(() => window.__xss), undefined);
    await page.locator('[data-mission-filter="active"]').click();assert.equal(await rows.count(), 2);
    await page.locator('[data-mission-filter="done"]').click();assert.equal(await rows.count(), 3);
    await page.locator('[data-mission-filter="attention"]').click();assert.equal(await rows.count(), 1);
    await page.locator('[data-mission-filter="all"]').click();
    await page.locator('#missionSearch').fill('order-multi');assert.equal(await rows.count(), 1);
    assert.match(await rows.first().innerText(), /Xe 01.*Xe 02/s);
    await page.locator('#missionSearch').fill('');
    await page.locator('#orderShowResolved').check();assert.equal(await rows.count(), 7);
    await page.locator('#orderShowResolved').uncheck();assert.equal(await rows.count(), 6);
    await page.evaluate(() => {window.__missionRow = document.querySelector('[data-order-id="order-finished"]');});
    for (let seq = 1; seq <= 6; seq++) {
      model.fleet.agvs[1].odometry = {seq, valid: false};push();
      await page.waitForFunction(value => lastState.fleet.agvs[1].odometry?.seq === value, seq);
    }
    assert.ok(await page.evaluate(() => window.__missionRow === document.querySelector('[data-order-id="order-finished"]')),
      'encoder-only updates preserve mission rows');
    const assertBelowMap = async label => {
      const map = await page.locator('#center-panel').boundingBox(), board = await page.locator('#missionBoard').boundingBox();
      assert.ok(board.y >= map.y + map.height - 1, `${label}: missions appear below the map`);
      assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), `${label}: no horizontal overflow`);
    };
    await assertBelowMap('expanded desktop');
    await page.locator('#mapExpandBtn').click();await assertBelowMap('compact desktop');
    await page.locator('#mapExpandBtn').click();
    assert.equal(await page.locator('[data-view-target="tasks"]').count(),0,'redundant tasks page is removed');
    await page.locator('[data-view-target="overview"]').click();
    await page.screenshot({path: path.join(directory, 'desktop-layout.png'), fullPage: true});
    await page.locator('#missionBoard').screenshot({path: path.join(directory, 'desktop-missions.png')});
    await page.setViewportSize({width: 900, height: 1000});await assertBelowMap('tablet');
    await page.setViewportSize({width: 390, height: 844});
    await assertBelowMap('mobile');
    const boardBox = await page.locator('#missionBoard').boundingBox(), controlsBox = await page.locator('#right-panel').boundingBox();
    assert.ok(controlsBox.y >= boardBox.y + boardBox.height - 1, 'mobile keeps missions directly after the map and before controls');
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
    await page.locator('#missionBoard').screenshot({path: path.join(directory, 'mobile-missions.png')});
    await page.setViewportSize({width: 1440, height: 1000});
    const multiple = page.locator('[data-order-id="order-multi"]');
    await multiple.locator('.mission-toggle').click();
    const detail = page.locator(`#${await multiple.locator('.mission-toggle').getAttribute('aria-controls')}`);
    assert.equal(await detail.locator('.mission-item').count(), 2);
    assert.match(await detail.innerText(), /K1 → K5.*K3 → K6/s);
    await detail.scrollIntoViewIfNeeded();
    await detail.screenshot({path: path.join(directory, 'expanded-order-detail.png')});
    await page.locator('#missionBoard').screenshot({path: path.join(directory, 'expanded-order.png')});
    push();await page.waitForTimeout(300);
    assert.equal(await multiple.locator('.mission-toggle').getAttribute('aria-expanded'), 'true');
    await multiple.locator('.mission-toggle').click();
    pushEnabled = false;
    await page.waitForFunction(() => !dataFresh());
    const independentId = db.getTaskReference('independent', session).id;
    await page.waitForFunction(id => document.querySelector(`[data-history-id="${id}"] button[data-control]`).disabled, independentId);
    pushEnabled = true;push();await page.waitForFunction(() => dataFresh());
    const attention = page.locator('[data-order-id="order-attention"]');
    await attention.locator('.mission-toggle').click();
    const attentionDetail = page.locator(`#${await attention.locator('.mission-toggle').getAttribute('aria-controls')}`);
    await attentionDetail.locator('.mission-technical').first().locator('summary').click();
    assert.equal(await page.evaluate(() => window.__xss), undefined);
    await attentionDetail.getByRole('button', {name: 'Hủy', exact: true}).click();
    await page.waitForFunction(() => lastState.task_history.find(task => task.core_task_id === 'attention-leg')?.status === 'cancelled');
    assert.equal(commands[0].task_id, 'attention-leg');
    assert.match(await attention.innerText(), /Cần đối soát/, 'cancelling a leg does not invent order completion');
    await attention.locator('.mission-toggle').click();
    await multiple.getByRole('button', {name: 'Xóa đơn order-multi', exact: true}).click();
    await multiple.waitFor({state: 'detached'});
    const current = await (await page.request.get(`http://127.0.0.1:${address.port}/api/state`)).json();
    assert.ok(current.task_history.filter(task => ['multi-a', 'multi-b'].includes(task.core_task_id)).every(task => task.order_deleted_at));
    assert.equal(await page.locator('#missionTable .mission-row[data-history-id]').count(), 2, 'deleted order children never reappear as separate work');
    await page.locator(`[data-history-id="${independentId}"]`).getByRole('button', {name: `Xóa lịch sử ${independentId}`, exact: true}).click();
    await page.locator(`[data-history-id="${independentId}"]`).waitFor({state: 'detached'});
    await page.setViewportSize({width: 390, height: 844});
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
    await page.evaluate(() => document.getElementById('toastContainer').replaceChildren());
    await page.locator('#missionBoard').screenshot({path: path.join(directory, 'mobile-after-actions.png')});
    await page.reload();await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    assert.equal(await page.locator('[data-order-id="order-multi"]').count(), 0);
    assert.equal(await page.locator(`[data-history-id="${independentId}"]`).count(), 0);
    assert.equal(await page.locator('#orderShowResolved').isChecked(), false);
    assert.deepEqual(errors, []);
    console.log(`PASS unified missions: exact grouping/session, independent legacy rows, filters/search, two-robot details, cancel/delete, stale, persistence, desktop/mobile. Evidence: ${directory}`);
  } finally {
    clearInterval(pump);if (browser) await browser.close();if (gateway) await gateway.close();
    for (const client of core.clients) client.terminate();await new Promise(resolve => core.close(resolve));db.close();
  }
}
main().catch(error => {console.error(error);process.exitCode = 1;});
