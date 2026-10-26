const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {once} = require('node:events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {createDatabase} = require('../AGVwebserver/db');
const {createGateway} = require('../AGVwebserver/gateway');

async function main() {
  const playwright = require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  const directory = fs.mkdtempSync(path.resolve(__dirname, '../.qa/record-delete-'));
  const filename = path.join(directory, 'test.db');
  let db = createDatabase(filename), gateway, browser, peer, pump, acceptDialog = true;
  const commands = [];
  const order = (id, status) => ({id, status, created: new Date().toISOString(), total: 1,
    core_session_id: 'record-delete-session', destinationOwners: {}, items: [
      {number: 1, pickup: 'K1', dropoff: 'K2', status: status === 'active' ? 'queued' :
        status === 'needs_attention' ? 'interrupted' : 'delivered', robot: null, task_id: null}]});
  for (const [id, status] of [['order-done-a', 'completed'], ['order-done-b', 'completed'],
    ['order-closed', 'closed'], ['order-attention', 'needs_attention'], ['order-active', 'active']]) db.saveOrder(order(id, status));
  const model = {protocol_version: 1, core_session_id: 'record-delete-session', mqtt_connected: true,
    map: {map_name: 'Kho kiểm thử xóa danh sách', revision: 1,
      nodes: [{id: 'K1', type: 'station', x: 0, y: 0}, {id: 'K2', type: 'station', x: 150, y: 0},
        {id: 'G1', type: 'waypoint', x: 0, y: 80}, {id: 'G2', type: 'waypoint', x: 150, y: 80}],
      edges: [{from: 'K1', to: 'K2', weight: 150}, {from: 'K1', to: 'G1', weight: 80}, {from: 'K2', to: 'G2', weight: 80}], obstacles: []},
    fleet: {agvs: [], accepting_tasks: true, pending_tasks: [], tasks: [
      {task_id: 'task-same-prefix-a', target: 'K1', priority: 0, assigned_agv: '01', status: 'completed'},
      {task_id: 'task-same-prefix-b', target: 'K2', priority: 0, assigned_agv: '02', status: 'interrupted', reason: 'LINE_LOST'},
      {task_id: 'task-live', target: 'K2', priority: 0, assigned_agv: '01', status: 'pending'}]},
    shift: {supported: true, phase: 'RUNNING', active_robot_ids: ['01', '02'], home_stations: {}, robots: []}, mutex: {}, logs: []};
  const core = new WebSocket.Server({host: '127.0.0.1', port: 0});
  await once(core, 'listening');
  const push = () => {if (peer?.readyState === WebSocket.OPEN) peer.send(JSON.stringify(model));};
  core.on('connection', socket => {peer = socket;push();socket.on('message', raw => commands.push(JSON.parse(raw)));});
  const start = async () => {
    gateway = createGateway({db, coreUrl: `ws://127.0.0.1:${core.address().port}`});
    return await gateway.start(0, '127.0.0.1');
  };
  try {
    let address = await start();pump = setInterval(push, 200);
    browser = await playwright.chromium.launch({headless: true, channel: process.env.AGV_BROWSER_CHANNEL || 'msedge'});
    const page = await browser.newPage({viewport: {width: 1440, height: 1050}}), errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('dialog', dialog => acceptDialog ? dialog.accept() : dialog.dismiss());
    await page.goto(`http://127.0.0.1:${address.port}`);
    await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    await page.locator('[data-view-target="overview"]').click();
    await page.locator('[data-order-id="order-done-a"]').waitFor();
    await page.locator('#missionTable tr[data-history-id]').first().waitFor();
    const history = db.getTaskHistory();
    const deletedId = history.find(row => row.core_task_id === 'task-same-prefix-a').id;
    const otherId = history.find(row => row.core_task_id === 'task-same-prefix-b').id;
    const liveId = history.find(row => row.core_task_id === 'task-live').id;
    const orderEntry = page.locator('[data-order-id="order-done-a"]');
    const deleteOrder = orderEntry.getByRole('button', {name: 'Xóa đơn order-done-a', exact: true});
    acceptDialog = false;await deleteOrder.click();
    assert.equal(await orderEntry.count(), 1);
    assert.equal(await orderEntry.locator('.mission-toggle').getAttribute('aria-expanded'), 'false', 'delete button does not toggle the details');
    acceptDialog = true;await deleteOrder.click();
    await orderEntry.waitFor({state: 'detached'});
    assert.equal(await page.locator('[data-order-id="order-done-b"]').count(), 1, 'delete affects only the chosen order');
    assert.equal(await page.locator('#orderProgressSelect option[value="order-done-a"]').count(), 0);
    await page.locator('#orderShowResolved').check();
    await page.locator('[data-order-id="order-closed"]').getByRole('button', {name: 'Xóa đơn order-closed', exact: true}).click();
    await page.locator('[data-order-id="order-closed"]').waitFor({state: 'detached'});
    assert.equal(await page.locator('#orderResolvedFilter').isHidden(), true);
    await page.locator(`[data-history-id="${deletedId}"]`).getByRole('button', {name: `Xóa lịch sử ${deletedId}`, exact: true}).click();
    await page.locator(`[data-history-id="${deletedId}"]`).waitFor({state: 'detached'});
    assert.equal(await page.locator(`[data-history-id="${otherId}"]`).count(), 1, 'shared ID prefixes never delete another row');
    await page.route(`**/api/tasks/history/${otherId}`, route => route.fulfill({status: 500, contentType: 'application/json', body: JSON.stringify({error: 'disk-test'})}));
    await page.locator(`[data-history-id="${otherId}"]`).getByRole('button', {name: `Xóa lịch sử ${otherId}`, exact: true}).click();
    await page.locator('#toastContainer').filter({hasText: 'disk-test'}).waitFor();
    assert.equal(await page.locator(`[data-history-id="${otherId}"]`).count(), 1, 'a failed API call does not remove the row');
    await page.unroute(`**/api/tasks/history/${otherId}`);
    await page.route(`**/api/tasks/history/${otherId}`, route => route.fulfill({status: 404, contentType: 'text/html', body: '<pre>Cannot DELETE /api/tasks/history/1</pre>'}));
    await page.locator(`[data-history-id="${otherId}"]`).getByRole('button', {name: `Xóa lịch sử ${otherId}`, exact: true}).click();
    await page.locator('#toastContainer').filter({hasText: 'Gateway đang chạy bản cũ'}).waitFor();
    assert.equal(await page.locator(`[data-history-id="${otherId}"]`).count(), 1);
    await page.unroute(`**/api/tasks/history/${otherId}`);
    await page.locator('[data-order-id="order-active"]').getByRole('button', {name: 'Xóa đơn order-active', exact: true}).click();
    await page.locator('[data-order-id="order-active"]').waitFor({state: 'detached'});
    await page.locator(`[data-history-id="${liveId}"]`).getByRole('button', {name: `Xóa lịch sử ${liveId}`, exact: true}).click();
    await page.locator(`[data-history-id="${liveId}"]`).waitFor({state: 'detached'});
    assert.equal(commands.length, 0, 'deletion never sends cancel or robot commands');
    assert.ok(db.getOpenTasks().some(row => row.id === liveId));
    await page.locator('[data-order-id="order-attention"]').getByRole('button', {name: 'Xóa đơn order-attention', exact: true}).click();
    await page.locator('[data-order-id="order-attention"]').waitFor({state: 'detached'});
    const attention = page.locator('[data-deleted-order-id="order-attention"]');
    await attention.getByRole('button', {name: 'Đối soát', exact: true}).click();
    await attention.waitFor({state: 'detached'});
    model.fleet.tasks[0].status = 'cancelled';push();
    await page.waitForFunction(() => lastState.fleet.tasks[0].status === 'cancelled');
    assert.equal(await page.locator(`[data-history-id="${deletedId}"]`).count(), 0);
    await page.reload();await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    await page.locator('[data-view-target="overview"]').click();
    assert.equal(await page.locator('[data-order-id="order-done-a"]').count(), 0);
    assert.equal(await page.locator(`[data-history-id="${deletedId}"]`).count(), 0);
    await page.evaluate(() => document.getElementById('toastContainer').replaceChildren());
    await page.screenshot({path: path.join(directory, 'desktop-delete.png'), fullPage: true});
    await page.setViewportSize({width: 390, height: 844});
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
    await page.screenshot({path: path.join(directory, 'mobile-delete.png'), fullPage: true});
    // Stop this fixture's reconnect loop before closing its HTTP/WS server.
    await page.goto('about:blank');
    await gateway.close();gateway = null;db.close();db = createDatabase(filename);
    address = await start();
    await page.goto(`http://127.0.0.1:${address.port}`);
    await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    await page.locator('[data-view-target="overview"]').click();
    await page.locator('[data-order-id="order-done-b"]').waitFor();
    assert.equal(await page.locator('[data-order-id="order-done-a"]').count(), 0);
    assert.equal(await page.locator(`[data-history-id="${deletedId}"]`).count(), 0);
    assert.equal(db.getOrders().length, 5, 'internal orders are preserved across restart');
    assert.equal(db.db.prepare('SELECT COUNT(*) AS n FROM task_log').get().n, 3, 'KPI source rows are preserved');
    assert.deepEqual(errors, []);
    console.log(`PASS record deletion: chosen order/history, confirm/cancel/error, active supervision, reconciliation, sync/reload/restart, desktop/mobile. Evidence: ${directory}`);
  } finally {
    clearInterval(pump);if (browser) await browser.close();if (gateway) await gateway.close();
    for (const client of core.clients) client.terminate();
    await new Promise(resolve => core.close(resolve));db.close();
  }
}
main().catch(error => {console.error(error);process.exitCode = 1;});
