const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createLauncher} = require('../simulation/launcher');

const wait = ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,timeout=12000) {
  const deadline = Date.now()+timeout;
  while (Date.now()<deadline) {const value=await fn();if(value)return value;await wait(150);}
  throw new Error('Hết thời gian chờ');
}
async function main() {
  const runParent = path.join(__dirname,'../.qa/simulation-on-view');
  fs.mkdirSync(runParent,{recursive:true});
  const launcher = createLauncher({port:0,runParent,idleTimeoutMs:2500,viewerTimeoutMs:800});
  const address = await launcher.start(),base=`http://127.0.0.1:${address.port}`;
  try {
    assert.equal((await (await fetch(base+'/launch/status')).json()).active,false);
    const denied = await fetch(base+'/launch/start',{method:'POST',headers:{Origin:'https://other.example'}});
    assert.equal(denied.status,403);
    const firstResponse = await fetch(base+'/launch/start',{method:'POST'});
    const first = await firstResponse.json();
    assert.equal(firstResponse.status,200,first.error);
    assert.match(first.url,/\/simulation$/);
    const second = await (await fetch(base+'/launch/start',{method:'POST'})).json();
    assert.equal(second.url,first.url,'hai tab dùng chung phiên');
    const simBase = new URL(first.url).origin;
    const send = async (id,leaving=false)=>fetch(simBase+'/sim-api/viewer',{
      method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id,leaving})
    });
    const a='viewer-aaaaaaaaaaaaaaaa',b='viewer-bbbbbbbbbbbbbbbb';
    assert.equal((await send(a)).status,200);
    assert.equal((await send(b)).status,200);
    assert.equal((await send(a,true)).status,200);
    for (let i=0;i<5;i++) {await wait(500);assert.equal((await send(b)).status,200);}
    assert.equal((await (await fetch(base+'/launch/status')).json()).active,true,'tab còn lại giữ phiên');
    await send(b,true);
    await until(async()=>!(await (await fetch(base+'/launch/status')).json()).active,10000);
    assert.equal((await (await fetch(base+'/launch/status')).json()).active,false,'điểm mở trang vẫn chạy');
    const dirs=fs.readdirSync(runParent).filter(x=>x.startsWith('run-'));
    assert.ok(dirs.some(x=>fs.existsSync(path.join(runParent,x,'report.json'))),'báo cáo được ghi');
    const reopened=await (await fetch(base+'/launch/start',{method:'POST'})).json();
    assert.notEqual(reopened.url,first.url,'mở lại tạo phiên mới');
    console.log('PASS on-view: lazy start, two tabs, idle shutdown, report, reopen');
  } finally {await launcher.close();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
