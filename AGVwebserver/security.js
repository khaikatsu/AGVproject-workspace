const {createHash,timingSafeEqual} = require('crypto');
function createSecurity({token = process.env.AGV_AUTH_TOKEN || '',origins = process.env.AGV_ALLOWED_ORIGINS || ''} = {}) {
  if (token && token.length < 32) throw new Error('AGV_AUTH_TOKEN cần ít nhất 32 ký tự');
  const allowed = new Set(Array.isArray(origins) ? origins : origins.split(',').map(s=>s.trim()).filter(Boolean));
  const digest = value=>createHash('sha256').update(value).digest();
  const matches = value=>!!token && timingSafeEqual(digest(value || ''),digest(token));
  const loopback = req=>['127.0.0.1','::1','::ffff:127.0.0.1'].includes(req.socket.remoteAddress);
  const supplied = req=>{
    const bearer = /^Bearer (.+)$/.exec(req.headers.authorization || '');
    if (bearer) return bearer[1];
    const cookie = /(?:^|;\s*)agv_session=([^;]*)/.exec(req.headers.cookie || '');
    try {return cookie ? decodeURIComponent(cookie[1]) : '';} catch {return '';}
  };
  function originAllowed(req) {
    // Anonymous development must also reject DNS rebinding through a foreign Host.
    if (!token) {
      try {
        if (!['localhost','127.0.0.1','[::1]'].includes(new URL(`http://${req.headers.host}`).hostname)) return false;
      } catch {return false;}
    }
    const origin = req.headers.origin;if (!origin) return true;
    if (allowed.has(origin)) return true;
    // Forwarded headers only work with a configured origin allowlist, never trust them here.
    return origin === `${req.socket.encrypted ? 'https' : 'http'}://${req.headers.host}`;
  }
  const authorized = req=>originAllowed(req) && (token ? matches(supplied(req)) : loopback(req));
  function middleware(req,res,next) {
    if (!originAllowed(req)) return res.status(403).json({ok:false,error:'Origin không được phép'});
    if (!authorized(req)) return res.status(401).json({ok:false,error:token ? 'Cần đăng nhập' : 'Truy cập ngoài localhost cần cấu hình xác thực'});
    if (!['GET','HEAD','OPTIONS'].includes(req.method) && !req.headers.origin && !req.headers.authorization && token)
      return res.status(403).json({ok:false,error:'Lệnh cần Origin hoặc Bearer token'});
    next();
  }
  return {token,allowed,matches,originAllowed,authorized,middleware};
}
module.exports = {createSecurity};
