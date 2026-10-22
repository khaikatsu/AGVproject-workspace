const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createGateway}=require('../AGVwebserver/gateway');

async function main() {
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.resolve(__dirname,'../.qa/shift-start-browser-'));
  const db=createDatabase(path.join(directory,'test.db'));
  const model={protocol_version:1,core_session_id:'shift-start-browser',mqtt_connected:true,
    map:{nodes:[1,2,3,4,5,6].map(n=>({id:`K${n}`,type:'station',x:n*50,y:50})),edges:[],obstacles:[]},
    fleet:{agvs:['01','02'].map(id=>({id,node:`S${id==='01'?1:2}`,state:'IDLE',online:true,position_valid:true,path:[],target:'',task_id:null})),tasks:[],pending_tasks:[],accepting_tasks:false},
    shift:{supported:true,phase:'CLOSED',can_start:true},mutex:{},logs:[]};
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  let peer,browser,gateway,pump,rejectStart=true,startCount=0,delayStart=0;
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',ws=>{peer=ws;push();ws.on('message',raw=>{
    const request=JSON.parse(raw);if(request.cmd!=='start_shift')return;
    startCount++;
    setTimeout(()=>{
      if(!rejectStart){model.shift.phase='RUNNING';model.shift.can_start=false;model.fleet.accepting_tasks=true;}
      if(ws.readyState===WebSocket.OPEN)ws.send(JSON.stringify({type:'command_result',request_id:request.request_id,
        core_session_id:model.core_session_id,ok:!rejectStart,status:rejectStart?409:200,error:rejectStart?'Fixture: xe chưa sẵn sàng':undefined,
        data:{shift:model.shift},state:model}));
    },delayStart);
  });});
  try {
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;pump=setInterval(push,200);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[],posts=[];
    page.on('pageerror',error=>errors.push(error.message));
    page.on('request',request=>{if(request.method()==='POST')posts.push(new URL(request.url()).pathname);});
    const dialog=page.locator('#shiftWarehouseDialog'),fields=page.locator('#shiftWarehouseFields');
    const readWarehouse=async()=>(await page.request.get(`${base}/api/warehouse`)).json();
    const open=async()=>{await page.locator('#startShiftBtn:enabled').click();await dialog.waitFor();};
    const confirm=async()=>{await page.locator('#shiftWarehouseVerified').check();await page.locator('#confirmStartShiftBtn').click();};
    await page.goto(base);await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    await open();assert.equal(await fields.locator('input').count(),6);
    assert.deepEqual(await fields.locator('input').evaluateAll(inputs=>inputs.map(input=>input.value)),Array(6).fill(''));
    await fields.locator('[data-rack-node="K2"]').fill('R01');await page.locator('#cancelStartShiftBtn').click();
    assert.equal((await readWarehouse()).initialized,false);assert.equal(posts.length,0,'cancel has no writes');
    await open();await page.locator('#confirmStartShiftBtn').click();
    assert.equal(posts.length,0,'physical verification is required');
    await fields.locator('[data-rack-node="K2"]').fill('R01');await fields.locator('[data-rack-node="K3"]').fill('R01');
    await confirm();await page.locator('#shiftWarehouseStatus').filter({hasText:'Chưa xác nhận lưu kho'}).waitFor();
    assert.equal(startCount,0,'rejected inventory must not open the shift');
    await fields.locator('[data-rack-node="K3"]').fill('R02');await confirm();
    await page.locator('#shiftWarehouseStatus').filter({hasText:'Đã lưu kho; chưa xác nhận mở ca'}).waitFor();
    assert.equal(startCount,1);assert.equal(model.shift.phase,'CLOSED');
    assert.equal((await readWarehouse()).slots.find(slot=>slot.node==='K2').rack_id,'R01');
    assert.deepEqual(posts.slice(-2),['/api/warehouse/configure','/api/shift/start']);
    assert.equal(await fields.locator('[data-rack-node="K3"]').inputValue(),'R02','failed start preserves the form');
    await page.route('**/api/shift/start',route=>route.fulfill({status:504,contentType:'application/json',body:JSON.stringify({ok:false,unknown:true,error:'Fixture: chưa xác định kết quả'})}));
    const postsBeforeUnknown=posts.length;
    await confirm();await page.locator('#shiftWarehouseStatus').filter({hasText:'Hệ thống không tự gửi lại lệnh'}).waitFor();
    assert.deepEqual(posts.slice(postsBeforeUnknown),['/api/warehouse/configure','/api/shift/start'],'unknown result is not retried');
    assert.equal(startCount,1);assert.equal(await dialog.isVisible(),true);
    await page.unroute('**/api/shift/start');
    await page.locator('#cancelStartShiftBtn').click();await open();
    assert.equal(await fields.locator('[data-rack-node="K2"]').inputValue(),'R01','saved layout is prefilled');
    assert.equal(await page.locator('#shiftWarehouseVerified').isChecked(),false,'each opening requires verification');
    // Another operator changes the layout: do not overwrite with this stale draft.
    const current=await readWarehouse();
    assert.equal((await page.request.post(`${base}/api/warehouse/configure`,{data:{slots:current.slots,expected_revision:current.revision,physically_verified:true}})).status(),200);
    await page.locator('#shiftWarehouseHint').filter({hasText:'đã thay đổi'}).waitFor();
    assert.equal(await page.locator('#confirmStartShiftBtn').isDisabled(),true);assert.equal(startCount,1);
    await page.locator('#cancelStartShiftBtn').click();await open();
    model.mqtt_connected=false;push();await page.waitForFunction(()=>document.querySelector('#confirmStartShiftBtn').disabled);
    assert.equal(await fields.locator('input').first().isDisabled(),true);
    model.mqtt_connected=true;push();await page.waitForFunction(()=>!document.querySelector('#confirmStartShiftBtn').disabled);
    assert.equal(await fields.locator('[data-rack-node="K2"]').inputValue(),'R01');
    model.core_session_id='new-core-session';push();
    await page.locator('#shiftWarehouseHint').filter({hasText:'đã thay đổi'}).waitFor();
    assert.equal(await page.locator('#confirmStartShiftBtn').isDisabled(),true);
    await page.locator('#cancelStartShiftBtn').click();await open();
    await page.setViewportSize({width:390,height:844});
    assert.ok(await dialog.evaluate(element=>element.getBoundingClientRect().width<=innerWidth),'dialog fits mobile');
    await dialog.screenshot({path:path.join(directory,'mobile-open-shift.png')});
    await page.setViewportSize({width:1440,height:1000});
    await dialog.screenshot({path:path.join(directory,'desktop-open-shift.png')});
    rejectStart=false;delayStart=600;
    await confirm();await page.locator('#shiftWarehouseStatus').filter({hasText:'Đang chờ Core'}).waitFor();
    assert.equal(await page.locator('#confirmStartShiftBtn').isDisabled(),true);
    assert.equal(await page.locator('#cancelStartShiftBtn').isDisabled(),true);
    await page.keyboard.press('Escape');assert.equal(await dialog.isVisible(),true,'cannot hide an in-flight operation');
    await page.evaluate(()=>confirmShiftWarehouse());
    await dialog.waitFor({state:'hidden'});assert.equal(startCount,2,'double submit sends one start');
    const saved=await readWarehouse();
    assert.equal(model.shift.phase,'RUNNING');
    assert.equal(saved.shift.initial_slots.find(slot=>slot.node==='K2').rack_id,'R01','baseline uses saved layout');
    assert.equal(await page.locator('#saveWarehouseBtn').isDisabled(),true);
    assert.equal(await page.locator('#startShiftBtn').isDisabled(),true);
    assert.deepEqual(errors,[]);
    console.log(`PASS shift start: cancel, validation, save/start ordering, refused/unknown start, prefill, revision/session conflicts, MQTT lock, double submit, initial snapshot, desktop/mobile. Evidence: ${directory}`);
  } finally {
    clearInterval(pump);if(browser)await browser.close();if(gateway)await gateway.close();
    for(const client of core.clients)client.terminate();await new Promise(resolve=>core.close(resolve));db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
