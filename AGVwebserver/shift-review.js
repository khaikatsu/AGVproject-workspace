const {createHash}=require('node:crypto');

// Resolved incidents stay in system_log/KPI; the review only closes the
// operator's active list, with a compare-and-swap token for what was reviewed.
function createShiftReview({db,getState,getSession,inventory,orders,actions}) {
  db.db.exec(`CREATE TABLE IF NOT EXISTS shift_error_reviews (
    id INTEGER PRIMARY KEY AUTOINCREMENT, shift_id TEXT NOT NULL,
    review_json TEXT NOT NULL, created_at TEXT NOT NULL);
    CREATE TABLE IF NOT EXISTS shift_review_windows (
    shift_id TEXT PRIMARY KEY, core_session TEXT NOT NULL, baseline_id INTEGER NOT NULL);`);
  const incident=/BAO LOI|KHONG ACK|MAT KET NOI|\[LOI\]|\[CANH BAO\]/;
  let observedWindow=null;
  function observe() {
    const current=inventory.snapshot().shift?.id;
    if(current&&current!==observedWindow){snapshot();observedWindow=current;}
  }
  function snapshot() {
    db.flush();
    const state=getState(),warehouse=inventory.snapshot(),session=getSession();
    const shiftId=warehouse.shift?.id||warehouse.latest_report_id||`${session}:startup`;
    let window=db.db.prepare('SELECT * FROM shift_review_windows WHERE shift_id=?').get(shiftId);
    if(!window) {
      const latest=db.db.prepare('SELECT COALESCE(MAX(id),0) AS id FROM system_log').get().id;
      const baseline=warehouse.shift ? latest : 0;
      db.db.prepare('INSERT INTO shift_review_windows VALUES(?,?,?)').run(shiftId,session||'legacy',baseline);
      window={baseline_id:baseline};
    }
    const previous=db.db.prepare('SELECT review_json FROM shift_error_reviews WHERE shift_id=? ORDER BY id DESC LIMIT 1').get(shiftId);
    const after=Math.max(window.baseline_id,previous?JSON.parse(previous.review_json).through_id:0);
    const logs=db.db.prepare('SELECT id,core_session,message,created_at FROM system_log WHERE id>? ORDER BY id LIMIT 5001').all(after);
    const has_more=logs.length>5000;if(has_more)logs.pop();
    const incidents=logs.filter(row=>incident.test(row.message)),blockers=[];
    if(state?.shift?.phase!=='CLOSED')blockers.push('Chờ ca đóng và các xe tập kết trước khi chốt đối soát');
    for(const robot of state?.fleet?.agvs||[]) {
      if(robot.online!==true||robot.error||!['IDLE','CHARGING'].includes(robot.state)||robot.task_id||
        robot.path?.length||robot.uncertain_path?.length||robot.lift?.active||robot.virtual_cargo===true||
        robot.cancel_waiting_for_node||robot.replan_waiting_for_node)
        blockers.push(`Xe ${robot.id}: còn lỗi, công việc hoặc vị trí cần xác minh`);
    }
    if(state?.fleet?.pending_tasks?.length)blockers.push('Core còn nhiệm vụ chờ');
    for(const [zone,claim] of Object.entries(state?.mutex||{}))if(claim.owner||claim.blocked_by_stopped_robot)
      blockers.push(`${zone}: còn quyền vùng hoặc xe dừng chưa giải tỏa`);
    if(orders.list({includeDeleted:true}).some(order=>['active','needs_attention'].includes(order.status))||
      actions.list().some(action=>!['completed','interrupted'].includes(action.status)))blockers.push('Còn đơn hoặc thao tác kho cần đối soát');
    if(warehouse.moves?.some(move=>['reserved','carrying','needs_attention'].includes(move.status))||
      warehouse.racks?.some(rack=>rack.location.type==='robot'||rack.status==='needs_attention'))blockers.push('Còn kệ/đặt chỗ chưa xác minh');
    const through_id=logs.at(-1)?.id||after;
    const result={shift_id:shiftId,core_session_id:session,through_id,incidents,blockers,has_more};
    return {...result,review_token:createHash('sha256').update(JSON.stringify(result)).digest('hex')};
  }
  function reconcile(body) {
    if(body?.physically_verified!==true)throw Object.assign(Error('Cần xác nhận đã kiểm tra xe, vùng và kệ thực tế'),{status:400});
    const current=snapshot();
    if(current.review_token!==body.review_token)throw Object.assign(Error('Trạng thái hoặc lỗi mới đã thay đổi; mở lại đối soát'),{status:409});
    if(current.blockers.length)throw Object.assign(Error(current.blockers.join('; ')),{status:409});
    db.db.prepare('INSERT INTO shift_error_reviews(shift_id,review_json,created_at) VALUES(?,?,?)')
      .run(current.shift_id,JSON.stringify({...current,evidence:'operator_verified'}),new Date().toISOString());
    return snapshot();
  }
  return {snapshot,reconcile,observe};
}
module.exports={createShiftReview};
