const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createWarehouseInventory}=require('../AGVwebserver/warehouse-inventory');
const {createGateway}=require('../AGVwebserver/gateway');

async function main() {
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const directory=fs.mkdtempSync(path.resolve(__dirname,'../.qa/warehouse-rollback-'));
  const filename=path.join(directory,'test.db');let db=createDatabase(filename),gateway,browser,pump,peer;
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const model={protocol_version:1,core_session_id:'rollback-browser',mqtt_connected:true,
    map:{nodes:[1,2,3,4].flatMap(n=>[{id:`K${n}`,type:'station',x:n*150,y:100},{id:`G${n}`,type:'waypoint',x:n*150,y:250}]),
      edges:[1,2,3,4].map(n=>({from:`K${n}`,to:`G${n}`,weight:1,two_way:true})),obstacles:[]},
    fleet:{agvs:[],tasks:[],pending_tasks:[],accepting_tasks:true},mutex:{},logs:[],
    shift:{supported:true,phase:'CLOSED',active_robot_ids:['01','02'],robots:[],home_stations:{}}};
  const inventory=createWarehouseInventory({db});
  inventory.configure({expected_revision:0,physically_verified:true,slots:[1,2,3,4].map(n=>({node:`K${n}`,rack_id:n%2?`R${n}`:null}))},model);
  for(const [id,source,target,robot] of [['interrupted-01','K1','K2','01'],['interrupted-02','K3','K4','02']]) {
    const rack_id=source==='K1'?'R1':'R3',command=robot==='01'?71:72;
    const vehicle={id:robot,node:source,state:'IDLE',online:true,position_valid:true,task_id:null,target:'',path:[],
      virtual_cargo:true,virtual_cargo_known:true,
      lift:{supported:true,ready:true,fresh:true,active:false,command_id:command,state:'DONE_UP',observed_node:source}};
    model.fleet.agvs.push(vehicle);
    inventory.reserve({key:`order:${id}:1`,pickup:source,dropoff:target,robot});
    inventory.confirmLift(`order:${id}:1`,'pickup',vehicle,command,model.core_session_id);
    inventory.interrupt(`order:${id}:1`,'Đã dừng lượt thử');
    db.saveOrder({id,status:robot==='02'?'closed':'needs_attention',
      ...(robot==='02'?{deleted_at:new Date().toISOString(),closed_at:new Date().toISOString()}:{}),
      total:1,created:new Date(0).toISOString(),items:[
      {number:1,pickup:source,dropoff:target,robot,rack_id,status:'interrupted'}]});
  }
  for(let n=0;n<55;n++)db.saveOrder({id:`newer-closed-${n}`,status:'closed',total:0,created:new Date(Date.now()+n).toISOString(),items:[]});
  model.shift.phase='RUNNING';const commands=[],errors=[],dialogs=[];let accept=false,reject=false;
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',ws=>{peer=ws;push();ws.on('message',raw=>{
    const request=JSON.parse(raw);commands.push(request);
    if(request.cmd!=='reconcile_test_cargo')throw new Error(`Unexpected ${request.cmd}`);
    if(!reject)for(const entry of request.robots) {
      const vehicle=model.fleet.agvs.find(robot=>robot.id===entry.id);vehicle.virtual_cargo=false;vehicle.virtual_cargo_known=true;
    }
    ws.send(JSON.stringify({type:'command_result',request_id:request.request_id,core_session_id:model.core_session_id,
      ok:!reject,status:409,error:'Chờ xe dừng',data:{reconciled:request.robots},state:model}));
  });});
  const start=async()=>{
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`,broadcastIntervalMs:0});
    return await gateway.start(0,'127.0.0.1');
  };
  try {
    let address=await start();pump=setInterval(push,100);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1440,height:1050}});
    page.on('pageerror',error=>errors.push(error.message));
    page.on('dialog',dialog=>{dialogs.push(dialog.message());return accept?dialog.accept():dialog.dismiss();});
    await page.goto(`http://127.0.0.1:${address.port}`);
    await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    await page.locator('[data-view-target="overview"]').click();
    const legacyButton=page.locator('[data-warehouse-rollback-order="interrupted-02"]');
    await legacyButton.waitFor();assert.equal(await legacyButton.textContent(),'Hoàn tác về K3');
    assert.equal(await page.locator('[data-order-id="interrupted-02"]').count(),0,'legacy deleted order remains hidden from mission history');
    await page.waitForFunction(()=>lastState.orders.some(order=>order.id==='interrupted-02'&&order.warehouse_rollback_pending));
    const entry=page.locator('[data-order-id="interrupted-01"]');
    const button=entry.getByRole('button',{name:'Hoàn tác về K1',exact:true});await button.waitFor();
    await button.click();assert.equal(commands.length,0,'dismiss makes no change');
    assert.match(dialogs[0],/kệ.*về kho lấy.*xe đã trống/);
    accept=true;reject=true;await button.click();
    await page.locator('#toastContainer').filter({hasText:'Chờ xe dừng'}).waitFor();
    assert.equal(db.getOrders().find(order=>order.id==='interrupted-01').status,'needs_attention');
    assert.equal(inventory.snapshot().racks.find(rack=>rack.id==='R1').location.type,'robot');
    await page.screenshot({path:path.join(directory,'desktop-before.png'),fullPage:true});
    reject=false;await button.click();await entry.waitFor({state:'detached'});
    await page.waitForFunction(()=>lastState.warehouse.racks.find(rack=>rack.id==='R1').location.id==='K1'&&
      lastState.fleet.agvs.find(robot=>robot.id==='01').virtual_cargo===false);
    assert.equal(inventory.snapshot().racks.find(rack=>rack.id==='R3').location.id,'02','the other vehicle stays unchanged');
    await page.setViewportSize({width:390,height:844});
    model.fleet.agvs[1].online=false;push();await page.waitForFunction(()=>lastState.fleet.agvs[1].online===false);
    await legacyButton.click();await page.locator('#toastContainer').filter({hasText:'Kết nối lại xe 02'}).waitFor();
    assert.equal(commands.length,2,'missing/offline robot cannot clear warehouse data');
    assert.equal(inventory.snapshot().racks.find(rack=>rack.id==='R3').location.id,'02');
    model.fleet.agvs[1].online=true;push();await page.waitForFunction(()=>lastState.fleet.agvs[1].online===true);
    await page.locator('#warehouseCarriedRacks').screenshot({path:path.join(directory,'mobile-legacy-button.png')});
    await legacyButton.click();await legacyButton.waitFor({state:'detached'});
    await page.waitForFunction(()=>lastState.warehouse.moves.length===0&&lastState.fleet.agvs.every(robot=>robot.virtual_cargo===false));
    assert.equal(await page.locator('[data-virtual-cargo]').count(),0);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
    await page.screenshot({path:path.join(directory,'mobile-after.png'),fullPage:true});
    assert.deepEqual(commands.map(command=>command.cmd),['reconcile_test_cargo','reconcile_test_cargo','reconcile_test_cargo']);
    assert.equal(inventory.history().filter(event=>event.type==='rollback').length,2);
    assert.ok(db.getOrders().find(order=>order.id==='interrupted-02').deleted_at,'reconciliation keeps display deletion');
    await page.goto('about:blank');await gateway.close();gateway=null;db.close();db=createDatabase(filename);
    address=await start();await page.goto(`http://127.0.0.1:${address.port}`);
    await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    await page.waitForFunction(()=>lastState.warehouse.racks.every(rack=>rack.location.type==='slot')&&lastState.warehouse.moves.length===0);
    assert.ok(db.getOrders().every(order=>order.status==='closed'));assert.deepEqual(errors,[]);
    console.log(`PASS warehouse rollback: confirm/cancel/Core rejection, legacy closed/deleted order beyond 50, panel button, offline, two robots, mobile, restart. Evidence: ${directory}`);
  } finally {
    clearInterval(pump);if(browser)await browser.close();if(gateway)await gateway.close();
    for(const client of core.clients)client.terminate();await new Promise(resolve=>core.close(resolve));db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
