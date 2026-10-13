// Core, MQTT, DB và cổng riêng; không tác động phiên mô phỏng hoặc robot vận hành.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createLauncher}=require('../simulation/launcher');
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function main(){
  const pw=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const parent=path.resolve(__dirname,'../.qa/simulation-connection');fs.mkdirSync(parent,{recursive:true});
  const launcher=createLauncher({port:0,runParent:parent});let browser;
  try{
    const address=await launcher.start(),entry=`http://127.0.0.1:${address.port}/simulation`;
    browser=await pw.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const context=await browser.newContext({viewport:{width:1366,height:900}}),page=await context.newPage(),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.goto(entry);
    await page.locator('#simHealth').filter({hasText:'Sẵn sàng'}).waitFor();
    await page.locator('#startShiftBtn:enabled').waitFor();
    const oldUrl=page.url(),base=new URL(oldUrl).origin;
    assert.equal(await page.locator('#simOpenLauncher').getAttribute('href'),entry);
    const controls=()=>page.evaluate(()=>[...document.querySelectorAll('[data-sim-action],#simGoto01,#simGoto02,#simBlockEdge,#simUnblockEdge,#simTimeScale,#simSpeed,#startShiftBtn,#submitOrderBtn')].every(n=>n.disabled));
    const disconnected=()=>page.waitForFunction(()=>document.body.dataset.simConnection==='offline');
    const ready=async()=>{await page.locator('#simHealth').filter({hasText:'Sẵn sàng'}).waitFor();await page.locator('#simSpeed:enabled').waitFor();};

    // Browser offline: dữ liệu vẫn hiện, hoạt ảnh đứng yên và mọi lệnh khóa.
    await context.setOffline(true);await disconnected();assert.ok(await controls());
    assert.match(await page.locator('#status').textContent(),/Mất mạng/);
    assert.doesNotMatch(await page.locator('#mqttStatus').textContent(),/Đã kết nối/);
    assert.equal(await page.locator('[data-sim-robot]').count(),2);
    assert.match(await page.locator('#simPosition01').textContent(),/Dữ liệu cũ/);
    const before=await page.locator('[data-sim-robot="01"]').getAttribute('transform');
    await wait(600);assert.equal(await page.locator('[data-sim-robot="01"]').getAttribute('transform'),before);
    await page.screenshot({path:path.join(parent,'offline.png'),fullPage:true});
    await context.setOffline(false);await ready();assert.equal(page.url(),oldUrl);

    // HTTP lỗi dù WS còn sống: dữ liệu cũ phải khóa cả nút dashboard.
    let mode='http-error',reads=0;
    await page.route('**/sim-api/state',async route=>{
      reads++;
      if(mode==='http-error')return route.fulfill({status:503,contentType:'application/json',body:'{"error":"fixture unavailable"}'});
      if(mode==='malformed')return route.fulfill({status:200,contentType:'application/json',body:'{"simulation":true,"robots":[]}'});
      if(mode==='core-wait'){const response=await route.fetch(),data=await response.json();data.health='Đang chờ Core / MQTT';return route.fulfill({response,json:data});}
      if(mode==='hang'){await wait(4500);return route.abort().catch(()=>{});}
      return route.continue();
    });
    await disconnected();assert.ok(await controls());await wait(2300);assert.ok(reads<8,'retry backoff tránh gọi dồn');
    mode='normal';await page.locator('#simRetryConnection').click();await ready();
    mode='core-wait';await page.locator('#simConnectionText').filter({hasText:'đang chờ Core / MQTT'}).waitFor();assert.ok(await controls());
    mode='normal';await page.locator('#simRetryConnection').click();await ready();
    mode='malformed';await disconnected();assert.ok(await controls());assert.equal(await page.locator('[data-sim-robot]').count(),2);
    mode='normal';await page.locator('#simRetryConnection').click();await ready();
    mode='hang';const began=Date.now();await disconnected();assert.ok(Date.now()-began<5000,'request treo có timeout');assert.ok(await controls());
    mode='normal';await page.locator('#simRetryConnection').click();await ready();

    // Chỉ mất WebSocket: HTTP còn tốt nhưng điều khiển vẫn khóa, rồi tự nối lại.
    let blockWs=true;
    await page.routeWebSocket('**/ws?delta=1',socket=>blockWs?socket.close({code:1013,reason:'fixture disconnect'}):socket.connectToServer());
    await page.evaluate(()=>ws.close());
    await page.locator('#simConnectionText').filter({hasText:'Mất kết nối WebSocket'}).waitFor();assert.ok(await controls());
    blockWs=false;await ready();

    // Mất phản hồi POST sau khi server đã xử lý: cảnh báo unknown, không phát lại.
    let commands=0;
    await page.route('**/sim-api/robots/01/action',async route=>{commands++;await route.fetch();await route.abort();});
    await page.locator('.sim-vehicle[data-robot="01"] > .sim-actions [data-sim-action="pause"]').click();
    await page.locator('.toast').filter({hasText:'Chưa xác định kết quả lệnh'}).waitFor();
    await ready();await wait(800);assert.equal(commands,1,'không tự phát lại POST');
    await page.unroute('**/sim-api/robots/01/action');

    // Phiên hết hạn/đóng: link cổng mở trang tạo phiên mới, không phục hồi lệnh cũ.
    await page.request.post(base+'/sim-api/stop',{data:{confirm:true}});
    await disconnected();assert.ok(await controls());await page.locator('#simOpenLauncher').waitFor({state:'visible'});
    await page.locator('#simOpenLauncher').click();await ready();assert.notEqual(page.url(),oldUrl);
    await page.setViewportSize({width:390,height:844});
    await context.setOffline(true);await disconnected();
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'mobile không tràn ngang');
    await page.screenshot({path:path.join(parent,'offline-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[],'không có lỗi JS');
    console.log('PASS simulation connection: offline/reconnect, HTTP 503, malformed snapshot, timeout/backoff, WS loss, unknown POST without replay, expired session/reopen, mobile');
    console.log('Artifacts: '+parent);
  }finally{if(browser)await browser.close();await launcher.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
