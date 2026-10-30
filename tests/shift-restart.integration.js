const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createGateway}=require('../AGVwebserver/gateway');

const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check) {
  const deadline=Date.now()+5000;
  while(Date.now()<deadline){if(await check())return;await pause(20);}
  throw Error('Timed out waiting for restart fixture');
}

test('saved warehouse reconciliation survives Core disconnect, new Core session and three gateway/SQLite restarts',async()=>{
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.resolve(__dirname,'../.qa/shift-restart-'));
  const filename=path.join(directory,'test.db');
  let db=createDatabase(filename),gateway,browser,peer,address;
  const old={id:'legacy-hidden',status:'needs_attention',closed_by_shift:true,deleted_at:'2026-01-02T00:00:00.000Z',
    created:'2026-01-01T00:00:00.000Z',updated:new Date().toISOString(),
    items:[{number:1,pickup:'K1',dropoff:'K2',rack_id:'R1',robot:'01',status:'to_dropoff'}]};
  db.saveOrder(old);
  const original=structuredClone(db.getOrders());
  const model={protocol_version:1,core_session_id:'restart-0',mqtt_connected:true,storage_healthy:true,
    map:{nodes:[{id:'K1',type:'station'},{id:'K2',type:'station'}],edges:[],obstacles:[]},
    fleet:{agvs:[],tasks:[],pending_tasks:[],accepting_tasks:false},shift:{supported:true,phase:'CLOSED'},mutex:{},logs:[]};
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const commands=[];
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',ws=>{
    peer=ws;push();
    ws.on('message',raw=>{commands.push(JSON.parse(raw));});
  });
  let pumping=true;
  const pump=setInterval(()=>{if(pumping)push();},100);
  const api=async(method,route,body)=>{
    const response=await fetch(`http://127.0.0.1:${address.port}${route}`,{method,
      headers:body?{'Content-Type':'application/json'}:undefined,body:body?JSON.stringify(body):undefined});
    return {status:response.status,data:await response.json()};
  };
  const start=async()=>{
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`,reconnectMs:30,broadcastIntervalMs:0});
    address=await gateway.start(0,'127.0.0.1');
    await until(async()=>(await api('GET','/api/health')).data.core_ready);
  };
  try {
    await start();
    assert.ok((await api('GET','/api/state')).data.orders.some(order=>order.id===old.id));
    const messages=[];
    browser=new WebSocket(`ws://127.0.0.1:${address.port}/ws`);
    browser.on('message',raw=>messages.push(JSON.parse(raw)));
    await once(browser,'open');await until(()=>messages.length);
    pumping=false;
    const saved=await api('POST','/api/warehouse/configure',{expected_revision:0,physically_verified:true,
      slots:[{node:'K1',rack_id:'R1'},{node:'K2',rack_id:null}]});
    assert.equal(saved.status,200,JSON.stringify(saved.data));
    await until(()=>messages.some(state=>state.warehouse?.initialized&&!state.orders.some(order=>order.id===old.id)));
    assert.ok(!(await api('GET','/api/state')).data.orders.some(order=>order.id===old.id));
    assert.deepEqual(db.getOrders(),original,'saving proof does not rewrite historical orders');
    browser.terminate();browser=null;pumping=true;
    peer.terminate();
    await until(()=>peer?.readyState===WebSocket.OPEN);
    await until(async()=>(await api('GET','/api/health')).data.core_ready);
    for(let cycle=1;cycle<=3;cycle++) {
      assert.ok(!(await api('GET','/api/state')).data.orders.some(order=>order.id===old.id));
      await gateway.close();gateway=null;db.close();db=null;
      model.core_session_id=`restart-${cycle}`;
      db=createDatabase(filename);await start();
      const state=(await api('GET','/api/state')).data;
      assert.equal(state.shift.phase,'CLOSED');
      assert.ok(!state.orders.some(order=>order.id===old.id),'reconciled hidden order stays absent after restart');
      assert.deepEqual(state.warehouse.slots,[{node:'K1',rack_id:'R1'},{node:'K2',rack_id:null}]);
      assert.deepEqual(db.getOrders(),original,'reconnect and startup preserve the original closure boundary');
      const review=(await api('GET','/api/shift/review')).data;
      assert.ok(!review.blockers.some(reason=>reason.includes('Còn đơn')));
    }
    assert.deepEqual(commands,[],'restart and operator inventory verification never dispatch robot commands');
    console.log(`Evidence: ${directory}`);
  } finally {
    clearInterval(pump);browser?.terminate();if(gateway)await gateway.close();
    for(const client of core.clients)client.terminate();await new Promise(resolve=>core.close(resolve));db?.close();
  }
});
