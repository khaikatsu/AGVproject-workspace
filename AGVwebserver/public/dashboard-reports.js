'use strict';

let selectedPerformanceReport=null;
function reportText(parent,text,tag='div',className='') {
  const element=document.createElement(tag);element.textContent=text;element.className=className;parent.append(element);return element;
}
function reportMetric(parent,label,value) {
  const card=reportText(parent,'','div','report-metric');reportText(card,label,'span');reportText(card,String(value),'strong');return card;
}
function reportDetails(parent,title) {
  const details=document.createElement('details');details.className='report-details';
  reportText(details,title,'summary');const content=reportText(details,'','div','report-details-content');parent.append(details);return content;
}
function reportTable(parent,head,rows) {
  const table=document.createElement('table');table.className='report-table';
  if(head.length) {const header=table.createTHead().insertRow();for(const title of head)reportText(header,title,'th');}
  const body=table.createTBody();for(const row of rows)appendRow(body,row);parent.append(table);return table;
}
function showReportSection(section) {
  if(!['performance','warehouse','logs'].includes(section))return;
  document.querySelectorAll('[data-report-panel]').forEach(panel=>panel.hidden=panel.dataset.reportPanel!==section);
  document.querySelectorAll('[data-report-target]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.reportTarget===section)));
  if(!backendOnline)return;
  if(section==='performance'){void loadLatestKpi();void loadKpiHistory();}
  if(section==='warehouse')void loadWarehouseReports();
  if(section==='logs')showReportLog();
}
function showReportLog() {
  const system=$('reportLogSource').value==='system';
  $('systemLogBox').hidden=!system;$('warehouseEventList').hidden=system;
  if(system&&backendOnline)void loadStoredLogs();
}
async function refreshReportLogs() {
  if($('reportLogSource').value==='system')return loadStoredLogs();
  const rows=await api('GET','/api/warehouse/events?limit=50');
  if(Array.isArray(rows))renderWarehouseEvents(rows);
}
function fmtKpiDuration(seconds) {
  if(seconds==null)return '—';const n=Math.max(0,Math.round(finite(seconds)));
  return n<60?`${n}s`:n<3600?`${Math.floor(n/60)}p ${n%60}s`:`${Math.floor(n/3600)}h ${Math.floor(n%3600/60)}p`;
}
function syncKpiReportOptions() {
  const select=$('kpiHistoryBox');select.replaceChildren();const rows=[...kpiHistoryData];
  if(selectedPerformanceReport&&!rows.some(report=>report.id===currentKpiId))rows.unshift(selectedPerformanceReport);
  for(const report of rows)select.add(new Option(`#${report.id} · ${fmtDate(report.created_at)} · ${finite(report.completed_tasks)}/${finite(report.total_tasks)} hoàn thành`,String(report.id)));
  if(!rows.length)select.add(new Option('Chưa có báo cáo',''));
  select.disabled=!rows.length;select.value=currentKpiId==null?'':String(currentKpiId);
}
function selectKpiReport(value) {
  const report=kpiHistoryData.find(report=>String(report.id)===value)||
    (String(selectedPerformanceReport?.id)===value?selectedPerformanceReport:null);
  if(report)renderKpi(report);
}
function renderKpi(report) {
  const box=$('kpiBox');selectedPerformanceReport=report;currentKpiId=report?.id??null;box.replaceChildren();syncKpiReportOptions();
  if(!report){reportText(box,'Chưa có báo cáo.','p','report-period');return;}
  reportText(box,`#${report.id} · ${fmtDate(report.period_start)} → ${fmtDate(report.period_end)}`,'p','report-period');
  const metrics=reportText(box,'','div','report-metrics');
  reportMetric(metrics,'Hoàn thành',`${finite(report.completed_tasks)} / ${finite(report.total_tasks)}`).title='Nhiệm vụ được tạo trong kỳ báo cáo';
  reportMetric(metrics,'Thời gian TB',fmtKpiDuration(report.avg_completion_seconds));
  reportMetric(metrics,'Đang xử lý',finite(report.pending_tasks)+finite(report.dispatched_tasks));
  reportMetric(metrics,'Sự cố xe',finite(report.agv_error_events));
  const detail=reportDetails(box,'Chi tiết nhiệm vụ');
  reportTable(detail,[],[
    ['Chờ phân xe',finite(report.pending_tasks)],['Đã giao xe',finite(report.dispatched_tasks)],
    ['Đã hủy',finite(report.cancelled_tasks)],['Gián đoạn',finite(report.interrupted_tasks)],
    ['Bị từ chối',finite(report.rejected_tasks)],['Chưa rõ kết quả',finite(report.unknown_tasks)],
    ['Hoàn thành trong kỳ (gồm nhiệm vụ kỳ trước)',finite(report.completed_in_period)],
    ['Chờ gán xe TB',fmtKpiDuration(report.avg_queue_seconds)],['Từ gán đến hoàn thành TB',fmtKpiDuration(report.avg_travel_seconds)],
    ['Vật cản',finite(report.obstacle_events)]
  ]);
  const utilization=Object.entries(report.agv_utilization||{})
    .filter(([id])=>id==='01'||id==='02').sort(([a],[b])=>a.localeCompare(b));
  const vehicles=reportDetails(box,`Hoạt động xe (${utilization.length})`);
  if(!utilization.length)reportText(vehicles,'Chưa có dữ liệu xe.','p','report-period');
  else reportTable(vehicles,['Xe','Có dữ liệu','Trạng thái'],utilization.map(([id,usage])=>{
    const bar=document.createElement('div');bar.className='utilization-bar';
    for(const part of Array.isArray(usage?.breakdown)?usage.breakdown:[]) {
      const segment=document.createElement('div');segment.style.width=Math.max(0,Math.min(100,finite(part.percent)))+'%';
      segment.style.background=colors[part.state]||'#64748b';segment.title=`${labels[part.state]||part.state}: ${finite(part.percent)}%`;
      bar.append(segment);
    }
    return [id,fmtKpiDuration(usage?.known_seconds),bar];
  }));
}
async function loadLatestKpi() {
  const report=await api('GET','/api/kpi/latest',undefined,true);
  if(!selectedPerformanceReport||selectedPerformanceReport.id===report?.id)renderKpi(report);
}
async function loadKpiHistory() {
  const rows=await api('GET','/api/kpi/history?limit=20');if(!Array.isArray(rows))return;
  kpiHistoryData=rows;
  const selected=rows.find(report=>report.id===currentKpiId);
  if(selected)renderKpi(selected);else if(!selectedPerformanceReport&&rows.length)renderKpi(rows[0]);else syncKpiReportOptions();
}
async function generateKpi() {
  const button=$('generateKpiBtn');if(button.dataset.busy==='true')return;
  button.dataset.busy='true';button.disabled=true;
  try {const result=await api('POST','/api/kpi/generate',{});if(result?.report){renderKpi(result.report);await loadKpiHistory();}}
  finally {button.dataset.busy='false';button.disabled=false;}
}
