const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const http=require('node:http');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createLauncher}=require('../simulation/launcher');
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check) {
  const end=Date.now()+12000;
  while(Date.now()<end){const value=await check();if(value)return value;await wait(100);}
  throw Error('Public simulation fixture timed out');
}
async function main(){
  const runParent=fs.mkdtempSync(path.join(__dirname,'../.qa/public-simulation-test-'));
  const launcher=createLauncher({port:0,publicProxy:true,runParent,idleTimeoutMs:1800,viewerTimeoutMs:700});
  const address=await launcher.start(),base=`http://127.0.0.1:${address.port}`;
  const headers={Host:'demo.example',Origin:'https://demo.example'};
  function request(route,options={}) {
    return new Promise((resolve,reject)=>{
      const req=http.request(base+route,{...options,headers:{...headers,...options.headers}},res=>{
        let body='';res.setEncoding('utf8');res.on('data',chunk=>body+=chunk);
        res.on('end',()=>resolve({status:res.statusCode,text:async()=>body,json:async()=>JSON.parse(body)}));
      });
      req.setTimeout(15000,()=>req.destroy(Error('HTTP timed out')));req.on('error',reject);req.end(options.body);
    });
  }
  const get=route=>request(route);
  const post=(route,body={},origin=headers.Origin)=>request(route,{method:'POST',headers:{Origin:origin,'Content-Type':'application/json'},body:JSON.stringify(body)});
  let ws;
  try {
    assert.equal((await (await get('/launch/status')).json()).public_proxy,true);
    assert.equal((await get('/sim-api/state')).status,503);
    assert.match(await (await get('/simulation')).text(),/launch\/start/);
    assert.equal((await post('/launch/start',{},'https://foreign.example')).status,403);
    const started=await post('/launch/start');assert.equal(started.status,200);
    assert.equal((await started.json()).url,'/simulation','public redirect stays on the shared origin');
    const html=await (await get('/simulation')).text();
    assert.match(html,/data-sim-launcher="\/simulation"/);
    assert.match(html,/sim-assets\/simulation.js/);
    for(const asset of ['/dashboard.js','/dashboard-layout.css','/sim-assets/simulation.js']) assert.equal((await get(asset)).status,200);
    assert.equal((await (await get('/auth/status')).json()).authenticated,true);
    assert.equal((await get('/sim-api/state')).status,200);
    assert.equal((await post('/sim-api/viewer',{id:'public-viewer-aaaaaaaaaaaa'})).status,200);
    const configured=await post('/sim-api/settings',{timeScale:2});assert.equal(configured.status,200);
    assert.equal((await configured.json()).settings.timeScale,2,'public POST reaches only simulation settings');
    assert.equal((await post('/sim-api/settings',{speedMultiplier:1},'https://foreign.example')).status,403);
    const health=await until(async()=>{const h=await (await get('/api/health')).json();return h.core_ready&&h;});
    assert.equal(health.mqtt_connected,true);
    ws=new WebSocket(base.replace('http:','ws:')+'/ws?delta=1',{headers});
    const frame=await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('WS timed out')),5000);ws.once('message',message=>{clearTimeout(timer);resolve(JSON.parse(message));});ws.once('error',reject);});
    assert.ok(frame.map.nodes.length,'WebSocket proxy carries simulation snapshots');
    ws.close();
    const foreignSocket=new WebSocket(base.replace('http:','ws:')+'/ws',{headers:{...headers,Origin:'https://foreign.example'}});
    foreignSocket.on('error',()=>{});
    const deniedSocket=await new Promise((resolve,reject)=>{
      foreignSocket.once('unexpected-response',(req,res)=>{resolve(res.statusCode);req.destroy();res.destroy();});
      foreignSocket.once('open',()=>{foreignSocket.terminate();reject(Error('Foreign WS was accepted'));});
    });
    assert.equal(deniedSocket,403);
    await post('/sim-api/viewer',{id:'public-viewer-aaaaaaaaaaaa',leaving:true});
    await until(async()=>!(await (await get('/launch/status')).json()).active);
    assert.equal((await get('/api/health')).status,503);
    assert.equal((await post('/launch/start')).status,200);
    assert.equal(fs.readdirSync(runParent).filter(name=>name.startsWith('run-')).length,2);
    console.log('PASS public simulation: HTTPS origin, relative redirect, assets, REST, WS, foreign origin, idle close, reopen');
  } finally {ws?.terminate();await launcher.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
