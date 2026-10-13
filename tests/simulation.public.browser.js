const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {createLauncher}=require('../simulation/launcher');
async function main(){
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const directory=fs.mkdtempSync(path.join(__dirname,'../.qa/public-simulation-browser-'));
  let launcher,browser;
  try {
    let url=process.env.SIM_PUBLIC_URL;
    if(!url){launcher=createLauncher({port:0,publicProxy:true,runParent:directory});const address=await launcher.start();url=`http://127.0.0.1:${address.port}/simulation`;}
    const origin=new URL(url).origin;
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage(),errors=[],websockets=[];
    page.on('pageerror',error=>errors.push(error.message));page.on('websocket',socket=>websockets.push(socket.url()));
    await page.goto(url,{waitUntil:'domcontentloaded',timeout:30000});
    await page.waitForSelector('#simConnectionPanel',{state:'attached',timeout:30000});
    await page.waitForFunction(()=>document.body.dataset.simConnection==='online'&&window.controlsReady&&window.controlsReady(),null,{timeout:30000});
    assert.equal(new URL(page.url()).origin,origin);
    assert.equal(await page.locator('[data-sim-robot]').count(),2);
    assert.ok(websockets.some(url=>url.startsWith(origin.replace(/^http/,'ws')+'/ws')),'WS uses the public origin');
    const viewer=await page.evaluate(async()=>{const response=await fetch('/sim-api/viewer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:'browser-public-aaaaaaaaaaaa'})});return response.status;});
    assert.equal(viewer,200);
    await page.reload({waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>document.body.dataset.simConnection==='online',null,{timeout:15000});
    await page.setViewportSize({width:390,height:844});
    await page.screenshot({path:path.join(directory,'public-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[]);
    fs.writeFileSync(path.join(directory,'verification.json'),JSON.stringify({url,websockets,errors,viewerStatus:viewer,passed:true},null,2));
    console.log('PASS browser public simulation: shared origin, HTTPS/WS, two robots, viewer heartbeat, reload, mobile; '+directory);
  } finally {await browser?.close();await launcher?.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
