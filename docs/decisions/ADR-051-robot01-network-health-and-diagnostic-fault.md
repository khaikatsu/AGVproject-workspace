# ADR-051 — Sửa phép đo tuổi kết nối và đồng bộ lỗi MCU xe 01

Ngày: 14/10/2026. Quyết định theo yêu cầu kiểm tra đường truyền của người dùng. Firmware mới đã xác nhận qua diagnostic; Core đã áp dụng lúc18:48 theo [ADR-052](ADR-052-core-debug-heap-heartbeat.md).

`SOURCE`: sau khi nạp bản encoder, server gửi lệnh nhưng xe01 không chạy, web báo lỗi kết nối. Người dùng xác nhận ESP32 lấy nguồn từ 5V L298N; chưa cung cấp Serial tại thời điểm lỗi.

`OBSERVED`: một Core và gateway/broker đang hoạt động đúng cổng, IP hotspot192.168.137.1 khớp cấu hình, robot MQTT ở192.168.137.110. Health đều true. Trace thụ động45s có44telemetry/44diagnostic/209odometry/48heartbeat, boot916273676 và connection session14 ổn định, firmware `20261014-robot01-encoder`, motor STOP/PWM0/0. MCU SAFE_STOP/MOTION_LOCKED trong khi Core IDLE. Ping15/15, RTT5–153ms. Không có lệnh/ACK trong lượt trace, không có bằng chứng MCU reboot trong45s; không suy ra nguồn5V tốt khi motor chạy từ điện áp pin khoảng7,31V.

## Quyết định

- `MQTTModule::isConnected()` trước sửa lấy millis trước khi đọc WiFi và timestamp atomic từ network worker. Worker có thể cập nhật timestamp mới hơn now đã chụp; phép trừ uint32 tràn và kết nối khỏe bị đọc thành stale. Kiểm thử tái hiện độc lập chứng minh lỗi này, chưa chứng minh nó là nguyên nhân duy nhất của mọi lần mất kết nối trên xe.
- Dùng `NetworkHealth::connected()` chụp flags/timestamps xong mới lấy millis; giữ timeout worker2s/heartbeat3s, kiểm tra WiFi/ready/restart và xử lý wrap. Không tăng timeout, đổi PWM/pin/motor, IP/SSID hoặc tự reset bo để che lỗi.
- Diagnostic01 thêm networkServiceAgeMs/coreHeartbeatAgeMs/mqttClientState/wifiRssi để lần thử tiếp có dữ kiện. Payload telemetry/command/ACK/mutex giữ nguyên; boot/connection/Core session theo ADR-050.
- Core mode robot01 gắn và kiểm tra đúng phiên diagnostic encoder trước xử lý motion. Diagnostic mới đúng node, WiFi/MQTT khỏe, báo SAFE_STOP/ERROR với error khác NONE phải đưa Core sang ERROR qua luồng reportError hiện có nếu Core chưa ERROR. Điều này bù khi gói `error` QoS0 bị mất; không tự xóa lỗi, nhả owner mutex hoặc cho robot chạy. Chỉ cancel waiting/gián đoạn task/dừng theo cơ chế lỗi hiện có. Các phiên cũ và firmware không có capability giữ chốt/tương thích đã quy định.
- Build/test vào thư mục riêng. Agent không upload khi COM6 không mở được; trace tiếp theo xác nhận người vận hành đã nạp firmwareBuild clock-fix. Core bản cuối áp đúng mode robot01 sau kiểm tra xe dừng, xem ADR-052; không rewind counter hoặc xóa dữ liệu.

## Kiểm chứng và việc còn mở

Unit clock tái hiện before/after, timeout thật và wrap; firmware/native regression01/02; unit Core và fixture encoder có tình huống mất riêng topic error nhưng diagnostic lỗi còn mới. [Báo cáo](../testing-network-robot01-2026-10-14.md) ghi kết quả thực tế và artifact.

`OPEN`: cần xác nhận rail5V/3,3V khi tải motor thay đổi và kiểm chứng đường thật sau phục hồi lỗi/RFID hợp lệ. Đã có RSSI/tuổi heartbeat bản mới và đã tái hiện, sửa nguyên nhân GDB làm Core chậm ở ADR-052. Khung45s xe dừng chưa nghiệm thu nguồn và mạng khi chạy.
