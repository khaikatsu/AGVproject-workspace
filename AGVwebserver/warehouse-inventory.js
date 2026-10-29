const {randomUUID} = require('node:crypto');
const {warehouseNodes} = require('./public/warehouse-rules');
const {cargoReading,cargoResult}=require('./cargo-proof');

const conflict = message=>Object.assign(new Error(message),{status:409});
const invalid = message=>Object.assign(new Error(message),{status:400});
const openMove = move=>['reserved','carrying','needs_attention'].includes(move.status);
const now = ()=>new Date().toISOString();

// Gateway-owned ledger: the Core remains the authority for robot/lift completion.
function createWarehouseInventory({db,publish=()=>{}}) {
  const sql=db.db;
  let reconciliationEvent;
  sql.exec(`
    CREATE TABLE IF NOT EXISTS warehouse_inventory (id INTEGER PRIMARY KEY CHECK(id=1), state_json TEXT NOT NULL);
    CREATE TABLE IF NOT EXISTS warehouse_events (
      id INTEGER PRIMARY KEY AUTOINCREMENT, event_key TEXT UNIQUE NOT NULL,
      shift_id TEXT, event_json TEXT NOT NULL);
    CREATE INDEX IF NOT EXISTS idx_warehouse_events_shift ON warehouse_events(shift_id,id);
    CREATE TABLE IF NOT EXISTS warehouse_shift_reports (
      id TEXT PRIMARY KEY, report_json TEXT NOT NULL, created_at TEXT NOT NULL);
  `);
  const read=()=>{
    const row=sql.prepare('SELECT state_json FROM warehouse_inventory WHERE id=1').get();
    return row ? JSON.parse(row.state_json) : {initialized:false,revision:0,slots:[],racks:[],moves:[],shift:null};
  };
  const history=(limit=50)=>sql.prepare('SELECT id,event_json FROM warehouse_events ORDER BY id DESC LIMIT ?').all(limit)
    .map(row=>({id:row.id,...JSON.parse(row.event_json)}));
  const reports=(limit=50)=>sql.prepare('SELECT report_json FROM warehouse_shift_reports ORDER BY created_at DESC,id DESC LIMIT ?').all(limit)
    .map(row=>JSON.parse(row.report_json));
  const reportById=id=>{
    const row=sql.prepare('SELECT report_json FROM warehouse_shift_reports WHERE id=?').get(id);
    return row ? JSON.parse(row.report_json) : null;
  };
  const snapshot=state=>({initialized:state.initialized,revision:state.revision,updated_at:state.updated_at,
    slots:state.slots,racks:state.racks,moves:state.moves.filter(openMove)});
  function reconciliationFor(order) {
    if(order?.status!=='needs_attention'||order.closed_by_shift!==true)return null;
    const closedAt=Date.parse(order.shift_closed_at||order.updated);
    if(!Number.isFinite(closedAt))return null;
    if(reconciliationEvent===undefined) {
      const row=sql.prepare(`SELECT id,event_json FROM warehouse_events
        WHERE json_extract(event_json,'$.type') IN ('initialized','reconciled')
        AND json_extract(event_json,'$.evidence')='operator_verified' ORDER BY id DESC LIMIT 1`).get();
      reconciliationEvent=row?{id:row.id,...JSON.parse(row.event_json)}:null;
    }
    if(!reconciliationEvent||!(Date.parse(reconciliationEvent.created_at)>closedAt))return null;
    const state=read(),keys=new Set((order.items||[]).map(item=>`order:${order.id}:${item.number}`));
    if(!state.initialized||state.moves.some(move=>keys.has(move.key)&&openMove(move)))return null;
    return {event_id:reconciliationEvent.id,created_at:reconciliationEvent.created_at};
  }
  function change(fn) {
    const result=sql.transaction(()=>{
      const state=read();
      const event=(event_key,data)=>{
        const entry={created_at:now(),shift_id:state.shift?.id || null,evidence:'server',...data};
        sql.prepare('INSERT INTO warehouse_events(event_key,shift_id,event_json) VALUES(?,?,?)')
          .run(event_key,entry.shift_id,JSON.stringify(entry));
      };
      const result=fn(state,event);
      state.revision++;state.updated_at=now();
      sql.prepare('INSERT INTO warehouse_inventory(id,state_json) VALUES(1,?) ON CONFLICT(id) DO UPDATE SET state_json=excluded.state_json')
        .run(JSON.stringify(state));
      return result;
    })();
    publish();return result;
  }
  function markInterrupted(state,event,move,reason) {
    if(!move || !['reserved','carrying'].includes(move.status))return;
    move.interrupted_from=move.status;move.status='needs_attention';move.reason=reason;
    const rack=state.racks.find(rack=>rack.id===move.rack_id);if(rack)rack.status='needs_attention';
    event(`interrupt:${move.key}:${state.revision}`,{type:'interrupted',rack_id:move.rack_id,robot:move.robot,
      move_key:move.key,source:move.pickup,target:move.dropoff,reason,message:`Kệ ${move.rack_id}: ${reason}`});
  }
  function interrupt(key,reason) {
    if(!read().moves.some(move=>move.key===key&&['reserved','carrying'].includes(move.status)))return;
    change((state,event)=>markInterrupted(state,event,state.moves.find(move=>move.key===key),reason));
  }
  function observationGap(reason) {
    const state=read();
    if(!state.shift || state.shift.observation_gap)return;
    change((state,event)=>{
      state.shift.observation_gap=true;
      event(`gap:${state.shift.id}`,{type:'observation_gap',reason,message:reason});
    });
  }
  // A restart must never replay a partially observed physical operation.
  if(read().moves.some(move=>['reserved','carrying'].includes(move.status))||read().racks.some(rack=>rack.location.type==='robot')) {
    change((state,event)=>{
      for(const move of state.moves)markInterrupted(state,event,move,'Gateway khởi động lại; cần đối soát kệ');
      for(const rack of state.racks)if(rack.location.type==='robot')rack.status='needs_attention';
      if(state.shift)state.shift.observation_gap=true;
      event(`restart:${randomUUID()}`,{type:'observation_gap',message:'Gateway khởi động lại; giữ vị trí kệ lần cuối và yêu cầu đối soát'});
    });
  } else if(read().shift)observationGap('Gateway khởi động lại trong ca');

  function configure(body,core,unresolved=false) {
    if(core?.shift?.phase!=='CLOSED')throw conflict('Chỉ nhập hoặc đối soát kho khi ca đã đóng');
    if(unresolved || core.fleet.pending_tasks?.length || core.fleet.agvs.some(robot=>
      !['IDLE','CHARGING'].includes(robot.state)||robot.task_id||robot.target||robot.path?.length||robot.lift?.active))
      throw conflict('Cần xử lý đơn và dừng các thao tác xe trước khi nhập kho');
    if(body?.physically_verified!==true)throw invalid('Cần xác nhận đã kiểm tra vị trí kệ và xe thực tế');
    const nodes=warehouseNodes(core.map);
    if(!nodes.length || !Array.isArray(body.slots)||body.slots.length!==nodes.length)throw invalid('Nhập đầy đủ các vị trí K trên bản đồ');
    const slots=body.slots.map(slot=>{
      if(!slot||!nodes.includes(slot.node)||!(slot.rack_id===null||typeof slot.rack_id==='string'))throw invalid('Vị trí hoặc mã kệ không hợp lệ');
      const rack_id=slot.rack_id===null ? null : slot.rack_id.trim();
      if(rack_id!==null&&!/^[A-Za-z0-9_-]{1,32}$/.test(rack_id))throw invalid('Mã kệ gồm 1–32 chữ, số, dấu gạch ngang hoặc gạch dưới');
      return {node:slot.node,rack_id};
    }).sort((a,b)=>a.node.localeCompare(b.node));
    const ids=slots.map(slot=>slot.rack_id).filter(Boolean);
    if(new Set(slots.map(slot=>slot.node)).size!==nodes.length||new Set(ids).size!==ids.length)throw invalid('Không được trùng vị trí hoặc mã kệ');
    const result=change((state,event)=>{
      if(body.expected_revision!==state.revision)throw conflict('Trạng thái kho đã thay đổi; tải lại trước khi lưu');
      const previous=snapshot(state);
      state.slots=slots;state.racks=slots.filter(slot=>slot.rack_id).map(slot=>({id:slot.rack_id,status:'stored',location:{type:'slot',id:slot.node}}));
      state.moves=[];state.initialized=true;
      event(`configure:${randomUUID()}`,{type:previous.initialized?'reconciled':'initialized',evidence:'operator_verified',
        previous,slots,message:previous.initialized?'Đã đối soát lại vị trí kệ và xác nhận xe trống':'Đã nhập trạng thái kho ban đầu'});
    });
    reconciliationEvent=undefined;return result;
  }
  function checkMove(state,{key,pickup,dropoff,robot,standalone}) {
    if(!state.initialized)throw conflict('Nhập trạng thái kệ ban đầu trước khi tạo đơn hàng kho');
    const existing=state.moves.find(move=>move.key===key);
    if(existing) {
      const sameRequest=existing.pickup===(pickup||null)&&existing.dropoff===(dropoff||null)&&
        existing.robot===robot&&existing.standalone===!!standalone;
      if(!sameRequest)throw conflict('Mã thao tác đã được dùng cho lượt chuyển kệ khác');
      if(existing.status==='needs_attention')throw conflict('Lượt chuyển kệ đang cần đối soát; không tự tiếp tục');
      if(!['reserved','carrying'].includes(existing.status))throw conflict('Lượt chuyển kệ đã kết thúc; không thể đặt chỗ lại');
      return existing;
    }
    const source=state.slots.find(slot=>slot.node===pickup),target=state.slots.find(slot=>slot.node===dropoff);
    const rack=pickup ? state.racks.find(rack=>rack.id===source?.rack_id) :
      state.racks.find(rack=>rack.location.type==='robot'&&rack.location.id===robot);
    if(!rack||rack.status==='needs_attention')throw conflict(pickup?`${pickup} không có kệ sẵn sàng`:`Chưa xác nhận kệ trên xe ${robot}`);
    if(pickup&&robot&&state.racks.some(candidate=>candidate.location.type==='robot'&&candidate.location.id===robot))
      throw conflict(`Xe ${robot} đang có kệ theo sổ kho; cần hạ hoặc đối soát trước khi lấy kệ khác`);
    if(dropoff&&(!target||target.rack_id))throw conflict(`${dropoff} chưa trống`);
    if(!standalone&&(!pickup||!dropoff||pickup===dropoff))throw invalid('Chọn hai vị trí lấy/giao khác nhau');
    const blocking=state.moves.find(move=>openMove(move)&&(
      move.rack_id===rack.id || move.robot===robot ||
      [pickup,dropoff].filter(Boolean).some(node=>[move.pickup,move.dropoff].includes(node))));
    if(blocking)throw conflict(`Kệ/vị trí đang dành cho xe ${blocking.robot} hoặc cần đối soát`);
    return {key,pickup:pickup||null,dropoff:dropoff||null,robot,rack_id:rack.id,status:pickup?'reserved':'carrying',standalone:!!standalone};
  }
  function assertPlan(plan) {
    const state=read(),used=new Set();
    for(const item of plan.items) {
      if(used.has(item.pickup)||used.has(item.dropoff))throw invalid('Mỗi vị trí chứa một kệ: chỉ chọn một lượt lấy/giao cho mỗi vị trí trong đơn');
      checkMove(state,{key:'preview',pickup:item.pickup,dropoff:item.dropoff,robot:null});
      used.add(item.pickup);used.add(item.dropoff);
    }
  }
  function reserve(spec) {
    const current=read();
    if(current.moves.some(move=>move.key===spec.key))return checkMove(current,spec);
    return change((state,event)=>{
      const move=checkMove(state,spec);state.moves.push(move);
      event(`reserve:${move.key}`,{type:'reserved',...move,message:`Dành kệ ${move.rack_id} cho xe ${move.robot}`});
      return move;
    });
  }
  function checkResume(key,robot,expectedRevision,state=read()) {
    if(expectedRevision!==state.revision)throw conflict('Kho đã thay đổi; tải lại trước khi tiếp tục');
    const move=state.moves.find(move=>move.key===key);
    if(!move||move.robot!==robot.id)throw conflict('Không tìm thấy lượt chuyển kệ của xe');
    if(move.status==='placed'||move.status==='completed'&&move.standalone) {
      const rack=state.racks.find(rack=>rack.id===move.rack_id),onRobot=move.status==='completed'&&!!move.pickup;
      if(!rack||(onRobot? rack.location.type!=='robot'||rack.location.id!==robot.id||
          robot.virtual_cargo_known!==true||robot.virtual_cargo!==true||!cargoResult(robot,robot.node,true) :
          rack.location.type!=='slot'||rack.location.id!==move.dropoff))throw conflict('Kệ đã đổi vị trí sau nâng/hạ; cần đối soát');
      return move; // Only the gate-exit task remains; never replay the servo.
    }
    if(move.status!=='needs_attention'||!['reserved','carrying'].includes(move.interrupted_from))
      throw conflict('Lượt chuyển không có trạng thái an toàn để tiếp tục');
    const rack=state.racks.find(rack=>rack.id===move.rack_id),up=move.interrupted_from==='carrying';
    const backingAfterLowering=up&&robot.lift?.dropoff_exit_pending===true&&robot.lift.dropoff_exit_supported===true&&
      robot.lift.node===move.dropoff&&robot.node===move.dropoff&&robot.lift.fresh===true&&
      robot.lift.state==='LOWERED'&&cargoReading(robot,robot.node).known;
    if(!rack || (up ? rack.location.type!=='robot'||rack.location.id!==robot.id||
      robot.virtual_cargo_known!==true||robot.virtual_cargo!==true||!backingAfterLowering&&!cargoResult(robot,robot.node,true) :
      rack.location.type!=='slot'||rack.location.id!==move.pickup||robot.virtual_cargo===true))
      throw conflict('Vị trí hoặc trạng thái kệ chưa khớp để tiếp tục');
    return move;
  }
  function resumeMove(key,robot,expectedRevision) {
    return change((state,event)=>{
      const move=checkResume(key,robot,expectedRevision,state);
      if(move.status==='placed'||move.status==='completed')return;
      move.status=move.interrupted_from;move.reason='';
      const rack=state.racks.find(rack=>rack.id===move.rack_id);
      rack.status=move.status==='carrying'?'on_robot':'stored';
      event(`resume:${randomUUID()}`,{type:'resumed',move_key:key,rack_id:rack.id,robot:robot.id,
        evidence:'operator_verified',message:`Tiếp tục chuyển kệ ${rack.id} sau xác minh RFID ${robot.node}`});
    });
  }
  function confirmLift(key,action,robot,commandId,session) {
    const eventKey=`lift:${session}:${robot.id}:${commandId}`;
    const duplicate=sql.prepare('SELECT event_json FROM warehouse_events WHERE event_key=?').get(eventKey);
    if(duplicate) {
      const entry=JSON.parse(duplicate.event_json);
      if(entry.move_key!==key||entry.type!==action)throw conflict('Kết quả servo đã gắn với thao tác khác');
      return;
    }
    change((state,event)=>{
      const move=state.moves.find(move=>move.key===key),up=action==='pickup';
      if(!move||move.status!==(up?'reserved':'carrying'))throw conflict('Kệ chưa được đặt chỗ hoặc đang cần đối soát');
      const node=up?move.pickup:move.dropoff,lift=robot.lift;
      const exitProof=!up&&lift?.dropoff_exit_supported===true;
      const observedNode=exitProof?node.replace(/^K/,'G'):node;
      if(!up&&robot.cargo_sensor?.required===true&&!exitProof)throw conflict('Cần firmware xác nhận hạ rồi lùi ra gate');
      if(!Number.isSafeInteger(commandId)||commandId<=0||robot.id!==move.robot||robot.node!==observedNode||robot.state!=='IDLE'||robot.error||
        robot.online!==true||robot.position_valid===false||robot.task_id||lift?.fresh!==true||lift.command_id!==commandId||
        lift.observed_node!==observedNode||exitProof&&(lift.node!==node||lift.dropoff_exit_pending!==false)||
        lift.state!==(up?'DONE_UP':'DONE_DOWN')||lift.active!==false||robot.virtual_cargo_known!==true||robot.virtual_cargo!==up||!cargoResult(robot,observedNode,up))
        throw conflict('Chưa có kết quả servo tương quan tại đúng vị trí');
      const slot=state.slots.find(slot=>slot.node===node),rack=state.racks.find(rack=>rack.id===move.rack_id);
      if(!slot||!rack||(up?slot.rack_id!==rack.id:slot.rack_id!==null))throw conflict('Vị trí kệ không khớp sổ kho');
      slot.rack_id=up?null:rack.id;rack.location={type:up?'robot':'slot',id:up?robot.id:node};
      rack.status=up?'on_robot':'stored';move.status=up?(move.standalone?'completed':'carrying'):'placed';
      move[up?'pickup_command_id':'dropoff_command_id']=commandId;
      event(eventKey,{type:action,evidence:robot.cargo_sensor?.required?'servo_and_cargo_sensor':'servo_cycle',rack_id:rack.id,robot:robot.id,move_key:key,source:up?node:`xe ${robot.id}`,
        target:up?`xe ${robot.id}`:node,command_id:commandId,core_session_id:session,
        message:up?`Xe ${robot.id} đã nâng kệ ${rack.id} tại ${node}`:`Xe ${robot.id} đã đặt kệ ${rack.id} tại ${node}`});
    });
  }
  function confirmMissing(key,robot,session) {
    const state=read(),move=state.moves.find(move=>move.key===key);
    if(move?.status==='missing')return; // Repeated observations cannot remove another rack.
    const reading=cargoReading(robot,move?.pickup);
    if(!move||move.status!=='reserved'||move.robot!==robot.id||!reading.required||!reading.known||reading.present!==false||
      robot.node!==move.pickup||robot.online!==true||robot.position_valid!==true||robot.state!=='IDLE'||robot.error||
      robot.task_id||robot.target||robot.path?.length||robot.lift?.active)
      throw conflict('Chưa có bằng chứng kho trống khi xe dừng đúng RFID trước nâng');
    change((state,event)=>{
      const move=state.moves.find(move=>move.key===key),slot=state.slots.find(slot=>slot.node===move.pickup),
        rack=state.racks.find(rack=>rack.id===move.rack_id);
      if(!slot||!rack||slot.rack_id!==rack.id||rack.location.type!=='slot'||rack.location.id!==move.pickup)
        throw conflict('Vị trí kệ đã thay đổi trước khi xác nhận thiếu hàng');
      slot.rack_id=null;
      rack.status='needs_attention';rack.location={type:'unknown',id:move.pickup};
      move.status='missing';move.reason='LM393 xác nhận kho trống trước nâng';
      event(`missing:${key}`,{type:'missing',evidence:'cargo_sensor',move_key:key,robot:robot.id,rack_id:rack.id,
        source:move.pickup,target:null,core_session_id:session,sensor_seq:robot.cargo_sensor.sample_seq,
        message:`${move.pickup} trống thực tế; cần tìm lại kệ ${rack.id}; không phát lệnh nâng`});
    });
  }
  function rollbackPlan(order,state=read()) {
    const moves=[];
    for(const item of order.items) {
      if(item.status==='delivered')continue;
      const move=state.moves.find(move=>move.key===`order:${order.id}:${item.number}`);
      // A confirmed dropoff remains at its destination, even if exiting failed.
      if(move?.status==='placed'||move?.status==='rolled_back'||move?.status==='missing')continue;
      if(!move) {
        if(sql.prepare("SELECT 1 FROM warehouse_events WHERE json_extract(event_json,'$.move_key')=? AND json_extract(event_json,'$.type')='dropoff' LIMIT 1")
          .get(`order:${order.id}:${item.number}`))continue;
        if(item.rack_id)throw conflict('Không còn dữ liệu lượt chuyển kệ; kiểm tra lại kho trước khi hoàn tác');
        continue; // Legacy/never-dispatched order has no inventory change to undo.
      }
      if(!['reserved','carrying','needs_attention'].includes(move.status)||!move.pickup)
        throw conflict('Lượt chuyển kệ không thể hoàn tác');
      const source=state.slots.find(slot=>slot.node===move.pickup),rack=state.racks.find(rack=>rack.id===move.rack_id);
      if(!source||!rack||source.rack_id&&source.rack_id!==rack.id)
        throw conflict(`${move.pickup} đang có kệ khác hoặc dữ liệu kho không khớp`);
      if(!(rack.location.type==='robot'&&rack.location.id===move.robot ||
          rack.location.type==='slot'&&rack.location.id===move.pickup))
        throw conflict(`Kệ ${rack.id} đã đổi vị trí; tải lại trước khi hoàn tác`);
      if(state.racks.some(other=>other.id!==rack.id&&other.location.type==='robot'&&other.location.id===move.robot))
        throw conflict(`Xe ${move.robot} đang có kệ khác; kiểm tra lại trước khi hoàn tác`);
      if(state.moves.some(other=>other.key!==move.key&&openMove(other)&&
          (other.rack_id===rack.id||other.robot===move.robot||[other.pickup,other.dropoff].includes(move.pickup))))
        throw conflict('Kệ, xe hoặc kho lấy đang thuộc lượt chuyển khác');
      moves.push({key:move.key,rack_id:rack.id,robot:move.robot,pickup:move.pickup});
    }
    return moves;
  }
  function rollbackOrder(order,expectedRevision,commitOrder) {
    return change((state,event)=>{
      if(state.initialized&&expectedRevision!==state.revision)throw conflict('Trạng thái kho đã thay đổi; tải lại trước khi hoàn tác');
      const moves=rollbackPlan(order,state);
      for(const entry of moves) {
        const move=state.moves.find(move=>move.key===entry.key),rack=state.racks.find(rack=>rack.id===entry.rack_id);
        const previous={...rack.location};
        state.slots.find(slot=>slot.node===entry.pickup).rack_id=rack.id;
        rack.location={type:'slot',id:entry.pickup};rack.status='stored';
        move.status='rolled_back';move.resolved_at=now();
        event(`rollback:${move.key}`,{type:'rollback',evidence:'operator_verified',order_id:order.id,
          rack_id:rack.id,robot:move.robot,move_key:move.key,previous,target:entry.pickup,
          message:`Đã hoàn tác kệ ${rack.id} về ${entry.pickup}; xác nhận xe ${move.robot} trống`});
      }
      // The order and ledger commit together; a failed save rolls back both.
      commitOrder(moves);return moves;
    });
  }
  function finishShift(state,event,reason,context) {
    const shift=state.shift;
    for(const move of state.moves)markInterrupted(state,event,move,'Ca đã kết thúc khi lượt chuyển kệ chưa hoàn tất');
    const events=sql.prepare('SELECT id,event_json FROM warehouse_events WHERE shift_id=? ORDER BY id').all(shift.id)
      .map(row=>({id:row.id,...JSON.parse(row.event_json)}));
    const report={id:shift.id,created_at:now(),started_at:shift.started_at,core_session_id:shift.core_session_id,
      reason,observation_gap:shift.observation_gap,initial_state:shift.initial_state,final_state:snapshot(state),
      events,summary:{pickups:events.filter(e=>e.type==='pickup').length,dropoffs:events.filter(e=>e.type==='dropoff').length,
        on_robots:state.racks.filter(rack=>rack.location.type==='robot').length,
        needs_attention:state.racks.filter(rack=>rack.status==='needs_attention').length},
       unfinished_orders:context.orders?.filter(order=>!['completed','closed'].includes(order.status)||
         order.closed_by_shift&&order.items?.some(item=>item.status!=='delivered'))||[],
      unfinished_actions:context.actions?.filter(action=>action.status!=='completed')||[]};
    sql.prepare('INSERT INTO warehouse_shift_reports(id,report_json,created_at) VALUES(?,?,?)')
      .run(report.id,JSON.stringify(report),report.created_at);
    event(`shift-end:${shift.id}`,{type:'shift_closed',message:'Đã lưu báo cáo trạng thái kho cuối ca',report_id:report.id,reason});
    state.shift=null;state.moves=state.moves.filter(openMove);
  }
  let lastPhase=null;
  function observeShift(core,session,context={}) {
    const phase=core.shift?.phase;if(!['RUNNING','CLOSING','CLOSED'].includes(phase))return;
    const previous=read(),wasClosed=lastPhase==='CLOSED';lastPhase=phase;
    if(!previous.shift&&phase!=='RUNNING')return;
    if(previous.shift&&previous.shift.core_session_id===session&&phase!=='CLOSED')return;
    change((state,event)=>{
      if(state.shift&&(state.shift.core_session_id!==session||phase==='CLOSED'))
        finishShift(state,event,state.shift.core_session_id===session?'shift_closed':'core_session_changed',context);
      if(!state.shift&&phase==='RUNNING') {
        state.shift={id:randomUUID(),core_session_id:session,started_at:now(),observation_gap:!wasClosed,initial_state:snapshot(state)};
        event(`shift-start:${state.shift.id}`,{type:'shift_started',message:'Bắt đầu theo dõi trạng thái kho trong ca'});
      }
    });
  }
  function currentSnapshot() {
    const state=read();
    return {...snapshot(state),shift:state.shift ? {...state.shift,initial_state:undefined,initial_slots:state.shift.initial_state.slots}:null,
      events:history(20),latest_report_id:sql.prepare('SELECT id FROM warehouse_shift_reports ORDER BY created_at DESC,id DESC LIMIT 1').get()?.id||null};
  }
  return {snapshot:currentSnapshot,configure,reconciliationFor,assertPlan,reserve,confirmLift,confirmMissing,interrupt,checkResume,resumeMove,rollbackPlan,rollbackOrder,
    observationGap,observeShift,history,reports,reportById,
    robotAvailable:id=>!read().racks.some(rack=>rack.location.type==='robot'&&rack.location.id===id)};
}

module.exports={createWarehouseInventory};
