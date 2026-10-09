const http = require('http');
const {createSimulation} = require('./server');

function createLauncher(options = {}) {
  const port = options.port ?? Number(process.env.SIM_WEB_PORT || 3100);
  let simulation = null, starting = null;
  const server = http.createServer(async (req,res)=>{
    res.setHeader('Cache-Control','no-store');
    if (req.method === 'GET' && (req.url === '/' || req.url === '/simulation')) {
      res.setHeader('Content-Type','text/html; charset=utf-8');
      res.end(`<!doctype html><html lang="vi"><meta charset="utf-8"><title>Mô phỏng AGV</title>
<style>body{font:18px system-ui;max-width:40rem;margin:15vh auto;padding:1rem;color:#172338}button{font:inherit;padding:.5rem 1rem}</style>
<h1>Đang mở mô phỏng AGV…</h1><p id="status">Đang chuẩn bị Core và hai xe ảo.</p>
<script>fetch('/launch/start',{method:'POST'}).then(async r=>{const x=await r.json();if(!r.ok)throw Error(x.error);location.replace(x.url)}).catch(e=>{document.querySelector('#status').textContent=e.message;document.body.insertAdjacentHTML('beforeend','<button onclick="location.reload()">Thử lại</button>')})</script></html>`);
      return;
    }
    if (req.method === 'POST' && req.url === '/launch/start') {
      const origin = req.headers.origin;
      if (origin && origin !== `http://127.0.0.1:${server.address().port}`) {
        res.writeHead(403,{'Content-Type':'application/json'}).end(JSON.stringify({error:'Origin không hợp lệ'}));return;
      }
      try {
        if (!simulation && !starting) starting = createSimulation({
          port:0,runParent:options.runParent,coreBinary:options.coreBinary,mosquittoBin:options.mosquittoBin,
          idleTimeoutMs:options.idleTimeoutMs || 20000,viewerTimeoutMs:options.viewerTimeoutMs || 12000,
          onClose:()=>{simulation=null;}
        }).then(value=>{simulation=value;return value;}).finally(()=>{starting=null;});
        const active = simulation || await starting;
        res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify({url:active.url}));
      } catch (error) {
        res.writeHead(503,{'Content-Type':'application/json'}).end(JSON.stringify({error:error.message}));
      }
      return;
    }
    if (req.method === 'GET' && req.url === '/launch/status') {
      res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify({active:!!simulation,starting:!!starting,url:simulation?.url || null}));return;
    }
    res.writeHead(404).end();
  });
  return {
    server,
    async start() {await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',resolve);});return server.address();},
    async close() {if (simulation) await simulation.close();await new Promise(resolve=>server.close(resolve));}
  };
}

if (require.main === module) {
  const launcher = createLauncher();
  launcher.start().then(address=>console.log(`Mở http://127.0.0.1:${address.port}/simulation để bắt đầu mô phỏng`))
    .catch(error=>{console.error(error.message);process.exitCode=1;});
  const stop = ()=>launcher.close().then(()=>process.exit(0));
  process.once('SIGINT',stop);process.once('SIGTERM',stop);
}
module.exports = {createLauncher};
