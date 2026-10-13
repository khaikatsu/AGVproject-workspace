const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createSimulation}=require('../simulation/server');
async function main(){
  const pw=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright'),parent=path.resolve(__dirname,'../.qa/simulation-direction');fs.mkdirSync(parent,{recursive:true});
  const sim=await createSimulation({port:0,runParent:parent});let browser;
  try{
    browser=await pw.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(sim.url);await page.locator('#simHealth').filter({hasText:'Sẵn sàng'}).waitFor();
    await page.waitForFunction(()=>document.querySelector('[data-sim-robot="01"]')?.dataset.heading==='90'&&document.querySelector('[data-sim-robot="02"]')?.dataset.heading==='270');
    assert.equal(await page.locator('.sim-heading-arrow').count(),2);
    await page.locator('#startShiftBtn:enabled').click();await page.locator('#simGoto01:enabled').waitFor();
    sim.runtime.configure({speedCmS:100,timeScale:5,drainPerCm:0});
    await page.locator('#orderPickups select').selectOption('K1');await page.locator('#orderDeliveries select').selectOption('K2');await page.locator('#submitOrderBtn').click();
    await page.waitForFunction(()=>[...document.querySelectorAll('[data-sim-robot]')].some(n=>n.dataset.phase==='pickup_service'&&n.textContent.includes('Bốc hàng')));
    const robot=await page.evaluate(()=>document.querySelector('[data-phase="pickup_service"][data-sim-robot]').dataset.simRobot);
    assert.equal(await page.locator(`[data-sim-robot="${robot}"]`).getAttribute('data-heading'),'180');
    await page.screenshot({path:path.join(sim.directory,'pickup.png'),fullPage:true});
    sim.runtime.configure({speedCmS:5,timeScale:1});
    await page.waitForFunction(id=>document.querySelector(`[data-sim-robot="${id}"]`)?.dataset.reverse==='true',robot,{timeout:20000});
    const reverse=page.locator(`[data-sim-robot="${robot}"]`);assert.equal(await reverse.getAttribute('data-heading'),'180');assert.equal(await reverse.getAttribute('data-travel'),'0');assert.equal(await reverse.locator('.sim-travel-arrow').count(),1);
    assert.match(await reverse.textContent(),/Lùi ra kho/);
    await page.screenshot({path:path.join(sim.directory,'reverse-exit.png'),fullPage:true});
    sim.runtime.configure({speedCmS:100,timeScale:5});
    await page.waitForFunction(()=>[...document.querySelectorAll('[data-sim-robot]')].some(n=>n.dataset.phase==='dropoff_service'&&n.textContent.includes('Dỡ hàng')),null,{timeout:30000});
    await page.screenshot({path:path.join(sim.directory,'dropoff.png'),fullPage:true});
    await page.waitForFunction(()=>lastState?.orders?.[0]?.status==='completed',null,{timeout:20000});
    const home=sim.runtime.robot(robot).home;sim.runtime.configure({speedCmS:25,timeScale:1});await sim.runtime.goto(robot,home);
    await page.waitForFunction(id=>document.querySelector(`[data-sim-robot="${id}"]`)?.textContent.includes('Tiến vào trạm'),robot,{timeout:30000});
    await page.waitForFunction(({id,home})=>document.getElementById('simPosition'+id).textContent.startsWith(home)&&document.querySelector(`[data-sim-robot="${id}"]`)?.dataset.travel==='stopped',{id:robot,home},{timeout:30000});
    await page.waitForTimeout(750);assert.equal(await reverse.getAttribute('data-heading'),home==='S1'?'90':'270');
    await page.screenshot({path:path.join(sim.directory,'station-outward.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});await page.waitForTimeout(300);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));assert.equal(await page.locator('.sim-heading-arrow').count(),2);
    await page.screenshot({path:path.join(sim.directory,'mobile.png'),fullPage:true});assert.deepEqual(errors,[]);
    console.log('PASS browser heading/travel arrows, reverse warehouse exit, pickup/dropoff labels, station outward, mobile; '+sim.directory);
  }finally{await browser?.close();await sim.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
