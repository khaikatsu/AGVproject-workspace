// ============================================================================
// db.js — Database SQLite cho hệ thống AGV
// ============================================================================
const Database = require('better-sqlite3');
const path = require('path');

function batteryForDisplay(agv) {
  const value = agv?.battery_display_available === true
    ? agv.battery_display_percent
    : agv?.battery_measured === true ? agv.battery : null;
  return Number.isInteger(value) && value >= 0 && value <= 100 ? value : null;
}

function createDatabase(filename = process.env.AGV_DB_PATH || path.join(__dirname, 'agv.db')) {
  const db = new Database(filename);
  db.pragma('journal_mode = WAL');

  db.exec(`
  CREATE TABLE IF NOT EXISTS agv_events (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      agv_id TEXT NOT NULL,
      node TEXT,
      battery INTEGER,
      state TEXT,
      created_at TEXT DEFAULT (datetime('now'))
  );
  CREATE INDEX IF NOT EXISTS idx_agv_events_agv ON agv_events(agv_id, created_at);

  CREATE TABLE IF NOT EXISTS task_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      target_node TEXT NOT NULL,
      priority INTEGER DEFAULT 0,
      status TEXT DEFAULT 'pending',
      assigned_agv TEXT,
      created_at TEXT DEFAULT (datetime('now')),
      completed_at TEXT
  );
  CREATE INDEX IF NOT EXISTS idx_task_log_status ON task_log(status);

  CREATE TABLE IF NOT EXISTS obstacle_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      node_id TEXT NOT NULL,
      blocked INTEGER NOT NULL,
      created_at TEXT DEFAULT (datetime('now'))
  );

  CREATE TABLE IF NOT EXISTS map_snapshots (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT DEFAULT 'Khong ten',
      map_json TEXT NOT NULL,
      saved_at TEXT DEFAULT (datetime('now'))
  );

  CREATE TABLE IF NOT EXISTS kpi_reports (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      reason TEXT NOT NULL,
      report_json TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now'))
  );

  CREATE TABLE IF NOT EXISTS warehouse_orders (
      id TEXT PRIMARY KEY,
      order_json TEXT NOT NULL,
      created_at TEXT NOT NULL
  );
  CREATE INDEX IF NOT EXISTS idx_warehouse_orders_created ON warehouse_orders(created_at);

  -- MỚI: nhật ký sự kiện toàn hệ thống, đồng bộ từ "logs" mà C++ Core phát kèm
  -- mỗi lần broadcast trạng thái (xem AGVserver.cpp / pushLog). core_seq cùng core_session là số
  -- thứ tự C++ Core gán cho từng sự kiện -> dùng UNIQUE index để INSERT OR IGNORE,
  -- tránh ghi trùng khi C++ Core gửi lại cùng 1 mục nhiều lần trong các lần
  -- broadcast liên tiếp (mỗi ~700ms, "logs" luôn chứa ~20 mục gần nhất).
  CREATE TABLE IF NOT EXISTS system_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      core_seq INTEGER,
      time_label TEXT,
      message TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now'))
  );

  -- MỚI: chỉ riêng các sự kiện "sự cố xe" (lỗi / mất ACK lệnh / mất kết nối) được
  -- trích ra từ system_log để tính KPI "Sự cố xe" trên dashboard.
  CREATE TABLE IF NOT EXISTS agv_error_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      agv_id TEXT,
      message TEXT,
      created_at TEXT DEFAULT (datetime('now'))
  );
  `);

  try {
    db.prepare(`SELECT name FROM map_snapshots LIMIT 1`).get();
  } catch (error) {
    db.exec(`ALTER TABLE map_snapshots ADD COLUMN name TEXT DEFAULT 'Khong ten'`);
  }

  // MỚI: cột dispatched_at -- thời điểm task được GÁN cho 1 AGV (chuyển sang status 'dispatched').
  // Trước đây chỉ có created_at (lúc tạo task) và completed_at (lúc xong), nên thời gian
  // "created_at -> completed_at" bị gộp chung 2 giai đoạn khác bản chất: (1) thời gian CHỜ
  // được gán xe (do hết xe rảnh/quá tải) và (2) thời gian DI CHUYỂN thực tế của xe. Có thêm
  // dispatched_at thì tách được 2 cái này riêng -- xem computeKpiReport() bên dưới.
  try {
    db.prepare(`SELECT dispatched_at FROM task_log LIMIT 1`).get();
  } catch (error) {
    db.exec(`ALTER TABLE task_log ADD COLUMN dispatched_at TEXT`);
  }

  // Additive migration: older task rows remain visible, without inventing a core identity.
  for (const [name, declaration] of Object.entries({core_task_id: 'TEXT', core_session: 'TEXT', reason: 'TEXT'})) {
    if (!db.prepare('PRAGMA table_info(task_log)').all().some(c => c.name === name))
      db.exec(`ALTER TABLE task_log ADD COLUMN ${name} ${declaration}`);
  }
  if (!db.prepare('PRAGMA table_info(system_log)').all().some(c => c.name === 'core_session'))
    db.exec("ALTER TABLE system_log ADD COLUMN core_session TEXT NOT NULL DEFAULT 'legacy'");
  db.transaction(() => {
    db.exec('DROP INDEX IF EXISTS idx_system_log_core_seq');
    db.exec('CREATE UNIQUE INDEX IF NOT EXISTS idx_system_log_session_seq ON system_log(core_session, core_seq)');
    db.exec('CREATE UNIQUE INDEX IF NOT EXISTS idx_task_core_id ON task_log(core_session, core_task_id)');
  })();

  db.exec(`CREATE TABLE IF NOT EXISTS task_status_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    core_session TEXT NOT NULL, core_task_id TEXT NOT NULL,
    status TEXT NOT NULL, created_at TEXT DEFAULT (datetime('now'))
  );`);

  const stmts = {
    saveOrder: db.prepare(`INSERT INTO warehouse_orders (id, order_json, created_at) VALUES (?, ?, ?)
      ON CONFLICT(id) DO UPDATE SET order_json=excluded.order_json`),
    allOrders: db.prepare(`SELECT order_json FROM warehouse_orders ORDER BY created_at DESC, id DESC`),
    insertAgvEvent: db.prepare(`INSERT INTO agv_events (agv_id, node, battery, state) VALUES (?, ?, ?, ?)`),
    lastAgvEvent: db.prepare(`SELECT * FROM agv_events WHERE agv_id = ? ORDER BY id DESC LIMIT 1`),
    agvHistory: db.prepare(`SELECT * FROM agv_events WHERE agv_id = ? ORDER BY id DESC LIMIT ?`),

    pendingOrDispatchedTasks: db.prepare(`SELECT * FROM task_log WHERE status IN ('pending', 'dispatched') ORDER BY id ASC`),
    taskHistory: db.prepare(`SELECT * FROM task_log ORDER BY id DESC LIMIT ?`),

    insertObstacle: db.prepare(`INSERT INTO obstacle_log (node_id, blocked) VALUES (?, ?)`),
    obstacleHistory: db.prepare(`SELECT * FROM obstacle_log ORDER BY id DESC LIMIT ?`),

    insertMapSnapshot: db.prepare(`INSERT INTO map_snapshots (name, map_json) VALUES (?, ?)`),
    latestMapSnapshot: db.prepare(`SELECT * FROM map_snapshots ORDER BY id DESC LIMIT 1`),
    allMapSnapshots: db.prepare(`SELECT id, name, saved_at FROM map_snapshots ORDER BY id DESC`),
    mapSnapshotById: db.prepare(`SELECT * FROM map_snapshots WHERE id = ?`),
    deleteMapSnapshot: db.prepare(`DELETE FROM map_snapshots WHERE id = ?`),

    insertKpi: db.prepare(`INSERT INTO kpi_reports (reason, report_json) VALUES (?, ?)`),
    kpiCreatedAt: db.prepare(`SELECT created_at FROM kpi_reports WHERE id = ?`),
    latestKpi: db.prepare(`SELECT * FROM kpi_reports ORDER BY id DESC LIMIT 1`),
    kpiHistory: db.prepare(`SELECT * FROM kpi_reports ORDER BY id DESC LIMIT ?`),

    insertSystemLog: db.prepare(`INSERT OR IGNORE INTO system_log (core_session, core_seq, time_label, message) VALUES (?, ?, ?, ?)`),
    recentSystemLogs: db.prepare(`SELECT * FROM system_log ORDER BY id DESC LIMIT ?`),
    insertAgvError: db.prepare(`INSERT INTO agv_error_log (agv_id, message) VALUES (?, ?)`),

    now: db.prepare(`SELECT datetime('now') AS t`),
    minAgvEventAt: db.prepare(`SELECT MIN(created_at) AS t FROM agv_events`),
    minTaskAt: db.prepare(`SELECT MIN(created_at) AS t FROM task_log`),
    // MỚI: nguyên liệu để tính % WORKING/IDLE/CHARGING/ERROR... theo TỪNG xe trong cửa sổ thời
    // gian -- xem computeAgvUtilization(). agv_events chỉ ghi khi state/node/battery ĐỔI (xem
    // logAgvEventIfChanged), nên để biết state tại đúng thời điểm period_start, cần lấy dòng
    // GẦN NHẤT trước period_start (lastAgvEventBeforeOrAt), rồi mới lấy các đổi trạng thái XẢY
    // RA TRONG cửa sổ (agvEventsWithinWindow) để dựng lại "dòng thời gian" trạng thái của xe đó.
    distinctAgvIdsUpTo: db.prepare(`SELECT DISTINCT agv_id FROM agv_events WHERE created_at <= ? ORDER BY agv_id`),
    lastAgvEventBeforeOrAt: db.prepare(`SELECT state, created_at FROM agv_events WHERE agv_id = ? AND created_at <= ? ORDER BY created_at DESC, id DESC LIMIT 1`),
    agvEventsWithinWindow: db.prepare(`SELECT state, created_at FROM agv_events WHERE agv_id = ? AND created_at > ? AND created_at <= ? ORDER BY created_at ASC, id ASC`),
  };

  // MỚI: dựng lại "dòng thời gian" trạng thái của 1 xe trong khoảng [periodStart, periodEnd],
  // rồi cộng dồn xem xe đó ở mỗi trạng thái (WORKING/IDLE/CHARGING/ERROR/OFFLINE/...) bao nhiêu
  // giây -- từ đó tính ra % thời gian sử dụng (utilization) của TỪNG xe, thay vì chỉ có số liệu
  // gộp chung cả đội xe như trước. Không cần đổi gì ở C++ Core/ESP32 vì agv_events đã có sẵn.
  function computeAgvUtilization(periodStart, periodEnd) {
    const toEpochMs = (s) => new Date(s.replace(' ', 'T') + 'Z').getTime();
    const agvIds = stmts.distinctAgvIdsUpTo.all(periodEnd).map(r => r.agv_id);
    const result = {};

    for (const agvId of agvIds) {
      const before = stmts.lastAgvEventBeforeOrAt.get(agvId, periodStart);
      const within = stmts.agvEventsWithinWindow.all(agvId, periodStart, periodEnd);
      if (!before && within.length === 0) continue; // xe chưa có dữ liệu nào tính tới thời điểm này -> bỏ qua

      // Mốc trạng thái đầu tiên khi BƯỚC VÀO cửa sổ: lấy state của sự kiện gần nhất trước đó;
      // nếu xe mới xuất hiện ngay trong cửa sổ (chưa có sự kiện nào trước) thì dùng luôn state
      // đầu tiên ghi nhận được trong cửa sổ làm mốc bắt đầu.
      const timeline = before
        ? [{ state: before.state, at: periodStart }]
        : [{ state: within[0].state, at: within[0].created_at }];
      within.forEach(ev => timeline.push({ state: ev.state, at: ev.created_at }));

      const seconds = {};
      let totalSec = 0;
      for (let i = 0; i < timeline.length; i++) {
        const startMs = toEpochMs(timeline[i].at);
        const endMs = toEpochMs(i + 1 < timeline.length ? timeline[i + 1].at : periodEnd);
        const dur = (endMs - startMs) / 1000;
        if (dur <= 0) continue;
        const st = timeline[i].state || 'UNKNOWN';
        seconds[st] = (seconds[st] || 0) + dur;
        totalSec += dur;
      }
      if (totalSec <= 0) continue;

      result[agvId] = {
        known_seconds: Math.round(totalSec),
        breakdown: Object.entries(seconds)
          .map(([state, sec]) => ({ state, seconds: Math.round(sec), percent: Number((sec / totalSec * 100).toFixed(1)) }))
          .sort((a, b) => b.seconds - a.seconds),
      };
    }
    return result;
  }

  const api = {
    db,

    saveOrder(order) {
      stmts.saveOrder.run(order.id,JSON.stringify(order),order.created);
    },

    getOrders() {
      return stmts.allOrders.all().map(row=>JSON.parse(row.order_json));
    },

    logAgvEventIfChanged(agvId, node, battery, state) {
      const last = stmts.lastAgvEvent.get(agvId);
      if (last && last.node === node && last.battery === battery && last.state === state) return;
      stmts.insertAgvEvent.run(agvId, node, battery, state);
    },

    getAgvHistory(agvId, limit = 200) {
      return stmts.agvHistory.all(agvId, limit);
    },

    getOpenTasks() {
      return stmts.pendingOrDispatchedTasks.all();
    },

    getTaskHistory(limit = 200) {
      return stmts.taskHistory.all(limit);
    },

    logObstacle(nodeId, blocked) {
      stmts.insertObstacle.run(nodeId, blocked ? 1 : 0);
    },

    getObstacleHistory(limit = 200) {
      return stmts.obstacleHistory.all(limit);
    },

    saveMapSnapshot(mapJson, name = 'Khong ten') {
      return stmts.insertMapSnapshot.run(name || 'Khong ten', JSON.stringify(mapJson)).lastInsertRowid;
    },

    getLatestMapSnapshot() {
      const row = stmts.latestMapSnapshot.get();
      return row ? JSON.parse(row.map_json) : null;
    },

    getAllMapSnapshots() {
      return stmts.allMapSnapshots.all();
    },

    getMapSnapshotById(id) {
      const row = stmts.mapSnapshotById.get(Number(id));
      if (!row) return null;
      return { id: row.id, name: row.name, saved_at: row.saved_at, map: JSON.parse(row.map_json) };
    },

    deleteMapSnapshot(id) {
      stmts.deleteMapSnapshot.run(Number(id));
    },

    // MỚI: ghi 1 mục nhật ký hệ thống (đồng bộ từ C++ Core). Trả về true nếu đây là mục MỚI
    // (chưa từng lưu, phân biệt theo core_session + core_seq) -- dùng để tránh xử lý trùng (VD đếm sự cố xe 2 lần).
    insertSystemLog(coreSeq, timeLabel, message, coreSession = "legacy") {
      const info = stmts.insertSystemLog.run(coreSession, coreSeq, timeLabel, message);
      return info.changes > 0;
    },

    getRecentSystemLogs(limit = 100) {
      return stmts.recentSystemLogs.all(limit);
    },

    insertAgvError(agvId, message) {
      stmts.insertAgvError.run(agvId, message);
    },

    // Báo cáo KPI theo CỬA SỔ THỜI GIAN: từ lúc kết thúc báo cáo TRƯỚC ĐÓ (period_end) tới hiện
    // tại -- giống 1 "báo cáo ca làm việc" -- thay vì luỹ kế từ đầu mãi mãi. Nếu chưa từng có báo
    // cáo nào, cửa sổ bắt đầu từ sự kiện AGV/task sớm nhất từng ghi nhận.
    // Định dạng thời gian: LUÔN dùng SQLite datetime('now') ("YYYY-MM-DD HH:MM:SS", giờ UTC) cho
    // period_start/period_end, để so sánh chuỗi ">=""<=" với cột created_at (cùng định dạng) luôn
    // đúng theo thời gian thực -- KHÔNG dùng JS toISOString() (khác định dạng, so sánh chuỗi sẽ sai).
    computeKpiReport(reason = 'manual') {
      const periodEnd = stmts.now.get().t;
      const prevRow = stmts.latestKpi.get();
      const prevReport = prevRow ? JSON.parse(prevRow.report_json) : null;
      const periodStart = (prevReport && prevReport.period_end)
        ? prevReport.period_end
        : [stmts.minAgvEventAt.get().t, stmts.minTaskAt.get().t].filter(Boolean).sort()[0] || periodEnd;

      // Row watermarks avoid counting boundary-second events twice across consecutive reports.
      const high = table => db.prepare(`SELECT COALESCE(MAX(id),0) AS n FROM ${table}`).get().n;
      const taskHigh = high('task_log'), obstacleHigh = high('obstacle_log'), errorHigh = high('agv_error_log');
      const eventHigh = high('agv_events'), statusHigh = high('task_status_events');
      const taskLow = prevReport?.task_high_watermark || 0;
      const cohort = db.prepare('SELECT * FROM task_log WHERE id > ? AND id <= ?').all(taskLow,taskHigh);
      const count = status => cohort.filter(t => t.status === status).length;
      const toMs = value => new Date(value.replace(' ','T') + 'Z').getTime();
      const average = (from,to) => {
        const values = cohort.filter(t=>t.status === 'completed' && t[from] && t[to]).map(t=>(toMs(t[to])-toMs(t[from]))/1000);
        return values.length ? Number((values.reduce((a,b)=>a+b,0)/values.length).toFixed(1)) : null;
      };
      const newCount = (table,low,upper,filter = '') => db.prepare(`SELECT COUNT(*) AS n FROM ${table} WHERE id > ? AND id <= ? ${filter}`).get(low || 0,upper).n;
      const report = {
        reason,period_start:periodStart,period_end:periodEnd,
        total_tasks:cohort.length,completed_tasks:count('completed'),
        completed_in_period:newCount('task_status_events',prevReport?.status_high_watermark,statusHigh,"AND status='completed'"),
        pending_tasks:db.prepare("SELECT COUNT(*) AS n FROM task_log WHERE status='pending'").get().n,
        dispatched_tasks:db.prepare("SELECT COUNT(*) AS n FROM task_log WHERE status='dispatched'").get().n,
        cancelled_tasks:count('cancelled'),interrupted_tasks:count('interrupted'),rejected_tasks:count('rejected'),
        unknown_tasks:count('unknown') + count('submitting'),
        avg_completion_seconds:average('created_at','completed_at'),
        avg_queue_seconds:average('created_at','dispatched_at'),
        avg_travel_seconds:average('dispatched_at','completed_at'),
        agv_error_events:newCount('agv_error_log',prevReport?.error_high_watermark,errorHigh),
        obstacle_events:newCount('obstacle_log',prevReport?.obstacle_high_watermark,obstacleHigh,'AND blocked=1'),
        telemetry_events:newCount('agv_events',prevReport?.event_high_watermark,eventHigh),
        agv_count:db.prepare('SELECT COUNT(DISTINCT agv_id) AS n FROM agv_events WHERE id > ? AND id <= ?').get(prevReport?.event_high_watermark || 0,eventHigh).n,
        agv_utilization:computeAgvUtilization(periodStart,periodEnd),
        task_high_watermark:taskHigh,obstacle_high_watermark:obstacleHigh,error_high_watermark:errorHigh,
        event_high_watermark:eventHigh,status_high_watermark:statusHigh,
      };

      const info = stmts.insertKpi.run(reason, JSON.stringify(report));
      const createdAt = stmts.kpiCreatedAt.get(info.lastInsertRowid).created_at;
      return { id: info.lastInsertRowid, created_at: createdAt, ...report };
    },

    getLatestKpiReport() {
      const row = stmts.latestKpi.get();
      if (!row) return null;
      return { id: row.id, created_at: row.created_at, ...JSON.parse(row.report_json) };
    },

    getKpiHistory(limit = 50) {
      return stmts.kpiHistory.all(limit).map(row => ({
        id: row.id,
        created_at: row.created_at,
        ...JSON.parse(row.report_json),
      }));
    }
  };
    api.createTrackedTask = (taskId, target, priority, session) => db.prepare(`
      INSERT INTO task_log (core_task_id, target_node, priority, core_session, status)
      VALUES (?, ?, ?, ?, 'submitting')`).run(taskId, target, priority, session).lastInsertRowid;
    api.setTrackedTaskStatus = (taskId, session, status, reason = '') => db.prepare(`
      UPDATE task_log SET status = ?, reason = ?, completed_at = CASE WHEN ? IN ('rejected', 'interrupted') THEN datetime('now') ELSE completed_at END
      WHERE core_task_id = ? AND core_session = ? AND status IN ('submitting', 'unknown')`).run(status, reason, status, taskId, session);
    const syncOne = db.prepare(`
      INSERT INTO task_log (core_task_id, core_session, target_node, priority, status, assigned_agv, reason, dispatched_at, completed_at)
      VALUES (@task_id, @session, @target, @priority, @status, @assigned_agv, @reason,
              CASE WHEN @status = 'dispatched' THEN datetime('now') END,
              CASE WHEN @terminal THEN datetime('now') END)
      ON CONFLICT(core_session, core_task_id) DO UPDATE SET
        status = excluded.status, assigned_agv = excluded.assigned_agv, reason = excluded.reason, priority = excluded.priority,
        dispatched_at = CASE WHEN excluded.status = 'dispatched' THEN COALESCE(task_log.dispatched_at, datetime('now')) ELSE task_log.dispatched_at END,
        completed_at = CASE WHEN @terminal THEN COALESCE(task_log.completed_at, datetime('now')) ELSE NULL END`);
    api.syncTasks = db.transaction((tasks, session) => {
      for (const t of tasks || []) {
        if (!t || typeof t.task_id !== 'string' || typeof t.target !== 'string' ||
            !['pending','dispatched','completed','cancelled','interrupted'].includes(t.status)) continue;
        const previous = db.prepare("SELECT status,assigned_agv,reason,priority FROM task_log WHERE core_session=? AND core_task_id=?").get(session,t.task_id);
        if (previous && previous.status === t.status && previous.assigned_agv === (t.assigned_agv || null) &&
            previous.reason === (t.reason || '') && previous.priority === (t.priority || 0)) continue;
        if (!previous || previous.status !== t.status) db.prepare("INSERT INTO task_status_events (core_session,core_task_id,status) VALUES (?,?,?)").run(session,t.task_id,t.status);
        syncOne.run({task_id:t.task_id, session, target:t.target, priority:t.priority || 0,
                     status:t.status, assigned_agv:t.assigned_agv || null, reason:t.reason || '',
                     terminal:['completed','cancelled','interrupted'].includes(t.status) ? 1 : 0});
      }
    });
    api.interruptPreviousSession = (session,recovered = []) => {
      api.flush();
      db.transaction(()=>{
        for (const t of recovered) {
          const existing = db.prepare('SELECT id,core_session FROM task_log WHERE core_task_id=? AND target_node=? ORDER BY id DESC LIMIT 1').get(t.task_id,t.target);
          if (existing && existing.core_session !== session) {
            db.prepare('UPDATE task_log SET core_session=? WHERE id=?').run(session,existing.id);
            db.prepare('UPDATE task_status_events SET core_session=? WHERE core_session=? AND core_task_id=?').run(session,existing.core_session,t.task_id);
          }
        }
      })();
      return db.prepare(`
      UPDATE task_log SET status='interrupted', reason='Core khoi dong lai; can xac minh', completed_at=datetime('now')
      WHERE (core_session IS NULL OR core_session != ?) AND status IN ('pending','dispatched','submitting','unknown')`).run(session);
    };
    const queue = [];let flushing = false;
    api.flush = () => {
      if (flushing || !queue.length) return;
      flushing = true;
      try {
        db.transaction(()=>{
          for (const {state,session,incident} of queue) {
            for (const a of state.fleet.agvs) api.logAgvEventIfChanged(a.id,a.node,batteryForDisplay(a),a.state);
            api.syncTasks(state.fleet.tasks,session);
            for (const entry of state.logs) {
              if (!entry || !Number.isSafeInteger(entry.seq) || typeof entry.msg !== 'string') continue;
              if (!api.insertSystemLog(entry.seq,entry.time || '',entry.msg,session)) continue;
              const match = incident.exec(entry.msg);if (match) api.insertAgvError(match[1],entry.msg);
            }
          }
        })();
        queue.length = 0;
      } finally {flushing = false;}
    };
    api.enqueueState = (state,session,incident) => {
      queue.push({state,session,incident});
      if (queue.length >= 256) api.flush();
    };
    const batchTimer = setInterval(()=>api.flush(),100);batchTimer.unref();
    for (const key of Object.keys(api)) if (/^(get|compute|save)/.test(key) && typeof api[key] === 'function') {
      const original = api[key];api[key] = (...args)=>{api.flush();return original(...args);};
    }
    api.prune = (days = Number(process.env.AGV_RETENTION_DAYS) || 30,maxRows = Number(process.env.AGV_RETENTION_ROWS) || 100000) => {
      api.flush();
      if (!Number.isInteger(days) || days < 1 || !Number.isInteger(maxRows) || maxRows < 100) throw new Error('Retention không hợp lệ');
      const cutoff = db.prepare("SELECT datetime('now',?) AS t").get(`-${days} days`).t;
      db.transaction(()=>{
        // Preserve a baseline observation per robot for utilization after retention.
        db.prepare(`DELETE FROM agv_events WHERE (created_at < ? OR id < (SELECT COALESCE(MAX(id),0)-? FROM agv_events))
          AND id NOT IN (SELECT MAX(id) FROM agv_events WHERE created_at < ? GROUP BY agv_id)`).run(cutoff,maxRows,cutoff);
        for (const table of ['system_log','agv_error_log','obstacle_log'])
          db.prepare(`DELETE FROM ${table} WHERE created_at < ? OR id < (SELECT COALESCE(MAX(id),0)-? FROM ${table})`).run(cutoff,maxRows);
        db.prepare(`DELETE FROM task_status_events WHERE EXISTS (SELECT 1 FROM task_log t WHERE t.core_session=task_status_events.core_session AND t.core_task_id=task_status_events.core_task_id AND t.status IN ('completed','cancelled','interrupted','rejected') AND t.completed_at < ?)`).run(cutoff);
        db.prepare("DELETE FROM task_log WHERE status IN ('completed','cancelled','interrupted','rejected') AND completed_at < ?").run(cutoff);
        db.exec('DELETE FROM map_snapshots WHERE id NOT IN (SELECT id FROM map_snapshots ORDER BY id DESC LIMIT 50); DELETE FROM kpi_reports WHERE id NOT IN (SELECT id FROM kpi_reports ORDER BY id DESC LIMIT 200)');
      })();
    };
    const retentionTimer = setInterval(()=>api.prune(),3600000);retentionTimer.unref();
    api.close = () => {clearInterval(batchTimer);clearInterval(retentionTimer);api.flush();db.close();};
    return api;
}

module.exports = {createDatabase};
