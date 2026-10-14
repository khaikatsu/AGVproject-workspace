# ADR-037 — Vật cản trên cạnh và một form đơn hàng mô phỏng

Ngày 10/10/2026. Trạng thái: triển khai theo REQ-SIM-08.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn mở lại web, chỉ chạy khi vào xem; đặt vật cản ở mọi cạnh để quan sát server điều phối; bỏ ghi chú dài và form đơn hàng chỉ có một kho lấy, giữ form thêm được nhiều kho lấy/giao, ghi rõ nơi lấy và nơi giao.

`OBSERVED`: dashboard đã có form nhiều kho của gateway, nhưng panel mô phỏng còn một form đơn khác chỉ nhận một kho lấy. Core chỉ hỗ trợ khóa node. Phiên mô phỏng đã có lease tab theo ADR-020.

## Quyết định

- Chỉ giữ form `orderTaskForm` của gateway, dùng `/api/orders` với `pickups`/`deliveries`. Nhãn **Kho lấy hàng** và **Kho giao hàng**, từng kiện hiện `kho lấy → kho giao`; giữ số kiện và chức năng thêm nhiều dòng ở cả hai phía. API đơn cũ của simulator vẫn phục vụ kiểm thử tương thích, không hiển thị thành form thứ hai.
- Mỗi đoạn vật lý có thể đặt/gỡ vật cản qua danh sách hoặc click cạnh trên bản đồ. Chặn cả hai hướng của đoạn đó; giữ nguyên topology, trọng số và mutex. Dijkstra Core bỏ qua đoạn bị chặn; bộ ước tính phân xe của gateway cũng bỏ qua cùng đoạn.
- Map JSON thêm `blocked_edges` tùy chọn, mặc định rỗng. WebCommands nhận `sim_edge_obstacle`/`sim_replan` chỉ khi Core chạy với `AGV_SIMULATION=1`. HTTP thao tác chỉ có tại server mô phỏng, không thêm endpoint vận hành.
- Xe đứng tại RFID nhận tuyến mới ngay. Xe đang trên cạnh giữ cạnh đang đi và đợi RFID kế tiếp để thay tuyến; vật cản nằm trên chính cạnh đang đi giữ xe tại vị trí nội suy đến khi gỡ. Không đổi hướng hoặc đặt lại vị trí giữa cạnh. Khi không có đường khác, task giữ BLOCKED; gỡ vật cản tính lại đường cho cùng task.
- Build riêng `build/simulation/agv_core.exe`; Core/broker/counter/map/DB vẫn riêng mỗi phiên. Giữ lease tab và khoảng dừng 20 giây của ADR-020. Ghi chú và thao tác ít dùng nằm trong mục thu gọn; form đơn hàng đặt trước điều khiển thử xe.

## Kiểm chứng và giới hạn

Core unit kiểm tra chặn hai hướng, đường vòng, roundtrip map và lệnh bị từ chối ngoài simulation mode. `tests/simulation.edges.js` dùng Core/broker/DB riêng kiểm tra đủ 31 đoạn, hết đường/gỡ cùng task, tuyến vòng, dừng giữa cạnh và đổi tuyến tại RFID khi chặn đoạn phía trước. `tests/simulation.edges.browser.js` kiểm tra chọn cạnh bằng click, dấu vật cản, một form nhiều kho, giao đủ bằng hai xe và desktop/mobile. Kết quả từng lần nằm trong `.qa/edge-obstacles` và `.qa/simulation-edge-browser`.

Khi kiểm tra form nhiều kho, phát hiện WebSocket delta giữ tham chiếu tới đối tượng order bị scheduler sửa, nên bỏ sót tiến độ. Gateway sao chép orders tại mỗi snapshot; test hồi quy xác nhận chuyển trạng thái được gửi qua delta. Simulator cũng nhận `RESET_FAULT` hiện có của Core; ACK không tự xóa lỗi hoặc đặt lại vị trí, phục hồi vẫn qua xác nhận RFID của runtime. Core unit PASS; gateway/orders 18/18 PASS; robot unit, vật cản 31 đoạn, browser đơn 2 xe, browser pin/phục hồi và on-view PASS. Artifact tiêu biểu `.qa/edge-obstacles/run-WJwClQ`, `.qa/simulation-edge-browser/run-qazFix`, `.qa/run-7iDdoF`.

`OPEN`: đây là mô phỏng S1 xe điểm; chưa mô phỏng kích thước vật cản/xe, cảm biến phát hiện hoặc quãng phanh thật. ACK vẫn chỉ là biên nhận. Không đổi payload MQTT hoặc firmware robot.
