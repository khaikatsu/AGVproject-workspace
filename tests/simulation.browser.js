const assert = require('assert/strict');
const fs = require('fs');
const path = require('path');
const {createSimulation} = require('../simulation/server');
async function main() {
  const playwright = require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  const qa = path.resolve(__dirname,'../.qa');fs.mkdirSync(qa,{recursive:true});
  const sim = await createSimulation({port:0,runParent:qa});let browser;
  try {
    browser = await playwright.chromium.launch({headless:true,...(process.env.AGV_BROWSER_CHANNEL ? {channel:process.env.AGV_BROWSER_CHANNEL} : {})});
    const page = await browser.newPage({viewport:{width:1600,height:1050}}),errors = [];
    page.on('pageerror',error=>errors.push(error.message));page.on('dialog',dialog=>dialog.accept());
    await page.goto(sim.url);await page.locator('#simHealth').filter({hasText:'Sẵn sàng'}).waitFor();
    await page.locator('#startShiftBtn:enabled').waitFor();
    await page.waitForFunction(()=>document.querySelectorAll('[data-sim-robot]').length===2);
    assert.equal(await page.locator('[data-sim-robot]').count(),2);
    assert.equal(await page.locator('[data-node]').count(),26);
    await page.screenshot({path:path.join(sim.directory,'simulation-homes.png'),fullPage:true});
    await page.locator('#simTimeScale').selectOption('5');
    await page.locator('#startShiftBtn').click();
    await page.locator('#shiftWarehouseVerified').check();await page.locator('#confirmStartShiftBtn').click();
    await page.locator('#simGoto01:enabled').waitFor();
    // API job cũ kiểm tra cache runtime riêng, cập nhật sau WS dashboard.
    const readyDeadline=Date.now()+3000;
    while((sim.runtime.coreState?.shift?.phase!=='RUNNING'||sim.runtime.coreState?.fleet?.agvs?.find(r=>r.id==='01')?.state!=='IDLE')&&Date.now()<readyDeadline)await new Promise(resolve=>setTimeout(resolve,50));
    // Legacy per-robot cargo API remains available; the duplicate UI form was removed.
    const jobResponse=await page.request.post(sim.base+'/sim-api/jobs',{data:{robot:'01',pickup:'K2',dropoff:'K3'}});
    assert.equal(jobResponse.status(),200,await jobResponse.text());
    await page.waitForFunction(()=>document.getElementById('simPosition01').textContent.includes('→'));
    await page.screenshot({path:path.join(sim.directory,'simulation-moving.png'),fullPage:true});
    const deadline=Date.now()+30000;
    while(sim.runtime.jobs[0]?.phase!=='completed'&&Date.now()<deadline)await new Promise(resolve=>setTimeout(resolve,100));
    assert.ok(sim.runtime.robot('01').distanceCm > 0);assert.equal(sim.runtime.jobs[0].phase,'completed');
    await page.locator('#simTarget02').fill('K5');await page.locator('#simGoto02').click();
    await page.waitForFunction(()=>document.getElementById('simPosition02').textContent.startsWith('K5'),{},{timeout:30000});
    await page.locator('.sim-vehicle[data-robot="01"] > details > summary').click();
    await page.locator('[data-sim-action="battery"][data-robot="01"][data-value="0"]').click();
    await page.locator('#simPosition01').filter({hasText:'Hết pin'}).waitFor();
    await page.locator('[data-sim-action="battery"][data-robot="01"][data-value="15"]').click();
    await page.locator('#simRecoveryHint01').filter({hasText:'Xe đang khóa lỗi'}).waitFor();
    await page.screenshot({path:path.join(sim.directory,'simulation-battery-recovery.png'),fullPage:true});
    await page.locator('[data-sim-action="recover"][data-robot="01"]').click();
    await page.waitForFunction(()=>document.getElementById('simPosition01').textContent.startsWith('S1'),{},{timeout:30000});
    await page.locator('#simGoto01:enabled').waitFor({timeout:30000});
    await page.screenshot({path:path.join(sim.directory,'simulation-returned-to-charge.png'),fullPage:true});
    await page.locator('[data-view-target="reports"]').click();await page.locator('#kpiPanel').waitFor({state:'visible'});
    await page.locator('[data-view-target="overview"]').click();
    await page.setViewportSize({width:1366,height:900});await page.waitForTimeout(300);
    await page.screenshot({path:path.join(sim.directory,'simulation-laptop.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});await page.waitForTimeout(300);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth <= innerWidth+1),'no mobile horizontal overflow');
    await page.screenshot({path:path.join(sim.directory,'simulation-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[],'browser JS clean');
    const report = await (await fetch(sim.base+'/sim-api/report')).json();assert.equal(report.simulation,true);assert.equal(report.jobs[0].phase,'completed');
    console.log('PASS simulation browser: cloned live dashboard, homes, animated robots, cargo controls, empty/recover, reports, laptop/mobile and report download');
    console.log('Artifacts: '+sim.directory);
  } finally {if (browser) await browser.close();await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
