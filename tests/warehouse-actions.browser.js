const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createGateway}=require('../AGVwebserver/gateway');
const {createDatabase}=require('../AGVwebserver/db');
async function main(){
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const directory=fs.mkdtempSync(path.join(__dirname,'../.qa/warehouse-actions-browser-'));
  const db=createDatabase(path.join(directory,'test.db'));
  db.saveOrder({id:'old-paper-order',status:'needs_attention',reason:'old interrupted paper order',total:1,created:new Date(0).toISOString(),
    items:[{number:1,pickup:'K5',dropoff:'K3',robot:'01',task_id:'old-task',status:'interrupted'}]});
  const robot={id:'01',node:'S1',state:'IDLE',error:'',online:true,position_valid:true,path:[],uncertain_path:[],task_id:null,battery:80};
  const map={nodes:[['S1',0,0,'charging'],['G5',100,0,'waypoint'],['K5',100,20,'station'],['G3',200,0,'waypoint'],['K3',200,20,'station']].map(([id,x,y,type])=>({id,x,y,type})),
    edges:[{from:'S1',to:'G5',weight:100},{from:'G5',to:'K5',weight:20,two_way:true},{from:'G5',to:'G3',weight:100},{from:'G3',to:'K3',weight:20,two_way:true}],obstacles:[]};
  const model={protocol_version:1,core_session_id:'test-session',mqtt_connected:true,storage_healthy:true,map,
    shift:{supported:true,phase:'RUNNING',test_mode:'robot01',trial_robot_id:'01'},fleet:{agvs:[robot],tasks:[],pending_tasks:[],accepting_tasks:true},mutex:{},logs:[]};
  const commands=[];let peer,pump,browser,gateway;
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',socket=>{peer=socket;push();socket.on('message',raw=>{
    const command=JSON.parse(raw);commands.push(command);const task={task_id:'task-'+commands.length,status:'dispatched',target:command.target,assigned_agv:'01'};
    model.fleet.tasks.push(task);robot.state='WORKING';robot.task_id=task.task_id;
    socket.send(JSON.stringify({type:'command_result',request_id:command.request_id,core_session_id:model.core_session_id,ok:true,data:{task_id:task.task_id}}));push();
    setTimeout(()=>{task.status='completed';robot.state='IDLE';robot.task_id=null;robot.node=command.target;push();},120);
  });});
  try {
    gateway=createGateway({db,authToken:'',coreToken:'',coreUrl:`ws://127.0.0.1:${core.address().port}`});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;pump=setInterval(push,100);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage(),errors=[];page.on('pageerror',error=>errors.push(error.message));await page.goto(base);
    await page.waitForFunction(()=>!document.querySelector('#warehouseTaskForm').hidden&& !document.querySelector('#pickupWarehouseBtn').disabled);
    assert.equal(await page.locator('#orderModeBtn').isVisible(),false);assert.equal(await page.locator('#orderList').isVisible(),false);
    for(const [button,warehouse,gate] of [['pickupWarehouseBtn','K5','G5'],['dropoffWarehouseBtn','K3','G3']]) {
      await page.selectOption('#warehouseTarget',warehouse);await page.locator('#'+button).click();
      await page.waitForFunction(()=>document.querySelector('#warehouseActionStatus').textContent.includes('Dừng tại'),null,{timeout:5000}).catch(async error=>{
        console.error(JSON.stringify({text:await page.locator('#warehouseActionStatus').textContent(),commands,state:await (await fetch(base+'/api/state')).json()}));throw error;
      });
      assert.equal(await page.locator('#pickupWarehouseBtn').isDisabled(),true);const start=Date.now();
      await page.waitForFunction(()=>document.querySelector('#warehouseActionStatus').textContent.includes('Đã ra gate'),null,{timeout:10000});
      assert.ok(Date.now()-start>=4500);assert.equal(robot.node,gate);
    }
    assert.deepEqual(commands.map(command=>command.target),['K5','G5','K3','G3']);
    assert.equal((await (await fetch(base+'/api/orders')).json()).length,1,'standalone actions create no orders');
    await page.setViewportSize({width:390,height:844});await page.screenshot({path:path.join(directory,'warehouse-actions-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[]);console.log('PASS warehouse buttons: old paper order ignored, K5/5s/G5 then K3/5s/G3, no new orders, mobile; '+directory);
  } finally{clearInterval(pump);await browser?.close();await gateway?.close();db.close();for(const socket of core.clients)socket.terminate();await new Promise(resolve=>core.close(resolve));}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
