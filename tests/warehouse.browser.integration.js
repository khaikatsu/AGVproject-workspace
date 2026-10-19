const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createGateway}=require('../AGVwebserver/gateway');

async function main() {
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const directory=fs.mkdtempSync(path.resolve(__dirname,'../.qa/warehouse-browser-'));
  const db=createDatabase(path.join(directory,'test.db'));
  const map={map_name:'Kho kiểm thử kệ',nodes:[1,2,3,4,5,6].map(n=>({id:`K${n}`,type:'station',x:n*80,y:n%2*100})),edges:[],obstacles:[]};
  map.nodes.push(...[1,2,3,4,5,6].map(n=>({id:`G${n}`,type:'waypoint',x:n*80,y:50})));
  for(const a of map.nodes)for(const b of map.nodes)if(a.id!==b.id)map.edges.push({from:a.id,to:b.id,weight:1});
  const model={protocol_version:1,core_session_id:'warehouse-browser',mqtt_connected:true,map,
    fleet:{agvs:[],tasks:[],pending_tasks:[],accepting_tasks:true},shift:{supported:true,phase:'CLOSED',can_start:true},mutex:{},logs:[]};
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  let peer,gateway,browser,pump,commandSequence=122;const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',ws=>{peer=ws;push();ws.on('message',raw=>{
    const request=JSON.parse(raw),data={};
    const robot=model.fleet.agvs.find(robot=>robot.id===request.agv_id);
    if(request.cmd==='warehouse_lift') {
      data.command_id=++commandSequence;robot.state=request.action==='pickup'?'LIFTING':'LOWERING';
    }
    else if(request.cmd==='direct_goto') {
      const task={task_id:`task-${model.fleet.tasks.length+1}`,target:request.target,assigned_agv:request.agv_id,status:'dispatched'};
      model.fleet.tasks.push(task);data.task_id=task.task_id;
      Object.assign(robot,{state:'WORKING',task_id:task.task_id,target:request.target,path:[request.target]});
    }
    ws.send(JSON.stringify({type:'command_result',core_session_id:model.core_session_id,request_id:request.request_id,ok:true,data,state:model}));
  });});
  try {
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`});const address=await gateway.start(0,'127.0.0.1');pump=setInterval(push,250);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];page.on('pageerror',error=>errors.push(error.message));
    await page.goto(`http://127.0.0.1:${address.port}`);await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    assert.equal(await page.locator('#operationsTitle').innerText(),'Trạng thái kho hàng');
    await page.locator('#warehouseConfig summary').click();
    for(let n=2;n<=5;n++)await page.locator(`[data-rack-node="K${n}"]`).fill(`R0${n-1}`);
    await page.locator('#warehouseVerified').check();await page.locator('#saveWarehouseBtn').click();
    await page.locator('#warehouseSummary').filter({hasText:'4 kệ'}).waitFor();
    assert.match(await page.locator('#warehouseStateTable').innerText(),/K2\s+R01/);
    await page.reload();await page.locator('#warehouseSummary').filter({hasText:'4 kệ'}).waitFor();
    const robot={id:'01',node:'K2',state:'IDLE',online:true,position_valid:true,task_id:null,
      lift:{supported:true,ready:true,fresh:true,observed_node:'K2',active:false,state:'IDLE',command_id:0},virtual_cargo:false,virtual_cargo_known:false};
    const robot02={...structuredClone(robot),id:'02',node:'G3'};robot02.lift.observed_node='G3';
    model.fleet.agvs=[robot,robot02];model.shift.phase='RUNNING';push();
    await page.waitForFunction(()=>document.querySelector('#saveWarehouseBtn').disabled);
    await page.locator('#orderPickups select').selectOption('K2');await page.locator('#orderDeliveries select').selectOption('K1');
    assert.match(await page.locator('#orderTransferPreview').innerText(),/K2\s+→\s+GIAO ĐẾN\s+K1/);
    assert.equal(await page.locator('#orderTaskForm input[type="number"]').count(),0,'fixed quantity does not need an input');
    await page.locator('#submitOrderBtn').click();
    await page.waitForFunction(()=>document.querySelector('#orderList').textContent.includes('Đang nâng kệ'));
    const firstOrder=(await (await page.request.get(`http://127.0.0.1:${address.port}/api/orders`)).json())[0];
    assert.equal(firstOrder.items[0].robot,'01');
    const card01=page.locator('[data-robot-transfer="01"]'),card02=page.locator('[data-robot-transfer="02"]');
    assert.match(await card01.innerText(),/K2\s+→\s+GIAO ĐẾN\s+K1/);
    assert.match(await page.locator('#warehouseStateTable').innerText(),/K2\s+R01/);
    robot.state='IDLE';robot.virtual_cargo=true;robot.virtual_cargo_known=true;Object.assign(robot.lift,{command_id:123,state:'DONE_UP'});push();
    await page.waitForFunction(()=>document.querySelector('#warehouseStateTable').textContent.includes('Trên xe'));
    await page.waitForFunction(()=>document.querySelector('#orderList').textContent.includes('Đưa kệ tới vị trí đích'));
    const response02=await page.request.post(`http://127.0.0.1:${address.port}/api/orders`,{data:{pickups:[{pickup:'K3',quantity:1}],deliveries:[{dropoff:'K6',quantity:1}]}});
    assert.equal(response02.status(),201);
    await card02.filter({hasText:'Đang đi lấy tại K3'}).waitFor();
    assert.match(await card01.innerText(),/Đang giao đến K1/);
    assert.match(await card02.innerText(),/K3\s+→\s+GIAO ĐẾN\s+K6/);
    assert.match(await page.locator('[data-warehouse-slot="K2"]').innerText(),/Đầu ca: R01/);
    assert.match(await page.locator('[data-warehouse-slot="K1"]').innerText(),/R01.*xe 01/s);
    assert.match(await page.locator('[data-warehouse-slot="K6"]').innerText(),/R02.*xe 02/s);
    await page.locator('#warehouseOperationsBoard').screenshot({path:path.join(directory,'desktop-operations.png')});
    await page.locator('#orderTaskForm').screenshot({path:path.join(directory,'transfer-form.png')});
    await page.evaluate(()=>document.getElementById('toastContainer').replaceChildren());
    await page.setViewportSize({width:390,height:844});
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'operations board fits mobile');
    await page.locator('#warehouseOperationsBoard').screenshot({path:path.join(directory,'mobile-operations.png')});
    await page.setViewportSize({width:1440,height:1000});
    robot02.state='ERROR';robot02.error='Lỗi động cơ (fixture)';push();
    await card02.filter({hasText:'Cần kiểm tra xe và kệ'}).waitFor();
    assert.equal(await card02.locator('[aria-current="step"]').count(),0,'interrupted route has no live step');
    assert.equal(await page.locator('[data-warehouse-slot="K3"]').getAttribute('data-tone'),'bad');
    const arrival=model.fleet.tasks.find(task=>task.task_id===robot.task_id);arrival.status='completed';
    Object.assign(robot,{node:'K1',state:'IDLE',task_id:null,target:'',path:[]});robot.lift.observed_node='K1';push();
    await page.waitForFunction(()=>document.querySelector('#orderList').textContent.includes('Đang hạ kệ'));
    // Each real command has a new ID; this fixture also returns a distinct ID for dropoff.
    const dropoff=(await page.request.get(`http://127.0.0.1:${address.port}/api/orders`));
    const item=(await dropoff.json()).find(order=>order.id===firstOrder.id).items[0];
    robot.state='IDLE';robot.virtual_cargo=false;Object.assign(robot.lift,{command_id:item.lift_command_id,state:'DONE_DOWN'});push();
    await page.waitForFunction(()=>document.querySelector('#warehouseStateTable').textContent.match(/K1R01/));
    const exit=model.fleet.tasks.find(task=>task.task_id===robot.task_id);exit.status='completed';
    Object.assign(robot,{node:'G1',state:'IDLE',task_id:null,target:'',path:[]});push();
    await page.waitForFunction(()=>document.querySelector('#orderList').textContent.includes('Đã đặt kệ'));
    model.mqtt_connected=false;push();
    await page.waitForFunction(()=>document.querySelector('#warehouseOperationsBoard').dataset.stale==='true');
    assert.match(await page.locator('#operationsFreshness').innerText(),/Dữ liệu cũ/);
    assert.equal(await page.locator('#submitOrderBtn').isDisabled(),true);
    model.mqtt_connected=true;push();
    model.shift.phase='CLOSING';push();await page.locator('[data-view-target="reports"]').click();
    assert.equal(await page.locator('#warehouseReportList button').count(),0);
    model.shift.phase='CLOSED';push();await page.locator('#warehouseReportList button').waitFor();
    await page.locator('#warehouseReportDetail').filter({hasText:'Nâng: 1 · Hạ: 1'}).waitFor();
    const download=page.waitForEvent('download');await page.locator('#warehouseReportDetail button').click();
    await (await download).saveAs(path.join(directory,'warehouse-report.json'));
    const saved=JSON.parse(fs.readFileSync(path.join(directory,'warehouse-report.json'),'utf8'));
    assert.equal(saved.final_state.slots.find(slot=>slot.node==='K1').rack_id,'R01');
    await page.screenshot({path:path.join(directory,'desktop-report.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});await page.locator('#warehousePanel').scrollIntoViewIfNeeded();
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'mobile does not overflow');
    await page.screenshot({path:path.join(directory,'mobile-report.png'),fullPage:true});
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('[data-view-target="tasks"]').click();
    const attentionOrders=await (await page.request.get(`http://127.0.0.1:${address.port}/api/orders`)).json();
    const attentionOrder=attentionOrders.find(order=>order.status==='needs_attention');assert.ok(attentionOrder);
    const resolvedEntry=page.locator(`[data-order-id="${attentionOrder.id}"]`);
    assert.equal(await resolvedEntry.count(),1,'unresolved orders stay visible');
    page.once('dialog',dialog=>dialog.accept());
    await resolvedEntry.getByRole('button',{name:'Đã đối soát',exact:true}).click();
    await page.waitForFunction(id=>!document.querySelector(`[data-order-id="${id}"]`),attentionOrder.id);
    assert.equal(await page.locator('#orderResolvedFilterLabel').innerText(),'Hiện đơn đã đối soát (1)');
    assert.equal(await page.locator(`#orderProgressSelect option[value="${attentionOrder.id}"]`).count(),0);
    assert.equal(await page.locator(`[data-order-id="${firstOrder.id}"]`).count(),1,'other completed orders stay visible');
    await page.locator('#orderShowResolved').check();await resolvedEntry.waitFor();
    assert.match(await resolvedEntry.innerText(),/Đã đóng sau đối soát/);
    assert.equal(await page.locator(`#orderProgressSelect option[value="${attentionOrder.id}"]`).count(),1);
    await page.reload();await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    assert.equal(await page.locator('#orderShowResolved').isChecked(),false);
    assert.equal(await resolvedEntry.count(),0,'closed orders are hidden after reload');
    const stored=await (await page.request.get(`http://127.0.0.1:${address.port}/api/orders`)).json();
    assert.equal(stored.find(order=>order.id===attentionOrder.id).status,'closed','filter never deletes stored history');
    const unchangedReport=await (await page.request.get(`http://127.0.0.1:${address.port}/api/warehouse/reports/${saved.id}`)).json();
    assert.deepEqual(unchangedReport,saved,'saved report is unchanged');
    assert.deepEqual(errors,[]);console.log(`PASS warehouse browser: form, two robot routes, slot baselines/reservations, interruption, servo moves, CLOSED report, JSON download, desktop/mobile, resolved-order filter/title/history preserved. Evidence: ${directory}`);
  } finally {
    clearInterval(pump);if(browser)await browser.close();if(gateway)await gateway.close();
    for(const client of core.clients)client.terminate();await new Promise(resolve=>core.close(resolve));db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
