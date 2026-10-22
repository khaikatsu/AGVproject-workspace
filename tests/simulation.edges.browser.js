const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createSimulation}=require('../simulation/server');
async function main(){
 const pw=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright'),parent=path.resolve(__dirname,'../.qa/simulation-edge-browser');fs.mkdirSync(parent,{recursive:true});
 const sim=await createSimulation({port:0,runParent:parent});let browser;
 try{
  browser=await pw.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
  const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(sim.url);await page.locator('#simHealth').filter({hasText:'Sẵn sàng'}).waitFor();await page.locator('#startShiftBtn:enabled').waitFor();
  assert.equal(await page.locator('#simOrderPanel').count(),0);assert.equal(await page.locator('#orderTaskForm').count(),1);
  assert.equal(await page.locator('[data-sim-robot]').count(),2);assert.ok(await page.locator('#simObstacleEdge option').count()>25);
  await page.locator('#simObstacleEdge').selectOption('M5|M6');await page.locator('#simBlockEdge').click();
  await page.waitForFunction(()=>document.getElementById('simObstacleCount').textContent==='1');
  assert.ok(await page.locator('[data-sim-edge="M5|M6"] rect').count());
  await page.screenshot({path:path.join(sim.directory,'desktop-obstacle.png'),fullPage:true});
  await page.locator('#simUnblockEdge').click();await page.waitForFunction(()=>document.getElementById('simObstacleCount').textContent==='0');
  // A real map edge click selects the segment without dispatching a node task.
  await page.locator('#simObstacleEdge').selectOption('G1|M5');
  await page.locator('[data-sim-edge="M5|M6"] .sim-edge-hit').click();assert.equal(await page.locator('#simObstacleEdge').inputValue(),'M5|M6');
  await page.locator('#startShiftBtn').click();
  await page.locator('#shiftWarehouseVerified').check();await page.locator('#confirmStartShiftBtn').click();
  await page.locator('#submitOrderBtn:enabled').waitFor();
  sim.runtime.configure({timeScale:5,speedCmS:100,drainPerCm:0});
  await page.locator('#orderPickups select').selectOption('K1');await page.getByRole('button',{name:'+ Kho lấy',exact:true}).click();
  await page.locator('#orderPickups select').nth(1).selectOption('K4');
  await page.locator('#orderDeliveries select').selectOption('K2');await page.getByRole('button',{name:'+ Kho giao',exact:true}).click();
  await page.locator('#orderDeliveries select').nth(1).selectOption('K5');
  await page.locator('#submitOrderBtn').click();
  await page.waitForFunction(()=>lastState?.orders?.length===1);
  assert.equal(await page.evaluate(()=>lastState.orders[0].total),2);
  await page.screenshot({path:path.join(sim.directory,'desktop-order.png'),fullPage:true});
  await page.waitForFunction(()=>lastState?.orders?.[0]?.status==='completed',null,{timeout:100000});
  const order=await page.evaluate(()=>lastState.orders[0]);assert.equal(order.items.filter(i=>i.status==='delivered').length,2);assert.equal(new Set(order.items.map(i=>i.robot)).size,2);
  assert.match(await page.locator('#orderList').innerText(),/K[1-6] → K[1-6]/);
  await page.setViewportSize({width:390,height:844});await page.waitForTimeout(400);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  await page.screenshot({path:path.join(sim.directory,'mobile.png'),fullPage:true});
  assert.deepEqual(errors,[]);console.log('PASS browser: one multi-pickup/delivery form, 2-robot order, edge click/block/clear, desktop/mobile, no JS errors; '+sim.directory);
 }finally{await browser?.close();await sim.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
