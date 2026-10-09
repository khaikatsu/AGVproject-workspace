const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createSimulation} = require('../simulation/server');

async function main() {
  const playwright = require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  const qa = path.resolve(__dirname,'../.qa/order-browser');fs.mkdirSync(qa,{recursive:true});
  const sim = await createSimulation({port:0,runParent:qa});let browser;
  try {
    browser = await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL || 'msedge'});
    const page = await browser.newPage({viewport:{width:1366,height:900}}),errors = [];
    page.on('pageerror',error=>errors.push(error.message));
    await page.goto(sim.url);
    await page.locator('#simHealth').filter({hasText:'Sẵn sàng'}).waitFor();
    await page.locator('#simTimeScale').selectOption('10');
    await page.locator('#startShiftBtn:enabled').click();
    await page.locator('#simCreateOrder:enabled').waitFor();
    await page.locator('#simOrderPickup').selectOption('K1');
    assert.deepEqual(await page.locator('.sim-order-destination').evaluateAll(nodes=>nodes.map(node=>node.value)),['K2','K4']);
    assert.equal(await page.locator('#simOrderTotal').textContent(),'5 kiện');
    await page.locator('#simCreateOrder').click();
    await page.locator('#simOrders').filter({hasText:'5/5 kiện'}).waitFor({timeout:60000});
    assert.equal(sim.runtime.orders[0].status,'completed');
    await page.screenshot({path:path.join(sim.directory,'order-desktop.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});await page.waitForTimeout(300);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth <= innerWidth+1),'mobile has no horizontal overflow');
    await page.screenshot({path:path.join(sim.directory,'order-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[]);
    console.log('PASS order browser: form 5 kiện, tạo đơn, tiến độ 5/5, desktop/mobile không lỗi JS');
    console.log('Artifacts: '+sim.directory);
  } finally {if (browser) await browser.close();await sim.close();}
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
