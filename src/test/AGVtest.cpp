// ============================================================================
// server.js — Backend Web cho hệ thống AGV
// ============================================================================
const express = require('express');
const cors = require('cors');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const db = require('./db');

const AGV_WS_URL = process.env.AGV_WS_URL || 'ws://localhost:8080';
const PORT = process.env.PORT || 3000;

const app = express();
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const server = http.createServer(app);

// ---------------------------------------------------------------------------
// PHẦN A — Cache trạng thái mới nhất nhận từ AGVServer.cpp
// ---------------------------------------------------------------------------
let latestState = { map: { nodes: [], edges: [], obstacles: [] }, fleet: { agvs: [], pending_tasks: [] }, mutex: {} };
let agvConnected = false;
const previousAgvTargets = new Map(); 

// ---------------------------------------------------------------------------
// PHẦN B — Kết nối WebSocket CLIENT tới AGVServer.cpp (có tự reconnect)
// ---------------------------------------------------------------------------
let agvSocket = null;

function connectToAgvServer() {
    console.log(`[AGV-LINK] Dang ket noi toi ${AGV_WS_URL} ...`);
    agvSocket = new WebSocket(AGV_WS_URL);

    agvSocket.on('open', () => {
        agvConnected = true;
        console.log('[AGV-LINK] Da ket noi AGVServer.cpp thanh cong.');

        // --- BỔ SUNG YÊU CẦU: Khôi phục map đã lưu & Đưa xe về trạm sạc làm điểm bắt đầu cố định ---
        const savedMap = db.getLatestMapSnapshot();
        if (savedMap && savedMap.nodes && savedMap.nodes.length > 0) {
            latestState.map = savedMap;
            sendCommandToAgv({ cmd: 'load_map_data', map: savedMap });
        }

        // Đợi 1 nhịp để đồng bộ map rồi tiến hành đặt xe vào trạm sạc
        setTimeout(() => {
            const nodes = latestState.map.nodes || [];
            // Tìm node là trạm sạc (type = 'charger' hoặc 'charging_station') hoặc lấy node đầu tiên làm fallback
            const chargerNode = nodes.find(n => n.type === 'charger' || n.type === 'charging_station') || nodes[0];
            
            if (chargerNode) {
                const currentAgvs = latestState.fleet.agvs || [];
                // Nếu chưa có xe nào trên hệ thống, tự động thêm xe mặc định xuất phát tại trạm sạc
                if (currentAgvs.length === 0) {
                    console.log(`[AGV-INIT] Khoi tao xe AGV_01 tai tram sac co dinh: ${chargerNode.id}`);
                    sendCommandToAgv({ cmd: 'add_agv', id: 'AGV_01', start: chargerNode.id, priority: false });
                }
            }
        }, 800);
    });

    agvSocket.on('message', (raw) => {
        try {
            const state = JSON.parse(raw.toString());
            onAgvState(state);
        } catch (e) {
            console.error('[AGV-LINK] Loi parse JSON:', e.message);
        }
    });

    agvSocket.on('close', () => {
        agvConnected = false;
        console.warn('[AGV-LINK] Mat ket noi AGVServer.cpp, se thu lai sau 3s...');
        setTimeout(connectToAgvServer, 3000);
    });

    agvSocket.on('error', (err) => {
        console.error('[AGV-LINK] Loi ket noi:', err.message);
        agvSocket.close();
    });
}

function sendCommandToAgv(cmdObj) {
    if (!agvSocket || agvSocket.readyState !== WebSocket.OPEN) {
        throw new Error('Chua ket noi duoc AGVServer.cpp (kiem tra server C++ da chay chua)');
    }
    agvSocket.send(JSON.stringify(cmdObj));
}

function onAgvState(state) {
    latestState = state;

    const agvs = (state.fleet && state.fleet.agvs) || [];
    for (const a of agvs) {
        db.logAgvEventIfChanged(a.id, a.node, a.battery, a.state);

        const prevTarget = previousAgvTargets.get(a.id) || '';
        const newTarget = a.target || '';

        if (newTarget && newTarget !== prevTarget) {
            const pendingTask = db.findPendingTaskByTarget(newTarget);
            if (pendingTask) db.markTaskDispatched(pendingTask.id, a.id);
        }
        if (prevTarget && !newTarget) {
            const dispatchedTask = db.findDispatchedTaskByTargetAgv(prevTarget, a.id);
            if (dispatchedTask) db.markTaskCompleted(dispatchedTask.id);
        }
        previousAgvTargets.set(a.id, newTarget);
    }

    broadcastToBrowsers(state);
}

// ---------------------------------------------------------------------------
// PHẦN C — WebSocket server RIÊNG cho trình duyệt (path /ws)
// ---------------------------------------------------------------------------
const wss = new WebSocket.Server({ server, path: '/ws' });

wss.on('connection', (ws) => {
    ws.send(JSON.stringify(latestState));
});

function broadcastToBrowsers(state) {
    const payload = JSON.stringify(state);
    wss.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) client.send(payload);
    });
}

// ---------------------------------------------------------------------------
// PHẦN D — REST API (Giữ nguyên toàn bộ API cũ, mở rộng lưu bản đồ vào DB)
// ---------------------------------------------------------------------------
function requireAgvOk(res) {
    if (!agvConnected) {
        res.status(503).json({ error: 'AGVServer.cpp chua ket noi duoc.' });
        return false;
    }
    return true;
}

app.get('/api/state', (req, res) => {
    res.json({ ...latestState, agv_connected: agvConnected });
});
app.get('/api/map', (req, res) => res.json(latestState.map));
app.get('/api/mutex', (req, res) => res.json(latestState.mutex));

app.get('/api/agvs', (req, res) => res.json(latestState.fleet.agvs));

app.post('/api/agvs', (req, res) => {
    if (!requireAgvOk(res)) return;
    const { id, start, priority } = req.body;
    if (!id || !start) return res.status(400).json({ error: 'Thieu id hoac start' });
    try {
        sendCommandToAgv({ cmd: 'add_agv', id, start, priority: !!priority });
        res.status(202).json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.delete('/api/agvs/:id', (req, res) => {
    if (!requireAgvOk(res)) return;
    try {
        sendCommandToAgv({ cmd: 'remove_agv', id: req.params.id });
        res.json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.post('/api/agvs/:id/goto', (req, res) => {
    if (!requireAgvOk(res)) return;
    const agvId = req.params.id;
    const { target } = req.body;
    if (!target) return res.status(400).json({ error: 'Thieu target' });
    try {
        sendCommandToAgv({ cmd: 'direct_goto', agv_id: agvId, target });
        const taskId = db.createDispatchedTask(target, 0, agvId);
        previousAgvTargets.set(agvId, target);
        res.status(202).json({ ok: true, task_id: taskId });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.get('/api/agvs/:id/history', (req, res) => {
    const limit = Math.min(parseInt(req.query.limit) || 200, 1000);
    res.json(db.getAgvHistory(req.params.id, limit));
});

app.get('/api/tasks', (req, res) => res.json(latestState.fleet.pending_tasks));

app.post('/api/tasks', (req, res) => {
    if (!requireAgvOk(res)) return;
    const { target, priority } = req.body;
    if (!target) return res.status(400).json({ error: 'Thieu target' });
    const p = priority || 0;
    try {
        sendCommandToAgv({ cmd: 'add_task', target, priority: p });
        const taskId = db.createTask(target, p);
        res.status(202).json({ ok: true, task_id: taskId });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.get('/api/tasks/history', (req, res) => {
    const limit = Math.min(parseInt(req.query.limit) || 200, 1000);
    res.json(db.getTaskHistory(limit));
});

app.post('/api/map/node', (req, res) => {
    if (!requireAgvOk(res)) return;
    const { id, x, y, type } = req.body;
    if (!id) return res.status(400).json({ error: 'Thieu id node' });
    try {
        sendCommandToAgv({ cmd: 'add_node', id, x: x || 0, y: y || 0, type: type || 'waypoint' });
        res.status(202).json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.delete('/api/map/node/:id', (req, res) => {
    if (!requireAgvOk(res)) return;
    try {
        sendCommandToAgv({ cmd: 'remove_node', id: req.params.id });
        res.json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.post('/api/map/edge', (req, res) => {
    if (!requireAgvOk(res)) return;
    const { from, to, weight, type, path_class, two_way } = req.body;
    if (!from || !to) return res.status(400).json({ error: 'Thieu from/to' });
    try {
        sendCommandToAgv({ cmd: 'add_edge', from, to, weight: weight || 1, type: type || 'main', path_class: path_class || '', two_way: !!two_way });
        res.status(202).json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.delete('/api/map/edge', (req, res) => {
    if (!requireAgvOk(res)) return;
    const { from, to } = req.body;
    if (!from || !to) return res.status(400).json({ error: 'Thieu from/to' });
    try {
        sendCommandToAgv({ cmd: 'remove_edge', from, to });
        res.json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.post('/api/map/obstacle', (req, res) => {
    if (!requireAgvOk(res)) return;
    const { node, blocked } = req.body;
    if (!node) return res.status(400).json({ error: 'Thieu node' });
    try {
        sendCommandToAgv({ cmd: 'set_obstacle', node, blocked: blocked !== false });
        db.logObstacle(node, blocked !== false);
        res.status(202).json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.get('/api/map/obstacles/history', (req, res) => {
    const limit = Math.min(parseInt(req.query.limit) || 200, 1000);
    res.json(db.getObstacleHistory(limit));
});

// Giữ nguyên API save_map cũ, tích hợp thêm lệnh lưu snapshot vào SQLite (db.js)
app.post('/api/map/save', (req, res) => {
    if (!requireAgvOk(res)) return;
    try {
        sendCommandToAgv({ cmd: 'save_map', path: req.body.path || 'map_layout.json' });
        db.saveMapSnapshot(latestState.map); // Thêm tính năng lưu bản đồ vào DB
        res.json({ ok: true });
    } catch (e) { res.status(503).json({ error: e.message }); }
});

app.get('/api/map/snapshots/latest', (req, res) => {
    const snap = db.getLatestMapSnapshot();
    if (!snap) return res.status(404).json({ error: 'Chua co snapshot nao duoc luu' });
    res.json(snap);
});

// ---------------------------------------------------------------------------
// PHẦN E — Khởi động
// ---------------------------------------------------------------------------
connectToAgvServer();
server.listen(PORT, () => {
    console.log(`[HTTP] Backend API + Dashboard: http://localhost:${PORT}`);
    console.log(`[WS]   Realtime cho trinh duyet: ws://localhost:${PORT}/ws`);
    console.log(`[LINK] Se ket noi toi AGVServer.cpp tai: ${AGV_WS_URL}`);
});