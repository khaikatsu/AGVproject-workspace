# Hồ sơ kiểm thử gia cố — 01/10/2026

OBSERVED tại workspace Windows, GCC/MSYS2, Node 24.21, Mosquitto 2.0.22 và Edge headless. Test dùng map/counter/runtime/SQLite, cổng và broker riêng trong `.qa`; không ghi DB, counter hay credential vận hành. Thiết kế/config ở [18-hardening](18-hardening.md); [lệnh chạy](../tests/README.md).

| Bộ đã chạy | Kết quả và phạm vi |
| --- | --- |
| `scripts/build-core.ps1 -Tests` | PASS build Core/C++; graph/route/task/shift, hysteresis 20/40, TTL roundtrip/manual lock, ACK/grant/recovery, queue/FIFO và runtime hỏng; Windows khóa file tạm thời rồi mở lại |
| `node --test --test-isolation=none tests/gateway.test.js tests/security.test.js` | PASS 11/11: identity/history/migration, result/timeout/stale; auth/token/Origin/cookie/CSRF/login limit, WS frame, ID validation, batch/retention và chặn Host ngoại lai ở local dev |
| `tests/hardening.integration.js` | PASS tám robot MQTT cạnh tranh một vùng: FIFO, mất ACK gửi lại cùng grant_id, ACK sai owner bị bỏ qua, pending giữ ID qua restart, active interrupted, owner giữ lại, token cũ không release, xác minh vùng trống có kiểm tra generation; delta và throttle |
| `tests/tls.integration.js` | PASS broker TLS thật: CA/hostname và user/password Core, HTTPS/token gateway, xe 01 không đọc command/gửi telemetry của 02, sai password bị từ chối, ngắt socket tạo LWT thật |
| `tests/system.integration.js` | PASS REST→gateway→C++→MQTT, map apply/save/reject, task hủy chính xác/weighted assignment/completion, lỗi/offline giữ vùng |
| `tests/startup.integration.js` | PASS Core trùng bị chặn trước counter/MQTT, wrapper cwd, MQTT/session hoạt động và counter cũ không rewind |
| `tests/map.integration.js` | PASS map v3, hai robot qua M5/M6 và telemetry/ID; chạy trước lần sửa helper atomic file cuối |
| `scripts/test-firmware.ps1` | PASS dùng trực tiếp .ino với ArduinoJson thật và adapter I/O: RC522 UID/NVS/UNKNOWN, parser/ACK/dedup/limits, watchdog/session latch, grant token/trùng/cũ, MQTT reconnect và map |
| `tests/firmware.integration.js` | PASS sketch host↔broker/gateway/Core thật, command retry/ACK/mutex/hủy/lỗi; chạy trước lần sửa helper atomic file cuối |
| `tests/shift.integration.js --sketch` | PASS hai receiver .ino: RFID/NVS adapter, boot UNKNOWN, tập kết/mở ca/kết thúc task/về trạm/charge, queue giữ, lỗi/pin/mutex và xác minh mới sau restart |
| `tests/browser.integration.js` | PASS dashboard desktop/mobile/map v3, editor/routes/history/KPI, reconnect/delta/gating, login sai/đúng/logout, token input xóa và text an toàn |
| Arduino CLI compile ESP32 | PASS sketch cuối với `esp32:esp32:esp32`, board package 3.3.12 và PubSubClient 2.8; 1,017,692 bytes flash (77%), 56,696 bytes RAM (17%); chưa upload |

## Bằng chứng local

- `.qa/hardening-test-SCks7c`, `.qa/hardening-test-sNcPID`: hai lần PASS liên tiếp sau sửa khóa file, runtime/counter/SQLite và fixture tám xe.
- `.qa/tls-test-rTWSbP`: CA/cert **test một ngày**, broker/ACL/config và DB riêng; không dùng credential này trong vận hành.
- `.qa/system-test-X7hjum`, `.qa/startup-test-yiZsNr`, `.qa/shift-test-JNYFId`: fixture kiểm thử cuối.
- `.qa/browser-test-CRD5E7`: ảnh desktop/mobile/map v3 và login-mobile.
- `.qa/firmware-hardening-final.log`, `.qa/firmware-esp32-hardening-build`, `.qa/firmware-esp32-hardening`: host log và build ESP32 cuối.
- `.qa/map-test-9qrQwV`, `.qa/firmware-test-B7kjRe`: test map/firmware integration trước helper atomic file cuối.

Một lần mô phỏng timeout đã phát hiện Windows từ chối thay file trạng thái do khóa đọc ngắn hạn; Core chuyển sang storage_healthy=false và ngừng quyền/lệnh như thiết kế. Đã bổ sung FlushFileBuffers/atomic replacement và retry tối đa 200 ms mỗi bước, test chủ động giữ reader rồi nhả, và chạy lại unit/system/shift/TLS/tám xe đều PASS. Khóa kéo dài hoặc lỗi ghi vẫn dừng phát lệnh, không bỏ qua lỗi lưu trữ.

## Giới hạn

Mô phỏng cấp dữ liệu node, không mô phỏng thân xe/phanh/động học. Sketch host thay I/O, không chứng minh RC522 điện/NVS thật hoặc mất nguồn MCU. Compile ESP32 không phải upload hoặc đo vận hành. Chưa kiểm chứng motor/local STOP, thời gian phanh, battery sensor/relay hoặc an toàn vật lý khi mất mạng. Chưa chạy Docker/Compose hoặc pipeline GitHub; mới tạo cấu hình và kiểm tra mã/script. Token web còn dùng chung, Core WS trong mạng riêng chưa TLS-native, batch SQLite vẫn cùng event loop.

Đổi mutex từ chuỗi sang JSON cần chạy lại Core/gateway và nạp sketch mới cho **cả hai xe**. TLS vận hành cần CA, cert khớp hostname/IP, tài khoản riêng/ACL và cấu hình đồng bộ; các test không tự chuyển broker 1883 đang chạy sang TLS.
