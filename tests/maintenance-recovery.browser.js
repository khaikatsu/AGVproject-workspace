// Actual browser DOM against recovery UI; fake state/commands only, no robot or DB.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
async function main(){
  const {chromium}=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const browser=await chromium.launch({headless:true,channel:'msedge'});
  try {
    const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
    const html=fs.readFileSync(path.resolve(__dirname,'../AGVwebserver/index.html'),'utf8');
    const start=html.indexOf('<dialog id="vehicleRecoveryDialog"');
    await page.setContent(html.slice(start,html.indexOf('</dialog>',start)+9));
    await page.addScriptTag({content:`var lastState={core_session_id:'browser',shift:{phase:'RUNNING',home_stations:{'01':'S1','02':'S2'}},fleet:{agvs:[]}};
      var requests=[];function $(id){return document.getElementById(id)};function locationText(n){return n};
      var headingLabels={N:'lên',S:'xuống',E:'phải',W:'trái'};function controlsReady(){return true};
      async function control(method,url,body){requests.push({url,body});return {ok:true}};`});
    await page.addScriptTag({path:path.resolve(__dirname,'../AGVwebserver/public/dashboard-recovery.js')});
    for(const id of ['01','02']) {
      await page.evaluate(id=>{
        lastState.fleet.agvs=[{id,node:'G1',online:true,state:'ERROR',recovery_stop_verified:true,heading_confirmation_supported:true}];
        openVehicleRecovery(id);
      },id);
      assert.equal(await page.locator('#vehicleRecoverySubmit').isDisabled(),true);
      await page.locator('#vehicleRecoveryDirection').selectOption('E');
      await page.locator('#vehicleRecoveryClear').check();await page.locator('#vehicleRecoverySubmit').click();
      const sent=await page.evaluate(()=>requests.at(-1));
      assert.equal(sent.url,`/api/agvs/${id}/recover`);assert.deepEqual(sent.body,{expected_node:'G1',heading:'E',physically_clear:true,continue_task:true});
      await page.evaluate(()=>{$('vehicleRecoveryDialog').close();lastState.fleet.agvs[0].node=lastState.shift.home_stations[lastState.fleet.agvs[0].id];openVehicleRecovery(lastState.fleet.agvs[0].id)});
      assert.equal(await page.locator('#vehicleRecoverySubmit').isDisabled(),true);
      assert.match(await page.locator('#vehicleRecoveryStatus').textContent(),/tự kiểm tra/);
      await page.evaluate(()=>$('vehicleRecoveryDialog').close());
    }
    assert.deepEqual(errors,[]);console.log('PASS browser 01/02: one physical check + direction, exact API, automatic home, no page errors');
  }finally{await browser.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
