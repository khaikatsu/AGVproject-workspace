const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createGateway}=require('../AGVwebserver/gateway');
async function main(){
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const directory=fs.mkdtempSync(path.join(__dirname,'../.qa/order-selection-browser-'));
  const map={nodes:[{id:'S1',x:0,y:0,type:'charging'},{id:'K1',x:100,y:0,type:'station'},
    {id:'K2',x:200,y:0,type:'station'},{id:'G1',x:100,y:30,type:'waypoint'},{id:'G2',x:200,y:30,type:'waypoint'}],edges:[
      {from:'S1',to:'K1',weight:100},{from:'K1',to:'K2',weight:50},
      {from:'K1',to:'G1',weight:20,two_way:true},{from:'K2',to:'G2',weight:20,two_way:true}],obstacles:[]};
  const model={protocol_version:1,core_session_id:'order-test-session',mqtt_connected:true,storage_healthy:true,map,
    shift:{supported:true,phase:'RUNNING',trial_robot_id:'01'},
    fleet:{agvs:[{id:'01',node:'S1',state:'ERROR',error:'LINE_LOST',online:true,position_valid:true,path:[],uncertain_path:[],battery:-1}],
      tasks:[],pending_tasks:[],accepting_tasks:true},mutex:{},logs:[]};
  let peer,browser,gateway,pump;const commands=[];
  const core=new WebSocket.Server({port:0,host:'127.0.0.1'});await once(core,'listening');
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',socket=>{peer=socket;push();socket.on('message',data=>commands.push(JSON.parse(data)));});
  try {
    gateway=createGateway({dbPath:path.join(directory,'test.db'),coreUrl:`ws://127.0.0.1:${core.address().port}`,authToken:'',coreToken:''});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    pump=setInterval(push,200);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage();await page.goto(base);
    await page.waitForFunction(()=>window.controlsReady&&window.controlsReady());
    await page.evaluate(()=>showView('overview'));
    await page.waitForFunction(()=>document.querySelector('#orderDispatchHint').textContent.includes('mặc định xe 01'));
    await page.locator('#submitOrderBtn').click();
    await page.waitForFunction(()=>document.querySelector('#missionTable').textContent.includes('LINE_LOST'));
    const text=await page.locator('#missionTable').textContent();
    assert.match(text,/01.*Chờ xe/);
    await page.locator('#missionTable .mission-toggle').first().click();
    const detail=await page.locator('#missionTable').textContent();
    assert.match(detail,/Tuyến lấy dự kiến: S1 → K1/);
    assert.match(detail,/Tuyến giao dự kiến: K1 → K2/);assert.equal(commands.length,0,'fault does not dispatch any movement');
    await page.setViewportSize({width:390,height:844});
    await page.screenshot({path:path.join(directory,'robot01-orders-mobile.png'),fullPage:true});
    console.log('PASS order selection browser: fixed01, fault reason, both weighted route previews, no motion; '+directory);
  } finally {clearInterval(pump);await browser?.close();await gateway?.close();for(const socket of core.clients)socket.terminate();await new Promise(resolve=>core.close(resolve));}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
