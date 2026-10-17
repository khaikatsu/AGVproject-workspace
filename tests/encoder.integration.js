// Fixture encoder -> MQTT -> Core test01 -> gateway -> dashboard, dữ liệu/cổng riêng.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const {createGateway}=require('../AGVwebserver/server'),{createDatabase}=require('../AGVwebserver/db');
const {MqttFixture}=require('./helpers/mqtt-client');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,timeout=10000){const end=Date.now()+timeout;while(Date.now()<end){if(await check())return;await pause(30);}throw Error('Encoder fixture timeout');}
async function port(){const server=net.createServer();server.listen(0,'127.0.0.1');await once(server,'listening');const n=server.address().port;await new Promise(r=>server.close(r));return n;}
async function available(port){return new Promise(r=>{const s=net.connect(port,'127.0.0.1');s.once('connect',()=>{s.destroy();r(true);});s.once('error',()=>r(false));});}
async function main(){
  const root=path.resolve(__dirname,'..');fs.mkdirSync(path.join(root,'.qa'),{recursive:true});
  const dir=fs.mkdtempSync(path.join(root,'.qa/encoder-test-')),mqttPort=await port(),corePort=await port();
  const bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(dir,'map.json'));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_COMMAND_ID_PATH:path.join(dir,'command.json'),
    AGV_TASK_ID_PATH:path.join(dir,'task.json'),AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),
    AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_HOST:'127.0.0.1',AGV_CORE_WS_PORT:String(corePort)};
  for(const key of ['AGV_CORE_TOKEN','AGV_AUTH_TOKEN','AGV_TEST_MODE','AGV_ONE_ROBOT_TRIAL','AGV_TWO_ROBOT_TRIAL','AGV_MONITOR_ONLY_02',
    'AGV_SECURITY_REQUIRED','AGV_MQTT_CA','AGV_MQTT_USER','AGV_MQTT_PASSWORD','AGV_HTTPS_CERT','AGV_HTTPS_KEY'])delete env[key];
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],commands=[],grants=[],observed=[],errors=[];let output='',mqtt,gateway,pump,view,socket,browser,page,coreSession='',seq=0,base;
  const started=Date.now(),uptime=()=>Date.now()-started;
  const db=createDatabase(path.join(dir,'fixture.db'));
  const r={node:'S1',state:'AGV_IDLE',error:'NONE',motor:'STOP',taskActive:false,leftDuty:0,rightDuty:0,
    heading:'S',headingSource:'bay_default',rfidAgeMs:0,routeCommandId:0,pathIndex:0,nextNode:'',awaitingNodeOrder:false,
    session:1,encoderBootId:1234};
  const child=(file,args)=>{const p=spawn(file,args,{env,cwd:dir,windowsHide:true});children.push(p);p.stdout.on('data',d=>output+=d);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;};
  const state=async()=>(await fetch(base+'/api/state')).json(),robot=async()=>(await state()).fleet.agvs.find(a=>a.id==='01');
  const post=async(route,body={})=>{const response=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const data=await response.json();assert.equal(response.status,200,JSON.stringify(data));return data;};
  const marker=async()=>page.evaluate(()=>{const g=document.querySelector('[data-robot="01"]'),t=g.transform.baseVal.consolidate().matrix;return {x:t.e,y:t.f,source:g.dataset.positionSource};});
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await until(()=>available(mqttPort));
    child(process.env.AGV_CORE_BIN||path.join(root,'.qa/encoder-build/agv_core.exe'),['--mode','robot01']);
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:80});
    const address=await gateway.start(0,'127.0.0.1');base=`http://127.0.0.1:${address.port}`;
    await until(async()=>{const s=await state();return s.core_ready&&s.mqtt_connected;});
    socket=new WebSocket(base.replace('http:','ws:')+'/ws?delta=1');socket.on('message',raw=>{
      const m=JSON.parse(raw);if(m.type==='state_delta'){assert.equal(m.base_state_seq,view.state_seq);view={...view,...m.changes,state_seq:m.state_seq};}else view=m;
    });await once(socket,'open');await until(()=>view);
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected);
    mqtt.on('publish',(topic,payload)=>{
      if(topic==='agv/server/heartbeat')coreSession=payload;
      if(topic==='agv/01/command'){
        const c=JSON.parse(payload);commands.push(c);mqtt.publish('agv/01/ack',String(c.cmd_id));
        if(c.cmd==='GOTO')Object.assign(r,{routeCommandId:c.cmd_id,pathIndex:0,nextNode:c.path[0],taskActive:true,state:'AGV_WAITING',awaitingNodeOrder:true});
        if(c.cmd==='NODE_ORDER')Object.assign(r,{state:'AGV_MOVING',motor:r.node.startsWith('K')?'BACKWARD':'FORWARD',leftDuty:180,rightDuty:220,awaitingNodeOrder:false});
      }
      if(topic==='agv/01/mutex_grant'){const g=JSON.parse(payload);grants.push(g);mqtt.publish('agv/01/mutex_ack',payload);}
    });mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed&&coreSession);
    const publish=()=>{
      mqtt.publish('agv/01/telemetry',`${r.node}|85|0`);
      mqtt.publish('agv/01/diagnostic',JSON.stringify({version:1,agvId:'01',uptimeMs:uptime(),...r,
        headingSupported:true,headingNode:r.node,nodeOrdersSupported:true,straightThroughSupported:true,
        wifi:true,mqtt:true,batteryCalibrated:true,batteryPercentage:85,encoderSupported:true,encoderCoreSessionId:coreSession}));
    };
    pump=setInterval(publish,250);publish();await until(async()=>(await robot())?.encoder_supported);
    await until(async()=>(await state()).shift.can_start);await post('/api/shift/start');
    const set=async(values)=>{Object.assign(r,values);publish();await until(async()=>(await robot())?.node===r.node);await pause(70);};
    await set({node:'G6',heading:'S',headingSource:'route'});
    if(process.argv.includes('--browser')){
      const pw=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
      browser=await pw.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
      page=await browser.newPage({viewport:{width:1500,height:1000}});page.on('pageerror',e=>errors.push(e.message));
      await page.goto(base);await page.locator('[data-robot="01"]').waitFor();
    }
    const task=await post('/api/agvs/01/goto',{target:'G1'});await until(()=>r.routeCommandId&&commands.some(c=>c.cmd==='NODE_ORDER'));
    mqtt.publish('agv/01/mutex_request','M4');await until(()=>grants.some(g=>g.zone==='M4'));
    const packet=(distance,speed,extra={})=>({schema_version:1,robot_id:'01',core_session_id:coreSession,boot_id:r.encoderBootId,
      connection_session:r.session,seq:++seq,uptime_ms:uptime(),left_ticks:Math.round(distance*1.61),right_ticks:Math.round(distance*1.61),
      route_cmd_id:r.routeCommandId,path_index:r.pathIndex,from_node:r.node,to_node:r.nextNode,tracking:true,translating:speed>0,
      reverse:r.motor==='BACKWARD',distance_cm:distance,speed_cm_s:speed,pulses_per_100cm:161,...extra});
    const send=async(distance,speed,extra={})=>{const p=packet(distance,speed,extra),at=Date.now();mqtt.publish('agv/01/odometry',JSON.stringify(p));
      await until(()=>view.fleet.agvs.find(a=>a.id==='01')?.odometry?.seq===p.seq);const a=await robot();
      assert.equal(a.node,r.node);assert.equal(a.speed_cm_s,speed);assert.equal(a.odometry.valid,true);
      observed.push({from:p.from_node,to:p.to_node,seq:p.seq,distance,speed,mqtt_to_web_ms:Date.now()-at});return p;};
    const first=await send(10,20);let previousMarker=page?await marker():null;
    await send(20,20);
    if(page){await page.locator('#robot01Speed').filter({hasText:'20.0 cm/s'}).waitFor();await pause(240);
      const moving=await marker();assert.equal(moving.source,'encoder');assert(moving.y>previousMarker.y);previousMarker=moving;
      assert.match(await page.locator('#robot01Position').innerText(),/ước tính G6 → M4/);}
    Object.assign(r,{obstacle:true,ultrasonicCm:12,frontUltrasonicCm:12,rearUltrasonicCm:null,obstacleDirection:'front'});
    publish();await until(async()=>!!(await robot()).obstacle_detection);
    const detection=(await robot()).obstacle_detection;
    assert.equal(detection.from,'G6');assert.equal(detection.to,'M4');assert.equal(detection.range_cm,12);
    assert.equal((await robot()).obstacle_alert.direction,'front');
    if(page){await page.locator('[data-sonar-obstacle="01"]').waitFor();
      await page.locator('[data-sonar-obstacle-edge="G6:M4"]').waitFor({state:'attached'});
      assert.equal(await page.locator('[data-sonar-obstacle-edge="G6:M4"]').evaluate(el=>getComputedStyle(el).stroke),'rgb(239, 68, 68)');
      await page.screenshot({path:path.join(dir,'sonar-front.png'),fullPage:true});}
    Object.assign(r,{obstacle:false,ultrasonicCm:25,frontUltrasonicCm:25});
    publish();await until(async()=>{const a=await robot();return a.obstacle_detection===null&&a.obstacle_alert===null;});
    assert.equal((await robot()).obstacle_alert,null);
    if(page)await page.locator('[data-sonar-obstacle="01"]').waitFor({state:'detached'});
    const saturated=await send(150,20);assert.equal((await robot()).estimated_position.progress,0.98);
    assert.equal((await state()).fleet.tasks.find(t=>t.task_id===task.task_id).status,'dispatched');
    assert.equal((await state()).mutex.M4.owner,'01');
    for(const invalid of [{...first,seq:saturated.seq},{...saturated,seq:seq+1,route_cmd_id:r.routeCommandId+999},
      {...saturated,seq:seq+1,core_session_id:'old-core'},{...saturated,seq:seq+1,connection_session:999},
      {...saturated,seq:seq+1,distance_cm:-10},{...saturated,seq:seq+1,speed_cm_s:'20'}]){
      mqtt.publish('agv/01/odometry',JSON.stringify(invalid));await pause(60);assert.equal((await robot()).odometry.seq,saturated.seq);
    }
    await until(async()=>(await robot()).speed_cm_s==null);
    if(page){await page.locator('#robot01Speed').filter({hasText:'— cm/s'}).waitFor();const held=await marker();await pause(350);
      assert.deepEqual(await marker(),held);assert.equal(held.source,'encoder_stale');}
    // Stop/pivot does not advance a fresh estimate; intermediate RFID resets the anchor.
    await send(150,0);await set({node:'M4',pathIndex:1,nextNode:'GS2',heading:'W'});
    if(page)await page.waitForFunction(()=>document.querySelectorAll('#toastContainer .toast.error').length===0);
    await send(5,15);
    assert.equal((await robot()).estimated_position.from,'M4');assert.equal((await robot()).estimated_position.progress,0.1);
    if(page){await page.locator('#robot01Speed').filter({hasText:'15.0 cm/s'}).waitFor();
      const bounds=await page.evaluate(()=>{
        const read=id=>{const el=document.querySelector(id),r=el.getBoundingClientRect(),s=getComputedStyle(el);return {top:r.top,bottom:r.bottom,height:r.height,flex:s.flex,display:s.display,minHeight:s.minHeight};};
        return {center:read('#center-panel'),viewport:read('#mapViewport'),foot:read('.map-foot'),legend:read('#mapLegend'),rootOverflow:getComputedStyle(document.documentElement).overflowY};
      });
      fs.writeFileSync(path.join(dir,'layout.json'),JSON.stringify(bounds,null,2));
      assert(bounds.legend.bottom<=bounds.center.bottom+1,JSON.stringify(bounds));
      assert.notEqual(bounds.rootOverflow,'hidden','Expanded desktop can scroll to the encoder legend');
      await page.screenshot({path:path.join(dir,'encoder-desktop.png'),fullPage:true});
      await page.setViewportSize({width:390,height:844});await pause(150);await page.screenshot({path:path.join(dir,'encoder-mobile.png'),fullPage:true});
      assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.setViewportSize({width:1500,height:1000});}
    // Bound session changes invalidate the old sample; a prior connection cannot publish fresh positions.
    r.session=2;publish();await until(async()=>(await robot()).speed_cm_s==null);
    mqtt.publish('agv/01/odometry',JSON.stringify({...saturated,seq:++seq,uptime_ms:uptime(),from_node:'M4',to_node:'GS2',path_index:1}));
    await pause(100);assert.equal((await robot()).speed_cm_s,null);seq=0;await send(0,0);await send(8,15);
    if(page)await pause(220);
    const stoppedMarker=page?await marker():null;
    await post('/api/tasks/cancel',{task_id:task.task_id});
    await set({state:'AGV_IDLE',motor:'STOP',taskActive:false,nextNode:'',awaitingNodeOrder:false});
    await send(0,0,{tracking:false,translating:false,route_cmd_id:0,path_index:0,to_node:'',reverse:false});
    if(page){await pause(220);const stopped=await marker();assert.equal(stopped.x,stoppedMarker.x);assert.equal(stopped.y,stoppedMarker.y);}
    const heldGrant=grants.findLast(g=>g.zone==='M4');mqtt.publish('agv/01/mutex_release',JSON.stringify(heldGrant));
    await until(async()=>(await state()).mutex.M4.owner==='');
    await set({node:'K5',state:'AGV_IDLE',motor:'STOP',taskActive:false,leftDuty:0,rightDuty:0,heading:'E',headingSource:'bay_default',nextNode:''});
    const reverse=await post('/api/agvs/01/goto',{target:'G5'});await until(()=>r.nextNode==='G5'&&r.motor==='BACKWARD');
    await send(5,12);assert.equal((await robot()).estimated_position.reverse,true);
    Object.assign(r,{obstacle:true,ultrasonicCm:10,rearUltrasonicCm:10,obstacleDirection:'rear'});
    publish();await until(async()=>(await robot()).obstacle_detection?.direction==='rear');
    const rearDetection=(await robot()).obstacle_detection;
    assert.equal(rearDetection.from,'K5');assert.equal(rearDetection.to,'G5');
    // Map thật: K5=(170,105), G5=(150,105); đi lùi 5 cm + sonar 10 cm.
    assert.equal(rearDetection.x,155);assert.equal(rearDetection.y,105);
    if(page){await page.locator('[data-sonar-obstacle="01"]').waitFor();
      await page.locator('#robot01Position').filter({hasText:'PHÍA SAU'}).waitFor();
      assert.equal(await page.locator('[data-sonar-obstacle="01"]').evaluate(el=>{
        const b=el.getBoundingClientRect();return document.elementFromPoint(b.x+b.width/2,b.y+b.height/2)===el;
      }),true,'Sonar dot must remain above RFID nodes');
      await page.screenshot({path:path.join(dir,'sonar-rear.png'),fullPage:true});}
    Object.assign(r,{obstacle:false,ultrasonicCm:null,rearUltrasonicCm:null});
    publish();await until(async()=>{const a=await robot();return a.obstacle_detection===null&&a.obstacle_alert===null;});
    if(page){await page.locator('#robot01Position').filter({hasText:'K5 → G5'}).waitFor();await page.screenshot({path:path.join(dir,'encoder-reverse.png'),fullPage:true});}
    // Lost QoS0 error event: the current MCU diagnostic alone must expose the fault.
    Object.assign(r,{state:'AGV_SAFE_STOP',error:'LINE_LOST',motor:'STOP',leftDuty:0,rightDuty:0,taskActive:false});
    publish();await until(async()=>(await robot()).state==='ERROR');
    const beforeFault=(await robot()).odometry.seq;mqtt.publish('agv/01/odometry',JSON.stringify(packet(10,12)));await pause(150);
    assert.equal((await robot()).odometry.seq,beforeFault);assert.equal((await robot()).speed_cm_s,null);
    assert.equal((await state()).fleet.tasks.find(t=>t.task_id===reverse.task_id).status,'interrupted');
    clearInterval(pump);mqtt.publish('agv/01/status','offline');await until(async()=>(await robot()).state==='OFFLINE');
    assert.equal((await robot()).estimated_position,null);
    if(page){await page.locator('#robot01Speed').filter({hasText:'— cm/s'}).waitFor();await page.locator('[data-robot="01"]').click({force:true});assert.deepEqual(errors,[]);}
    const durable=JSON.parse(fs.readFileSync(path.join(dir,'runtime.json'),'utf8'));
    assert(!durable.fleet.agvs.some(a=>'odometry' in a||'estimated_position' in a||'obstacle_detection' in a));
    fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify({pass:true,observed,commands,grants,errors},null,2));
    console.log(`PASS encoder integration: motion/speed, node/task/mutex invariants, wrong session/route/schema/order, stale/fault/offline, reverse${page?', Edge desktop/mobile':''}. ${dir}`);
  }catch(error){fs.writeFileSync(path.join(dir,'failure.json'),JSON.stringify({error:error.stack,r,view,output,errors},null,2));console.error(output.slice(-2000));console.error(dir);throw error;}
  finally{clearInterval(pump);await browser?.close();socket?.terminate();mqtt?.close();await gateway?.close();db.close();
    for(const p of children)if(p.exitCode===null){p.kill();await Promise.race([once(p,'exit'),pause(1000)]);}}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
