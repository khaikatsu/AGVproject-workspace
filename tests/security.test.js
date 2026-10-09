const {test} = require('node:test');
const assert = require('node:assert/strict');
const {once} = require('node:events');
const WebSocket = require('../AGVwebserver/node_modules/ws');
const {createGateway} = require('../AGVwebserver/gateway');
const {createDatabase} = require('../AGVwebserver/db');
const {createSecurity} = require('../AGVwebserver/security');
const token = 'test-token-only-'.repeat(4);
async function fixture(t) {
  const db = createDatabase(':memory:');
  const gateway = createGateway({db,authToken:token,coreUrl:'ws://127.0.0.1:1',reconnectMs:60000});
  const addr = await gateway.start(0,'127.0.0.1');const base = `http://127.0.0.1:${addr.port}`;
  t.after(async()=>{await gateway.close();db.close();});return {base,db};
}
test('API requires token; explicit CORS/Origin, cookie CSRF and login attempts',async t=>{
  const {base} = await fixture(t);
  assert.equal((await fetch(base+'/api/state')).status,401);
  assert.equal((await fetch(base+'/api/state',{headers:{Authorization:`Bearer ${token}`,Origin:'https://untrusted.example'}})).status,403);
  const ok = await fetch(base+'/api/state',{headers:{Authorization:`Bearer ${token}`,Origin:base}});
  assert.equal(ok.status,200);assert.equal(ok.headers.get('access-control-allow-origin'),null);
  const login = await fetch(base+'/auth/login',{method:'POST',headers:{'Content-Type':'application/json',Origin:base},body:JSON.stringify({token})});
  assert.equal(login.status,200);const cookie = login.headers.get('set-cookie');
  assert.match(cookie,/HttpOnly/);assert.match(cookie,/SameSite=Strict/);
  assert.equal((await fetch(base+'/api/state',{headers:{Cookie:cookie}})).status,200);
  assert.equal((await fetch(base+'/api/shift/end',{method:'POST',headers:{Cookie:cookie,'Content-Type':'application/json'},body:'{}'})).status,403);
  for (let i=0;i<10;i++) await fetch(base+'/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:'{"token":"bad"}'});
  assert.equal((await fetch(base+'/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({token})})).status,429);
});
test('WS rejects missing token/bad Origin and limits incoming frames',async t=>{
  const {base} = await fixture(t),url = base.replace('http','ws')+'/ws';
  const denied = new WebSocket(url);denied.on('error',()=>{});
  const [,response] = await once(denied,'unexpected-response');assert.equal(response.statusCode,401);response.resume();denied.terminate();
  const peer = new WebSocket(url,{headers:{Authorization:`Bearer ${token}`,Origin:base}});
  peer.on('error',()=>{});await once(peer,'open');
  const closed = once(peer,'close');peer.send('x'.repeat(4097));const [code] = await closed;assert.equal(code,1009);
});
test('Strict ID validation; core-generated ID history adoption and batch durability',async t=>{
  const {base,db} = await fixture(t);
  assert.equal((await fetch(base+'/api/agvs/bad%7Cid/history',{headers:{Authorization:`Bearer ${token}`}})).status,400);
  const incident = /^AGV_(\S+)\s+(BAO LOI)/;
  const state = {fleet:{agvs:[{id:'01',node:'S1',battery:85,state:'IDLE'}],tasks:[{task_id:'task-7',target:'K1',priority:0,status:'pending'}]},logs:[]};
  db.enqueueState(state,'before',incident);assert.equal(db.getTaskHistory().length,1);
  db.interruptPreviousSession('after',state.fleet.tasks);db.syncTasks(state.fleet.tasks,'after');
  const rows = db.getTaskHistory();assert.equal(rows.length,1);assert.equal(rows[0].core_session,'after');assert.equal(rows[0].status,'pending');
  db.db.prepare("UPDATE agv_events SET created_at='2000-01-01 00:00:00'").run();
  db.insertSystemLog(1,'old','old log','before');db.db.exec("UPDATE system_log SET created_at='2000-01-01 00:00:00'");
  db.prune(1,100);assert.equal(db.getRecentSystemLogs().length,0);assert.equal(db.getTaskHistory().length,1);assert.equal(db.getAgvHistory('01').length,1);
});
test('Development access is loopback only; configured secrets have minimum size',()=>{
  const security = createSecurity({token:''});
  assert.equal(security.authorized({socket:{remoteAddress:'192.168.1.9'},headers:{}}),false);
  assert.equal(security.authorized({socket:{remoteAddress:'127.0.0.1'},headers:{host:'foreign.example:3000',origin:'http://foreign.example:3000'}}),false);
  assert.equal(security.authorized({socket:{remoteAddress:'127.0.0.1'},headers:{host:'localhost:3000',origin:'http://localhost:3000'}}),true);
  assert.throws(()=>createSecurity({token:'short'}));
});
