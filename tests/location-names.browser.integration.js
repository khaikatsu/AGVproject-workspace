const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createGateway}=require('../AGVwebserver/gateway');

async function main() {
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.resolve(__dirname,'../.qa/location-names-browser-'));
  const db=createDatabase(path.join(directory,'test.db')),commands=[];
  const nodes=[{id:'K1',type:'station',x:0,y:0},{id:'G1',type:'waypoint',x:30,y:0},
    {id:'K4',type:'station',x:200,y:0},{id:'G4',type:'waypoint',x:170,y:0},
    {id:'S1',type:'charging',x:80,y:-70},{id:'S2',type:'charging',x:120,y:70}];
  const model={protocol_version:1,core_session_id:'names-session',mqtt_connected:true,
    map:{revision:1,map_name:'Kho kiểm thử tên',nodes,edges:nodes.flatMap(a=>nodes.filter(b=>b!==a)
      .map(b=>({from:a.id,to:b.id,weight:30,two_way:true}))),obstacles:[]},
    fleet:{agvs:['01','02'].map((id,i)=>({id,node:i?'S2':'S1',state:'IDLE',online:true,position_valid:true,
      last_seen_s:0,path:[],task_id:'',battery:80,battery_measured:true})),tasks:[],pending_tasks:[],accepting_tasks:false},
    shift:{supported:true,phase:'CLOSED',home_stations:{'01':'S1','02':'S2'},robots:[],can_start:true},mutex:{},logs:[]};
  db.saveOrder({id:'old-order',total:1,status:'completed',items:[{number:1,pickup:'K1',dropoff:'K4',status:'delivered'}],created:'2026-10-26T00:00:00Z'});
  let peer,gateway,browser,pump;
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',socket=>{peer=socket;push();socket.on('message',raw=>{
    const request=JSON.parse(raw);commands.push(request);
    assert.ok(['add_task','direct_goto'].includes(request.cmd),'fixture receives only explicitly requested tasks');
    const task={task_id:`names-task-${commands.length}`,target:request.target,priority:0,status:'completed',assigned_agv:request.agv_id||''};
    model.fleet.tasks.push(task);
    socket.send(JSON.stringify({type:'command_result',request_id:request.request_id,core_session_id:model.core_session_id,
      ok:true,data:{task_id:task.task_id},state:model}));
  });});
  const start=async()=>{
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`,staleMs:1600});
    return `http://127.0.0.1:${(await gateway.start(0,'127.0.0.1')).port}`;
  };
  try {
    let base=await start();pump=setInterval(push,200);
    const call=async(route,body)=>{const response=await fetch(base+route,{method:body?'POST':'GET',
      headers:body?{'Content-Type':'application/json'}:undefined,body:body?JSON.stringify(body):undefined});
      assert.equal(response.ok,true,`${route}: ${response.status}`);return response.json();};
    for(let tries=0;tries<50;tries++){if((await call('/api/health')).core_ready)break;await new Promise(r=>setTimeout(r,30));}
    await call('/api/warehouse/configure',{expected_revision:0,physically_verified:true,slots:[{node:'K1',rack_id:'R01'},{node:'K4',rack_id:null}]});
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
    const other=await browser.newPage();other.on('pageerror',error=>errors.push(error.message));
    page.setDefaultTimeout(10000);other.setDefaultTimeout(10000);
    page.on('pageerror',error=>errors.push(error.message));
    await page.goto(base);await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    await other.goto(base);await other.waitForFunction(()=>lastState?.location_names?.revision===0);
    await page.locator('[data-view-target="map"]').click();
    await page.waitForFunction(()=>!document.querySelector('#swapLocationNamesBtn').disabled);
    await page.locator('#locationNameFirst').selectOption('K1');await page.locator('#locationNameSecond').selectOption('K4');
    await page.route('**/api/map/location-names/swap',route=>route.fulfill({status:500,contentType:'application/json',body:JSON.stringify({ok:false,error:'Lỗi lưu kiểm thử'})}));
    const failed=page.waitForResponse(response=>response.url().endsWith('/location-names/swap')&&response.status()===500);
    await page.locator('#swapLocationNamesBtn').click();await failed;
    await page.locator('#toastContainer').filter({hasText:'Lỗi lưu kiểm thử'}).waitFor();
    assert.equal(await page.locator('[data-node-label="K1"]').textContent(),'K1');
    await page.unroute('**/api/map/location-names/swap');
    await page.locator('#swapLocationNamesBtn').click();
    await page.waitForFunction(()=>lastState.location_names.revision===1);
    await other.waitForFunction(()=>lastState.location_names.revision===1);
    assert.equal(await other.locator('[data-node-label="K1"]').textContent(),'K4');
    assert.equal(await page.locator('[data-node-label="K1"]').textContent(),'K4');
    assert.equal(await page.locator('[data-node-label="K4"]').textContent(),'K1');
    assert.match(await page.locator('[data-node="K1"] title').textContent(),/K4 \(vị trí K1\)/);
    assert.equal(commands.length,0,'renaming sends no movement/config commands');
    await page.locator('#locationNameFirst').selectOption('S1');
    assert.deepEqual(await page.locator('#locationNameSecond option').allTextContents(),['S2']);
    await page.locator('#swapLocationNamesBtn').click();await page.waitForFunction(()=>lastState.location_names.revision===2);
    assert.equal(await page.locator('[data-node-label="S1"]').textContent(),'S2');
    await page.locator('#locationNameFirst').selectOption('K4');
    await page.locator('#locationNameSecond').selectOption('K1');
    await page.locator('#locationNamesEditor').screenshot({path:path.join(directory,'desktop-editor.png')});
    await page.setViewportSize({width:390,height:844});
    await page.locator('#locationNamesEditor').scrollIntoViewIfNeeded();
    await page.locator('#locationNamesEditor').screenshot({path:path.join(directory,'mobile-editor.png')});
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('[data-view-target="overview"]').click();
    assert.match(await page.locator('[data-warehouse-slot="K1"]').innerText(),/K4 \(vị trí K1\).*R01/s);
    assert.match(await page.locator('[data-robot-transfer="01"]').innerText(),/Trạm riêng: S2 \(vị trí S1\)/);
    assert.match(await page.locator('[data-order-id="old-order"]').innerText(),/K1 → K4/,'old history is not relabelled');
    await page.locator('#warehouseConfig summary').click();
    await page.locator('#warehouseConfigFields label').first().waitFor();
    assert.match(await page.locator('#warehouseConfigFields').innerText(),/K4 \(vị trí K1\)/);
    await page.locator('#warehouseConfig summary').click();
    model.shift.phase='RUNNING';model.fleet.accepting_tasks=true;push();
    await page.waitForFunction(()=>lastState.shift.phase==='RUNNING');
    await page.locator('[data-view-target="map"]').click();
    assert.equal(await page.locator('#swapLocationNamesBtn').isDisabled(),true);
    await page.locator('[data-view-target="overview"]').click();
    await page.locator('#nodeModeBtn').click();
    for(const [index,id] of ['01','02'].entries()) {
      await page.locator('#task_agv').selectOption(id);await page.locator('#task_target').fill('K4');
      await page.locator('#submitTaskBtn').click();
      await page.waitForFunction(count=>lastState.fleet.tasks.length===count,index+1);
      assert.equal(commands.at(-1).target,'K1');assert.equal(commands.at(-1).agv_id,id);
    }
    await page.locator('[data-robot-transfer="02"]').getByRole('button',{name:'Chọn xe',exact:true}).click();
    await page.locator('[data-node="K4"]').click();
    await page.waitForFunction(()=>lastState.fleet.tasks.length===3);
    assert.equal(commands.at(-1).target,'K4','clicking renamed K1 addresses physical K4 exactly once');
    await page.locator('#orderModeBtn').click();
    await page.locator('#orderPickups select').selectOption('K1');
    await page.locator('#orderDeliveries select').selectOption('K4');
    assert.match(await page.locator('#orderTransferPreview').innerText(),/K4 \(vị trí K1\).*K1 \(vị trí K4\)/s);
    const sent=page.waitForRequest(request=>request.method()==='POST'&&request.url().endsWith('/api/orders'));
    await page.locator('#submitOrderBtn').click();const payload=(await sent).postDataJSON();
    assert.equal(payload.pickups[0].pickup,'K4');assert.equal(payload.deliveries[0].dropoff,'K1');
    await page.waitForFunction(()=>lastState.orders.some(order=>order.id!=='old-order'));
    const order=(await call('/api/orders')).find(order=>order.id!=='old-order');
    assert.equal(order.items[0].pickup,'K1');assert.equal(order.items[0].dropoff,'K4');
    assert.equal(order.location_names.revision,2);
    await page.locator('#missionBoard').screenshot({path:path.join(directory,'named-missions.png')});
    await other.close();await page.goto('about:blank');
    await gateway.close();gateway=null;
    base=await start();await page.goto(base);
    await page.waitForFunction(()=>lastState?.location_names?.revision===2);
    assert.equal(await page.locator('[data-node-label="K1"]').textContent(),'K4');
    assert.match(await page.locator('[data-order-id="old-order"]').innerText(),/K1 → K4/);
    assert.deepEqual(errors,[]);await page.goto('about:blank');
    console.log(`PASS location names: swap warehouses/stations, fixed UID destinations for 01/02, map click, orders, history, rack/home identity, desktop/mobile, gateway restart. Artifacts: ${directory}`);
  } finally {
    clearInterval(pump);if(browser)await browser.close();if(gateway)await gateway.close();
    for(const socket of core.clients)socket.terminate();await new Promise(resolve=>core.close(resolve));db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
