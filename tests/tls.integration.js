const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),net=require('node:net'),https=require('node:https');
const {spawn,spawnSync}=require('node:child_process'),{once}=require('node:events');
const {createGateway}=require('../AGVwebserver/gateway'),{createDatabase}=require('../AGVwebserver/db');
const {MqttFixture}=require('./helpers/mqtt-client');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,timeout=7000){const end=Date.now()+timeout;while(Date.now()<end){if(await fn())return;await pause(40);}throw new Error('TLS condition timed out');}
async function freePort(){const s=net.createServer();s.listen(0,'127.0.0.1');await once(s,'listening');const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function main(){
 const root=path.resolve(__dirname,'..'),bin=process.env.AGV_MOSQUITTO_BIN || (process.platform==='win32'?'C:/msys64/ucrt64/bin':'/usr/bin');
 fs.mkdirSync(path.join(root,'.qa'),{recursive:true});const dir=fs.mkdtempSync(path.join(root,'.qa/tls-test-'));
 const tool=name=>path.join(bin,name+(process.platform==='win32'?'.exe':''));
 function run(name,args){const r=spawnSync(tool(name),args,{cwd:dir,windowsHide:true,encoding:'utf8'});if(r.status!==0)throw new Error(`${name}: ${r.stderr || r.error}`);}
 run('openssl',['req','-x509','-newkey','rsa:2048','-nodes','-days','1','-subj','/CN=AGV isolated test CA','-keyout','ca.key','-out','ca.crt']);
 run('openssl',['req','-newkey','rsa:2048','-nodes','-subj','/CN=localhost','-keyout','server.key','-out','server.csr']);
 fs.writeFileSync(path.join(dir,'ext.cnf'),'subjectAltName=DNS:localhost,IP:127.0.0.1\n');
 run('openssl',['x509','-req','-in','server.csr','-CA','ca.crt','-CAkey','ca.key','-CAcreateserial','-days','1','-extfile','ext.cnf','-out','server.crt']);
 const secret='isolated-test-password';for(const [i,user] of ['core','01','02'].entries())run('mosquitto_passwd',[...(i===0?['-c']:[]),'-b','passwords',user,secret]);
 const mqttPort=await freePort(),corePort=await freePort(),p=name=>path.join(dir,name).replace(/\\/g,'/');
 fs.copyFileSync(path.join(root,'deploy/mosquitto.acl'),p('acl'));
 fs.writeFileSync(p('broker.conf'),`listener ${mqttPort} 127.0.0.1\nallow_anonymous false\npassword_file ${p('passwords')}\nacl_file ${p('acl')}\ncafile ${p('ca.crt')}\ncertfile ${p('server.crt')}\nkeyfile ${p('server.key')}\ntls_version tlsv1.2\n`);
 fs.copyFileSync(path.join(root,'data/map_layout.json'),p('map.json'));
 const coreToken='test-core-token-'.repeat(4),webToken='test-web-token-'.repeat(4),children=[],clients=[],db=createDatabase(p('db.sqlite'));
 let gateway,log='';const env={...process.env,AGV_MAP_PATH:p('map.json'),AGV_MQTT_HOST:'localhost',AGV_MQTT_PORT:String(mqttPort),AGV_MQTT_USER:'core',AGV_MQTT_PASSWORD:secret,AGV_MQTT_CA:p('ca.crt'),AGV_SECURITY_REQUIRED:'1',AGV_CORE_WS_PORT:String(corePort),AGV_CORE_TOKEN:coreToken};
 function child(command,args=[]){const c=spawn(command,args,{cwd:dir,env,windowsHide:true});children.push(c);c.stdout.on('data',d=>log+=d);c.stderr.on('data',d=>log+=d);c.on('error',e=>log+=e.message);return c;}
 function request(url,headers={}){return new Promise((resolve,reject)=>{https.get(url,{ca:fs.readFileSync(p('ca.crt')),headers},r=>{let text='';r.on('data',d=>text+=d);r.on('end',()=>resolve({status:r.statusCode,body:JSON.parse(text)}));}).on('error',reject);});}
 try{
   child(tool('mosquitto'),['-c',p('broker.conf'),'-v']);await pause(400);
   child(process.env.AGV_CORE_BIN || path.join(root,'build/agv_core.exe'));
   gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${corePort}`,coreToken,authToken:webToken,tls:{cert:fs.readFileSync(p('server.crt')),key:fs.readFileSync(p('server.key'))},reconnectMs:100});
   const address=await gateway.start(0,'127.0.0.1'),base=`https://localhost:${address.port}`,auth={Authorization:`Bearer ${webToken}`};
   await until(async()=>{const r=await request(base+'/api/health',auth);return r.body.core_ready && r.body.mqtt_connected;});
   assert.equal((await request(base+'/api/state')).status,401);
   const options={ca:fs.readFileSync(p('ca.crt')),password:secret};
   const observe=new MqttFixture(mqttPort,{...options,username:'core'});clients.push(observe);observe.on('error',e=>log+=e.message);await once(observe,'connected');assert.ok(observe.connected);
   const messages=[];observe.on('publish',(topic,payload)=>messages.push({topic,payload}));observe.subscribe('agv/+/telemetry');await once(observe,'subscribed');
   const robot=new MqttFixture(mqttPort,{...options,username:'01',will:{topic:'agv/01/status',payload:'offline',retained:true}});clients.push(robot);robot.on('error',e=>log+=e.message);await once(robot,'connected');assert.ok(robot.connected);
   robot.publish('agv/01/telemetry','S1|85|0');await until(()=>messages.some(m=>m.topic==='agv/01/telemetry'));
   robot.publish('agv/02/telemetry','S2|85|0');await pause(300);assert.ok(!messages.some(m=>m.topic==='agv/02/telemetry'),'01 cannot impersonate 02');
   const received=[];robot.on('publish',(topic,payload)=>received.push({topic,payload}));robot.subscribe('agv/server/heartbeat');await once(robot,'subscribed');await until(()=>received.some(m=>m.topic==='agv/server/heartbeat'));
   robot.subscribe('agv/02/command');await once(robot,'subscribed');observe.publish('agv/02/command','{"cmd":"WAIT","cmd_id":5000}');await pause(300);
   assert.ok(!received.some(m=>m.topic==='agv/02/command'),'01 cannot read commands for 02');
   const wrong=new MqttFixture(mqttPort,{...options,username:'02',password:'incorrect'});clients.push(wrong);wrong.on('error',()=>{});await once(wrong,'connected');assert.equal(wrong.connected,false);
   robot.close();await until(async()=>(await request(base+'/api/state',auth)).body.fleet.agvs.find(a=>a.id==='01')?.state==='OFFLINE');
   console.log('PASS TLS: verified CA/hostname, libmosquitto auth+TLS, HTTPS token, per-robot MQTT write/read ACL, rejected password, real LWT');console.log('Artifacts: '+dir);
 }catch(error){fs.writeFileSync(p('failure.log'),log);console.error(log.slice(-3000));throw error;}
 finally{for(const c of clients)c.close();if(gateway)await gateway.close();for(const c of children)if(c.exitCode==null)c.kill();db.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
