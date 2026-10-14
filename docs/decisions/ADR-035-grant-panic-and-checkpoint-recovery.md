# ADR-035 — Sửa crash grant và phục hồi checkpoint bị khóa

Đính chính khi khôi phục 10/10/2026 theo [ADR-041](ADR-041-restore-morning-robot01-firmware.md): PWM245 ghi bên dưới không khớp binary thực tế đã nạp. Build PWM220/RFID20ms tạo SHA-256 trùng hoàn toàn ảnh `23A181...`; cấu hình thực tế là PWM220, trim trái-40. Các đoạn dưới giữ hồ sơ trước đính chính.

Ngày 10/10/2026. IMPLEMENTED; đã nạp firmware xe01 và cập nhật Core/gateway.

SOURCE: người dùng báo lỗi cần xác minh RFID lặp ở GS1, rồi xe bật nhưng không kết nối, yêu cầu sửa nguyên nhân.

OBSERVED: ESP32 báo resetReason=PANIC sau task K5 interrupted MQTT LWT, mất hướng tuyến tại GS1; RFID vẫn mới, line000. Callback grant ArduinoJson zero-copy zone trỏ vào RX buffer; PubSubClient mutex_ack ghi đè buffer trước strcpy vào zone8byte. Host link thư viện PubSubClient2.8 thật tái hiện M2 biến thành chuỗi dài hơn capacity. Chưa có UART backtrace của lần PANIC để loại trừ mọi nguyên nhân reset khác.

OBSERVED: Core còn bị Windows error5 khi thay checkpoint trong OneDrive. storage_healthy=false làm ngừng heartbeat/bỏ MQTT input vĩnh viễn tới khi restart; gateway vẫn core_ready=true dù lỗi lưu. UART xe Got IP, MQTT Connected/awaiting Core heartbeat, RFID S1 đọc lặp xác nhận điều này.

IMPLEMENTED: CoreProtocol decodeGrant parse const stream, copy/validate grant vào cấu trúc riêng trước ACK, không còn pointer RX dùng sau publish. Payload/token/ACK/dedup không đổi. Diagnostic firmwareBuild=20261010-grant-buffer-fix. Giữ PWM245, RFID20ms, hướng kho/trạm và node-order.

IMPLEMENTED: atomic Windows retry các lỗi share/lock/access với ngân sách chung2s; lỗi thật giữ file cũ và dừng outbound. Core thử ghi checkpoint lại mỗi2s; chỉ sau save thành công mới phục hồi network/heartbeat, không phát lại outbound/tuyến gián đoạn. Gateway storage=false báo core_ready=false, command503, gián đoạn đơn cần đối soát và không dispatch đơn khi checkpoint lỗi.

VERIFIED: native108/108; thư viện PubSubClient thật1.000 callback/đảo field, dữ liệu grant không đổi. Core unit lock650ms và lock vượt deadline kiểm tra giữ file cũ/lưu lại sau mở khóa. Fixture .qa/node-order-test-pMq4WP gây storage=false, command503 rồi mở khóa phục hồi mà không tự GOTO; LEFT/RIGHT/mutex/stationReady và heading .qa/heading-test-ogQY2j PASS. Gateway/security/orders21/21 PASS. ESP32 build/nạp COM6 đúng MAC ec:c9:ff:ff:09:dc SUCCESS và hash flash verified. Đã đối chiếu firmwareBuild từ bo, resetReason POWERON, S1/IDLE/NONE/headingS/motorSTOP. Core/MQTT/storage ready, ca CLOSED/can_start=true. Log hệ thống ghi tự trả M2 sau RFID/diagnostic mới tại S1 và tự xóa ERROR; không manual confirm-clear, không phát GOTO hoặc start_shift.

Artifact: .qa/live-grant-panic-update lưu binary cũ, dữ liệu/counter/runtime/shift backup, before snapshot, diagnostic-after.jsonl và log. .qa/mqtt-grant-buffer-verification.txt lưu tái hiện pointer RX. .qa/robot01-panic-audit/serial.log lưu UART. Counter/map/runtime/DB giữ đúng dữ liệu, không rewind hoặc xóa.

OPEN: đo quay/bắt line và xác minh không PANIC khi cấp grant trên xe chạy thật. Test host/mô phỏng và online tại S1 không chứng minh đã hết mọi lỗi LINE_LOST hoặc phần cứng. Không tự xóa chốt khi chưa biết heading hoặc thân xe chưa rời mutex.
Kiểm tra cuối: ngân sách retry Windows2s dùng chung cho mở và thay file để tránh hai lần2s nối nhau vượt Core watchdog3s; build/Core unit bản cuối PASS. Nạp firmware SHA25623A181746E65498AC9447E7D0243778600A49EC10678EBFE22DA0C1571D30CBF, mã build đã đọc từ bo. Snapshot và log cuối tại .qa/live-grant-panic-final.

Đối soát cuối sau người dùng quét lại S1: .qa/live-grant-panic-final/diagnostic-final.jsonl gồm4 mẫu AGV_IDLE/NONE/S1/headingS/build đúng, RFID tuổi161/26/192/82ms, motorSTOP. Core/MQTT/storage ready, caCLOSED/can_start=true, không queue/path/uncertain hoặc mutexowner. Log tự RESET_FAULT một lần sau đổi phiên Core và tự xóa ERROR; không phát GOTO/start_shift. Binary vận hành khớp binary Core đã test; counter command 60416, task 44032 tiếp nối, không rewind.
