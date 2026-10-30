'use strict';
let shiftReviewSnapshot=null;
let vehicleRecoveryContext=null;
function openVehicleRecovery(id) {
  const robot=lastState?.fleet?.agvs?.find(robot=>robot.id===id);
  if(!robot)return;
  vehicleRecoveryContext={id,node:robot.node,session:lastState.core_session_id,submitted:false,busy:false};
  $('vehicleRecoveryClear').checked=false;
  $('vehicleRecoveryDirection').value='';$('vehicleRecoveryHeadingVerified').checked=false;
  $('vehicleRecoveryDialog').showModal();updateVehicleRecovery();
}
function updateVehicleRecovery() {
  if(!$('vehicleRecoveryDialog')?.open||!vehicleRecoveryContext)return;
  const ctx=vehicleRecoveryContext,robot=lastState?.fleet?.agvs?.find(robot=>robot.id===ctx.id);
  const button=$('vehicleRecoverySubmit'),status=$('vehicleRecoveryStatus');
  $('vehicleRecoveryTitle').textContent=`Khôi phục xe ${ctx.id}`;
  $('vehicleRecoveryPosition').textContent=`RFID hiện tại: ${robot?.node?locationText(robot.node):'Chưa xác định'}`;
  button.disabled=true;
  if(!controlsReady()||!robot?.online){status.textContent='Chờ kết nối và dữ liệu mới từ xe.';return;}
  if(ctx.session!==lastState.core_session_id||ctx.node!==robot.node){
    status.textContent='Vị trí hoặc phiên Core đã đổi. Đóng và mở lại để kiểm tra trạng thái mới.';return;
  }
  if(robot.recovery_reset_pending){status.textContent='Đã gửi reset; chờ xe xác nhận hết lỗi. Xe vẫn đứng yên.';return;}
  if(robot.heading_confirmation_pending){status.textContent='Đang chờ xe xác nhận hướng. ACK nhận lệnh chưa hoàn tất bước này.';return;}
  const home=lastState.shift?.home_stations?.[ctx.id]===robot.node;
  const ready=robot.state==='IDLE'&&(home?robot.position_valid===true:robot.recovery_node_verified===true)&&
    !robot.task_id&&!robot.target&&!(robot.path||[]).length&&!(robot.uncertain_path||[]).length&&
    !robot.cancel_waiting_for_node&&!robot.replan_waiting_for_node&&!robot.lift?.active;
  $('vehicleRecoveryChecks').hidden=home;
  $('vehicleRecoveryHeading').hidden=home;
  button.textContent='Khôi phục và tiếp tục';
  if(ctx.busy){status.textContent='Đang gửi yêu cầu…';return;}
  if(home){status.textContent=ready&&!robot.heading_confirmation_required?'Xe đã sẵn sàng tại trạm. Không cần xác nhận hướng thủ công.':'Xe tự khôi phục tại trạm riêng, không cần chọn hướng. Đang chờ diagnostic khỏe và xác nhận dừng; lỗi hoặc công việc chưa xử lý vẫn giữ chốt.';return;}
  {
    const expected=robot.recovery_expected_heading;
    $('vehicleRecoveryHeadingHint').textContent=expected?
      `Tại ${locationText(robot.node)}, đặt đầu xe ${headingLabels[expected]} theo quy ước kho/trạm rồi chọn xác nhận. Thao tác này không làm xe tự quay.`:
      'Chọn hướng đầu xe thực tế theo bản đồ. Thao tác này không làm xe tự quay.';
    for(const option of $('vehicleRecoveryDirection').options)option.disabled=!!option.value&&!!expected&&option.value!==expected;
    if(!robot.heading_confirmation_supported){status.textContent='Firmware chưa hỗ trợ xác nhận hướng.';return;}
  }
  if(ready&&ctx.submitted&&!robot.heading_confirmation_required){
    status.textContent='Đã khôi phục. Server tự tiếp tục chặng cũ khi vị trí, hàng và tuyến an toàn hợp lệ.';
    return;
  }
  if(!home&&lastState.shift?.phase!=='RUNNING'){
    status.textContent='Ngoài trạm riêng, chỉ khôi phục trong ca đang chạy. Không đặt lại RFID bằng nút bấm.';return;
  }
  if(!robot.recovery_stop_verified){
    const reasons={telemetry_stale:'Chưa có telemetry mới. Kiểm tra kết nối xe.',rfid_unknown:'Chưa xác định RFID. Đặt đầu đọc tại thẻ đúng vị trí thực tế.',
      stop_unverified:'MCU chưa xác nhận dừng: kiểm tra motor, tuyến và cơ cấu nâng/hạ đã dừng.',
      diagnostic_stale:'Diagnostic đã cũ. Kiểm tra đường truyền hoặc firmware.',session_changed:'Chờ diagnostic đúng phiên kết nối mới.',
      rfid_mismatch:'RFID và diagnostic chưa khớp. Kiểm tra đầu đọc tại thẻ.',rfid_stale:'RFID đang báo là lần đọc cũ. Đặt đầu đọc tại thẻ để đọc lại.'};
    status.textContent=reasons[robot.recovery_stop_reason]||'Chờ dữ liệu mới xác nhận xe dừng tại RFID hiện tại.';return;
  }
  status.textContent='Kiểm tra vị trí, hướng đầu xe và vùng đường rồi xác nhận một lần.';
  button.disabled=!$('vehicleRecoveryClear').checked||!$('vehicleRecoveryDirection').value;
}
async function submitVehicleRecovery() {
  const ctx=vehicleRecoveryContext,robot=lastState?.fleet?.agvs?.find(robot=>robot.id===ctx?.id);
  if(!ctx||!robot||ctx.busy)return;
  updateVehicleRecovery();if($('vehicleRecoverySubmit').disabled)return;
  const body={expected_node:ctx.node,heading:$('vehicleRecoveryDirection').value,physically_clear:true,continue_task:true};
  ctx.busy=true;updateVehicleRecovery();
  try {
    const result=await control('POST',`/api/agvs/${encodeURIComponent(ctx.id)}/recover`,body);
    if(result)ctx.submitted=true;
  } finally {ctx.busy=false;updateVehicleRecovery();}
}
function recoveryAllowsResume(id) {
  const robot=lastState?.fleet?.agvs?.find(robot=>robot.id===id);
  if(robot&&(robot.heading_confirmation_required||robot.heading_confirmation_pending||
      !robot.heading_supported||!['N','E','S','W'].includes(robot.heading))) {
    openVehicleRecovery(id);return false;
  }
  return true;
}
async function resumeStandaloneTask(task,button) {
  if(!recoveryAllowsResume(task.assigned_agv||task.robot))return;
  if(!confirm(`Tiếp tục nhiệm vụ cũ từ RFID ${locationText(task.resume_node)}? Xác nhận xe và hàng đúng trạng thái thực tế.`))return;
  if(await control('POST',`/api/tasks/${encodeURIComponent(task.core_task_id)}/resume`,{expected_node:task.resume_node},button))
    toast('Core đã cấp tuyến mới cho nhiệm vụ cũ.');
}
async function resumeWarehouseItem(order,item,button) {
  if(!recoveryAllowsResume(item.robot))return;
  if(!confirm(`Tiếp tục lượt ${locationText(item.pickup)} → ${locationText(item.dropoff)} từ RFID ${locationText(item.resume_node)}? Xác nhận vị trí kệ và xe thực tế đúng sổ kho.`))return;
  if(await control('POST',`/api/orders/${encodeURIComponent(order.id)}/resume`,{
    item_number:item.number,physically_clear:true,expected_revision:lastState.warehouse?.revision},button))
    toast('Đã tiếp tục chặng vận chuyển của lượt cũ.');
}
async function resumeWarehouseAction(task,button) {
  if(!recoveryAllowsResume(task.robot))return;
  if(!confirm(`Tiếp tục thao tác kho từ RFID ${locationText(task.resume_node)}? Xác nhận xe và kệ đúng vị trí.`))return;
  if(await control('POST',`/api/warehouse-actions/${encodeURIComponent(task.id)}/resume`,{
    physically_clear:true,expected_revision:lastState.warehouse?.revision},button))toast('Đã tiếp tục thao tác kho.');
}
async function openShiftReview() {
  $('shiftReviewDialog').showModal();await refreshShiftReview();
}
async function refreshShiftReview() {
  const box=$('shiftReviewContent');box.replaceChildren();shiftReviewSnapshot=null;
  $('confirmShiftReviewBtn').disabled=true;$('shiftReviewVerified').checked=false;
  const review=await api('GET','/api/shift/review');if(!review)return;
  shiftReviewSnapshot=review;
  const groups=new Map();
  for(const incident of review.incidents) {
    const previous=groups.get(incident.message)||{count:0,at:incident.created_at};previous.count++;groups.set(incident.message,previous);
  }
  warehouseText(box,`${review.incidents.length} cảnh báo chưa đối soát · ${groups.size} nhóm lỗi`,'p');
  if(review.has_more)warehouseText(box,'Đang xem một phần nhật ký; đối soát xong sẽ hiện phần tiếp theo.','p');
  for(const [message,group] of groups)warehouseText(box,`${message} · ${group.count} lần · ${fmtDate(group.at)}`,'p','log-item');
  if(!groups.size)warehouseText(box,'Không có lỗi chưa đối soát.','p');
  for(const blocker of review.blockers)warehouseText(box,blocker,'p','transfer-alert');
  $('shiftReviewStatus').textContent=review.blockers.length?'Xử lý các mục còn treo rồi tải lại danh sách.':'Sẵn sàng xác nhận; lịch sử ca được lưu trong báo cáo.';
  $('confirmShiftReviewBtn').disabled=!!review.blockers.length;
}
async function confirmShiftReview() {
  if(!shiftReviewSnapshot||!$('shiftReviewVerified').checked)return toast('Cần xác nhận đã kiểm tra thực tế.','warn');
  const result=await api('POST','/api/shift/reconcile',{
    review_token:shiftReviewSnapshot.review_token,physically_verified:true});
  if(result){await refreshShiftReview();toast('Đã chốt các lỗi đã xem; lịch sử và báo cáo được giữ.');}
}
