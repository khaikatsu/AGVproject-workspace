'use strict';
let warehouseFormRevision=null,warehouseLatestReport=null,warehouseReportSelection=null;
const rackStateNames={stored:'Tại kho',on_robot:'Trên xe',needs_attention:'Cần đối soát'};
function warehouseText(parent,text,tag='p',className='') {
  const element=document.createElement(tag);element.textContent=text;element.className=className;parent.append(element);return element;
}
function warehouseRows(inventory) {
  const rows=(inventory?.slots||[]).map(slot=>{
    const rack=inventory.racks.find(rack=>rack.id===slot.rack_id);
    const held=inventory.moves?.find(move=>['reserved','carrying','needs_attention'].includes(move.status)&&[move.pickup,move.dropoff].includes(slot.node));
    return [slot.node,slot.rack_id||'—',rack?.status==='needs_attention'?'Cần đối soát':held?.status==='needs_attention'?'Cần đối soát':held?`Đặt chỗ xe ${held.robot}`:slot.rack_id?'Có kệ':'Trống'];
  });
  for(const rack of inventory?.racks||[])if(rack.location.type==='robot')
    rows.push([`Xe ${rack.location.id}`,rack.id,rackStateNames[rack.status]||rack.status]);
  return rows;
}
function renderWarehouse(inventory) {
  const box=$('warehouseSummary'),tbody=$('warehouseStateTable').tBodies[0];tbody.replaceChildren();
  if(!inventory?.initialized)box.textContent='Chưa nhập trạng thái kho. Mở phần nhập vị trí kệ khi ca đã đóng.';
  else {
    const onRobot=inventory.racks.filter(rack=>rack.location.type==='robot').length;
    const uncertain=inventory.racks.filter(rack=>rack.status==='needs_attention').length;
    box.textContent=`${inventory.racks.length} kệ · ${inventory.slots.filter(slot=>!slot.rack_id).length} ô trống · ${onRobot} kệ trên xe · ${uncertain} cần đối soát`;
    for(const row of warehouseRows(inventory))appendRow(tbody,row);
  }
  updateWarehouseInventoryControls();
  const logs=$('warehouseEventList');logs.replaceChildren();
  for(const entry of inventory?.events||[])warehouseText(logs,`${fmtDate(entry.created_at)} · ${entry.message||entry.type}`,'div','log-item');
  if(!logs.children.length)logs.textContent='Chưa có thao tác kệ.';
  if(inventory?.latest_report_id&&inventory.latest_report_id!==warehouseLatestReport) {
    warehouseLatestReport=inventory.latest_report_id;if(backendOnline)void loadWarehouseReports();
  }
}
function updateWarehouseInventoryControls() {
  const enabled=controlsReady()&&lastState?.shift?.phase==='CLOSED';
  $('saveWarehouseBtn').disabled=!enabled;
  for(const field of $('warehouseConfigFields').querySelectorAll('input'))field.disabled=!enabled;
}
function editWarehouseLayout() {
  const container=$('warehouseConfigFields');container.replaceChildren();
  const inventory=lastState?.warehouse;warehouseFormRevision=inventory?.revision??0;$('warehouseVerified').checked=false;
  const nodes=(lastState?.map.nodes||[]).filter(node=>node.type==='station'&&/^K[1-6]$/.test(node.id)).sort((a,b)=>a.id.localeCompare(b.id));
  for(const node of nodes) {
    const label=document.createElement('label');label.textContent=node.id;
    const input=document.createElement('input');input.dataset.rackNode=node.id;input.setAttribute('aria-label',`Mã kệ tại ${node.id}`);
    input.placeholder='Trống';input.maxLength=32;input.pattern='[A-Za-z0-9_-]{1,32}';
    input.value=inventory?.slots.find(slot=>slot.node===node.id)?.rack_id||'';
    input.disabled=!controlsReady()||lastState?.shift?.phase!=='CLOSED';label.append(input);container.append(label);
  }
}
async function saveWarehouseLayout() {
  const slots=[...$('warehouseConfigFields').querySelectorAll('[data-rack-node]')].map(input=>({node:input.dataset.rackNode,rack_id:input.value.trim()||null}));
  const result=await api('POST','/api/warehouse/configure',{slots,expected_revision:warehouseFormRevision,physically_verified:$('warehouseVerified').checked});
  if(result?.warehouse) {
    lastState.warehouse=result.warehouse;warehouseFormRevision=result.warehouse.revision;renderWarehouse(result.warehouse);
    $('warehouseConfig').open=false;toast('Đã lưu vị trí kệ và nhật ký đối soát.');
  }
}
async function loadWarehouseReports() {
  const rows=await api('GET','/api/warehouse/reports?limit=20',undefined,true);if(!Array.isArray(rows))return;
  const box=$('warehouseReportList');box.replaceChildren();
  for(const report of rows) {
    const button=warehouseText(box,`${fmtDate(report.created_at)} · nâng ${report.summary.pickups}, hạ ${report.summary.dropoffs} · ${report.summary.needs_attention} cần đối soát`,'button','btn-outline full-width');
    button.type='button';button.onclick=()=>showWarehouseReport(report);
  }
  if(!rows.length)box.textContent='Chưa có báo cáo kho cuối ca.';
  else showWarehouseReport(rows.find(report=>report.id===warehouseReportSelection)||rows[0]);
}
function showWarehouseReport(report) {
  warehouseReportSelection=report.id;const box=$('warehouseReportDetail');box.replaceChildren();
  warehouseText(box,`${fmtDate(report.started_at)} → ${fmtDate(report.created_at)}`);
  if(report.reason!=='shift_closed'||report.observation_gap)warehouseText(box,'Ca có gián đoạn quan sát hoặc đổi phiên Core; cần đối chiếu thực tế.');
  warehouseText(box,`Nâng: ${report.summary.pickups} · Hạ: ${report.summary.dropoffs} · Trên xe: ${report.summary.on_robots} · Cần đối soát: ${report.summary.needs_attention}`);
  for(const [title,inventory] of [['Đầu ca',report.initial_state],['Cuối ca',report.final_state]]) {
    warehouseText(box,title,'strong');const table=document.createElement('table'),body=document.createElement('tbody');
    for(const row of warehouseRows(inventory))appendRow(body,row);table.append(body);box.append(table);
    if(!inventory.initialized)warehouseText(box,'Kho chưa được nhập trạng thái tại thời điểm này.');
  }
  if(report.unfinished_orders?.length||report.unfinished_actions?.length)
    warehouseText(box,`Chưa hoàn tất: ${report.unfinished_orders.length} đơn, ${report.unfinished_actions.length} thao tác kho.`);
  for(const entry of report.events)warehouseText(box,`${fmtDate(entry.created_at)} · ${entry.message||entry.type}`,'div','log-item');
  const download=warehouseText(box,'Tải báo cáo JSON','button','btn');download.type='button';
  download.onclick=async()=>{
    const data=await api('GET',`/api/warehouse/reports/${encodeURIComponent(report.id)}`);if(!data)return;
    const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));
    const anchor=document.createElement('a');anchor.href=url;anchor.download=`kho-${report.id}.json`;anchor.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  };
}
