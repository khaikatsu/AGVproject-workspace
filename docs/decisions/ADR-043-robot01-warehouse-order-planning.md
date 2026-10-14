# ADR-043 — Mặc định xe 01 và hiện tuyến lấy/giao trong chế độ test

Ngày 10/10/2026. Source và kiểm thử hoàn tất; chưa khởi động lại gateway vận hành.

Cập nhật sau ảnh web10/10/2026: gateway đã được người dùng khởi động lại (PID22124 thay22688); backend nạp source mới. Core khi đó không còn chạy, được khởi động lại đúng binary ghim hồi sáng theo ADR-040 (PID26112). Health hiện Core/MQTT/storage ready, xe01 S1/IDLE, caCLOSED. Chưa tạo đơn mới hoặc mở ca để thử trên xe thật; các đơn cũ vẫn giữ kết quả/đối soát theo phiên, không phát lại.

`SOURCE`: người dùng báo giao kho→kho chưa chọn xe/tuyến, yêu cầu kiểm tra chọn xe gần nhất và tạm mặc định xe01 trong chế độ test. Đã giải thích thay đổi thuộc gateway/dashboard3000, không build/thay Core hồi sáng.

`OBSERVED`: orders.js đã xếp hạng xe sẵn sàng theo trọng số đường xe→kho lấy + kho lấy→kho giao, tie-break ID. Với một kiện, chặng hàng giống nhau nên tương đương chọn gần kho lấy. Trial ID đã giới hạn xe tham gia. Hai chặng dùng direct_goto/task_id Core; giao sau task lấy completed, xe IDLE/RFID đúng kho và10s bốc mô phỏng.

`OBSERVED`: gateway mode robot01, xe01 M5/ERROR/LINE_LOST. Hai đơnK6→K5 còn queued/robotnull; đơn cũ needs_attention gắn01 cũng giữ đặt chỗ để tránh nhận chuyến khi hàng chưa đối soát. Scheduler cũ lặng lẽ chờ khi không có xe đủ điều kiện, UI không hiện lý do/tuyến.

## Thay đổi

- Mode robot01 mặc định01 ngay khi queued; robot02 chọn02, thường giữ nearest. Không giao xe khác khi xe được chọn lỗi/offline/bận hoặc giữ kiện đối soát.
- Order thêm selection_policy; queued item thêm waiting_reason và route_plan gồm robot_start, pickup/delivery path/cost. Không có đường trả path[]/costnull. Dijkstra tôn trọng hướng, trọng số, node/cạnh chặn; UI hiện hai tuyến dự kiến và lý do chờ.
- Route preview không xác nhận đã phát lệnh/Core cấp mutex; Core vẫn tính tuyến thực tế. Không đổi Core/firmware/MQTT/counter hoặc tự clear lỗi/quyền vùng/hàng cũ.
- Gateway restart chỉ giữ active khi toàn bộ items queued và không task_id (chưa phát lệnh). Đơn đã dispatching/to_pickup/service/to_dropoff hoặc có task_id vẫn needs_attention. Đơn giữ lại vẫn phải khớp Core session/RUNNING trước dispatch.

## Kiểm chứng và giới hạn

`VERIFIED`: orders/gateway/security25/25 PASS; thêm xe02 gần hơn theo weight dù nhiều node, mode01 thiếu trial ID, xe01 LINE_LOST không dispatch, preview hai tuyến, queued restart và đúng01 sau khỏe. Ca bốc/dỡ/task completion/offline/restart trong chuyến vẫn PASS.

`VERIFIED`: Edge fixture `tests/orders-selection.browser.js` PASS: mặc định01, LINE_LOST, tuyến S1→K1 và K1→K2, không lệnh di chuyển, mobile390px; ảnh `.qa/order-selection-browser-95i4Vi/robot01-orders-mobile.png`.

`OPEN`: restart gateway bị duyệt tự động chặn `blocked by policy`, không nêu lý do chi tiết. Không tiến trình nào bị dừng; code backend mới chưa nạp vào gateway sống. Người dùng khởi động lại gateway3000 để áp dụng, giữ Core hồi sáng đang chạy. Chưa thử chuyển động/lấy trả hàng vật lý;10s bốc/dỡ chỉ là mô phỏng.
