// Real Core / MQTT / gateway, private fixtures. No hardware safety claim.
const assert = require('node:assert/strict');
const fs = require('node:fs'),path = require('node:path');
const {createSimulation} = require('../simulation/server');
const pause = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,label,timeout = 15000) {
  const end = Date.now()+timeout;
  while (Date.now()<end) {if (await fn()) return;await pause(30);}
  throw new Error('Timeout: '+label);
}
async function main() {
  const parent = path.resolve(__dirname,'../.qa');fs.mkdirSync(parent,{recursive:true});
  const directory = fs.mkdtempSync(path.join(parent,'fault-audit-')),results = [];
  const selected = new Set(process.argv.slice(2));
  async function scenario(id,name,test,options = {}) {
    if (selected.size && !selected.has(id)) return;
    const started = Date.now(),sim = await createSimulation({port:0,runParent:directory,...options}),r = sim.runtime;
    const robot = id=>r.robot(id),state = id=>r.coreState?.fleet?.agvs?.find(v=>v.id === id);
    async function action(id,action,value) {return r.action(id,{action,...(value == null ? {} : {value})});}
    async function request(route,body) {
      const response = await fetch(sim.base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      return {status:response.status,data:await response.json()};
    }
    async function go(id,target) {const task = await r.goto(id,target);await until(()=>r.tasks.get(task.task_id)?.status === 'completed',id+' arrives '+target);return task;}
    try {
      r.configure({timeScale:10,speedCmS:100,drainPerCm:0,chargeRate:20,serviceSeconds:0.2});
      await until(()=>r.coreState?.shift?.can_start,'fresh homes');await r.api('POST','/api/shift/start',{});
      await until(()=>r.coreState.shift.phase === 'RUNNING' && state('01')?.state === 'IDLE' && state('02')?.state === 'IDLE' && !robot('01').charging && !robot('02').charging,'shift ready');
      await test({sim,r,robot,state,action,request,go});
      results.push({id,name,status:'PASS',seconds:(Date.now()-started)/1000,artifacts:sim.directory});
      console.log('PASS '+id+': '+name);
    } catch (error) {
      fs.writeFileSync(path.join(sim.directory,'failure.json'),JSON.stringify({error:error.stack,core:r.coreState,simulation:r.report()},null,2));
      results.push({id,name,status:'FAIL',error:error.message,seconds:(Date.now()-started)/1000,artifacts:sim.directory});
      console.error('FAIL '+id+': '+error.message);
    } finally {await sim.close();}
    fs.writeFileSync(path.join(directory,'results.json'),JSON.stringify({level:'S1',results},null,2));
  }
  await scenario('F01','Hai xe: 30 lượt đích kho với seed 20261002, kiểm tra cùng node/cạnh ngược',async({r,robot,state})=>{
    let seed = 20261002,rounds = 0;const random = ()=>{seed = (Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296;};
    const trace = [],original = r.event.bind(r);r.event = (type,data)=>{
      original(type,data);
      if (type === 'rfid' || type === 'edge_start') {
        const one = robot('01'),two = robot('02');
        if (one.node === two.node && !one.edge && !two.edge) trace.push({type:'same_node',node:one.node});
        if (one.edge && two.edge && one.edge.from === two.edge.to && one.edge.to === two.edge.from)
          trace.push({type:'opposite_edge',one:one.edge,two:two.edge});
      }
    };
    for (;rounds<30;rounds++) {
      const choices = ['K1','K2','K3','K4','K5','K6'].filter(n=>n !== robot('01').node && n !== robot('02').node);
      const first = choices.splice(Math.floor(random()*choices.length),1)[0],second = choices[Math.floor(random()*choices.length)];
      const orders = await Promise.all(['01','02'].map(async(id,i)=>{
        try {return await r.goto(id,i ? second : first);} catch (error) {assert.equal(error.status,409);return null;}
      }));
      assert.ok(orders.some(Boolean),'at least one route progresses');
      await until(()=>orders.every(order=>!order || r.tasks.get(order.task_id)?.status === 'completed'),'both routes round '+rounds);
      assert.deepEqual(trace,[],'no point node / opposite edge collision');
      assert.equal(robot('01').fault,'');assert.equal(robot('02').fault,'');
      assert.equal(state('01').state,'IDLE');assert.equal(state('02').state,'IDLE');
    }
    r.event('seeded_stress_result',{seed:20261002,rounds,collisions:trace.length});
  });
  await scenario('F02','Hủy task giữa cạnh, không báo hoàn thành; giao lại cùng hướng',async({r,robot})=>{
    r.configure({timeScale:1,speedCmS:50});
    const task = await r.goto('01','K6');await until(()=>robot('01').edge?.progress > 0,'moving');
    await r.api('POST','/api/tasks/cancel',{task_id:task.task_id});
    await until(()=>r.tasks.get(task.task_id)?.status === 'cancelled','cancelled');
    const distance = robot('01').distanceCm;await pause(200);assert.equal(robot('01').distanceCm,distance);
    r.configure({timeScale:10,speedCmS:100});const next = await r.goto('01','K4');
    await until(()=>r.tasks.get(next.task_id)?.status === 'completed','new task');
    assert.equal(r.tasks.get(task.task_id).status,'cancelled');assert.equal(robot('01').fault,'');
  });
  await scenario('F03','Vật cản TTL: chờ đường rồi tiếp tục đúng task',async({r,robot,state})=>{
    await goPaused();
    async function goPaused() {
      robot('01').paused = true;const task = await r.goto('01','K1');
      await r.api('POST','/api/map/obstacle',{node:'G1',blocked:true,ttl_s:1});
      await until(()=>state('01').state === 'BLOCKED','blocked');robot('01').paused = false;
      await until(()=>r.tasks.get(task.task_id)?.status === 'completed','TTL unblocks route');
      assert.equal(robot('01').node,'K1');assert.equal(robot('01').fault,'');
    }
  });
  await scenario('F04','Pin thấp hai xe đồng thời: về đúng trạm, task dở không hoàn thành giả',async({r,robot,state,go})=>{
    await Promise.all([go('01','K3'),go('02','K5')]);robot('01').batteryChange(15);robot('02').batteryChange(15);
    await until(()=>robot('01').node === 'S1' && robot('02').node === 'S2' && state('01').state === 'IDLE' && state('02').state === 'IDLE','both charged',25000);
    assert.equal(robot('01').fault,'');assert.equal(robot('02').fault,'');
  });
  await scenario('F05','Core restart lúc 01 giữ M5: giữ quyền, ID tăng và phục hồi rời vùng',async({sim,r,robot,state,go,action})=>{
    await go('01','M5');const grant = r.coreState.mutex.M5.grant_id,session = r.coreState.core_session_id,lastCommand = robot('01').lastCommand;
    await sim.restartProcess('core');
    await until(()=>r.coreState?.core_ready && r.coreState.core_session_id !== session && state('01').state === 'ERROR','new session recovery');
    assert.equal(r.coreState.mutex.M5.owner,'01');assert.equal(r.coreState.mutex.M5.recovery_required,true);
    await action('01','recover');await action('02','recover');
    await until(()=>robot('01').node === 'S1' && robot('02').node === 'S2' && r.coreState.shift.phase === 'CLOSED','restart returns home',20000);
    assert.notEqual(r.coreState.mutex.M5.owner,'01','old grant cannot leave a permanent owner');
    assert.ok(robot('01').lastCommand > lastCommand);assert.ok(grant > 0);
  });
  await scenario('F06','Broker restart giữa chuyến: giữ dừng, task interrupted, phục hồi rõ ràng',async({sim,r,robot,state,action})=>{
    r.configure({timeScale:1,speedCmS:50});const task = await r.goto('01','K6');await until(()=>robot('01').edge?.progress > 0,'moving');
    await sim.restartProcess('broker');await until(()=>!robot('01').connected,'disconnected');
    const distance = robot('01').distanceCm;await pause(500);assert.equal(robot('01').distanceCm,distance);
    await until(()=>r.coreState.mqtt_connected,'Core broker reconnected');
    await action('01','recover');await action('02','recover');
    await until(()=>r.tasks.get(task.task_id)?.status === 'interrupted','task interrupted');assert.notEqual(state('01').state,'WORKING');
  });
  await scenario('F07','Grant/ACK/telemetry sai không làm đổi vị trí/quyền',async({r,robot,go})=>{
    await go('01','M5');const grant = r.coreState.mutex.M5.grant_id;
    robot('02').send('mutex_release',JSON.stringify({zone:'M5',grant_id:grant}));
    robot('01').send('mutex_release',JSON.stringify({zone:'M5',grant_id:grant+1}));
    robot('01').send('telemetry','DOES_NOT_EXIST|100|0');robot('01').send('telemetry','M5|abc|0');robot('01').send('ack','-1');
    await pause(400);assert.equal(r.coreState.mutex.M5.owner,'01');assert.equal(robot('01').node,'M5');
    assert.equal(r.coreState.fleet.agvs.find(v=>v.id === '01').node,'M5');assert.equal(r.coreState.storage_healthy,true);
  });
  await scenario('F08','Chuỗi hàng bị hủy ở chặng trả: giữ hàng, không có kết quả hoàn thành giả',async({r,robot})=>{
    r.configure({timeScale:1,speedCmS:100});const job = await r.cargoJob('01','K4','K6');
    await until(()=>job.phase === 'to_dropoff' && job.taskIds.length === 2,'cargo loaded',20000);
    await r.api('POST','/api/tasks/cancel',{task_id:job.taskIds[1]});await until(()=>job.ended,'job ends');
    assert.equal(job.phase,'interrupted');assert.equal(robot('01').cargo,true);
    assert.equal(r.tasks.get(job.taskIds[1]).status,'cancelled');
  });
  await scenario('F09','Đóng ca khi có task/queue: xong chặng cuối, về trạm và giữ queue',async({r,robot,state})=>{
    r.configure({timeScale:1,speedCmS:100});const first = await r.goto('01','K6'),second = await r.goto('02','K4');
    const pending = await r.api('POST','/api/tasks',{target:'K2',priority:8});
    await r.api('POST','/api/shift/end',{});await until(()=>r.coreState.shift.phase === 'CLOSING','closing');
    const denied = await fetch(r.base+'/api/agvs/01/goto',{method:'POST',headers:{'Content-Type':'application/json'},body:'{"target":"K3"}'});assert.equal(denied.status,409);
    r.configure({timeScale:10,speedCmS:100});await until(()=>r.coreState.shift.phase === 'CLOSED','closed',25000);
    assert.equal(r.tasks.get(first.task_id).status,'completed');assert.equal(r.tasks.get(second.task_id).status,'completed');
    assert.equal(r.tasks.get(pending.task_id).status,'pending');assert.equal(robot('01').node,'S1');assert.equal(robot('02').node,'S2');
    await until(()=>state('01').battery >= 90 && state('02').battery >= 90,'charged');
    await r.api('POST','/api/shift/start',{});await until(()=>r.tasks.get(pending.task_id)?.status === 'completed','queue after next shift');
  });
  await scenario('F10','Đầu vào API sai và map sửa lúc có xe bị từ chối',async({request,r})=>{
    for (const body of [null,[],{}, {action:'unknown'},{action:'battery',value:-1},{action:'battery',value:'15'}]) {
      const result = await request('/sim-api/robots/01/action',body);assert.equal(result.status,400,'invalid action '+JSON.stringify(body));
    }
    assert.equal((await request('/sim-api/settings',{timeScale:NaN})).status,400);
    assert.equal((await request('/api/agvs/01/goto',{target:'DOES_NOT_EXIST'})).status,404);
    assert.equal((await request('/api/map/node',{id:'X',x:0,y:0,type:'waypoint'})).status,409);
    assert.equal(r.coreState.fleet.agvs.length,2);
  });
  const corridor = path.join(directory,'corridor-map.json');
  fs.writeFileSync(corridor,JSON.stringify({map_name:'Two adjacent mutex head-on fixture',obstacles:[],
    nodes:[['S1',0,0,'charging'],['S2',0,100,'charging'],['A1',50,0,'waypoint'],['A2',50,100,'waypoint'],
      ['M5',100,40,'mutex'],['M6',100,60,'mutex'],['K1',150,100,'station'],['K2',150,0,'station']].map(([id,x,y,type])=>({id,x,y,type})),
    edges:[['S1','A1'],['S2','A2'],['A1','M5'],['A2','M6'],['M5','M6'],['M6','M5'],['M6','K1'],['M5','K2'],['K1','S1'],['K2','S2']].map(([from,to])=>({from,to,weight:10}))}));
  await scenario('F11','Hai mutex liền kề: hai tuyến ngược chiều phải chờ, tránh khóa chéo',async({r,robot,state})=>{
    const first = await r.goto('01','K1'),second = await r.goto('02','K2');
    await until(()=>state('02').state === 'BLOCKED' || r.tasks.get(first.task_id)?.status === 'completed','conflict waits');
    await until(()=>r.tasks.get(first.task_id)?.status === 'completed' && r.tasks.get(second.task_id)?.status === 'completed','corridor serial admission');
    assert.equal(robot('01').fault,'');assert.equal(robot('02').fault,'');
    const events = fs.readFileSync(path.join(r.directory,'events.jsonl'),'utf8').trim().split('\n').map(JSON.parse);
    assert.ok(events.find(e=>e.type === 'rfid' && e.robot === '01' && e.node === 'K1').seq < events.find(e=>e.type === 'edge_start' && e.robot === '02' && e.to === 'M6').seq);
  },{mapPath:corridor});
  await scenario('F12','Mất telemetry nhưng MQTT còn sống: không hoàn thành giả, timeout giữ đường',async({r,robot,state,action})=>{
    const one = robot('01'),telemetry = one.telemetry.bind(one);one.telemetry = ()=>{};
    const task = await r.goto('01','K6');await until(()=>one.node === 'K6','virtual arrival');
    assert.equal(r.tasks.get(task.task_id).status,'dispatched');
    await until(()=>state('01').state === 'OFFLINE','telemetry timeout',13000);
    assert.equal(r.tasks.get(task.task_id).status,'interrupted');assert.ok(state('01').uncertain_path.length);
    one.telemetry = telemetry;await action('01','recover');assert.equal(one.fault,'');
  });
  await scenario('F13','Mất grant đầu tiên: xe chờ; retry cùng token mới cho vào',async({r,robot})=>{
    const one = robot('01'),emit = one.mqtt.emit.bind(one.mqtt);let dropped = null;
    one.mqtt.emit = (event,...args)=>{
      if (event === 'message' && args[0].endsWith('/mutex_grant') && !dropped) {
        dropped = JSON.parse(args[1]);r.event('test_grant_drop',dropped);return true;
      }
      return emit(event,...args);
    };
    const task = await r.goto('01','K4');await until(()=>dropped,'grant dropped');
    assert.equal(one.edge,null);assert.equal(one.grants.has(dropped.zone),false);
    await until(()=>r.tasks.get(task.task_id)?.status === 'completed','grant retry completes');
    const applied = r.events.find(e=>e.type === 'grant_ack' && e.robot === '01' && e.zone === dropped.zone);
    assert.equal(applied.grant_id,dropped.grant_id);
  });
  await scenario('F14','Mất toàn bộ ACK: task gián đoạn, không tự chạy lại; ACK trễ bị bỏ qua',async({r,robot,state})=>{
    const one = robot('01');one.paused = true;one.dropAcks = 10;
    const task = await r.goto('01','K6');await until(()=>one.path.length > 0,'GOTO applied');const command = one.lastCommand;
    await until(()=>state('01').state === 'ERROR','ACK exhausted',15000);
    assert.equal(r.tasks.get(task.task_id).status,'interrupted');assert.equal(one.distanceCm,0);
    one.send('ack',String(command));await pause(200);assert.equal(state('01').state,'ERROR');
  });
  await scenario('F15','Phục hồi đồng thời một xe: từ chối thao tác chồng, không nhân task',async({r,robot,state,action,request})=>{
    await action('01','battery',0);await until(()=>state('01').state === 'ERROR','error');await action('01','battery',85);
    const recovery = r.action('01',{action:'recover'});
    const duplicate = await request('/sim-api/robots/01/action',{action:'recover'});assert.equal(duplicate.status,409);
    await recovery;assert.equal(robot('01').paused,false);assert.equal(robot('01').fault,'');
  });
  console.log('Audit results: '+directory);if (results.some(test=>test.status !== 'PASS')) process.exitCode = 1;
}
main().catch(error=>{console.error(error);process.exitCode = 1;});
