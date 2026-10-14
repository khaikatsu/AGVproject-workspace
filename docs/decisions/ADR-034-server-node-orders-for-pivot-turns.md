# ADR-034 — Quét RFID, chờ server, xoay tại chỗ rồi đi tiếp

Ngày 10/10/2026. Trạng thái: IMPLEMENTED trong mã; kiểm chứng phần mềm đang chạy, chưa nạp/thử xe thật.

## Nguồn và phạm vi

`SOURCE`: người dùng đính chính yêu cầu xoay tại chỗ áp dụng cho lệnh quẹo trái/phải tại các node trên tuyến, không chỉ quay 180° tại S1/S2. Xe vừa quét thẻ phải dừng, báo server, nhận lệnh hướng rồi xoay tại chỗ/bắt lại line trước khi đi tiếp. Giữ quy tắc trạm hướng ra và kho tiến vào/lùi ra ở ADR-033.

`OBSERVED`: trước sửa, Core gửi toàn tuyến GOTO; firmware tự lập các lượt rẽ rồi chạy sau dwell/grant, không có command hướng riêng tại từng RFID. Driver hiện có PivotLeft/PivotRight với hai duty khác dấu. Quyết định mới thay cách kích hoạt cục bộ này bằng bước chờ lệnh server.

## Luồng mới

GOTO vẫn mang path/target/cmd_id. NCKHAGV mới luôn dừng chờ NODE_ORDER tại đầu tuyến và từng thẻ trung gian; diagnostic gửi nodeOrdersSupported, awaitingNodeOrder, routeCommandId, pathIndex, nextNode và stationTurnRequired. Core yêu cầu telemetry đúng node, diagnostic mới có bằng chứng từng đọc RFID trong phiên hiện tại, AGV_WAITING/NONE/STOP, duty 0/0, task active và route_cmd_id/step khớp tuyến đã giao. Hướng đầu xe phải đúng node và từ nguồn route/bay_default. Không yêu cầu tuổi RFID ≤250 ms để nhận lệnh khi xe đã dừng chờ: người vận hành quét rồi chờ bấm GOTO vẫn được đi. Giới hạn 250 ms cho phục hồi lỗi/mutex và stationReady không đổi.

Core quyết định STRAIGHT/LEFT/RIGHT/REVERSE/TURN_AROUND_RIGHT hoặc STATION_TURN_RIGHT từ map và hướng đầu xe; gửi NODE_ORDER với node, next_node, route_cmd_id, path_index, maneuver và turn_crossings. Firmware đối chiếu với tuyến/map cục bộ; không cho lệnh khác node/chặng/hướng chạy motor. Nếu chưa có lệnh thì giữ STOP; hết thời hạn đoạn báo lỗi. Lệnh không bỏ qua mutex, chốt mạng/nguồn/cảm biến hoặc RFID dwell.

LEFT/RIGHT điều khiển hai bánh ngược chiều, PWM quay xe 01 vẫn 245. Trong pha quay, line 000 là rời line cũ để bắt nhánh mới; chưa áp lỗi LINE_LOST bám thẳng ở pha này. Chỉ sau 000 và centered đúng thời gian/số nhánh mới chuyển sang bám line. Không bắt được trong 2500 ms thì dừng TURN_TIMEOUT. Khi xoay heading UNKNOWN; sau bắt line cập nhật hướng mới. Không tăng PWM hoặc thời gian quay theo suy đoán.

Retry dùng nguyên cmd_id/payload. Cùng node/chặng không cấp ID mới cho các diagnostic lặp. MCU dedup so cả node/route/step/maneuver; ACK chỉ nhận lệnh, không phải đã quay xong. Firmware cũ không yêu cầu NODE_ORDER và giữ luồng cũ; cần cập nhật Core rồi nạp firmware mới đồng bộ. Không tạo API web bấm xoay tự do.

## Kiểm chứng và giới hạn

`VERIFIED`: native 105/105 PASS; parser/correlation, dừng chờ tại mỗi thẻ, LEFT/RIGHT pivot rồi bắt line, không bắt lại junction cũ, không vượt mutex, lệnh cũ/sai hướng bị từ chối và mất mạng/timeout dừng. Audit server/MCU 650 cặp tuyến PASS, đối chiếu hướng và số nhánh. Motor adapter thật với Arduino stub PASS: hai bánh duty khác dấu ở PWM 245, LEFT/RIGHT đảo dấu, STOP đưa hai bridge về 0.

`VERIFIED`: Core unit/MQTT fixture `.qa/node-order-test-efsqaA` và heading `.qa/heading-test-pUOy1X` PASS; build ESP32 01 PASS. Người dùng đã nạp ảnh NODE_ORDER (quan sát capability=true trên bo), chưa xác nhận chuyển động. Sổ REQ-20 ghi lần sửa bỏ chốt tuổi RFID quá chặt và kiểm tra đồng bộ Core.

`OPEN`: nạp bản bỏ chốt tuổi RFID cho NODE_ORDER, đo lực quay, hướng trái/phải và bắt line trên xe thật. Host/mô phỏng không chứng minh hết mất line hay đúng góc quay vật lý.

Đối soát REQ-20: native 105/105 và RFID adapter đọc cùng thẻ 50 lần PASS; Core unit/MQTT .qa/node-order-test-Ylkqw6 và heading .qa/heading-test-9PO1Fr PASS với quét rồi chờ 5 giây vẫn nhận lệnh. ESP32 01 build PASS (RAM 55996, Flash 949405 byte), firmware SHA256 D29654F8FE8FD39367167ACB7F2B05D43923ECE4460127CDCA1A83190E16A563. Core cuối đã cập nhật đúng binary test khi không có task/path/owner mutex, giữ counter/map/runtime/DB và robot01; backup/snapshot/log tại .qa/live-node-order-update. Người dùng xác nhận S1 đầu ra GS1; diagnostic trên bo xác nhận đọc lặp, AGV_IDLE/NONE/STOP/heading S/line010. Không phát GOTO hoặc nghiệm thu góc quay thật.
