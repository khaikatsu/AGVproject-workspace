# Kiểm tra đường truyền xe 01 — 14/10/2026

`SOURCE`: xe nhận lệnh nhưng không chạy, web báo mất kết nối sau bản encoder. Nguồn ESP32 theo người dùng: 5V L298N. [ADR-051](decisions/ADR-051-robot01-network-health-and-diagnostic-fault.md).

## Quan sát vận hành, chỉ đọc

- Core PID12504 tại build/agv_core.exe, gateway PID19960 cổng3000, broker PID4708 cổng1883. Một Core, MQTT socket robot192.168.137.110 ↔ hotspot192.168.137.1; không có bất khớp IP/port trong lượt kiểm tra.
- Health: agv_connected/core_ready/protocol_supported/mqtt_connected/storage_healthy đều true.
- Trace17:58:37–17:59:22 giờ Việt Nam,45s: telemetry44, diagnostic44, odometry209, Core heartbeat48. Gap lớn nhất lần lượt2557/2760/2034/2694ms. Chỉ một Core session, MCU boot916273676, MQTT session14; resetReason POWERON giữ nguyên, không chứng minh brownout/reboot trong lượt này.
- Ping15/15, mất0gói, RTT min5/max153/avg49ms. Phép đo khi xe dừng; không xác nhận mạng/nguồn lúc motor chạy.
- Firmware thực tế `20261014-robot01-encoder`, motionEnabled true/bench, RFID S1 mới/linePattern3, packADC khoảng7,31V. MCU SAFE_STOP, error MOTION_LOCKED_PENDING_ZONE_CLEARANCE_CALIBRATION, motor STOP/PWM0/0; Core snapshot lại IDLE/không error. Đây là lỗi chốt MCU chưa được Core thể hiện đầy đủ, không phải bằng chứng motor đã hỏng.
- Trước trace có log LWT/NETWORK_UNAVAILABLE/mất ACK và auto-reset. Trong45s không có command/ACK hoặc error event mới. Không kết luận tỷ lệ ACK từ khoảng không có lệnh.
- Artifact: `.qa/encoder-network-live/health.json`, state.json, report.json, events.jsonl, states.json và ping.txt. Probe chỉ subscribe, không publish command/reset/xóa lỗi.

## Sửa source và kiểm thử

Đã tái hiện lỗi lấy now trước khi đọc timestamp worker: millis1000 nhưng worker timestamp1001 làm uint32 underflow, kết nối khỏe bị đọc thành stale. Sửa chụp status trước clock bằng NetworkHealth.h; timeout2s/3s và mất WiFi/ready/heartbeat/restart vẫn trả false, wrap vẫn đúng. `tests/network-clock-host.cpp` PASS.

Core test01 bổ sung đồng bộ lỗi từ diagnostic đúng phiên, đề phòng gói error QoS0 mất. Không tự clear-error/mở motor; reportError hiện có gián đoạn task và giữ owner mutex. Unit encoder Core PASS cả unbound diagnostic không tác động, bound fault chốt ERROR và healthy riêng không tự xóa lỗi qua helper.

Build firmware01/02 PASS; native52/52 PASS. Core/unit PASS; fixture bản cuối `.qa/encoder-test-2znWzA` PASS với Edge desktop/mobile và mất topic error. Diagnostic01 mới có networkServiceAgeMs/coreHeartbeatAgeMs/mqttClientState/wifiRssi. FirmwareBuild `20261014-robot01-network-clock-fix` đã thấy trên bo ở trace18:36/18:48; agent không upload lượt này.

## Chu kỳ telemetry treo và sửa debugger

18:36:12–18:36:57: telemetry38/diagnostic39/odometry165/heartbeat21, max gap3306/3270/2843/5575ms. Boot1015295379 ổn định, session6→11. RSSI−44…−38, worker age12…16ms. ACK tới broker nhanh nhưng Core vẫn retry. Trace nằm report.json/events.jsonl/states.json ở gốc `.qa/encoder-network-live`; probe đã đổi sang thư mục từng lượt. Trace17:58 trước đó không còn event riêng, chỉ còn state/health/ping và số đo ghi ở trên.

Tái hiện GDB debug heap cùng snapshot400task: trực tiếp16,50ms, GDB mặc định314,29ms, tắt debug heap16,09ms; chỉ đặt environment cho inferior14,23ms. Đã thêm `_NO_DEBUG_HEAP=1` vào F5. Fixture1000task/GDB/encoder5Hz PASS: heartbeat max278ms, MQTT→WS encoder max197ms,20giây, giữ task. [ADR-052](decisions/ADR-052-core-debug-heap-heartbeat.md).

Đã áp Core SHA256957ABB39F4B1A1CB9276658CAD31F8C23A7B60A67F43935CFC50C1E61EC82A44 lúc18:48:23 sau xác nhận ca CLOSED/xe STOP/không task/mutex; chạy nền PID28972 mode robot01 ngoài debugger. Backup `.qa/core-network-apply-20261014`. Counter/map/runtime/shift/DB giữ vòng đời hiện có, không rewind/xóa dữ liệu hoặc gửi thao tác chạy/reset/xóa lỗi.

Trace18:48:37–18:49:22 tại `.qa/encoder-network-live/run-2026-10-14T11-48-37-232Z`: telemetry44/diag44/odometry218/heartbeat221, max gap1047/1078/307/269ms. Một boot356294169/connection55, không offline/error event/command hoặc ACK mới. MCU vẫn SAFE_STOP/NETWORK_UNAVAILABLE, RFID UNKNOWN; truyền thông ổn định chưa chứng minh đã phục hồi chốt lỗi/chuyển động.

## Giới hạn

Lỗi clock và chi phí GDB đã chứng minh độc lập; sau áp Core không lặp mất kết nối trong45giây xe dừng. Nguồn5V L298N cần đối chiếu khi tải thay đổi; ADC pack không đo rail ESP32. Agent không upload/mở Serial. Cần RFID hợp lệ/phục hồi lỗi để thử xe; chưa nghiệm thu motor/nguồn/mạng khi chạy.
