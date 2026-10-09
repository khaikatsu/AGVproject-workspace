# Kết quả kiểm tra web/server ngày 29/09/2026

## Thiết kế lại dashboard 01/10/2026

Thay đổi `AGVwebserver/index.html`, `public/dashboard-layout.css`, `public/dashboard.js` và ca trình duyệt. Edge headless chạy với gateway/core giả lập và DB riêng: **PASS** `node tests/browser.integration.js`; ảnh tại `.qa/browser-test-LvQBTu/`. Đã xem ảnh Tổng quan desktop, Nhiệm vụ desktop, Bản đồ mobile vừa màn/phóng to và editor. Trình duyệt 390 px không tràn ngang toàn trang; bản đồ phóng to cuộn trong khung. Ca kiểm tra bao gồm tab, lệnh/task/history, snapshot/map editor, khóa/mở node trên một nút, ca làm việc, stale/reconnect, mất MQTT và ẩn xe chưa xác minh. Đây là kiểm thử trình duyệt với core giả, không phải kiểm thử điều khiển robot thật.

## Firmware RC522 đồng bộ ca — bản cuối

Yêu cầu mới cho phép cập nhật firmware sau lần sửa server/web. Sketch bỏ G1 preset, học UID/NVS và gửi UNKNOWN khi chưa định vị; core thêm UNLOCALIZED và giữ task/đường/mutex khi mất xác nhận RFID. SHA256 bản cuối: `E505F54EA35AE5461EAAF5ED8737039D328E15DE547000202D343F4ACAA77C76`.

| Kiểm tra | Kết quả / bằng chứng |
| --- | --- |
| Arduino CLI1.4.1, esp32:esp32:esp32, core3.3.11 | PASS; flash923708byte/70%, globals55264byte/16%; .qa/esp32-rfid-test-build |
| Thư viện | PubSubClient2.8, ArduinoJson7.4.3, MFRC5221.4.12; SPI/Preferences theo board package |
| Host sketch bản cuối | PASS; learn/collision/NVS failure/UID4-7-10byte, boot unknown, guard GOTO/CHARGE_START, ACK/dedup/retry/reconnect, WUPA xác minh lại thẻ HALT đứng yên, không flood thẻ lạ, đến đích chờ lệnh và duplicate không chạy lại route |
| Core unit | PASS; UNKNOWN→UNLOCALIZED→RFID hợp lệ; UNKNOWN giữa route gián đoạn task/ERROR và giữ uncertain_path/mutex; heartbeat không spam WAIT |
| Firmware MQTT | PASS; .qa/firmware-test-yR1t34: actual .ino receiver/UID/RFID với core/gateway/broker thật riêng, ACK không hoàn thành task, M5/M6 grant/release |
| Hai receiver trong ca | PASS; .qa/shift-test-02R4mc: chạy shift.integration.js --sketch, board ID01/02, boot UNKNOWN, xác minh S1/S2, task cuối/queue/tự về trạm, CHARGE_START/WAIT/STOP/ACK và mutex; reinit UID mapping vẫn phải nhận thẻ mới |
| Web | PASS; .qa/browser-test-FBfWqf: nhãn Chờ quét RFID, marker bị ẩn khi position_valid=false, ca/trạm/task gating/mobile |
| Hồi quy gateway/system | PASS7 ca gateway; system MQTT thật .qa/system-test-lbSVsj |

Adapter thay I/O RC522/NVS/WiFi, không thay parser hoặc logic sketch. Không có upload hoặc đo RC522/WiFi/motor/relay sạc trên ESP32 thật. Pin và charging_done vẫn test bằng tay; dedup qua MCU reboot, docking/sạc điện và vận hành vật lý còn OPEN. Các ghi nhận “firmware giữ nguyên” dưới đây thuộc lần sửa ca trước khi người dùng cho phép cập nhật RC522.

## Bổ sung ca và về trạm riêng

Đã build bản cuối bằng `scripts/build-core.ps1 -Tests`: **PASS**, gồm unit về ca, tuyến trạm/kho, queue tạm dừng, MQTT down, RFID sai, lỗi/offline và mutex chưa trả. Firmware không sửa; SHA256 sketch trước/sau đều `D6C7D6881B918F838218AEE5A603A11DAE6FDB31E2D4441CAC26197321A190EC`.

| Bộ kiểm tra | Kết quả / dữ liệu riêng |
| --- | --- |
| `shift.integration.js` | PASS; `.qa/shift-test-e6FbYw`: boot chờ vị trí thật, 01→S1/02→S2, task cuối xong trước về, queue giữ pending, ACK không đổi vị trí, đóng ca cần đủ hai xe, pin/lỗi và fresh RFID sau restart |
| `browser.integration.js` | PASS; `.qa/browser-test-030Ifl`: nút bắt đầu/kết thúc gửi đúng command, trạm riêng, task gate, vật cản vẫn điều khiển được, ẩn robot chưa xác minh, desktop/mobile; ảnh `shift-home-stations.png` đã xem |
| `gateway.test.js` | PASS 7 ca hồi quy REST/WS/SQLite |
| `system.integration.js` | PASS; `.qa/system-test-hsDAAt` |
| `map.integration.js` | PASS; `.qa/map-test-dSrDpq` |
| `startup.integration.js` | PASS; `.qa/startup-test-KE8s4b`: chạy trùng/cwd/restart/counter |
| `firmware.integration.js` | PASS; `.qa/firmware-test-bIHsVx`: sketch host đã có giao tiếp với core mới, MQTT giữ payload/topic cũ; không compile/upload thêm firmware |

Chỉ kiểm chứng phần mềm với broker/core/gateway thật và robot telemetry giả lập. Chưa có kết quả motor, docking, sạc điện hoặc RFID phần cứng. File SQLite/map/counter/shift_state dùng riêng trong `.qa`. Hướng dẫn [ca làm việc](17-shifts-and-home-charging.md).

## Các bộ kiểm tra trước cập nhật ca

Kết quả: **PASS** trên môi trường Windows của workspace. Kiểm tra sau khi hoàn tất thay đổi; phạm vi là phần mềm web/gateway/core và dữ liệu giả lập, chưa nghiệm thu phần cứng.

| Kiểm tra | Kết quả | Bằng chứng thực thi |
| --- | --- | --- |
| Build core và C++ test | PASS | `scripts/build-core.ps1 -Tests`, tạo build/agv_core.exe và chạy core_tests.exe |
| Gateway/SQLite | PASS, 7 ca | `node --test --test-isolation=none tests/gateway.test.js` |
| C++/MQTT tích hợp | PASS | `node tests/system.integration.js` với broker localhost riêng, map/counter/DB riêng |
| Trình duyệt | PASS | Edge headless qua Playwright; desktop 1600×1050 và mobile 390×844; không có pageerror, không tràn ngang ở mobile |
| Kiểm tra ảnh | PASS | Xem ảnh desktop/mobile; bảng Fleet, hướng cạnh và đường dự kiến đọc được |
| Tài liệu workspace | PASS | scripts/check_workspace.py: 35 Markdown, 117 link nội bộ, 6 kỹ năng, 3 JSON và 2 Word container ở lần kiểm tra trước khi thêm hồ sơ này |

Các tình huống quan trọng đã chạy: hai task cùng đích nhưng khác ID; hủy một task; lỗi robot không ghi completed; map lỗi giữ graph cũ; core từ chối được trả về HTTP; timeout/disconnect không sửa obstacle cache; stale snapshot khóa điều khiển; log seq trùng ở hai phiên vẫn lưu và DB mở lại được; command counter qua restart và file lỗi; robot offline giữ zone/đường chưa xác minh; mất MQTT khóa điều khiển; chuỗi pin telemetry sai không biến thành 0%; chọn đúng robot khi chung node; tên/log/error chứa HTML hiển thị như text.

Các bộ tích hợp tự đóng tiến trình fixture. Dữ liệu/ảnh đối chiếu nằm trong `.qa/system-test-90SqpX` và `.qa/browser-test-XKcEBm`. Không chạy gateway với DB `AGVwebserver/agv.db` trong kiểm thử.

Word được kiểm tra cấu trúc container trong bước kiểm tra workspace; không xuất hoặc nghiệm thu bố cục Word mới ở lần sửa ứng dụng này. Chưa có số đo pin, phanh, vị trí hoặc mạng của robot thật. Session/dedup MCU, phục hồi vùng sau core reboot và TLS/ACL vẫn chờ triển khai theo firmware/yêu cầu.

Hướng dẫn chạy lại: [tests/README.md](../tests/README.md). Phạm vi và cách dùng bản mới: [14-web-server-update.md](14-web-server-update.md).

## Kiểm tra sau sửa startup

Build/core tests, gateway 7 ca, [startup integration](../tests/startup.integration.js), map integration và system integration chạy lại đều **PASS** ngày 29/09/2026. Core trùng bị từ chối trước khi dùng MQTT/counter; core đang chạy giữ nguyên phiên và MQTT, wrapper chuẩn hóa cwd, restart và fallback counter src dùng chuỗi ID tiếp nối trong workspace fixture. Không chạy lại browser vì lần sửa này chỉ đổi startup/address và không đổi giao diện.

Fixture mới: `.qa/startup-test-uCknOJ`, `.qa/map-test-xDDcTJ`, `.qa/system-test-GXrUmM`; broker, map, counter và DB của test được tách riêng. Chi tiết nguyên nhân và cách chạy tại [chẩn đoán startup](16-startup-troubleshooting.md).

## Sketch ESP32 test giao tiếp

**PASS** compile `firmware/AGV_Connection_Test/AGV_Connection_Test.ino` bằng Arduino CLI 1.4.1, FQBN esp32:esp32:esp32, ESP32 core 3.3.11, PubSubClient 2.8, ArduinoJson 7.4.3. Compiler báo 908360 byte flash (69%), 54560 byte globals (16%). Không upload/đo board thật.

**PASS** host probe biên dịch trực tiếp .ino/ArduinoJson và **PASS** [firmware.integration.js](../tests/firmware.integration.js) với core/gateway/broker thật: WAIT khởi tạo/hủy, GOTO retry cùng cmd_id, duplicate ACK không lặp nhận hành động, ACK dừng resend mà task vẫn dispatched, telemetry node trung gian/đích, M5/M6 grant/release và báo lỗi dẫn tới STOP. Log/events tại `.qa/firmware-test-nVdbco`, output ESP32 `.qa/esp32-connection-test-build`. Host adapter không mô phỏng radio WiFi, motor hoặc cảm biến. [Cách dùng sketch](../firmware/AGV_Connection_Test/README.md).
