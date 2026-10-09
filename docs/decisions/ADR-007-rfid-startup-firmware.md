# ADR-007: Firmware RC522 và vị trí khởi động đã xác minh

Ngày: 29/09/2026. Trạng thái: **ACCEPTED/IMPLEMENTED phần mềm**; chưa nghiệm thu board thật.

## Nguồn

Sau lần sửa server/web, người dùng yêu cầu sửa firmware cho khớp server. Quyết định phần cứng đã có: ESP32 DevKit/Arduino IDE/PubSubClient, RC522 với SCK18/MISO19/MOSI23/SS21/RST22/nguồn3.3V, học UID qua Serial; trạm riêng 01→S1/02→S2. Yêu cầu mới cho phép triển khai phần RC522 đã hoãn ở ADR-006.

## Quyết định

Giữ một sketch, thêm MFRC522/SPI/Preferences. Mỗi node lưu UID4/7/10byte trong NVS; UID không trùng node, lỗi ghi không báo thành công. Mỗi boot không đọc vị trí lưu cũ: phải có thẻ hợp lệ mới đặt positionKnown. Đọc thẻ cập nhật telemetry, không tự hoàn thành GOTO chỉ vì ACK.

Giữ topic và format v0, thêm giá trị vị trí UNKNOWN. Core nhận heartbeat chưa định vị thành UNLOCALIZED, position_valid=false, không gán node. Mất xác nhận RFID trong kế hoạch đang chạy chuyển ERROR/gián đoạn task và giữ uncertain_path/mutex. Thẻ hợp lệ không tự xóa ERROR. Web hiển thị Chờ quét RFID và ẩn marker chưa xác minh.

WAIT/STOP vẫn nhận khi chưa định vị; GOTO mới cần vị trí hợp lệ, CHARGE_START mới cần RFID đúng trạm riêng. Receiver test có trạng thái nhận route/sạc, đến RFID đích thì chờ command mới; duplicate không kích hoạt lại route. Không có motor/relay; charged là thao tác test có kiểm tra điều kiện. Simulation bật rõ ràng, mặc định tắt. Reconnect không tự khôi phục route, yêu cầu xác minh RFID mới, giữ cache/quyền vùng trong RAM. WUPA đọc lại thẻ HALT còn đứng yên ở trạm; refresh chỉ khi cần, không flood thẻ chưa học.

## Kiểm chứng và giới hạn

Compile FQBN esp32:esp32:esp32 với board package3.3.11 và MFRC5221.4.12. Host probe biên dịch trực tiếp .ino với I/O adapter; MQTT integration và hai receiver chạy đầy đủ ca với core/gateway/broker thật riêng. Chưa flash hoặc đo reader/WiFi/động học/sạc điện. Dedup qua MCU reboot, session ID, bộ đo pin và điều khiển motor/charger còn OPEN.
