const http = require('http');

// Public requests terminate here. The target is always the isolated simulation,
// never the operational gateway, Core or MQTT listener.
function sameOrigin(req) {
  if (!req.headers.origin) return true;
  try {
    const origin = new URL(req.headers.origin);
    return ['http:', 'https:'].includes(origin.protocol) &&
      origin.host === new URL(`http://${req.headers.host}`).host;
  } catch { return false; }
}
function createPublicProxy() {
  const sockets = new Set();
  function upstream(req, simulation) {
    const target = new URL(simulation.base);
    if (target.hostname !== '127.0.0.1') throw new Error('Proxy chỉ dùng phiên mô phỏng loopback');
    const headers = {...req.headers, host: target.host};
    for (const key of Object.keys(headers)) if (key.startsWith('x-forwarded-') || key === 'forwarded') delete headers[key];
    if (headers.origin) headers.origin = target.origin;
    return http.request({hostname:target.hostname,port:target.port,path:req.url,method:req.method,headers});
  }
  function request(req, res, simulation) {
    if (!sameOrigin(req)) {res.writeHead(403).end('Origin không hợp lệ');return;}
    if (!simulation) {res.writeHead(503).end('Mở /simulation để khởi động phiên mô phỏng');return;}
    const remote = upstream(req, simulation);
    remote.on('response', response => {res.writeHead(response.statusCode,response.headers);response.pipe(res);});
    remote.on('error', () => {
      if (!res.headersSent) res.writeHead(503).end('Phiên mô phỏng đã dừng; mở lại /simulation');
      else res.destroy();
    });
    req.on('aborted',()=>remote.destroy());
    res.on('close',()=>{if (!res.writableEnded) remote.destroy();});
    req.pipe(remote);
  }
  function upgrade(req, socket, head, simulation) {
    socket.on('error',()=>{});
    if (!sameOrigin(req) || !simulation || new URL(req.url,'http://localhost').pathname !== '/ws') {
      socket.end('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n');return;
    }
    sockets.add(socket);socket.on('close',()=>sockets.delete(socket));
    const remote = upstream(req, simulation);
    remote.on('upgrade',(response,connection,remoteHead)=>{
      connection.on('error',()=>socket.destroy());
      socket.on('close',()=>connection.destroy());
      connection.on('close',()=>socket.destroy());
      const headers=Object.entries(response.headers).flatMap(([key,value])=>
        (Array.isArray(value)?value:[value]).map(item=>`${key}: ${item}`));
      socket.write(`HTTP/1.1 101 Switching Protocols\r\n${headers.join('\r\n')}\r\n\r\n`);
      if (remoteHead.length) socket.write(remoteHead);
      if (head.length) connection.write(head);
      socket.pipe(connection);connection.pipe(socket);
    });
    remote.on('response',()=>socket.end('HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\n'));
    remote.on('error',()=>socket.destroy());
    socket.on('close',()=>remote.destroy());
    remote.end();
  }
  return {request,upgrade,close(){for (const socket of sockets) socket.destroy();}};
}
module.exports={sameOrigin,createPublicProxy};
