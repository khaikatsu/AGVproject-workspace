# ADR-078 — Replan theo toàn bộ dấu vết xe lỗi và hàng đợi mutex

Ngày 23/10/2026. Trạng thái: IMPLEMENTED và VERIFIED bằng kiểm thử phần mềm; chưa thử hai robot thật.

## Bối cảnh

`SOURCE`: người dùng yêu cầu khi một trong hai xe đang đi bị lỗi, xe còn lại được tính lại đường để né xe lỗi thay vì chỉ đứng chờ mutex nếu còn lối khác.

`OBSERVED`: `FleetManager::reportError()` trước đây chỉ gọi replan cho xe có `activePath` chứa đúng `currentNode` của xe lỗi. Nhánh MQTT LWT `reportOffline()` giữ mutex an toàn nhưng không gọi replan. Do đó một tuyến giao với phần còn lại trong `uncertainPath`, node khác cùng hành lang mutex, hoặc một xe đang xếp hàng sau mutex do xe lỗi giữ có thể tiếp tục giữ kế hoạch cũ. Bộ tìm đường khi được gọi đã biết chặn vị trí/đường chưa xác minh; khoảng trống nằm ở tập xe và sự kiện kích hoạt replan.

## Quyết định

- Không tự nhả mutex chỉ vì ERROR, LWT hoặc timeout. Owner vẫn giữ quyền cho tới khi firmware trả đúng token sau clearance, Core tự đối soát bằng RFID/diagnostic an toàn, hoặc người vận hành xác nhận vùng trống.
- Khi xe chuyển ERROR/OFFLINE do báo lỗi, MQTT LWT, mất telemetry hoặc hết retry ACK, Core tạo `failure footprint` gồm `currentNode`, `activePath`, `uncertainPath` và mọi thành phần mutex nối trực tiếp chứa các node đó.
- Core chọn mọi xe đang `WORKING`, `CHARGING_EN_ROUTE` hoặc `BLOCKED` có vị trí, đích hay `activePath` giao với footprint. Core còn lấy các xe đang nằm trong FIFO của mutex do xe lỗi sở hữu.
- Trước khi replan một xe bị ảnh hưởng, Core xóa các lượt chờ mutex cũ của xe đó để tránh grant muộn cho tuyến đã bỏ. Nếu có đường vòng, Core phát GOTO mới; nếu không còn đường an toàn, xe chuyển `BLOCKED`, nhận `WAIT` và thử lại ở tick sau.
- Snapshot hàng đợi mutex được đọc ngoài FleetManager lock; không giữ đồng thời FleetManager mutex và MutexArbiter mutex trong bước hợp nhất này.

## Kiểm chứng và giới hạn

`scripts/build-core.ps1 -Tests -OutputDirectory .qa/failure-replan-build` PASS, gồm ba hồi quy mới:

- lỗi với giao nhau ở `uncertainPath` nhưng không tại `currentNode`: xe còn lại chuyển `BLOCKED`, FIFO cũ bị xóa, owner xe lỗi vẫn giữ;
- MQTT LWT tại node nằm trên tuyến xe kia: xe kia đổi sang đường vòng;
- lỗi tại một node của thành phần mutex nối trực tiếp: tuyến qua node mutex còn lại được replan sang đường vòng.

`tests/two-robot-recovery.integration.js` PASS với Core mới: LWT vẫn giữ M2; telemetry đơn lẻ không nhả; chỉ RFID trạm và diagnostic dừng hợp lệ mới tự trả quyền; lỗi chuyển động không tự reset.

Kết quả trên là logic/fixture loopback, không chứng minh khoảng chiếm chỗ, quãng dừng hoặc khả năng xe thật đi qua đường vòng. Nếu footprint chặn mọi đường, hệ thống cố ý giữ `BLOCKED/WAIT`; không thu hồi owner của xe lỗi để tăng thông lượng.
