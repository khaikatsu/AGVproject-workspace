const {randomUUID} = require('node:crypto');

const conflict = message=>Object.assign(new Error(message),{status:409});
const invalid = message=>Object.assign(new Error(message),{status:400});
const openMove = move=>['reserved','carrying','needs_attention'].includes(move.status);
const now = ()=>new Date().toISOString();

// Gateway-owned ledger: the Core remains the authority for robot/lift completion.
function createWarehouseInventory({db,publish=()=>{}}) {
  const sql=db.db;
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
    move.status='needs_attention';move.reason=reason;
    const rack=state.racks.find(rack=>rack.id===move.rack_id);if(rack)rack.status='needs_attention';
    event(`interrupt:${move.key}`,{type:'interrupted',rack_id:move.rack_id,robot:move.robot,
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
    const nodes=core.map.nodes.filter(node=>node.type==='station'&&/^K[1-6]$/.test(node.id)).map(node=>node.id).sort();
    if(!nodes.length || !Array.isArray(body.slots)||body.slots.length!==nodes.length)throw invalid('Nhập đầy đủ các vị trí K trên bản đồ');
    const slots=body.slots.map(slot=>{
      if(!slot||!nodes.includes(slot.node)||!(slot.rack_id===null||typeof slot.rack_id==='string'))throw invalid('Vị trí hoặc mã kệ không hợp lệ');
      const rack_id=slot.rack_id===null ? null : slot.rack_id.trim();
      if(rack_id!==null&&!/^[A-Za-z0-9_-]{1,32}$/.test(rack_id))throw invalid('Mã kệ gồm 1–32 chữ, số, dấu gạch ngang hoặc gạch dưới');
      return {node:slot.node,rack_id};
    }).sort((a,b)=>a.node.localeCompare(b.node));
    const ids=slots.map(slot=>slot.rack_id).filter(Boolean);
    if(new Set(slots.map(slot=>slot.node)).size!==nodes.length||new Set(ids).size!==ids.length)throw invalid('Không được trùng vị trí hoặc mã kệ');
    return change((state,event)=>{
      if(body.expected_revision!==state.revision)throw conflict('Trạng thái kho đã thay đổi; tải lại trước khi lưu');
      const previous=snapshot(state);
      state.slots=slots;state.racks=slots.filter(slot=>slot.rack_id).map(slot=>({id:slot.rack_id,status:'stored',location:{type:'slot',id:slot.node}}));
      state.moves=[];state.initialized=true;
      event(`configure:${randomUUID()}`,{type:previous.initialized?'reconciled':'initialized',evidence:'operator_verified',
        previous,slots,message:previous.initialized?'Đã đối soát lại vị trí kệ và xác nhận xe trống':'Đã nhập trạng thái kho ban đầu'});
    });
  }
  function checkMove(state,{key,pickup,dropoff,robot,standalone}) {
    if(!state.initialized)throw conflict('Nhập trạng thái kệ ban đầu trước khi tạo đơn hàng kho');
    const existing=state.moves.find(move=>move.key===key);
    if(existing)return existing;
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
    const existing=read().moves.find(move=>move.key===spec.key);if(existing)return existing;
    return change((state,event)=>{
      const move=checkMove(state,spec);state.moves.push(move);
      event(`reserve:${move.key}`,{type:'reserved',...move,message:`Dành kệ ${move.rack_id} cho xe ${move.robot}`});
      return move;
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
      if(!Number.isSafeInteger(commandId)||commandId<=0||robot.id!==move.robot||robot.node!==node||robot.state!=='IDLE'||robot.error||
        robot.online!==true||robot.position_valid===false||robot.task_id||lift?.fresh!==true||lift.command_id!==commandId||
        lift.observed_node!==node||lift.state!==(up?'DONE_UP':'DONE_DOWN')||lift.active!==false||robot.virtual_cargo_known!==true||robot.virtual_cargo!==up)
        throw conflict('Chưa có kết quả servo tương quan tại đúng vị trí');
      const slot=state.slots.find(slot=>slot.node===node),rack=state.racks.find(rack=>rack.id===move.rack_id);
      if(!slot||!rack||(up?slot.rack_id!==rack.id:slot.rack_id!==null))throw conflict('Vị trí kệ không khớp sổ kho');
      slot.rack_id=up?null:rack.id;rack.location={type:up?'robot':'slot',id:up?robot.id:node};
      rack.status=up?'on_robot':'stored';move.status=up?(move.standalone?'completed':'carrying'):'placed';
      move[up?'pickup_command_id':'dropoff_command_id']=commandId;
      event(eventKey,{type:action,evidence:'servo_cycle',rack_id:rack.id,robot:robot.id,move_key:key,source:up?node:`xe ${robot.id}`,
        target:up?`xe ${robot.id}`:node,command_id:commandId,core_session_id:session,
        message:up?`Xe ${robot.id} đã nâng kệ ${rack.id} tại ${node}`:`Xe ${robot.id} đã đặt kệ ${rack.id} tại ${node}`});
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
      unfinished_orders:context.orders?.filter(order=>!['completed','closed'].includes(order.status))||[],
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
  return {snapshot:currentSnapshot,configure,assertPlan,reserve,confirmLift,interrupt,
    observationGap,observeShift,history,reports,reportById,
    robotAvailable:id=>!read().racks.some(rack=>rack.location.type==='robot'&&rack.location.id===id)};
}

module.exports={createWarehouseInventory};
