// Real Core + broker + gateway + firmware parser/ServoModule/LiftCycle host.
// RFID travel is injected, GPIO/time are adapters; never connects to broker1883.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{once}=require('node:events'),readline=require('node:readline');
const {createGateway}=require('../AGVwebserver/gateway'),{createDatabase}=require('../AGVwebserver/db');
const {MqttFixture}=require('./helpers/mqtt-client');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,label,timeout=12000){const end=Date.now()+timeout;while(Date.now()<end){if(await fn())return;await pause(40);}throw Error('Timed out: '+label);}
async function port(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function open(p){return new Promise(r=>{const s=net.connect(p,'127.0.0.1');s.once('connect',()=>{s.destroy();r(true);});s.once('error',()=>r(false));});}
async function main(){
  const robotId=process.env.AGV_TEST_ROBOT||'01';assert.ok(['01','02'].includes(robotId));
  const root=path.resolve(__dirname,'..'),dir=fs.mkdtempSync(path.join(root,'.qa/servo-integration-'));
  const mqttPort=await port(),corePort=await port(),bin=process.env.AGV_MOSQUITTO_BIN||'C:/msys64/ucrt64/bin';
  fs.copyFileSync(path.join(root,'data/map_layout.json'),path.join(dir,'map.json'));
  fs.writeFileSync(path.join(dir,'broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous true\npersistence false\n`);
  const env={...process.env,AGV_MAP_PATH:path.join(dir,'map.json'),AGV_COMMAND_ID_PATH:path.join(dir,'counter.json'),AGV_TASK_ID_PATH:path.join(dir,'task-counter.json'),
    AGV_RUNTIME_STATE_PATH:path.join(dir,'runtime.json'),AGV_SHIFT_STATE_PATH:path.join(dir,'shift.json'),AGV_MQTT_HOST:'127.0.0.1',AGV_MQTT_PORT:String(mqttPort),AGV_CORE_WS_PORT:String(corePort),AGV_CORE_WS_HOST:'127.0.0.1'};
  delete env.AGV_CORE_TOKEN;delete env.AGV_TEST_MODE;
  const pathKey=Object.keys(env).find(k=>k.toLowerCase()==='path')||'PATH';env[pathKey]=bin+path.delimiter+env[pathKey];
  const children=[],commands=[],trace=[],pending=[];let output='',mqtt,gateway,pump,browser,firmware,hostBusy=false;
  const db=createDatabase(path.join(dir,'test.db'));
  const child=(file,args)=>{const p=spawn(file,args,{cwd:dir,env,windowsHide:true});children.push(p);p.stderr.on('data',d=>output+=d);p.on('error',e=>output+=e.message);return p;};
  try{
    child(path.join(bin,'mosquitto.exe'),['-c',path.join(dir,'broker.conf')]);await until(()=>open(mqttPort),'broker');
    const core=child(process.env.AGV_CORE_BIN||path.join(root,'.qa/servo45-build/agv_core.exe'),['--mode',`active${robotId}`]);core.stdout.on('data',d=>output+=d);
    gateway=createGateway({db,authToken:'',coreToken:'',coreUrl:`ws://127.0.0.1:${corePort}`,reconnectMs:80});
    const address=await gateway.start(0,'127.0.0.1'),base=`http://127.0.0.1:${address.port}`;
    const state=async()=>(await fetch(base+'/api/state')).json();
    const robot=async()=>(await state()).fleet.agvs.find(a=>a.id===robotId);
    const post=async(route,body={})=>{const response=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return{status:response.status,data:await response.json()};};
    await until(async()=>(await state()).core_ready,'Core');const session=(await state()).core_session_id;
    const host=child(process.env.AGV_SERVO_HOST||path.join(root,'.qa/servo45-build/servo_01.exe'),['--receiver']);
    readline.createInterface({input:host.stdout}).on('line',line=>{const resolve=pending.shift();if(resolve)resolve(JSON.parse(line));});
    const started=Date.now(),request=command=>new Promise(resolve=>{pending.push(resolve);host.stdin.write(JSON.stringify({now:Date.now()-started+1,...(command?{command:JSON.stringify(command)}:{})})+'\n');});
    firmware=await request();assert.equal(firmware.angle,-1);assert.equal(firmware.pwm,0);
    mqtt=new MqttFixture(mqttPort);mqtt.on('error',e=>output+=e.message);await until(()=>mqtt.connected,'MQTT');
    const r={node:(robotId==='01'?'S1':'S2'),heading:robotId==='01'?'S':'N',state:'AGV_IDLE',taskActive:false};let travelling=false;
    const publish=()=>{mqtt.publish(`agv/${robotId}/telemetry`,`${r.node}|80|0`);mqtt.publish(`agv/${robotId}/diagnostic`,JSON.stringify({
      version:1,agvId:robotId,session:1,uptimeMs:Date.now()-started+1,encoderSupported:true,encoderBootId:101,encoderCoreSessionId:session,
      ...r,...firmware,state:travelling?r.state:firmware.liftBusy?(firmware.liftState==='LIFTING'?'AGV_LOADING':'AGV_UNLOADING'):'AGV_IDLE',
      taskActive:travelling||firmware.liftBusy,error:'NONE',motor:'STOP',leftDuty:0,rightDuty:0,rfidAgeMs:0,wifi:true,mqtt:true,
      headingSupported:true,headingNode:r.node,headingSource:'bay_default',nodeOrdersSupported:false,
      batteryCalibrated:true,batteryPercentage:80,liftSupported:true}));};
    let commandChain=Promise.resolve();
    mqtt.on('publish',(topic,payload)=>{
      if(topic===`agv/${robotId}/command`)commandChain=commandChain.then(async()=>{
        const c=JSON.parse(payload);commands.push({...c,received_at:Date.now()});
        if(c.cmd==='LIFT_UP'||c.cmd==='LIFT_DOWN'){
          firmware=await request(c);assert.equal(firmware.accepted,true);publish();
          // Deliberately lose the first UP ACK. Retry must not issue PWM again.
          if(c.cmd==='LIFT_DOWN'||commands.filter(prior=>prior.cmd_id===c.cmd_id).length>1)mqtt.publish(`agv/${robotId}/ack`,String(c.cmd_id));
        }else{
          mqtt.publish(`agv/${robotId}/ack`,String(c.cmd_id));
          if(c.cmd==='GOTO'){
            travelling=true;r.state='AGV_MOVING';publish();
            setTimeout(()=>{r.node=c.target;r.heading=c.target==='K5'?'E':'S';travelling=false;r.state='AGV_IDLE';publish();},180);
          }
        }
      }).catch(e=>{output+=e.stack;});
      if(topic===`agv/${robotId}/mutex_grant`)mqtt.publish(`agv/${robotId}/mutex_ack`,payload);
    });
    mqtt.subscribe('agv/#');await until(()=>mqtt.subscribed,'subscribe');
    pump=setInterval(async()=>{if(hostBusy)return;hostBusy=true;try{firmware=await request();publish();}finally{hostBusy=false;}},100);
    publish();await until(async()=>(await robot())?.lift?.supported,'servo capability');
    assert.equal((await post('/api/shift/start')).status,200);
    let page,errors=[];
    if(process.env.AGV_SERVO_BROWSER==='1'){
      const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
      browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
      page=await browser.newPage();page.on('pageerror',e=>errors.push(e.message));await page.goto(base);
      await page.waitForFunction(()=>!document.querySelector('#pickupWarehouseBtn').disabled);
    }
    for(const kind of ['pickup','dropoff']){
      const up=kind==='pickup',response=await post(`/api/agvs/${robotId}/warehouse-action`,{action:kind,warehouse:'K5'});
      assert.equal(response.status,201,JSON.stringify(response.data));assert.equal(response.data.action.servo,true);
      const id=response.data.action.id;await until(async()=>(await robot()).lift?.state===(up?'LIFTING':'LOWERING'),'MCU busy').catch(async e=>{
        console.error(JSON.stringify({commands,firmware,robot:await robot(),actions:(await state()).warehouse_actions}));throw e;
      });
      const first=commands.find(c=>c.cmd===(up?'LIFT_UP':'LIFT_DOWN')),pwmWrites=firmware.writes;
      const shiftRobot=(await state()).shift.robots.find(a=>a.id===robotId);
      assert.equal(shiftRobot.reason,up?'Dang nang hang':'Dang ha hang');
      assert.equal(firmware.angle,up?45:0);assert.equal((await robot()).virtual_cargo,!up);
      assert.equal((await post(`/api/agvs/${robotId}/goto`,{target:'G5'})).status,409);
      if(page){await page.waitForFunction(({id,text})=>document.querySelector(`[data-robot-transfer="${id}"]`).textContent.includes(text),{id:robotId,text:up?'Đang nâng kệ':'Đang hạ kệ'});
        assert.equal(await page.locator(`[data-virtual-cargo="${robotId}"]`).count(),up?0:1);
        await page.screenshot({path:path.join(dir,up?'lifting.png':'lowering.png')});}
      await until(async()=>(await state()).warehouse_actions.find(a=>a.id===id).status==='completed','servo + exit',16000);
      await commandChain;const end=await robot();assert.equal(end.node,'G5');assert.equal(end.virtual_cargo,up);assert.equal(end.virtual_cargo_known,true);
      assert.equal(firmware.writes,pwmWrites,'same cmd_id retry must not rewrite PWM');
      const exit=commands.find(c=>c.cmd==='GOTO'&&c.target==='G5'&&c.received_at>first.received_at);
      assert.ok(exit.received_at-first.received_at>=4950,'exit must wait for 5s firmware completion');
      if(up)assert.ok(commands.filter(c=>c.cmd_id===first.cmd_id).length>=2,'lost ACK retried');
      if(page){await page.waitForFunction(({loaded,id})=>document.querySelectorAll(`[data-virtual-cargo="${id}"]`).length===(loaded?1:0),{loaded:up,id:robotId});
        await page.screenshot({path:path.join(dir,up?'cargo-on.png':'cargo-off.png')});}
      trace.push({kind,angle:firmware.angle,pwm:firmware.pwm,writes:firmware.writes,exit_delay_ms:exit.received_at-first.received_at,cargo:end.virtual_cargo});
    }
    assert.deepEqual(errors,[]);assert.equal((await (await fetch(base+'/api/orders')).json()).length,0);
    fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify({pass:true,robotId,browser:!!page,trace,commands},null,2));
    console.log('PASS servo Core/MQTT/gateway + firmware PWM host: 5s lift/lower, lost ACK dedup, motion lock, cargo, gate; '+dir);
  }catch(e){console.error(output.slice(-5000));throw e;}
  finally{clearInterval(pump);await browser?.close();mqtt?.close();await gateway?.close();db.close();for(const p of children)if(p.exitCode===null){p.kill();await Promise.race([once(p,'exit'),pause(1000)]);}}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
