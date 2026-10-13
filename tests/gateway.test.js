const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {once} = require('node:events');
const {createGateway} = require('../AGVwebserver/server');
const {createDatabase} = require('../AGVwebserver/db');
const Database = require('../AGVwebserver/node_modules/better-sqlite3');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout = 3000) {
  const end = Date.now()+timeout;
  while (Date.now()<end) {if (await fn()) return;await pause(20);}
  throw new Error('Timed out waiting for fixture');
}
function state() {return {protocol_version:1,core_session_id:'session-1',map:{nodes:[{id:'A',x:0,y:0,type:'waypoint'},{id:'T',x:1,y:1,type:'station'}],edges:[{from:'A',to:'T',weight:1}],obstacles:[]},fleet:{agvs:[],pending_tasks:[],tasks:[]},mutex:{},logs:[]};}
async function fixture(t,options = {}) {
  fs.mkdirSync(path.join(__dirname,'../.qa'),{recursive:true});
  const directory = fs.mkdtempSync(path.join(__dirname,'../.qa/gateway-test-'));
  const db = createDatabase(path.join(directory,'test.db'));
  const core = new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const model = state();let peer;const commands = [];
  const push = ()=>peer?.send(JSON.stringify(model));
  core.on('connection',ws=>{
    peer = ws;push();
    ws.on('message',raw=>{
      const request = JSON.parse(raw);commands.push(request);
      const result = {type:'command_result',request_id:request.request_id,core_session_id:model.core_session_id,ok:true,data:{}};
      if (request.cmd === 'direct_goto' && request.agv_id === 'missing') Object.assign(result,{ok:false,status:409,error:'Robot not found'});
      else if (['add_task','direct_goto'].includes(request.cmd)) {
        assert.equal(request.task_id,undefined,'gateway does not allocate task IDs');
        const row = {task_id:'task-'+(model.fleet.tasks.length+1),target:request.target,priority:request.priority,status:request.cmd === 'add_task' ? 'pending' : 'dispatched',assigned_agv:request.agv_id || ''};
        model.fleet.tasks.push(row);model.fleet.pending_tasks = model.fleet.tasks.filter(r=>r.status === 'pending');result.data.task_id = row.task_id;
      } else if (request.cmd === 'cancel_task') {
        const row = model.fleet.tasks.find(r=>r.task_id === request.task_id);row.status = 'cancelled';
        model.fleet.pending_tasks = model.fleet.tasks.filter(r=>r.status === 'pending');result.data.cancelled = 1;
      } else if (request.cmd === 'load_map_data') {model.map = request.map;result.data.map_revision = 2;}
      else if (request.cmd === 'set_obstacle' && options.noObstacleReply) return;
      if (options.delay) setTimeout(()=>ws.readyState === WebSocket.OPEN && ws.send(JSON.stringify({...result,state:model})),options.delay);
      else ws.send(JSON.stringify({...result,state:model}));
    });
  });
  const gateway = createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`,commandTimeoutMs:options.timeout || 500,staleMs:options.staleMs || 5000,reconnectMs:50});
  const address = await gateway.start(0,'127.0.0.1'),base = `http://127.0.0.1:${address.port}`;
  const api = async (method,route,body)=>{
    const r = await fetch(base+route,{method,headers:body ? {'Content-Type':'application/json'} : undefined,body:body ? JSON.stringify(body) : undefined});
    return {status:r.status,data:await r.json()};
  };
  await until(async ()=> (await api('GET','/api/health')).data.core_ready);
  t.after(async ()=>{await gateway.close();for (const client of core.clients) client.terminate();await new Promise(resolve=>core.close(resolve));db.close();});
  return {db,model,push,api,commands,peer:()=>peer,base};
}
test('failed checkpoint reports Core unready and rejects commands until a healthy state',async t=>{
  const f=await fixture(t);f.model.storage_healthy=false;f.push();
  await until(async()=>(await f.api('GET','/api/health')).data.storage_healthy===false);
  assert.equal((await f.api('GET','/api/health')).data.core_ready,false);
  assert.equal((await f.api('GET','/api/state')).data.core_ready,false);
  const before=f.commands.length;const rejected=await f.api('POST','/api/tasks',{target:'T'});
  assert.equal(rejected.status,503);assert.match(rejected.data.error,/checkpoint/);assert.equal(f.commands.length,before);
  f.model.storage_healthy=true;f.push();await until(async()=>(await f.api('GET','/api/health')).data.core_ready);
  assert.equal((await f.api('POST','/api/tasks',{target:'T'})).status,200);
});

test('delta WebSocket includes order status changes after scheduler mutates an existing order',async t=>{
  const f=await fixture(t);f.model.map.nodes.push(...['K1','K2'].map(id=>({id,type:'station'})),...['G1','G2'].map(id=>({id,type:'waypoint'})));
  f.model.map.edges.push({from:'A',to:'K1',weight:1},{from:'K1',to:'K2',weight:1},
    {from:'K1',to:'G1',weight:1,two_way:true},{from:'K2',to:'G2',weight:1,two_way:true});
  f.model.shift={phase:'RUNNING'};f.model.fleet.accepting_tasks=true;f.push();
  await until(async()=>(await f.api('GET','/api/state')).data.shift?.phase==='RUNNING');
  const socket=new WebSocket(f.base.replace('http:','ws:')+'/ws?delta=1'),messages=[];
  socket.on('message',raw=>messages.push(JSON.parse(raw)));t.after(()=>socket.terminate());
  await once(socket,'open');await until(()=>messages.length);
  assert.equal((await f.api('POST','/api/orders',{pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]})).status,201);
  await until(()=>messages.some(m=>m.changes?.orders?.[0]?.status==='active'));
  f.model.shift.phase='CLOSING';f.push();
  await until(()=>messages.some(m=>m.changes?.orders?.[0]?.status==='needs_attention'));
});

test('manual heading endpoint is removed and sends no command',async t=>{
  const f=await fixture(t),before=f.commands.length;
  const r=await fetch(f.base+'/api/agvs/01/heading',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({node:'A',heading:'E',physically_confirmed:true})});
  assert.equal(r.status,404);assert.equal(f.commands.length,before);
});

test('Array contracts: snapshot list, history, logs; snapshot apply waits for core',async t=>{
  const f = await fixture(t);
  assert.equal((await f.api('POST','/api/map/snapshots',{name:'Kho <script>alert(1)</script>'})).status,201);
  const snapshots = await f.api('GET','/api/map/snapshots');assert.ok(Array.isArray(snapshots.data));assert.equal(snapshots.data.length,1);
  const applied = await f.api('POST',`/api/map/snapshots/${snapshots.data[0].id}/apply`,{});assert.equal(applied.data.map_revision,2);
  await f.api('POST','/api/kpi/generate',{});
  assert.ok(Array.isArray((await f.api('GET','/api/kpi/history')).data));
  assert.ok(Array.isArray((await f.api('GET','/api/logs')).data));
});
test('warehouse orders use Core task IDs and wait at pickup before any delivery command',async t=>{
  const f = await fixture(t);
    f.model.map = {nodes:[{id:'A',type:'waypoint'},...['K1','K2','K3','K4'].map(id=>({id,type:'station'})),...['G1','G2','G3','G4'].map(id=>({id,type:'waypoint'}))],
    edges:[{from:'A',to:'K1',weight:1},{from:'K1',to:'K3',weight:1},{from:'K1',to:'K4',weight:2},
        {from:'K2',to:'K3',weight:2},{from:'K2',to:'K4',weight:1},
        ...[1,2,3,4].map(index=>({from:`K${index}`,to:`G${index}`,weight:1,two_way:true}))],obstacles:[]};
  f.model.shift = {phase:'RUNNING'};f.model.fleet.accepting_tasks = true;
  f.model.fleet.agvs = [{id:'01',node:'A',state:'IDLE',online:true,position_valid:true,task_id:null}];
  f.push();await until(async()=> (await f.api('GET','/api/state')).data.shift?.phase === 'RUNNING');
  const body = {pickups:[{pickup:'K1',quantity:1},{pickup:'K2',quantity:1}],
    deliveries:[{dropoff:'K3',quantity:1},{dropoff:'K4',quantity:1}]};
  const invalid = await f.api('POST','/api/orders',{...body,deliveries:[{dropoff:'K3',quantity:1}]});
  assert.equal(invalid.status,400);
  const created = await f.api('POST','/api/orders',body);assert.equal(created.status,201);
  assert.equal(created.data.order.total,2);
  await until(async()=> (await f.api('GET','/api/orders')).data[0].items.some(item=>item.task_id));
  const first = (await f.api('GET','/api/orders')).data[0];
  assert.ok(first.items.some(item=>item.status === 'to_pickup'));
  assert.ok((await f.api('GET','/api/state')).data.orders.some(order=>order.id === first.id));
  const task = f.model.fleet.tasks.find(row=>row.task_id === first.items.find(item=>item.task_id).task_id);
  task.status = 'completed';f.model.fleet.agvs[0].node = task.target;f.push();
  await until(async()=> (await f.api('GET','/api/orders')).data[0].items.some(item=>item.status === 'pickup_service'));
  assert.equal(f.commands.filter(command=>command.cmd === 'direct_goto' && ['K3','K4'].includes(command.target)).length,0);
  f.model.fleet.agvs[0].online = false;f.push();
  await until(async()=> (await f.api('GET','/api/orders')).data[0].status === 'needs_attention');
  assert.equal((await f.api('POST',`/api/orders/${first.id}/resolve`,{})).status,400);
  assert.equal((await f.api('POST',`/api/orders/${first.id}/resolve`,{physically_clear:true})).data.order.status,'closed');
});
test('Same destination stays distinct; cancel one ID; clearing target never completes history',async t=>{
  const f = await fixture(t);
  const one = await f.api('POST','/api/tasks',{target:'T',priority:0});
  const two = await f.api('POST','/api/tasks',{target:'T',priority:1});
  assert.notEqual(one.data.task_id,two.data.task_id);
  await f.api('POST','/api/tasks/cancel',{task_id:one.data.task_id});
  let rows = (await f.api('GET','/api/tasks/history')).data;
  assert.equal(rows.find(r=>r.core_task_id === one.data.task_id).status,'cancelled');
  assert.equal(rows.find(r=>r.core_task_id === two.data.task_id).status,'pending');
  const record = f.model.fleet.tasks.find(r=>r.task_id === two.data.task_id);record.status = 'dispatched';record.assigned_agv = '01';
  f.model.fleet.agvs = [{id:'01',node:'A',battery:80,state:'WORKING',target:'T',path:['T']}];f.push();await pause(40);
  f.model.fleet.agvs[0].target = '';f.model.fleet.agvs[0].state = 'ERROR';record.status = 'interrupted';f.push();await pause(40);
  rows = (await f.api('GET','/api/tasks/history')).data;
  assert.equal(rows.find(r=>r.core_task_id === two.data.task_id).status,'interrupted');
  assert.equal(rows.filter(r=>r.status === 'completed').length,0);
});
test('Core rejection, strict REST validation and unknown timeout do not invent success',async t=>{
  const f = await fixture(t,{noObstacleReply:true,timeout:80});
  assert.equal((await f.api('POST','/api/agvs/missing/goto',{target:'T'})).status,409);
  assert.equal(f.db.getTaskHistory().length,0,'rejected request is not a core task');
  const before = f.commands.length;
  assert.equal((await f.api('POST','/api/tasks',{target:'T',priority:'3'})).status,400);
  assert.equal((await f.api('POST','/api/map/edge',{from:'A',to:'T',weight:-1})).status,400);
  assert.equal((await f.api('POST','/api/map/obstacle',{node:'A',blocked:'false'})).status,400);
  assert.equal((await f.api('GET','/api/tasks/history?limit=-1')).status,400);
  assert.equal(f.commands.length,before);
  const result = await f.api('POST','/api/map/obstacle',{node:'A',blocked:true});
  assert.equal(result.status,504);assert.equal(result.data.unknown,true);
  assert.deepEqual((await f.api('GET','/api/map')).data.obstacles,[]);assert.equal(f.db.getObstacleHistory().length,0);
});
test('Socket connected with stale data blocks controls; restart seq is scoped by session',async t=>{
  const f = await fixture(t,{staleMs:80});
  f.model.logs = [{seq:1,time:'12:00:00',msg:'AGV_01 BAO LOI: motor'}];f.push();await pause(30);f.push();await pause(30);
  assert.equal(f.db.getRecentSystemLogs().length,1);
  f.model.core_session_id = 'session-2';f.model.logs = [{seq:1,time:'12:00:01',msg:'AGV_02 BAO LOI: sensor'}];f.push();await pause(30);
  assert.equal(f.db.getRecentSystemLogs().length,2);
  await pause(100);const health = (await f.api('GET','/api/health')).data;
  assert.equal(health.agv_connected,true);assert.equal(health.core_ready,false);
  assert.equal((await f.api('POST','/api/tasks',{target:'T'})).status,503);
});
test('Disconnect while awaiting a command returns uncertainty and preserves the old map',async t=>{
  const f = await fixture(t,{noObstacleReply:true,timeout:2000});
  const request = f.api('POST','/api/map/obstacle',{node:'A',blocked:true});
  await until(()=>f.commands.length === 1);f.peer().terminate();
  const result = await request;assert.equal(result.status,503);assert.equal(result.data.unknown,true);
  assert.deepEqual((await f.api('GET','/api/map')).data.obstacles,[]);
});
test('KPI watermarks, cross-period completions and first observation',()=>{
  const db = createDatabase(':memory:');
  db.createTrackedTask('one','T',0,'s');db.syncTasks([{task_id:'one',target:'T',status:'pending'}],'s');
  const first = db.computeKpiReport();assert.equal(first.total_tasks,1);
  const second = db.computeKpiReport();assert.equal(second.total_tasks,0);
  db.syncTasks([{task_id:'one',target:'T',status:'completed'}],'s');
  const third = db.computeKpiReport();assert.equal(third.completed_in_period,1);
  assert.equal(db.computeKpiReport().completed_in_period,0);
  // A robot first observed late in a period must not be assigned the earlier unobserved hours.
  db.db.exec("INSERT INTO agv_events(agv_id,node,battery,state,created_at) VALUES('late','A',80,'IDLE','2026-09-01 00:59:00')");
  db.db.exec("DELETE FROM kpi_reports");
  const prior = {period_end:'2026-09-01 00:00:00'};
  db.db.prepare("INSERT INTO kpi_reports(reason,report_json) VALUES('test',?)").run(JSON.stringify(prior));
  const report = db.computeKpiReport();
  assert.ok(report.agv_utilization.late.known_seconds < (Date.parse(report.period_end.replace(' ','T')+'Z')-Date.parse(prior.period_end.replace(' ','T')+'Z'))/1000);
  db.close();
});
test('Duplicate HTTP port rejects cleanly and keeps the existing gateway healthy',async t=>{
  const f=await fixture(t);
  const directory=fs.mkdtempSync(path.join(__dirname,'../.qa/duplicate-gateway-'));
  const duplicate=createGateway({dbPath:path.join(directory,'test.db'),coreUrl:'ws://127.0.0.1:1'});
  try{
    await assert.rejects(duplicate.start(new URL(f.base).port,'127.0.0.1'),error=>error.code==='EADDRINUSE');
    assert.equal((await f.api('GET','/api/health')).data.core_ready,true);
  }finally{await duplicate.close();}
});
test('Migrate old DB without deleting history; reopen with duplicate seq across sessions',()=>{
  const directory = fs.mkdtempSync(path.join(__dirname,'../.qa/legacy-db-test-'));
  const filename = path.join(directory,'old.db');
  const old = new Database(filename);
  old.exec(`CREATE TABLE task_log(id INTEGER PRIMARY KEY,target_node TEXT NOT NULL,priority INTEGER DEFAULT 0,status TEXT,assigned_agv TEXT,created_at TEXT DEFAULT(datetime('now')),completed_at TEXT);
    INSERT INTO task_log(id,target_node,status,completed_at) VALUES(1,'T','completed','2026-09-01 00:00:00');
    CREATE TABLE system_log(id INTEGER PRIMARY KEY,core_seq INTEGER,time_label TEXT,message TEXT NOT NULL,created_at TEXT DEFAULT(datetime('now')));
    CREATE UNIQUE INDEX idx_system_log_core_seq ON system_log(core_seq);
    INSERT INTO system_log(core_seq,message) VALUES(1,'legacy event');`);
  old.close();
  let db = createDatabase(filename);assert.equal(db.getTaskHistory()[0].status,'completed');
  assert.equal(db.insertSystemLog(1,'','new event','session-2'),true);db.close();
  db = createDatabase(filename);assert.equal(db.getRecentSystemLogs().length,2);
  assert.equal(db.getTaskHistory()[0].completed_at,'2026-09-01 00:00:00');db.close();
});
