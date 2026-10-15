# ADR-033 — Trạm quay đầu ra cửa, tiến xuất phát

15/10/2026: [ADR-056](ADR-056-robot01-reverse-home-request.md) thay riêng cách vào S1 cho xe01 có capability mới: xoay tại GS1 theo line nối dài đã được người dùng xác nhận rồi lùi vào S1, không xoay tại S1. Các mô tả không có line đối diện GS1 bên dưới là khảo sát cũ; xe02/S2 và tương thích firmware cũ giữ thiết kế hiện tại.

14/10/2026: [ADR-053](ADR-053-station-completion-rfid-anchor.md) thay điều kiện rfidAgeMs≤250 khi hoàn thành đỗ bằng RFID đã đọc trong phiên cùng diagnostic đỗ còn mới. Ngưỡng250ms vẫn giữ riêng cho phục hồi lỗi/mutex. Các ghi nhận cũ bên dưới là lịch sử trước sửa.

Ngày 10/10/2026. Trạng thái: IMPLEMENTED; thay riêng quy tắc S1/S2 của ADR-032. Chưa nạp/kiểm chứng chuyển động trên xe thật.

## Nguồn và quyết định

`SOURCE`: người dùng sửa rõ quy ước: hướng vào trong dành cho kho khi xử lý hàng; khi về S1/S2 phải quay đầu ra ngoài để lộ trình mới đi tiến thẳng. Yêu cầu mới nhất thay câu trả lời gộp kho/trạm trước đó; không phục hồi bộ chọn hướng thủ công.

Kho K1–K6 giữ tiến vào/lùi ra. Trạm S1/S2 tiến vào, dừng khi đọc RFID, quay phải tại trạm tới line hướng ra GS rồi dừng; S1 đầu ↓ S tới GS1, S2 đầu ↑ N tới GS2. Lượt mới xuất trạm bằng Forward. Không quay ở GS để lùi vào S: map GS không có nhánh line đối diện cửa trạm để đầu đọc phía trước bắt hướng đó.

`OBSERVED`: S là nhánh cụt có một cạnh tới GS. Firmware yêu cầu rời line (`000`) rồi bắt lại line centered sau thời gian quay tối thiểu; giữ timeout 2500 ms và chốt cấu hình/mạng/vật cản. RFID dwell vẫn 1000 ms trong bản trial. Giữ navigator active suốt thao tác đỗ, không cho GOTO mới ghi đè. Khi quay heading UNKNOWN; chỉ sau khi bắt line xong mới gán hướng trạm. Quay lỗi không báo stationReady hoặc tự gán lại hướng ngoài.

## Hợp đồng hoàn thành

Diagnostic thêm `stationDockingSupported:true` và `stationReady`. Telemetry pipe vẫn chứng minh vị trí RFID; ACK chỉ xác nhận nhận lệnh. Với firmware hỗ trợ, Core giữ task WORKING/CHARGING_EN_ROUTE ở S cho tới diagnostic mới đúng node, hướng ra cửa, `AGV_IDLE/NONE/STOP`, duty 0/0, taskActive=false, tuổi RFID ≤250 ms. GOTO mới vô hiệu mẫu ready cũ. Thiếu capability sau khi đã bắt đầu lượt về không được bỏ chốt đang chờ. Firmware cũ không có capability giữ hành vi cũ, cần nạp bản mới để áp dụng thao tác quay.

Không thêm cảm biến nhận biết hàng trong kho: IR hàng hiện có đo hàng trên xe; lựa chọn lấy hàng/đỗ kho vẫn theo nhiệm vụ hiện tại. Hướng mặc định lúc đặt xe ở trạm là quy ước đặt đầu ra cửa, không phải hướng tuyệt đối đo bằng IR.

## Kiểm chứng

`VERIFIED`: native 96/96 PASS; ca S1/S2 tiến vào, navigator còn active khi RFID tới trạm, dừng chờ rồi quay/bắt line, chỉ sau đó mở lượt mới Forward; timeout quay dừng và không báo oriented.

`OPEN`: nạp ảnh xe 01 và đo góc quay/bắt line tại S1/S2 trên mô hình thật. Test host/mô phỏng không chứng minh robot quay đúng 180° hoặc hết lố RFID GS2.

VERIFIED: Core unit/MQTT fixture .qa/heading-test-7n6u0V PASS; mẫu hướng vào hoặc RFID cũ không hoàn thành. Audit Dijkstra/firmware 650 cặp PASS. Build ESP32 xe 01 PASS, RAM 55220/327680 byte, Flash 947353/1310720 byte; chưa nạp. Core vận hành đã cập nhật cùng map/runtime/counter và robot01, không task/queue/mutex đang chạy; giữ lỗi và uncertain_path GS1 để đối soát, không reset hoặc phát GOTO. Sau restart ca CLOSING, Core/MQTT ready; command reserved_through 55296, task 38912, không rewind. Artifact .qa/live-station-heading-update.

Core cuối và fixture .qa/heading-test-2Ghv8D PASS: lệnh GOTO trực tiếp từ dashboard trong pha đỗ trả 409, không phát command mới; đã thay binary Core vận hành bằng đúng ảnh đã kiểm thử.

Đối soát cuối: Core/MQTT ready=true, robot01/CLOSING, pending=0; counter tiếp tục command 56320 và task 39936 sau lần restart bản cuối, không rewind. Firmware 01 chưa upload.
