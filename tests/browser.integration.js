const assert = require('assert/strict');
const fs = require('fs');
const path = require('path');
const {once} = require('events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
async function main() {
  const playwright = require(process.env.AGV_PLAYWRIGHT_MODULE || 'playwright');
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory = fs.mkdtempSync(path.resolve(__dirname,'../.qa/browser-test-'));
  const db = createDatabase(path.join(directory,'test.db'));
  const map = {map_name:'Kho kiểm thử giao diện',revision:1,nodes:[{id:'A',x:0,y:0,type:'charging'},
    {id:'B',x:100,y:0,type:'station'},{id:'M',x:50,y:50,type:'mutex'}],edges:[{from:'A',to:'B',weight:100,type:'main',two_way:false},{from:'A',to:'M',weight:70,type:'branch',two_way:true}],obstacles:[]};
  const model = {protocol_version:1,core_session_id:'browser-session',map,fleet:{agvs:[],pending_tasks:[],tasks:[]},mutex:{M:{owner:'',waiting:[]}},logs:[]};
  db.saveMapSnapshot(map,'Kho <script>window.__xss=1</script>');db.computeKpiReport();
  const commands = [];let peer,browser,gateway,authGateway,pump,pushEnabled = true;
  const core = new WebSocket.Server({port:0,host:'127.0.0.1'});await once(core,'listening');
  const push = ()=>{if (pushEnabled && peer?.readyState === WebSocket.OPEN) peer.send(JSON.stringify(model));};
  core.on('connection',ws=>{
    peer = ws;push();ws.on('message',raw=>{
      const request = JSON.parse(raw);commands.push(request);const data = {};
      if (request.cmd === 'load_map_data') {model.map = {...request.map,revision:model.map.revision+1};data.map_revision = model.map.revision;}
      if (request.cmd === 'add_node') {model.map.nodes.push({id:request.id,x:request.x,y:request.y,type:request.type});model.map.revision++;}
      if (request.cmd === 'add_edge') model.map.edges.push(request);
      if (request.cmd === 'set_obstacle') {
        const obstacles = new Set(model.map.obstacles);
        if (request.blocked) obstacles.add(request.node); else obstacles.delete(request.node);
        model.map.obstacles = [...obstacles];
      }
      if (request.cmd === 'save_map') data.path = 'data/map_layout.json';
      if (request.cmd === 'start_shift') {model.shift.phase='RUNNING';model.shift.can_start=false;model.fleet.accepting_tasks=true;data.shift=model.shift;}
      if (request.cmd === 'end_shift') {model.shift.phase='CLOSING';model.shift.can_start=false;model.fleet.accepting_tasks=false;data.shift=model.shift;}
      if (['add_task','direct_goto'].includes(request.cmd)) {
        const t = {task_id:'task-'+(model.fleet.tasks.length+1),target:request.target,priority:request.priority,status:request.cmd === 'add_task' ? 'pending' : 'dispatched',assigned_agv:request.agv_id || ''};
        model.fleet.tasks.push(t);data.task_id = t.task_id;
        if (request.agv_id) {const a = model.fleet.agvs.find(a=>a.id === request.agv_id);Object.assign(a,{state:'WORKING',target:t.target,path:[t.target],task_id:t.task_id});}
      }
      if (request.cmd === 'cancel_task') {
        model.fleet.tasks.find(t=>t.task_id === request.task_id).status = 'cancelled';data.cancelled = 1;
        model.fleet.agvs.filter(a=>a.task_id === request.task_id).forEach(a=>Object.assign(a,{state:'IDLE',target:'',path:[],task_id:''}));
      }
      model.fleet.pending_tasks = model.fleet.tasks.filter(t=>t.status === 'pending');
      ws.send(JSON.stringify({type:'command_result',core_session_id:model.core_session_id,request_id:request.request_id,ok:true,data,state:model}));
    });
  });
  try {
    gateway = createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`,staleMs:1200,reconnectMs:100});
    const address = await gateway.start(0,'127.0.0.1');pump = setInterval(push,300);
    browser = await playwright.chromium.launch({headless:true,...(process.env.AGV_BROWSER_CHANNEL ? {channel:process.env.AGV_BROWSER_CHANNEL} : {})});
    const page = await browser.newPage({viewport:{width:1600,height:1050}}),errors = [];
    page.on('pageerror',error=>errors.push(error.message));page.on('dialog',dialog=>dialog.accept());
    await page.goto(`http://127.0.0.1:${address.port}`);await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    assert.ok((await page.locator('#center-panel').boundingBox()).width > 1500,'map uses nearly the full desktop width by default');
    await page.locator('#mapExpandBtn').click();
    assert.ok((await page.locator('#center-panel').boundingBox()).width < 1100,'compact layout restores side panels');
    await page.locator('#mapExpandBtn').click();
    assert.equal(await page.locator('#connectionSettings').getAttribute('open'),null);
    await page.locator('#connectionSettings summary').click();
    await page.locator('#wsUrl').waitFor({state:'visible'});
    await page.locator('#connectionSettings summary').click();
    assert.equal(await page.locator('[data-view-target="overview"]').getAttribute('aria-current'),'page');
    await page.locator('[data-view-target="reports"]').click();
    await page.locator('#kpiPanel').waitFor({state:'visible'});
    await page.locator('[data-view-target="map"]').click();
    await page.locator('#mapListBox button').waitFor();
    assert.match(await page.locator('#mapListBox').innerText(),/<script>window.__xss=1<\/script>/);
    assert.equal(await page.evaluate(()=>window.__xss),undefined);
    assert.equal(await page.locator('#kpiHistoryBox button').count(),1);
    assert.equal(await page.locator('#saveNodeBtn').isEnabled(),true);
    await page.locator('#mapListBox button').click();await page.locator('#applySnapshotBtn').click();
    await page.locator('#mapPreviewOverlay').waitFor({state:'hidden'});
    await page.locator('#edit_node_id').fill('Z');await page.locator('#edit_node_x').fill('75');await page.locator('#edit_node_y').fill('90');await page.locator('#saveNodeBtn').click();
    await page.locator('#nodesG text').filter({hasText:/^Z$/}).waitFor();
    await page.locator('#edit_edge_from').fill('B');await page.locator('#edit_edge_to').fill('Z');await page.locator('#edit_edge_weight').fill('40');await page.locator('#edit_edge_two_way').check();await page.locator('#saveEdgeBtn').click();
    await page.waitForFunction(()=>document.querySelector('#edgesG').textContent.includes('40'));
    await page.locator('#map_save_name').fill('Bản đồ mới');await page.locator('#saveSnapshotBtn').click();
    await page.locator('#mapListBox').filter({hasText:'Bản đồ mới'}).waitFor();
    await page.screenshot({path:path.join(directory,'empty-fleet-map-editor.png'),fullPage:true});
    model.fleet.agvs = [
      {id:'01',node:'A',battery:80,battery_measured:true,position_valid:true,online:true,state:'IDLE',path:[],target:'',last_seen_s:1},
      {id:'02',node:'A',battery:45,battery_measured:true,position_valid:true,online:true,state:'IDLE',path:[],target:'',last_seen_s:2}];push();
    await page.locator('[data-robot="02"]').waitFor();assert.equal(await page.locator('#saveNodeBtn').isDisabled(),true);
    await page.locator('#nodesG circle').filter({has:page.locator('title',{hasText:'Z;'})}).click();
    await page.waitForFunction(()=>document.querySelector('#selectionText').textContent.includes('Core sẽ chọn xe gần nhất'));
    assert.equal(commands.find(c=>c.cmd === 'add_task').target,'Z');
    assert.equal(commands.find(c=>c.cmd === 'add_task').agv_id,undefined);
    await page.locator('[data-robot="02"]').click();assert.match(await page.locator('#selectionText').innerText(),/Đã chọn 02/);
    await page.locator('#nodesG circle').filter({has:page.locator('title',{hasText:'Z;'})}).click();
    await page.locator('[data-view-target="tasks"]').click();
    await page.locator('#taskHistoryTable').filter({hasText:'Đã giao xe'}).waitFor();
    assert.equal(commands.find(c=>c.cmd === 'direct_goto').agv_id,'02');
    assert.equal(await page.locator('[data-robot-route="02"]').count(),1);
    await page.locator('#fleetTable button').filter({hasText:/^02$/}).click();await page.locator('#robotHistoryPanel').waitFor({state:'visible'});
    await page.locator(`#taskHistoryTable tr[title="${model.fleet.tasks[0].task_id}"] button`).filter({hasText:'Hủy'}).click();
    await page.locator('#taskHistoryTable').filter({hasText:'Đã hủy'}).waitFor();
    assert.equal(commands.find(c=>c.cmd === 'cancel_task').task_id,model.fleet.tasks[0].task_id);
    // Automatic heading telemetry; no editor and no SET_HEADING request.
    assert.equal(await page.locator('#headingPanel').count(),0);
    assert.equal(await page.locator('[data-heading-choice]').count(),0);
    Object.assign(model.fleet.agvs[0],{heading_supported:true,heading:'N',heading_source:'bay_default',uncertain_path:[]});push();
    await page.locator('[data-robot-heading="01"][data-heading="N"]').waitFor();
    assert.equal(await page.locator('[data-robot-heading="01"]').getAttribute('transform'),'rotate(-90)');
    await page.locator('#center-panel').screenshot({path:path.join(directory,'robot-heading-default.png')});
    model.fleet.agvs[0].heading='E';model.fleet.agvs[0].heading_source='route';push();
    await page.waitForFunction(()=>document.querySelector('[data-robot-heading="01"]')?.getAttribute('transform')==='rotate(0)');
    model.fleet.agvs[0].online=false;push();
    await page.waitForFunction(()=>!document.querySelector('[data-robot-heading="01"]'));
    model.fleet.agvs[0].online=true;model.fleet.agvs[0].heading='UNKNOWN';push();
    await page.waitForFunction(()=>!document.querySelector('[data-robot-heading="01"]'));
    assert.equal(commands.some(c=>c.cmd==='set_heading'),false);
    model.fleet.agvs[0].state = 'ERROR';model.fleet.agvs[0].error = '<img src=x onerror="window.__xss=2"> lỗi motor';push();
    await page.locator('#fleetTable').filter({hasText:'lỗi motor'}).waitFor();assert.equal(await page.evaluate(()=>window.__xss),undefined);
    model.fleet.agvs[1].state = 'WORKING';model.fleet.agvs[1].path = ['B','Z'];model.fleet.agvs[1].target = 'Z';push();
    await page.waitForFunction(()=>document.querySelectorAll('[data-robot-route="02"]').length === 2);
    await page.screenshot({path:path.join(directory,'two-robots-routes-history.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});await page.screenshot({path:path.join(directory,'mobile.png'),fullPage:true});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth <= innerWidth),true);
    await page.setViewportSize({width:1600,height:1050});
    model.mqtt_connected = false;push();await page.locator('#mqttStatus').filter({hasText:'Mất kết nối'}).waitFor();
    assert.equal(await page.locator('#submitTaskBtn').isDisabled(),true);
    assert.match(await page.locator('#dataFreshness').innerText(),/MQTT mất liên kết/);
    model.mqtt_connected = true;push();await page.waitForFunction(()=>!document.querySelector('#submitTaskBtn').disabled);
    pushEnabled = false;await page.waitForFunction(()=>document.querySelector('#submitTaskBtn').disabled);
    assert.match(await page.locator('#dataFreshness').innerText(),/Dữ liệu cũ/);
    assert.equal(await page.locator('#nodesG text').filter({hasText:/^Z$/}).count(),1);
    pushEnabled = true;push();await page.waitForFunction(()=>!document.querySelector('#submitTaskBtn').disabled);
    await page.evaluate(()=>ws.close());await page.locator('#status').filter({hasText:'Mất kết nối'}).waitFor();
    await page.locator('#status').filter({hasText:'Đã kết nối'}).waitFor({timeout:6000});
    // Render the real warehouse map, including snapshot preview and mobile scaling.
    model.map = JSON.parse(fs.readFileSync(path.resolve(__dirname,'../data/map_layout.json'),'utf8'));
    model.fleet = {agvs:[],pending_tasks:[],tasks:[]};
    model.mutex = Object.fromEntries(model.map.mutex_zones.map(id=>[id,{owner:'',waiting:[]}]));push();
    await page.locator('[data-view-target="map"]').click();
    await page.locator('#nodesG [data-node="M6"]').waitFor();
    assert.equal(await page.locator('#nodesG circle').count(),22);
    assert.equal(await page.locator('#mapNodeOptions option').count(),22);
    assert.equal(await page.locator('#edgesG path[data-rounded-edge]').count(),4);
    assert.equal(await page.locator('#nodesG [data-node="C1"]').count(),0);
    assert.equal(await page.locator('#mutexTable tbody tr').count(),6);
    assert.match(await page.locator('#mapLegend').innerText(),/RFID/);
    await page.waitForFunction(()=>document.body.dataset.mapDocked === 'true');
    const assertSidePanels = async()=>{
      const boxes = await page.evaluate(()=>{
        const box = id=>{const r=document.querySelector(id).getBoundingClientRect();return {left:r.left,right:r.right,top:r.top,bottom:r.bottom};};
        return {left:box('#left-panel'),right:box('#right-panel'),graph:box('#nodesG'),canvas:box('#mapViewport')};
      });
      assert.ok(boxes.left.right+12 <= boxes.graph.left,'left cards do not cover map nodes');
      assert.ok(boxes.right.left-12 >= boxes.graph.right,'right controls do not cover map nodes');
      assert.ok(Math.abs(boxes.left.top-boxes.canvas.top)<2,'cards occupy canvas margins');
    };
    await assertSidePanels();
    await page.setViewportSize({width:1366,height:768});
    await page.waitForFunction(()=>document.body.dataset.mapDocked === 'true');await assertSidePanels();
    await page.setViewportSize({width:1600,height:1050});
    await page.waitForFunction(()=>document.body.dataset.mapDocked === 'true');await assertSidePanels();
    const geometry = await page.evaluate(()=>{
      const circles = Object.fromEntries([...document.querySelectorAll('#nodesG circle')].map(c=>[c.dataset.node,{x:+c.getAttribute('cx'),y:+c.getAttribute('cy')}]));
      const labels = [...document.querySelectorAll('#nodesG text')].map(t=>{const b=t.getBBox();return {id:t.textContent,x:b.x,y:b.y,width:b.width,height:b.height};});
      return {circles,labels};
    });
    for (const [gate,warehouse,top,bottom] of [['G2','K2','M1','M5'],['G3','K3','M5','M3'],['G4','K4','M2','M6'],['G5','K5','M6','M4']]) {
      assert.ok(Math.abs(geometry.circles[gate].y-(geometry.circles[top].y+geometry.circles[bottom].y)/2)<0.001);
      assert.equal(geometry.circles[gate].y,geometry.circles[warehouse].y);
    }
    for (let i=0;i<geometry.labels.length;i++) {
      const a=geometry.labels[i];assert.ok(a.width <= 32,`${a.id} fits inside its circle`);
      for (const b of geometry.labels.slice(i+1)) assert.ok(a.x+a.width<=b.x || b.x+b.width<=a.x || a.y+a.height<=b.y || b.y+b.height<=a.y,`${a.id}/${b.id} labels overlap`);
    }
    await page.locator('#center-panel').screenshot({path:path.join(directory,'warehouse-map-v4.png')});
    db.saveMapSnapshot(model.map,'Kho AGV v4');
    await page.evaluate(()=>loadMapList());
    await page.locator('#mapListBox .log-item').filter({hasText:'Kho AGV v4'}).locator('button').click();
    await page.locator('#previewNodesG [data-node="M5"]').waitFor();
    assert.equal(await page.locator('#previewNodesG circle').count(),22);
    assert.equal(await page.locator('#previewEdgesG path[data-rounded-edge]').count(),4);
    await page.evaluate(()=>closeMapPreview());
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth <= innerWidth),true);
    await page.locator('#center-panel').screenshot({path:path.join(directory,'warehouse-map-v4-mobile.png')});
    await page.locator('#mapZoomBtn').click();
    assert.equal(await page.locator('#mapZoomBtn').getAttribute('aria-pressed'),'true');
    assert.equal(await page.evaluate(()=>document.querySelector('#mapViewport').scrollWidth > document.querySelector('#mapViewport').clientWidth),true);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth <= innerWidth),true);
    await page.locator('#center-panel').screenshot({path:path.join(directory,'warehouse-map-v4-mobile-zoom.png')});
    await page.locator('#mapZoomBtn').click();
    // Chính sách ca chỉ thay quyền giao task, vẫn cho quản lý vật cản và đọc lịch sử.
    await page.setViewportSize({width:1600,height:1050});
    model.fleet={agvs:[{id:'01',node:'S1',battery:85,state:'CHARGING',path:[],target:'',task_id:'',position_valid:true,battery_measured:true,last_seen_s:0,online:true}],pending_tasks:[],tasks:[],accepting_tasks:false};
    model.shift={supported:true,phase:'CLOSING',can_start:false,home_stations:{'01':'S1','02':'S2'},robots:[
      {id:'01',home_station:'S1',actual_node:'S1',at_home:true,ready:true,reason:'Da xac minh tai tram'},
      {id:'02',home_station:'S2',actual_node:'',at_home:false,ready:false,reason:'Cho telemetry / RFID'}]};push();
    await page.locator('[data-view-target="overview"]').click();
    await page.locator('#shiftStatus').filter({hasText:'Đang hoàn tất'}).waitFor();
    assert.equal(await page.locator('#submitTaskBtn').isDisabled(),true);
    assert.equal(await page.locator('#startShiftBtn').isDisabled(),true);
    assert.match(await page.locator('#homeStationList').innerText(),/02 → S2: Chờ telemetry/);
    assert.match(await page.locator('#homeStationCaption').innerText(),/01 → S1/);
    assert.match(await page.locator('#fleetTable').innerText(),/Ở trạm sạc/);
    assert.match(await page.locator('#fleetTable').innerText(),/Trạm riêng: S1/);
    model.shift.phase='CLOSED';model.shift.can_start=true;push();
    await page.waitForFunction(()=>!document.querySelector('#startShiftBtn').disabled);
    await page.locator('#startShiftBtn').click();await page.waitForFunction(()=>!document.querySelector('#submitTaskBtn').disabled);
    assert.equal(commands.at(-1).cmd,'start_shift');
    // Menu supervised solo: correct ID on the dashboard, other robot disabled.
    const fleetBeforeMenu=model.fleet.agvs;
    model.fleet.agvs=[...['01','02'].map(id=>({id,node:id==='01'?'S1':'S2',battery:-1,state:'IDLE',
      path:[],target:'',task_id:'',position_valid:true,battery_measured:false,last_seen_s:0,online:true}))];
    for(const id of ['01','02']) {
      model.shift.trial_robot_id=id;model.shift.supervised_trial=true;model.shift.test_mode='robot'+id;
      model.shift.home_stations={[id]:id==='01'?'S1':'S2'};push();
      await page.locator('#coreModeStatus').filter({hasText:`thử riêng xe ${id}`}).waitFor();
      await page.waitForFunction(selected=>{
        const options=[...document.querySelector('#task_agv').options];
        return options.find(o=>o.value===selected)?.disabled===false &&
          options.find(o=>o.value===(selected==='01'?'02':'01'))?.disabled===true;
      },id);
      assert.match(await page.locator('#taskBatteryHint').innerText(),new RegExp(`Thử riêng xe ${id}`));
      assert.equal(await page.locator('#task_agv option').first().innerText(),`Tự động giao cho xe ${id}`);
    }
    model.shift.trial_robot_id='';model.shift.supervised_trial=false;model.shift.test_mode='normal';
    model.fleet.agvs=fleetBeforeMenu;
    model.shift.home_stations={'01':'S1','02':'S2'};push();
    await page.locator('#endShiftBtn').click();await page.waitForFunction(()=>document.querySelector('#submitTaskBtn').disabled);
    assert.equal(commands.at(-1).cmd,'end_shift');
    await page.screenshot({path:path.join(directory,'shift-home-stations.png'),fullPage:true});
    await page.locator('[data-view-target="map"]').click();
    await page.locator('#obstacle_node').fill('G2');
    assert.equal(await page.locator('#blockNodeBtn').innerText(),'Khóa node');
    await page.locator('#blockNodeBtn').click();
    await page.locator('#blockNodeBtn').filter({hasText:'Mở node'}).waitFor();
    assert.equal(commands.at(-1).blocked,true);
    await page.locator('#blockNodeBtn').click();
    await page.locator('#blockNodeBtn').filter({hasText:'Khóa node'}).waitFor();
    assert.equal(commands.at(-1).blocked,false);
    await page.locator('[data-view-target="overview"]').click();
    model.fleet.agvs[0].position_valid=false;model.fleet.agvs[0].state='UNLOCALIZED';push();
    await page.waitForFunction(()=>document.querySelectorAll('#agvsG [data-robot]').length===0);
    assert.match(await page.locator('#fleetTable').innerText(),/Chưa xác minh/);
    assert.match(await page.locator('#fleetTable').innerText(),/Chờ quét RFID/);
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth <= innerWidth),true);
    assert.deepEqual(errors,[]);
    const loginSecret='browser-test-token-'.repeat(3);
    authGateway=createGateway({db,authToken:loginSecret,coreUrl:`ws://127.0.0.1:${core.address().port}`,staleMs:1200,reconnectMs:100});
    const authenticatedAddress=await authGateway.start(0,'127.0.0.1');
    await page.goto(`http://127.0.0.1:${authenticatedAddress.port}`);
    await page.locator('#loginDialog').waitFor({state:'visible'});
    await page.locator('#loginToken').fill('incorrect');await page.locator('#loginForm button').click();
    await page.locator('#loginError').filter({hasText:'Token không đúng'}).waitFor();
    await page.locator('#loginToken').fill(loginSecret);await page.locator('#loginForm button').click();
    await page.locator('#loginDialog').waitFor({state:'hidden'});
    await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Đã kết nối'));
    assert.equal(await page.locator('#loginToken').inputValue(),'');
    await page.locator('#connectionSettings summary').click();await page.locator('#logoutBtn').click();
    await page.locator('#loginDialog').waitFor({state:'visible'});
    assert.equal(await page.locator('#submitTaskBtn').isDisabled(),true);
    await page.screenshot({path:path.join(directory,'login-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[]);
    console.log('PASS browser: operator views, arrays/editor/robot routes/history/stale/reconnect; warehouse v4/mobile; shift/home/task gating/obstacle toggle/unverified robot hidden');
    console.log('Screenshots: '+directory);
  } finally {
    clearInterval(pump);await browser?.close();if(authGateway)await authGateway.close();if (gateway) await gateway.close();
    for (const ws of core.clients) ws.terminate();await new Promise(resolve=>core.close(resolve));db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
