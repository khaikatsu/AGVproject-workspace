const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {once} = require('node:events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {createDatabase} = require('../AGVwebserver/db');
const {createGateway} = require('../AGVwebserver/gateway');

async function main() {
  const playwright = require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  const directory = fs.mkdtempSync(path.resolve(__dirname, '../.qa/vehicle-cards-'));
  const db = createDatabase(path.join(directory, 'test.db'));
  const robot = id => ({id, node: id === '01' ? 'S1' : 'S2', state: 'IDLE', online: true,
    battery: 82, battery_measured: true, last_seen_s: 0, position_valid: true,
    task_id: null, path: [], target: '', odometry: {seq: 0, valid: false}});
  const model = {protocol_version: 1, core_session_id: 'vehicle-cards-session', mqtt_connected: true,
    map: {map_name: 'Kho kiểm thử thẻ xe', revision: 1,
      nodes: [{id: 'S1', type: 'charging', x: 0, y: 0}, {id: 'S2', type: 'charging', x: 100, y: 0},
        {id: 'K1', type: 'station', x: 0, y: 100}, {id: 'K2', type: 'station', x: 100, y: 100}],
      edges: [{from: 'S1', to: 'K1', weight: 100}, {from: 'S2', to: 'K2', weight: 100}], obstacles: []},
    fleet: {agvs: [], tasks: [], pending_tasks: [], accepting_tasks: true},
    shift: {supported: true, operation_profile: 'scoped', active_robot_ids: ['01', '02'], phase: 'RUNNING',
      home_stations: {'01': 'S1', '02': 'S2'}, robots: []}, mutex: {}, logs: []};
  const commands = [];
  let peer, gateway, browser, pump, rejectRemoval = false, pushEnabled = true;
  const core = new WebSocket.Server({host: '127.0.0.1', port: 0});
  await once(core, 'listening');
  const push = () => {if (pushEnabled && peer?.readyState === WebSocket.OPEN) peer.send(JSON.stringify(model));};
  core.on('connection', socket => {
    peer = socket;
    push();
    socket.on('message', raw => {
      const request = JSON.parse(raw);
      commands.push(request);
      const current = model.fleet.agvs.find(candidate => candidate.id === request.id);
      if (request.cmd === 'remove_agv') {
        if (rejectRemoval || current?.state !== 'OFFLINE') {
          socket.send(JSON.stringify({type: 'command_result', request_id: request.request_id,
            core_session_id: model.core_session_id, ok: false, status: 409, error: 'Robot vẫn giữ mutex zone (fixture)'}));
          return;
        }
        model.fleet.agvs = model.fleet.agvs.filter(candidate => candidate.id !== request.id);
      }
      if (['recover_vehicle','recover_and_continue'].includes(request.cmd))
        Object.assign(current, {state: 'IDLE', error: '',recovery_node_verified:true});
      socket.send(JSON.stringify({type: 'command_result', request_id: request.request_id,
        core_session_id: model.core_session_id, ok: true, data: {}, state: model}));
    });
  });
  try {
    gateway = createGateway({db, coreUrl: `ws://127.0.0.1:${core.address().port}`, staleMs: 1200});
    const address = await gateway.start(0, '127.0.0.1');
    pump = setInterval(push, 200);
    browser = await playwright.chromium.launch({headless: true, channel: process.env.AGV_BROWSER_CHANNEL || 'msedge'});
    const page = await browser.newPage({viewport: {width: 1440, height: 1000}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => {if (message.type() === 'error' && message.text().includes('Snapshot không hợp lệ')) errors.push(message.text());});
    page.on('dialog', dialog => dialog.accept());
    const cards = page.locator('#robotTransferCards [data-robot-transfer]');
    const card = id => page.locator(`[data-robot-transfer="${id}"]`);
    await page.goto(`http://127.0.0.1:${address.port}`);
    await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    assert.equal(await page.locator('#fleetTable').count(), 0, 'the duplicate fleet table is removed');
    assert.equal(await cards.count(), 2, 'two cards exist before any robot telemetry');
    for (const id of ['01', '02']) {
      assert.match(await card(id).innerText(), /Chưa kết nối/);
      assert.equal(await card(id).locator('[data-vehicle-action]').count(), 0);
    }
    await page.locator('#warehouseOperationsBoard').screenshot({path: path.join(directory, 'empty-cards.png')});
    model.fleet.agvs = [robot('01'), robot('02')];
    push();
    await card('01').filter({hasText: 'Pin: 82%'}).waitFor();
    await page.waitForFunction(() => !document.querySelector('[data-robot-transfer="02"] [data-vehicle-action="select"]').disabled);
    await card('02').getByRole('button', {name: 'Chọn xe', exact: true}).click();
    assert.match(await page.locator('#selectionText').innerText(), /Đã chọn 02/);
    await page.evaluate(() => cancelSelection());
    for(const id of ['01','02']) {
      const candidate=model.fleet.agvs.find(r=>r.id===id);
      for(const patch of [{state:'WORKING',task_id:'active-route',target:'K1',path:['K1']},
          {state:'BLOCKED',task_id:'waiting-route'}, {state:'IDLE',task_id:'not-yet-cleared'},
          {state:'IDLE',recovery_reset_pending:true}, {state:'IDLE',lift:{active:true}}]) {
        Object.assign(candidate,robot(id),patch);push();
        await page.waitForFunction(({id,patch})=>Object.entries(patch).every(([key,value])=>
          JSON.stringify(lastState.fleet.agvs.find(r=>r.id===id)[key])===JSON.stringify(value)),{id,patch});
        assert.equal(await card(id).getByRole('button',{name:'Chọn xe',exact:true}).isDisabled(),true);
        assert.equal(await page.locator(`#task_agv option[value="${id}"]`).isDisabled(),true);
        const before=commands.length;
        await page.evaluate(id=>{document.getElementById('task_agv').value=id;void addTask();},id);
        await page.waitForTimeout(50);assert.equal(commands.length,before,'busy/recovering robot sends no new GOTO');
        delete candidate.recovery_reset_pending;delete candidate.lift;
      }
      Object.assign(candidate,robot(id));push();
      await page.waitForFunction(id=>!document.querySelector(`[data-robot-transfer="${id}"] [data-vehicle-action="select"]`).disabled,id);
    }
    await page.evaluate(() => {
      window.__vehicleCards = [...document.querySelectorAll('[data-robot-transfer]')];
      window.__vehicleHeaders = window.__vehicleCards.map(element => element.firstElementChild);
      window.__warehouseSlot = document.querySelector('[data-warehouse-slot="K1"]');
    });
    for (let seq = 1; seq <= 12; seq++) {
      model.fleet.agvs[0].odometry.seq = seq;
      model.fleet.agvs[0].speed_cm_s = seq;
      push();
      await page.waitForFunction(value => lastState.fleet.agvs[0].odometry.seq === value, seq);
    }
    assert.ok(await page.evaluate(() => window.__vehicleCards.every((element, index) =>
      element === document.querySelector(`[data-robot-transfer="0${index + 1}"]`) &&
      element.firstElementChild === window.__vehicleHeaders[index])), 'encoder updates preserve both cards and their content');
    assert.ok(await page.evaluate(() => window.__warehouseSlot === document.querySelector('[data-warehouse-slot="K1"]')));
    model.fleet.agvs[0].battery = 18;
    push();
    await card('01').filter({hasText: 'Pin: 18%'}).waitFor();
    assert.equal(await card('01').locator('.vehicle-battery').getAttribute('data-low'), 'true');
    assert.ok(await page.evaluate(() => window.__vehicleHeaders[1] === document.querySelector('[data-robot-transfer="02"]').firstElementChild),
      'changing robot 01 does not rebuild robot 02');
    model.shift.active_robot_ids = ['01'];
    push();
    await card('02').filter({hasText: 'Ngoài phạm vi vận hành'}).waitFor();
    assert.equal(await card('02').getByRole('button', {name: 'Chọn xe', exact: true}).isDisabled(), true);
    model.shift.active_robot_ids = ['01', '02'];
    push();
    await page.waitForFunction(() => !document.querySelector('[data-robot-transfer="02"] [data-vehicle-action="select"]').disabled);
    await card('02').getByRole('button', {name: 'Chọn xe', exact: true}).click();
    model.shift.active_robot_ids = ['01'];
    push();
    await page.waitForFunction(() => selectedAgvId === null);
    model.shift.active_robot_ids = ['01', '02'];
    push();

    for (const id of ['01', '02']) {
      const current = model.fleet.agvs.find(candidate => candidate.id === id);
      Object.assign(current, {online: false, state: 'OFFLINE'});
      const beforeRemoval = commands.filter(command => command.cmd === 'remove_agv').length;
      push();
      await card(id).filter({hasText: 'Mất kết nối'}).waitFor();
      assert.equal(commands.filter(command => command.cmd === 'remove_agv').length, beforeRemoval, 'offline never auto-removes a robot');
      rejectRemoval = true;
      await card(id).getByRole('button', {name: 'Gỡ khỏi Core', exact: true}).click();
      await page.locator('#toastContainer').filter({hasText: 'Robot vẫn giữ mutex zone'}).waitFor();
      assert.match(await card(id).innerText(), /Mất kết nối/);
      assert.doesNotMatch(await card(id).innerText(), /Đã gỡ khỏi Core/);
      rejectRemoval = false;
      await page.waitForFunction(value => !document.querySelector(`[data-robot-transfer="${value}"] [data-vehicle-action="remove"]`).disabled, id);
      await card(id).getByRole('button', {name: 'Gỡ khỏi Core', exact: true}).click();
      await card(id).filter({hasText: 'Đã gỡ khỏi Core'}).waitFor();
      assert.equal(await cards.count(), 2);
      assert.match(await card(id).innerText(), /RFID: — · Pin: —/);
      assert.equal(await card(id).locator('[data-vehicle-action]').count(), 0);
      assert.equal(await page.locator(`#agvsG [data-robot="${id}"]`).count(), 0, 'an absent robot has no invented map marker');
      await card(id).getByRole('button', {name: 'Lịch sử', exact: true}).click();
      await page.locator('#robotHistoryTitle').filter({hasText: `Lịch sử ${id}`}).waitFor();
      model.fleet.agvs.push(robot(id));
      push();
      await card(id).filter({hasText: 'Sẵn sàng'}).waitFor();
      assert.equal(await cards.count(), 2, 'reconnection updates the same card');
      assert.ok(await page.evaluate(value => window.__vehicleCards[Number(value) - 1] === document.querySelector(`[data-robot-transfer="${value}"]`), id));
    }

    const first = model.fleet.agvs.find(candidate => candidate.id === '01');
    Object.assign(first, {state: 'ERROR', error: '<img src=x onerror="window.__xss=1"> lỗi motor',
      node:'K1',heading_confirmation_supported:true,
      recovery_stop_verified:true,heading:'S',heading_supported:true,heading_source:'route'});
    push();
    await card('01').filter({hasText: 'lỗi motor'}).waitFor();
    assert.equal(await page.evaluate(() => window.__xss), undefined);
    await card('01').getByRole('button', {name: 'Khôi phục xe', exact: true}).click();
    await page.locator('#vehicleRecoveryClear').check();await page.locator('#vehicleRecoveryDirection').selectOption('N');
    await page.locator('#vehicleRecoverySubmit').click();
    await page.locator('#vehicleRecoveryStatus').filter({hasText:'Đã khôi phục'}).waitFor();
    await page.locator('#vehicleRecoveryDialog').evaluate(el=>el.close());
    await card('01').filter({hasText: 'Sẵn sàng'}).waitFor();
    Object.assign(first, {online: false, state: 'OFFLINE'});
    push();
    await card('01').getByRole('button', {name: 'Gỡ khỏi Core', exact: true}).waitFor();
    pushEnabled = false;
    await card('01').filter({hasText: 'Dữ liệu cũ'}).waitFor();
    assert.equal(await card('01').getByRole('button', {name: 'Gỡ khỏi Core', exact: true}).isDisabled(), true);
    assert.equal(await card('02').getByRole('button', {name: 'Chọn xe', exact: true}).isDisabled(), true);
    assert.equal(await cards.count(), 2);
    pushEnabled = true;
    Object.assign(first, {online: true, state: 'IDLE'});
    push();
    await card('01').filter({hasText: 'Sẵn sàng'}).waitFor();
    await page.locator('[data-view-target="overview"]').click();
    await page.evaluate(() => document.getElementById('toastContainer').replaceChildren());
    await page.locator('#warehouseOperationsBoard').screenshot({path: path.join(directory, 'desktop-cards.png')});
    await page.setViewportSize({width: 390, height: 844});
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
    await page.locator('#warehouseOperationsBoard').screenshot({path: path.join(directory, 'mobile-cards.png')});

    // A new Core session cannot inherit the browser's previous removal label.
    Object.assign(first, {online: false, state: 'OFFLINE'});
    push();
    await card('01').getByRole('button', {name: 'Gỡ khỏi Core', exact: true}).click();
    await card('01').filter({hasText: 'Đã gỡ khỏi Core'}).waitFor();
    await page.evaluate(() => document.getElementById('toastContainer').replaceChildren());
    await page.locator('#warehouseOperationsBoard').screenshot({path: path.join(directory, 'removed-card-mobile.png')});
    model.core_session_id = 'vehicle-cards-next-session';
    model.fleet.agvs = [];
    push();
    await card('01').filter({hasText: 'Chưa kết nối'}).waitFor();
    await page.reload();
    await page.locator('#agvLinkStatus').filter({hasText: 'Có dữ liệu mới'}).waitFor();
    assert.equal(await cards.count(), 2);
    assert.deepEqual(errors, []);
    console.log(`PASS vehicle cards: fixed 01/02; select/history/recover/remove; rejection/stale/scope; reconnect/session; keyed DOM; desktop/mobile. Evidence: ${directory}`);
  } finally {
    clearInterval(pump);
    if (browser) await browser.close();
    if (gateway) await gateway.close();
    for (const client of core.clients) client.terminate();
    await new Promise(resolve => core.close(resolve));
    db.close();
  }
}
main().catch(error => {console.error(error); process.exitCode = 1;});
