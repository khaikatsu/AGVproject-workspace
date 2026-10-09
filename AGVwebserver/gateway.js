// HTTP/WS gateway. Core owns live map, robot state and task results.
const express = require('express');
const cors = require('cors');
const http = require('http');
const https = require('https');
const fs = require('fs');
const WebSocket = require('ws');
const path = require('path');
const {randomUUID} = require('crypto');
const {createDatabase} = require('./db');
const {createSecurity} = require('./security');
const {createOrderService} = require('./orders');
const INCIDENT_RE = /^AGV_(\S+)\s+(BAO LOI|KHONG ACK lenh|MAT KET NOI)/;
const emptyState = () => ({map:{nodes:[],edges:[],obstacles:[]},fleet:{agvs:[],pending_tasks:[],tasks:[]},mutex:{},logs:[]});
function batteryForDisplay(agv) {
    const value = agv?.battery_display_available === true
        ? agv.battery_display_percent
        : agv?.battery_measured === true ? agv.battery : null;
    return Number.isInteger(value) && value >= 0 && value <= 100 ? value : null;
}
function httpError(status,message,unknown = false) {return Object.assign(new Error(message),{status,unknown});}
function id(value,name = 'id') {
    if (typeof value !== 'string' || !/^[A-Za-z0-9_.-]{1,128}$/.test(value))
        throw httpError(400,`${name} không hợp lệ`);
    return value;
}
function number(value,name,min,max,integer = false) {
    if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max || (integer && !Number.isInteger(value)))
        throw httpError(400,`${name} không hợp lệ`);
    return value;
}
function limit(req,fallback = 200,max = 1000) {
    return req.query.limit == null ? fallback : number(Number(req.query.limit),'limit',1,max,true);
}
function createGateway(options = {}) {
    const db = options.db || createDatabase(options.dbPath);
    const coreUrl = options.coreUrl || process.env.AGV_WS_URL || 'ws://127.0.0.1:8080';
    const timeoutMs = options.commandTimeoutMs || Number(process.env.AGV_COMMAND_TIMEOUT_MS) || 5000;
    const staleMs = options.staleMs || 5000;
    const app = express();
    const security = createSecurity({token:options.authToken ?? process.env.AGV_AUTH_TOKEN,origins:options.allowedOrigins ?? process.env.AGV_ALLOWED_ORIGINS});
    app.disable('x-powered-by');
    app.use((req,res,next)=>{res.set('X-Content-Type-Options','nosniff');res.set('Referrer-Policy','no-referrer');res.set('X-Frame-Options','DENY');next();});
    app.use(cors({origin:(origin,callback)=>callback(null,!!origin && security.allowed.has(origin)),credentials:true}));
    app.use(express.json({limit:'1mb'}));
    const loginAttempts = new Map();
    app.get('/auth/status',(req,res)=>res.json({required:!!security.token,authenticated:security.authorized(req)}));
    app.post('/auth/login',(req,res)=>{
        if (!security.originAllowed(req)) return res.status(403).json({ok:false,error:'Origin không được phép'});
        const key = req.socket.remoteAddress,now = Date.now();
        for (const [key,entry] of loginAttempts) if (entry.until < now) loginAttempts.delete(key);
        const attempt = loginAttempts.get(key) || {count:0,until:now+60000};
        if (++attempt.count > 10) return res.status(429).json({ok:false,error:'Thử lại sau một phút'});
        loginAttempts.set(key,attempt);
        if (typeof req.body.token !== 'string' || !security.matches(req.body.token)) return res.status(401).json({ok:false,error:'Token không đúng'});
        res.cookie('agv_session',req.body.token,{httpOnly:true,sameSite:'strict',secure:!!req.socket.encrypted,maxAge:8*3600000,path:'/'});
        res.json({ok:true});
    });
    app.post('/auth/logout',(req,res)=>{if (!security.originAllowed(req)) return res.sendStatus(403);res.clearCookie('agv_session',{path:'/'});res.json({ok:true});});
    app.use('/api',security.middleware);
    app.use(express.static(path.join(__dirname,'public')));
    app.get('/',(req,res) => res.sendFile(path.join(__dirname,'index.html')));
    const tls = options.tls || (process.env.AGV_HTTPS_CERT && process.env.AGV_HTTPS_KEY ? {cert:fs.readFileSync(process.env.AGV_HTTPS_CERT),key:fs.readFileSync(process.env.AGV_HTTPS_KEY)} : null);
    if (process.env.AGV_SECURITY_REQUIRED && (!tls || !security.token)) throw new Error('Chế độ secure cần HTTPS và AGV_AUTH_TOKEN');
    const server = tls ? https.createServer(tls,app) : http.createServer(app);
    const wss = new WebSocket.Server({server,path:'/ws',maxPayload:4096,verifyClient:({req},done)=>done(security.authorized(req),401,'Unauthorized')});
    let latestState = emptyState(),socket = null,connected = false,stopped = false;
    let receivedAt = 0,session = null,reconnectTimer = null,wasFresh = false;
    const requests = new Map();
    let broadcastTimer = null,lastBroadcastAt = 0,broadcastExtra = {};
    let browserSequence = 0;
    let streamState = null;
    const fresh = () => connected && receivedAt > 0 && Date.now() - receivedAt <= staleMs;
    const supported = () => latestState.protocol_version === 1;
    const browserState = () => ({...latestState,agv_connected:connected,core_ready:fresh(),protocol_supported:supported(),
        state_received_at:receivedAt ? new Date(receivedAt).toISOString() : null,stale_after_ms:staleMs,
        task_history:db.getTaskHistory(50),orders:orders.list().slice(0,50)});
    function broadcast(extra = {}) {
        Object.assign(broadcastExtra,extra);
        if (broadcastTimer) return;
        const wait = Math.max(0,200-(Date.now()-lastBroadcastAt));
        broadcastTimer = setTimeout(()=>{
            broadcastTimer = null;lastBroadcastAt = Date.now();
            const state = {...browserState(),...broadcastExtra};broadcastExtra = {};const seq = ++browserSequence;
            for (const ws of wss.clients) if (ws.readyState === WebSocket.OPEN) {
                if (ws.bufferedAmount > 2*1024*1024) {ws.terminate();continue;}
                if (ws.deltaCapable) {
                    const changes = {};
                    for (const [key,value] of Object.entries(state)) if (JSON.stringify(ws.previous?.[key]) !== JSON.stringify(value)) changes[key] = value;
                    ws.send(JSON.stringify({type:'state_delta',base_state_seq:ws.sequence,state_seq:seq,changes}));
                } else ws.send(JSON.stringify(state));
                ws.previous = state;ws.sequence = seq;
            }
        },wait);
    }
    wss.on('connection',(ws,req) => {
        ws.on('error',()=>{});ws.deltaCapable = new URL(req.url,'http://localhost').searchParams.get('delta') === '1';
        const state = browserState();ws.previous = state;ws.sequence = browserSequence;
        ws.send(JSON.stringify({...state,state_seq:browserSequence}));
    });
    function onState(state) {
        if (!state || !state.map || !state.fleet || !Array.isArray(state.map.nodes) ||
            !Array.isArray(state.map.edges) || !Array.isArray(state.fleet.agvs)) return;
        const nextSession = typeof state.core_session_id === 'string' ? state.core_session_id : 'legacy';
        if (session !== nextSession) {session = nextSession;db.interruptPreviousSession(session,state.fleet.tasks || []);}
        latestState = {...state,map:{...state.map,obstacles:state.map.obstacles || []},
            fleet:{...state.fleet,pending_tasks:state.fleet.pending_tasks || [],tasks:state.fleet.tasks || []},
            mutex:state.mutex || {},logs:Array.isArray(state.logs) ? state.logs : []};
        receivedAt = Date.now();
        db.enqueueState(latestState,session,INCIDENT_RE);
        if (latestState.mqtt_connected === false) orders.interruptAll('Mất MQTT; đối soát xe và kiện hàng');
        broadcast();
        void orders.tick();
    }
    function failRequests(message) {
        for (const [key,request] of requests) {
            clearTimeout(request.timer);request.reject(httpError(503,message,true));requests.delete(key);
        }
    }
    function connectCore() {
        if (stopped) return;
        streamState = null;
        const coreToken = options.coreToken ?? process.env.AGV_CORE_TOKEN;
        const current = socket = new WebSocket(coreUrl,{maxPayload:8*1024*1024,headers:coreToken ? {Authorization:`Bearer ${coreToken}`} : undefined});
        current.on('open',() => {connected = true;receivedAt = 0;broadcast();});
        current.on('message',raw => {
            try {
                const payload = JSON.parse(raw.toString());
                if (payload.type === 'state_delta') {
                    if (!streamState || payload.base_state_seq !== streamState.state_seq) {current.close();return;}
                    streamState = {...streamState,...payload.changes,state_seq:payload.state_seq};return onState(streamState);
                }
                if (payload.type !== 'command_result') {streamState = payload;return onState(payload);}
                if (payload.state) onState(payload.state);
                const request = requests.get(payload.request_id);
                if (!request) return;
                clearTimeout(request.timer);requests.delete(payload.request_id);
                if (payload.core_session_id !== request.session) request.reject(httpError(503,'Phiên core đã thay đổi',true));
                else if (payload.ok === true) request.resolve(payload.data || {});
                else request.reject(httpError([400,404,409,500,503].includes(payload.status) ? payload.status : 409,payload.error || 'Core từ chối lệnh'));
            } catch (error) {console.error('[AGV-LINK]',error.message);}
        });
        current.on('error',error => {if (!stopped) console.error('[AGV-LINK]',error.message);});
        current.on('close',() => {
            if (current !== socket) return;
            const hadState = connected && receivedAt > 0;
            connected = false;failRequests('Mất kết nối core; kết quả lệnh chưa xác định, hãy đối chiếu trạng thái trước khi gửi lại');
            orders.interruptAll('Mất kết nối Core; đối soát xe và kiện hàng');
            // UNKNOWN is an observation gap, not a claim that all robots stopped.
            for (const a of latestState.fleet.agvs) db.logAgvEventIfChanged(a.id,a.node,batteryForDisplay(a),'UNKNOWN');
            if (!stopped) {
                broadcast(hadState ? {kpi_alert:db.computeKpiReport('disconnect')} : {});
                reconnectTimer = setTimeout(connectCore,options.reconnectMs || 3000);
            }
        });
    }
    function requireCore() {
        if (!fresh()) throw httpError(503,'Core chưa có dữ liệu mới hoặc đã mất kết nối');
        if (!supported()) throw httpError(409,'Core chưa hỗ trợ phản hồi lệnh; hãy build bản core trong workspace');
        if (latestState.mqtt_connected === false) throw httpError(503,'Core đang mất kết nối MQTT broker');
    }
    function command(payload) {
        requireCore();const requestId = randomUUID();
        if (requests.size >= 128) throw httpError(429,'Có quá nhiều lệnh đang chờ Core');
        const text = JSON.stringify({...payload,request_id:requestId});
        if (Buffer.byteLength(text) > 1024*1024) throw httpError(413,'Lệnh vượt giới hạn 1 MiB của core');
        return new Promise((resolve,reject) => {
            const timer = setTimeout(() => {requests.delete(requestId);
                reject(httpError(504,'Core chưa xác nhận kết quả lệnh; hãy đối chiếu trạng thái trước khi gửi lại',true));},timeoutMs);
            requests.set(requestId,{resolve,reject,timer,session});
            socket.send(text,error => {
                if (error && requests.has(requestId)) {clearTimeout(timer);requests.delete(requestId);reject(httpError(503,'Không gửi được lệnh tới core',true));}
            });
        });
    }
    const orders = createOrderService({db,getState:()=>fresh() && supported() ? latestState : null,
        getSession:()=>session,send:command,publish:()=>broadcast()});
    const route = fn => (req,res,next) => Promise.resolve().then(()=>fn(req,res)).catch(next);
    function target(value) {
        const node = id(value,'target');
        if (!latestState.map.nodes.some(n=>n.id === node)) throw httpError(404,'Node không tồn tại trong bản đồ core');
        return node;
    }
    async function task(req,res,robot = null) {
        requireCore();
        const destination = target(req.body.target),prio = number(req.body.priority ?? 0,'priority',0,100,true);
        if (robot) id(robot);
        const data = await command({cmd:robot ? 'direct_goto' : 'add_task',target:destination,priority:prio,...(robot ? {agv_id:robot} : {})});
        res.json({ok:true,...data});
    }
    app.get('/api/health',(req,res)=>res.json({ok:true,backend:'online',agv_connected:connected,core_ready:fresh(),protocol_supported:supported(),mqtt_connected:latestState.mqtt_connected ?? null,agv_ws_url:coreUrl,time:new Date().toISOString()}));
    app.get('/api/state',(req,res)=>res.json(browserState()));
    app.get('/api/shift',(req,res)=>res.json(latestState.shift || {supported:false,phase:'UNAVAILABLE'}));
    app.post('/api/shift/start',route(async (req,res)=>res.json({ok:true,...await command({cmd:'start_shift'})})));
    app.post('/api/shift/end',route(async (req,res)=>res.json({ok:true,...await command({cmd:'end_shift'})})));
    app.get('/api/map',(req,res)=>res.json(latestState.map));
    app.get('/api/mutex',(req,res)=>res.json(latestState.mutex));
    app.post('/api/mutex/:zone/confirm-clear',route(async(req,res)=>{
        if (req.body.physically_clear !== true) throw httpError(400,'Cần xác nhận xe đã dừng và vùng trống');
        res.json({ok:true,...await command({cmd:'confirm_zone_clear',zone:id(req.params.zone),owner:id(req.body.owner),
            grant_id:number(req.body.grant_id,'grant_id',0,2147483647,true),physically_clear:true})});
    }));
    app.get('/api/agvs',(req,res)=>res.json(latestState.fleet.agvs));
    app.get('/api/tasks',(req,res)=>res.json(latestState.fleet.pending_tasks));
    app.get('/api/tasks/history',route((req,res)=>res.json(db.getTaskHistory(limit(req)))));
    app.get('/api/orders',(req,res)=>res.json(orders.list()));
    app.post('/api/orders',route((req,res)=>{requireCore();res.status(201).json({ok:true,order:orders.create(req.body)});}));
    app.post('/api/orders/:id/resolve',route((req,res)=>res.json({ok:true,order:orders.resolve(id(req.params.id,'order id'),req.body.physically_clear)})));
    app.get('/api/agvs/:id/history',route((req,res)=>res.json(db.getAgvHistory(id(req.params.id),limit(req)))));
    app.get('/api/logs',route((req,res)=>res.json(db.getRecentSystemLogs(limit(req,100)))));
    app.post('/api/tasks',route((req,res)=>task(req,res)));
    app.post('/api/agvs/:id/goto',route((req,res)=>task(req,res,req.params.id)));
    app.post('/api/tasks/cancel',route(async (req,res)=>res.json({ok:true,...await command({cmd:'cancel_task',...(req.body.task_id ? {task_id:id(req.body.task_id,'task_id')} : {target:target(req.body.target)})})})));
    app.post('/api/agvs/:id/clear-error',route(async (req,res)=>res.json({ok:true,...await command({cmd:'clear_error',id:id(req.params.id)})})));
    app.delete('/api/agvs/:id',route(async (req,res)=>res.json({ok:true,...await command({cmd:'remove_agv',id:id(req.params.id)})})));
    app.post('/api/map/obstacle',route(async (req,res)=>{
        const node = target(req.body.node);
        if (typeof req.body.blocked !== 'boolean') throw httpError(400,'blocked phải là boolean');
        const ttl = number(req.body.ttl_s ?? 0,'ttl_s',0,86400,true);
        const data = await command({cmd:'set_obstacle',node,blocked:req.body.blocked,ttl_s:ttl});
        db.logObstacle(node,req.body.blocked);res.json({ok:true,...data});
    }));
    app.get('/api/map/obstacles/history',route((req,res)=>res.json(db.getObstacleHistory(limit(req)))));
    app.post('/api/map/node',route(async (req,res)=>{
        const type = req.body.type || 'waypoint';
        if (!['waypoint','mutex','charging','station'].includes(type)) throw httpError(400,'type node không hợp lệ');
        res.json({ok:true,...await command({cmd:'add_node',id:id(req.body.id),x:number(req.body.x,'x',-1000000,1000000),y:number(req.body.y,'y',-1000000,1000000),type})});
    }));
    app.delete('/api/map/node/:id',route(async (req,res)=>res.json({ok:true,...await command({cmd:'remove_node',id:target(req.params.id)})})));
    app.post('/api/map/edge',route(async (req,res)=>{
        const {weight,type = 'main',path_class = '',two_way = false} = req.body;
        if (!['main','branch','loop'].includes(type) || typeof path_class !== 'string' || path_class.length > 128 || typeof two_way !== 'boolean') throw httpError(400,'Thuộc tính cạnh không hợp lệ');
        res.json({ok:true,...await command({cmd:'add_edge',from:target(req.body.from),to:target(req.body.to),weight:number(weight,'weight',0,1000000,true),type,path_class,two_way})});
    }));
    app.delete('/api/map/edge',route(async (req,res)=>res.json({ok:true,...await command({cmd:'remove_edge',from:target(req.body.from),to:target(req.body.to)})})));
    function snapshotName(body) {
        if (body.name != null && (typeof body.name !== 'string' || body.name.length > 200)) throw httpError(400,'Tên snapshot không hợp lệ');
        return body.name && body.name.trim() || `Bản đồ ${new Date().toLocaleString('vi-VN')}`;
    }
    app.post('/api/map/save',route(async (req,res)=>{
        const name = snapshotName(req.body);
        if (req.body.path != null) throw httpError(400,'Đường dẫn lưu do core cấu hình');
        const data = await command({cmd:'save_map'}),snapshotId = db.saveMapSnapshot(latestState.map,name);
        res.json({ok:true,id:snapshotId,name,...data});
    }));
    app.get('/api/map/snapshots',route((req,res)=>res.json(db.getAllMapSnapshots())));
    app.get('/api/map/snapshots/latest',route((req,res)=>{
        const snap = db.getLatestMapSnapshot();if (!snap) throw httpError(404,'Chưa có snapshot');res.json(snap);
    }));
    app.post('/api/map/snapshots',route((req,res)=>{
        requireCore();const name = snapshotName(req.body);res.status(201).json({ok:true,id:db.saveMapSnapshot(latestState.map,name),name});
    }));
    const snapshotId = req=>number(Number(req.params.id),'snapshot id',1,Number.MAX_SAFE_INTEGER,true);
    function snapshot(req) {const snap = db.getMapSnapshotById(snapshotId(req));if (!snap) throw httpError(404,'Không tìm thấy snapshot');return snap;}
    app.get('/api/map/snapshots/:id',route((req,res)=>res.json(snapshot(req))));
    app.post('/api/map/snapshots/:id/apply',route(async (req,res)=>res.json({ok:true,...await command({cmd:'load_map_data',map:snapshot(req).map})})));
    app.delete('/api/map/snapshots/:id',route((req,res)=>{snapshot(req);db.deleteMapSnapshot(snapshotId(req));res.json({ok:true});}));
    app.get('/api/kpi/latest',route((req,res)=>{const report = db.getLatestKpiReport();if (!report) throw httpError(404,'Chưa có báo cáo KPI');res.json(report);}));
    app.get('/api/kpi/history',route((req,res)=>res.json(db.getKpiHistory(limit(req,50,200)))));
    app.post('/api/kpi/generate',route((req,res)=>res.json({ok:true,report:db.computeKpiReport('manual')})));
    app.use((error,req,res,next)=>{
        if (res.headersSent) return next(error);
        res.status(error.status || 500).json({ok:false,error:error.message,unknown:!!error.unknown});
    });
    const monitor = setInterval(()=>{
        const current = fresh();
        if (current !== wasFresh) {
            if (!current) {
                orders.interruptAll('Dữ liệu Core đã cũ; đối soát xe và kiện hàng');
                for (const a of latestState.fleet.agvs) db.logAgvEventIfChanged(a.id,a.node,batteryForDisplay(a),'UNKNOWN');
            }
            broadcast();wasFresh = current;
        }
    },1000);monitor.unref();
    async function start(port = Number(process.env.PORT) || 3000,host = process.env.HOST || (security.token ? '0.0.0.0' : '127.0.0.1')) {
        if (!['127.0.0.1','::1','localhost'].includes(host) && !security.token) throw new Error('Gateway LAN cần AGV_AUTH_TOKEN');
        await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,host,resolve);});
        connectCore();return server.address();
    }
    async function close() {
        stopped = true;clearInterval(monitor);clearTimeout(reconnectTimer);clearTimeout(broadcastTimer);failRequests('Gateway đóng');
        await orders.close();
        if (socket) {socket.removeAllListeners();socket.on('error',()=>{});socket.terminate();}
        for (const ws of wss.clients) ws.terminate();
        await new Promise(resolve=>wss.close(resolve));
        if (server.listening) await new Promise(resolve=>server.close(resolve));
        if (!options.db) db.close();
    }
    return {app,server,start,close};
}
module.exports = {createGateway};
