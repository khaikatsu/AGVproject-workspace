// HTTP/browser regression with a fake Core and private DB; no robot or MCU.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createWarehouseInventory}=require('../AGVwebserver/warehouse-inventory');
const {createGateway}=require('../AGVwebserver/gateway');
async function main(){
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  const dir=fs.mkdtempSync(path.resolve(__dirname,'../.qa/recovery-browser-')),db=createDatabase(path.join(dir,'test.db'));
  const robot={id:'02',node:'S2',state:'ERROR',error:'LINE_LOST',online:true,position_valid:true,
    path:[],uncertain_path:['M3','G1'],task_id:'',target:'',virtual_cargo:false,virtual_cargo_known:true,
    lift:{supported:true,ready:true,fresh:true,active:false,command_id:0,observed_node:'S2'}};
  robot.cargo_sensor={required:true,supported:true,fresh:true,stable:true,present:false,stopped_verified:true,observed_node:'S2'};
  Object.assign(robot,{heading:'UNKNOWN',heading_supported:true,heading_source:'unknown',recovery_stop_verified:true,
    heading_confirmation_supported:true,heading_confirmation_required:false,recovery_expected_heading:'N'});
  const model={protocol_version:1,core_session_id:'recovery-browser',mqtt_connected:true,
    map:{nodes:['S2','G1','G2','K1','K2'].map((id,i)=>({id,x:50+i*100,y:50,type:id[0]==='K'?'station':id==='S2'?'charging':'waypoint'})),
      edges:[{from:'S2',to:'G1',weight:1,two_way:true},{from:'G1',to:'K1',weight:1,two_way:true},
        {from:'G1',to:'G2',weight:1,two_way:true},{from:'G2',to:'K2',weight:1,two_way:true}],obstacles:[]},
    shift:{supported:true,phase:'CLOSED',can_start:false,home_stations:{'02':'S2'}},
    fleet:{agvs:[robot],accepting_tasks:true,pending_tasks:[],tasks:[{task_id:'old-task',target:'K1',priority:0,
      assigned_agv:'02',status:'interrupted',resume_available:true,resume_node:'S2'}]},
    mutex:{M3:{owner:'02',grant_id:3,waiting:[]}},logs:[]};
  const inventory=createWarehouseInventory({db});
  const original=robot.state;robot.state='IDLE';
  inventory.configure({physically_verified:true,expected_revision:0,slots:[{node:'K1',rack_id:'R02'},{node:'K2',rack_id:null}]},model);
  robot.state=original;model.shift.phase='RUNNING';
  inventory.reserve({key:'order:paused:1',robot:'02',pickup:'K1',dropoff:'K2'});inventory.interrupt('order:paused:1','LINE_LOST');
  db.saveOrder({id:'paused',status:'needs_attention',created:new Date().toISOString(),core_session_id:model.core_session_id,
    reason:'LINE_LOST',total:1,destinationOwners:{},items:[{number:1,robot:'02',pickup:'K1',dropoff:'K2',rack_id:'R02',
      status:'interrupted',suspended_status:'to_pickup',resume_node:'S2',task_id:'old-task'}]});
  let peer,gateway,browser,pump;const commands=[],core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',ws=>{peer=ws;push();ws.on('message',raw=>{
    const request=JSON.parse(raw);commands.push(request);const data={};
    if(request.cmd==='recover_vehicle'){
      assert.equal(request.id,'02');assert.equal(request.expected_node,'S2');assert.equal(request.physically_clear,true);
      assert.equal(request.repositioned,true);
      robot.recovery_reset_pending=true;
      setTimeout(()=>{Object.assign(robot,{state:'IDLE',error:'',uncertain_path:[],recovery_node_verified:true,
        recovery_reset_pending:false,heading_confirmation_required:true});model.mutex={};push();},350);
      data.node='S2';
    }else if(request.cmd==='confirm_recovery_heading'){
      assert.equal(request.heading,'N');assert.equal(request.physically_confirmed,true);
      robot.heading_confirmation_pending=true;
      setTimeout(()=>{Object.assign(robot,{heading:'N',heading_source:'operator',heading_confirmation_required:false,
        heading_confirmation_pending:false});push();},350);
    }else if(request.cmd==='resume_task'){
      assert.equal(request.task_id,'old-task');Object.assign(model.fleet.tasks[0],{status:'dispatched',resume_available:false});
      Object.assign(robot,{state:'WORKING',task_id:'old-task'});data.task_id='old-task';
    }else if(request.cmd==='end_shift'){
      model.shift.phase='CLOSED';model.fleet.accepting_tasks=false;
      Object.assign(robot,{state:'IDLE',task_id:'',path:[],target:''});model.fleet.tasks[0].status='cancelled';data.shift=model.shift;
    }else if(request.cmd!=='reconcile_test_cargo')throw Error(`Unexpected fake Core command ${request.cmd}`);
    ws.send(JSON.stringify({type:'command_result',request_id:request.request_id,core_session_id:model.core_session_id,ok:true,data,state:model}));
  });});
  try{
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`});const addr=await gateway.start(0,'127.0.0.1');
    const base=`http://127.0.0.1:${addr.port}`;pump=setInterval(push,150);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
    await page.goto(base);await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    await page.locator('[data-robot-transfer="02"]').filter({hasText:'LM393: Không có hàng'}).waitFor();
    robot.cargo_sensor.present=true;push();
    await page.locator('[data-robot-transfer="02"]').filter({hasText:'LM393: Có hàng'}).waitFor();
    robot.cargo_sensor.stable=false;robot.cargo_sensor.present=null;push();
    await page.locator('[data-robot-transfer="02"]').filter({hasText:'LM393: Chưa xác định'}).waitFor();
    robot.cargo_sensor.stable=true;robot.cargo_sensor.present=false;push();
    model.logs.push({seq:1,msg:'AGV_02 BAO LOI: LINE_LOST',time:'10:00'});push();
    await page.locator('#reviewShiftBtn').click();await page.locator('#shiftReviewContent').filter({hasText:'LINE_LOST'}).waitFor();
    assert.equal(await page.locator('#confirmShiftReviewBtn').isDisabled(),true);
    await page.locator('#shiftReviewDialog').evaluate(el=>el.close());
    assert.equal(await page.getByRole('button',{name:'Xác nhận tại S2',exact:true}).count(),0);
    assert.equal(await page.getByRole('button',{name:'Xóa lỗi',exact:true}).count(),0);
    await page.getByRole('button',{name:'Khôi phục xe',exact:true}).click();
    await page.locator('#vehicleRecoveryClear').check();await page.locator('#vehicleRecoveryMoved').selectOption('yes');
    await page.locator('#vehicleRecoverySubmit').click();
    await page.locator('#vehicleRecoveryStatus').filter({hasText:'chờ xe xác nhận hết lỗi'}).waitFor();
    await page.locator('#vehicleRecoveryHeading:visible').waitFor();
    assert.equal(await page.locator('#vehicleRecoverySubmit').isDisabled(),true);
    await page.locator('#vehicleRecoveryDialog').screenshot({path:path.join(dir,'desktop-recovery.png')});
    await page.setViewportSize({width:390,height:844});
    assert.ok(await page.locator('#vehicleRecoveryDialog').evaluate(el=>el.getBoundingClientRect().width<=innerWidth));
    await page.locator('#vehicleRecoveryDialog').screenshot({path:path.join(dir,'mobile-recovery.png')});
    await page.setViewportSize({width:1440,height:1100});
    assert.ok(!commands.some(command=>command.cmd==='resume_task'));
    await page.locator('#vehicleRecoveryDirection').selectOption('N');
    await page.locator('#vehicleRecoveryHeadingVerified').check();await page.locator('#vehicleRecoverySubmit').click();
    await page.locator('#vehicleRecoveryStatus').filter({hasText:'Đang chờ xe xác nhận hướng'}).waitFor();
    await page.locator('#vehicleRecoveryStatus').filter({hasText:'Xe đã sẵn sàng'}).waitFor();
    await page.locator('#vehicleRecoveryDialog').evaluate(el=>el.close());
    await page.locator('[data-mission-id="order:paused"] .mission-toggle').click();
    await page.getByRole('button',{name:'Tiếp tục lượt 1',exact:true}).click();
    await page.waitForFunction(()=>lastState.orders.find(o=>o.id==='paused').items[0].status==='to_pickup');
    assert.equal(commands.filter(c=>c.cmd==='resume_task').length,1);assert.ok(!commands.some(c=>c.cmd==='warehouse_lift'));
    await page.locator('#endShiftBtn').click();
    await page.waitForFunction(()=>lastState.orders.find(o=>o.id==='paused').closed_by_shift===true);
    await page.getByRole('button',{name:'Hoàn tác về K1',exact:true}).first().click();
    await page.waitForFunction(()=>lastState.orders.find(o=>o.id==='paused').status==='closed');
    await page.locator('#reviewShiftBtn').click();await page.locator('#shiftReviewContent').filter({hasText:'LINE_LOST'}).waitFor();
    await page.locator('#shiftReviewVerified').check();await page.locator('#confirmShiftReviewBtn:enabled').click();
    await page.locator('#shiftReviewContent').filter({hasText:'Không có lỗi chưa đối soát'}).waitFor();
    await page.setViewportSize({width:390,height:844});
    assert.ok(await page.locator('#shiftReviewDialog').evaluate(el=>el.getBoundingClientRect().width<=innerWidth));
    await page.locator('#shiftReviewDialog').screenshot({path:path.join(dir,'mobile-review.png')});
    assert.deepEqual(errors,[]);console.log(`PASS recovery browser: S2, resume, end, rollback, review and mobile. ${dir}`);
  }finally{clearInterval(pump);await browser?.close();if(gateway)await gateway.close();for(const ws of core.clients)ws.terminate();await new Promise(r=>core.close(r));db.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
