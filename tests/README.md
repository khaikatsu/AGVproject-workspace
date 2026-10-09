# Hồ sơ kiểm thử AGV

Đã kiểm tra phần mềm web/server/sketch ngày 29/09/2026 bằng GCC, Node, Mosquitto, Edge headless và Arduino CLI. DB/map/broker fixture riêng. Chưa kiểm thử firmware trên board hoặc robot thật.

Kết quả cụ thể tại [hồ sơ chạy kiểm thử](../docs/testing-web-server-2026-09-29.md).

## Bộ kiểm thử

Vòng đời web mô phỏng theo tab: `node tests/simulation.on-view.js` (Core đã build, Mosquitto có sẵn; dữ liệu riêng trong `.qa/`).

Cập nhật 01/10/2026: [security.test.js](security.test.js) kiểm tra auth/Origin/WS frame/login/retention và identity qua session; [hardening.integration.js](hardening.integration.js) chạy tám xe MQTT, grant ACK/retry, checkpoint trước outbound, restart queue/mutex/ID, LWT và delta/throttle; [tls.integration.js](tls.integration.js) tạo CA một ngày trong .qa, chạy broker TLS/ACL thật và HTTPS/token. Sketch host có watchdog/session latch và NVS cmd_id; browser có login/logout và delta. [Thiết kế/config](../docs/18-hardening.md).

```powershell
node --test --test-isolation=none tests/gateway.test.js tests/security.test.js
node --test tests/orders.test.js
node tests/hardening.integration.js
node tests/tls.integration.js
```

TLS test cần OpenSSL và mosquitto_passwd trong AGV_MOSQUITTO_BIN trên Windows; chỉ credential test trong .qa, không sửa broker1883/DB vận hành. Linux CI dùng /usr/sbin cho broker. Docker/CI chưa chạy runtime tại máy này; compile ESP32 đã chạy, chưa upload board. [Kết quả 01/10](../docs/testing-hardening-2026-10-01.md).

| Bộ | Phạm vi |
| --- | --- |
| [core_tests.cpp](core_tests.cpp) | Candidate map không làm hỏng graph cũ, cạnh có hướng/hai chiều, tổng trọng số, task ID/hủy/hoàn thành/lỗi, mất ACK/offline giữ vùng, mạng down, counter qua restart và lỗi file |
| [gateway.test.js](gateway.test.js) | Array API, đơn hàng kho/Core task ID, cùng đích khác ID, core từ chối, validation, timeout/disconnect không báo thành công, stale/socket/session, KPI/watermark và migration/reopen DB cũ |
| [orders.test.js](orders.test.js) | Ghép nhiều kho theo số kiện và đường có hướng; task_id/RFID, mô phỏng 10 giây, hai xe, offline, đối soát và restart |
| [system.integration.js](system.integration.js) | HTTP → gateway → core C++ thật → Mosquitto localhost riêng; MQTT fixture ACK/telemetry/error/mutex; apply/save/reject map, weighted dispatch, completion/interruption, offline zone và broker disconnect |
| [browser.integration.js](browser.integration.js) | Edge headless với mock core: bốn khu vực mới, snapshot/KPI/editor, bấm node tự giao task cho Core, chọn robot chỉ định ở chung node, routes/history, khóa/mở một nút, text an toàn, Web/Core/MQTT, stale/reconnect, desktop và mobile 390 px có phóng to map |
| [map.integration.js](map.integration.js) | Map v3 với core/broker thật và hai robot MQTT giả lập: GOTO qua M5/M6, tranh quyền zone, telemetry trung gian/đích, ID mới; map/counter/DB riêng |
| [startup.integration.js](startup.integration.js) | Windows: chạy core trùng bị từ chối trước MQTT/counter, wrapper từ thư mục khác, phiên MQTT đang chạy ổn định, restart và giữ counter cũ trong workspace fixture |
| [firmware-host/probe.cpp](firmware-host/probe.cpp) | Compile trực tiếp sketch .ino + ArduinoJson thật với adapter I/O: command/ACK/dedup/limits/malformed, reconnect cache, mutex/manual telemetry; đối chiếu node/fixture |
| [firmware-host/driver_bench.cpp](firmware-host/driver_bench.cpp) | GPIO/PWM shim cho driver: motor tắt khi không có quyền, bám line khi siêu âm rõ, 010/101/111 bám thẳng, dừng khi line mất hoặc siêu âm ≤15 cm; không thay kiểm thử xe thật |
| [firmware.integration.js](firmware.integration.js) | Parser .ino host ↔ broker/gateway/core thật riêng: WAIT khởi tạo, GOTO retry cùng ID, ACK dừng retry và không hoàn thành task, progress/grant/release M5/M6, hủy WAIT và báo lỗi STOP |
| [shift.integration.js](shift.integration.js) | Hai robot trên map kho thật: tập kết khi mở core, đúng trạm riêng, mở/đóng ca, hoàn thành task cuối và giữ queue, ACK khác đến đích, mutex/pin/lỗi, vị trí mới sau restart |
| [two-robot-recovery.integration.js](two-robot-recovery.integration.js) | Broker/Core/gateway và hai MQTT giả lập riêng: LWT giữ zone, RFID trạm + diagnostic mới trả zone, reset lỗi mạng một lần, diagnostic khỏe mới xóa lỗi; chờ chạy ở môi trường cho phép kết nối TCP loopback (`net.connect` trong sandbox hiện trả `EACCES`) |
| [solo-shift.integration.js](solo-shift.integration.js) | Mở ca một xe: chạy `--default --robot=01` hoặc `--default --robot=02` cho Core cấu hình hai trạm; mặc định kiểm tra trial 01. Pin `?` mở ca, pin đo 25% khóa và 85% mở lại mà không phát `CHARGE_START`/GOTO khi đã ở trạm; broker/Core/gateway/DB riêng |

Firmware-host còn đối chiếu 26 UID người dùng cung cấp, quét S1/S2 không cần `learn`, bỏ qua mapping NVS cũ, kiểm tra override/forget; đồng thời kiểm tra RC522 SPI/UID4-7-10byte, collision/NVS failure, boot không khôi phục node, lệnh bị chặn khi chưa định vị/sai trạm, simulation mặc định tắt và xác minh lại sau reconnect. Chạy shift.integration.js với **--sketch** để dùng hai receiver .ino thật (I/O host adapter) thay cho fixture ACK/telemetry trực tiếp.

C++ test còn đối chiếu ba JSON với map mặc định và kiểm tra trung điểm/trọng số/hướng qua M5/M6. Browser test còn kiểm tra map v3 thật trên SVG, nhãn node, snapshot và mobile. Kết quả bản đồ tại [cập nhật v3](../docs/15-map-layout-update.md).

MQTT fixture dùng [client tối thiểu](helpers/mqtt-client.js) chỉ trong kiểm thử. Di chuyển node được fixture cấp dữ liệu, không mô phỏng động học, phanh hoặc thân robot. Browser test dùng core giả; system test dùng core/broker thật. Hai phạm vi này được ghi riêng để tránh nhầm bằng chứng.

## Chạy lại

Từ gốc workspace, với toolchain/thư viện hiện có:

```powershell
./scripts/build-core.ps1 -Tests
node --test --test-isolation=none tests/gateway.test.js
node tests/system.integration.js
node tests/map.integration.js
node tests/startup.integration.js
node tests/shift.integration.js
```

`system.integration.js` mặc định dùng `build/agv_core.exe`; có thể truyền đường dẫn executable ở argument đầu. Broker tự chạy bằng `C:/msys64/ucrt64/bin/mosquitto.exe` trên máy đã kiểm tra; máy khác đặt `AGV_MOSQUITTO_BIN` tới thư mục broker. Test chọn cổng ngẫu nhiên và bind localhost, không dùng MQTT 1883 hoặc core 8080 đang vận hành.

Kiểm tra sketch với ArduinoJson 7.x đã cài (thay đường dẫn cho máy tương ứng):

```powershell
./scripts/test-firmware.ps1 -ArduinoJsonInclude 'C:/Users/DELL/OneDrive/noob coder/libraries/ArduinoJson/src'
node tests/firmware.integration.js
node tests/shift.integration.js --sketch
```

Script tạo `.qa/firmware-host-build/sketch_probe.exe` và `driver_bench.exe`. Adapter dùng parser/ACK/dedup trực tiếp từ `.ino`; bench shim kiểm tra nhánh GPIO/PWM của driver. MQTT integration bridge dùng broker/cổng/counter/map/DB riêng. Bản sketch trước driver đã compile ESP32 bằng Arduino CLI 1.4.1 và board package 3.3.11; bản driver 03/10/2026 mới kiểm tra bằng host compiler, chưa build ESP32/upload/đo trên board thật. [Hướng dẫn sketch](../firmware/AGV_Connection_Test/README.md).

Browser test cần Playwright có sẵn. Trong môi trường Codex đã kiểm tra:

```powershell
$env:AGV_PLAYWRIGHT_MODULE = 'C:/Users/DELL/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright'
$env:AGV_BROWSER_CHANNEL = 'msedge'
node tests/browser.integration.js
```

Máy khác dùng module Playwright và browser đã cấu hình tương ứng. Không thêm Playwright vào dependency runtime của gateway. Test tự đóng browser, gateway, core/broker fixture; ảnh/DB/map còn trong `.qa/*-test-*` để đối chiếu.

## Core thử hai xe 09/10/2026

Build riêng `build/two-robot-trial/agv_core.exe` và unit Core PASS với TEMP/TMP đặt trong `.qa/tmp-two-robot-trial` (thư mục tạm ngoài workspace bị Windows từ chối ghi thay file). `node --test tests/orders.test.js` PASS 6/6. `tests/simulation.orders.js` chạy trên broker/Core/map/counter/DB riêng với `AGV_TWO_ROBOT_TRIAL=1` PASS: năm kiện do hai xe nhận và di chuyển song song, hai đơn độc lập chồng thời gian, lỗi pin dừng đơn để đối soát. Run artifact: `.qa/order-tests/run-Mee2RR`. Đây là mô phỏng S1; chưa mở motor hoặc kiểm tra vùng vật lý của hai xe. [ADR-025](../docs/decisions/ADR-025-two-robot-supervised-trial.md).

Theo xác nhận tiếp ngày 09/10/2026, route firmware S2→GS2 có hướng thẳng đầu tiên. PlatformIO `test -e native -e native_trial` PASS 62/62, build `esp32dev` và `esp32dev_robot2_monitor` thành công. Hai binary này vẫn khóa PWM; chưa nạp bản vừa build lên xe. Khoảng hở 15 cm ở thẻ tiếp theo là phép tính từ số đo người dùng, chưa có test quét RFID sớm hoặc thân xe khi rẽ.

Sau số đo RC522 đọc sớm 8 cm và xác nhận map vật lý, `MutexClearance` được thêm: thẻ 35 cm đầu tiên không nhả mutex (chỉ còn 7 cm danh nghĩa), thẻ ≥50 cm hoặc thẻ thứ hai của chuỗi thẳng 35+35 cm mới có thể queue release đúng grant ID. PlatformIO native/native_trial PASS 66/66 và build `esp32dev`, `esp32dev_robot2_monitor` thành công ngày 09/10/2026. Các ca mới kiểm tra chưa vào zone, sai nhánh, mất phiên và token release, gồm chặn dùng lại cùng ID sau reconnect. Chưa flash hoặc kiểm chứng khoảng hở vật lý. Trong lượt chẩn đoán, hai xe từng offline nhưng đã online trở lại; 01 chưa có RFID mới để xác minh vị trí.

Bổ sung env bench hai xe ngày 09/10/2026: `native`, `native_trial`, `native_two_robot_bench` PASS 81/81, gồm ca bắt buộc grant, thu hồi đúng ID và chốt điện áp bench 6,4–8,6 V. Build `esp32dev_two_robot_bench_01` và `_02` PASS; bản mới có chốt điện áp đã nạp/verify flash qua COM6 lần lượt đúng MAC xe 01/02. Chưa kiểm chứng lấy/trả hàng vật lý.

## Firmware NCKHAGV mới

Bản điều khiển cơ bản 04/10/2026: host test kiểm tra GOTO S1→GS1 lùi, dừng ở RFID, mất line/node sai, đợi grant M, quay tới khi bắt lại line và dừng nếu quá 2,5 giây; Core unit và MQTT integration fixture xác nhận `battery_measured=false` nhưng `position_valid=true` khi telemetry mang pin `?`. Build Core, PlatformIO ESP32 và 52 native test đã PASS. Chưa upload/chạy MQTT và motor trên ESP32 thật; chi tiết ở [ADR-015](../docs/decisions/ADR-015-basic-rfid-line-motion.md).

Bản thử chạy 01 S1→GS1: [one-robot-trial.integration.js](one-robot-trial.integration.js) dùng broker/Core/gateway riêng, xác nhận Core từ chối M2, phát đúng GOTO S1→GS1 khi pin `?`, và chỉ đánh dấu hoàn thành khi telemetry RFID GS1 đến. `esp32dev_trial` build thành công, 52 native test PASS; thử motor/line trên xe thật vẫn chờ kiểm chứng. [ADR-016](../docs/decisions/ADR-016-single-robot-server-motion-trial.md).

Cập nhật 05/10/2026: [ADR-017](../docs/decisions/ADR-017-solo-full-map-trial.md) thay phạm vi trial cũ. [solo_route.cpp](solo_route.cpp) kiểm tra 650 cặp node đi–đến theo Dijkstra có hướng và route firmware sau khi tới node đầu từ S1; PASS. Core unit PASS, 53 native firmware test PASS, build `esp32dev_trial` PASS. [one-robot-trial.integration.js](one-robot-trial.integration.js) giờ xác nhận S1→GS1→M2, từ chối giao tự động, RFID M2 hoàn thành task và telemetry xe 02 dừng trial; chạy với broker/Core/gateway/DB riêng PASS. Chưa nạp hoặc đo xe thật; không dùng kết quả này làm bằng chứng motor/line đã hoạt động ngoài đời.

Kiểm thử host còn xác nhận route S1→GS1 lùi, lệnh node-order tương ứng, các mẫu line khi lùi và dừng an toàn ở `000`. Các kết quả này chỉ kiểm tra logic C++, chưa xác nhận cực tính bánh hoặc cảm biến trên xe.

Từ `firmware/NCKHAGV/NCKHAGV`, chạy `platformio test -e native` để kiểm tra codec Core v0 và logic cũ; `platformio run -e esp32dev` để build board đúng dependency đã pin. Có thêm `tests/nckhagv_protocol.cpp` chạy host g++ với ArduinoJson đã cài, bao gồm 26 UID duy nhất, khóa motor mặc định, command/ACK/telemetry, cạnh một chiều và hướng rẽ. Tại gốc workspace, `node scripts/generate-firmware-route.js --check` kiểm tra bảng route MCU đồng bộ với `data/map_layout.json`; khi đổi map, chạy script không có `--check` rồi nạp lại firmware. Ngày 04/10/2026 kiểm thử host và map sinh PASS; PlatformIO nằm ở `$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe` (chưa có trong PATH). Build `esp32dev` thành công với ArduinoJson 6.21.5, 52 native test PASS; chưa upload hoặc đo trên board thật.

## Giới hạn bằng chứng

Audit 02/10/2026: **16/16 bộ PASS**, thêm 15 nhóm lỗi và stress 30 lượt chọn đích hai xe. Đã sửa giữ đường thường/hành lang mutex, mất broker và phục hồi token Core restart; [báo cáo chi tiết và cách chạy](../docs/testing-simulation-audit-2026-10-02.md), [ADR-011](../docs/decisions/ADR-011-simulation-conflict-audit.md). Runner `node tests/run-simulation-audit.js --browser` lưu result/log trong `.qa/full-audit-*`; cần build Core/unit và cấu hình Playwright như trên. `AGV_CORE_BIN`/`AGV_CORE_TEST_BIN` chọn binary kiểm thử riêng, có thể build bằng `-OutputDirectory .qa/audit-build` khi Core vận hành đang chạy.

Web clone mô phỏng có tám bộ kiểm thử riêng (bổ sung đơn hàng 05/10/2026):

```powershell
node tests/simulation.robot.test.js
node tests/simulation.faults.js
node tests/simulation.low-battery.js
node tests/simulation.integration.js
node tests/simulation.mutex.js
node tests/simulation.browser.js
node tests/simulation.orders.js
node tests/simulation.orders.browser.js
```

Các bộ integration dùng binary Core đã build và Mosquitto; bộ browser thêm Playwright cấu hình như trên. Unit robot không cần broker. Broker/Core/gateway/DB/counter/map test đều riêng trong `.qa/`. Bao phủ hai trạm/ca, task và chuỗi lấy trả, pin thấp/sạc/0% failsafe, ACK retry/dedup, offline giữ mutex rồi phục hồi, hai xe tranh M5, phân biệt distance/weight, UI nội suy/per-vehicle trên desktop/laptop/mobile và xuất report. [Cách dùng simulator](../simulation/README.md). Bằng chứng PASS và ảnh trong thư mục được in sau mỗi lần chạy; không dùng kết quả S1 làm nghiệm thu phần cứng.

Hai test đơn hàng ngày 05/10/2026 dùng fixture riêng trong `.qa/order-tests` và `.qa/order-browser`: 5 kiện từ K1 tới K2/K4 bằng cả hai xe, hai đơn khác kho chạy chồng thời gian, lỗi pin không báo giao giả, form Edge và màn hình mobile 390 px. `simulation.orders.browser.js` dùng cùng `AGV_PLAYWRIGHT_MODULE`/`AGV_BROWSER_CHANNEL` nêu trên.

PASS ở các bộ trên xác nhận hành vi phần mềm trong kịch bản đã định nghĩa. Chưa xác nhận timestamp/dedup/reboot của MCU, xử lý vùng sau core reboot khi robot thật còn đang di chuyển, hay an toàn vật lý. Telemetry/command giữ hợp đồng cũ; mutex có token/ACK theo [bản gia cố 01/10](../docs/18-hardening.md). Kiểm thử host/NVS chưa thay thế reboot và đo an toàn trên MCU thật.

## Ghi kết quả phần cứng về sau

Mỗi kết quả ghi:

- ID ca và yêu cầu liên quan.
- Phiên bản mã, contract và map.
- Thiết bị/simulator, điều kiện mạng và seed nếu có.
- Thao tác, thời điểm, kết quả mong đợi và kết quả thật.
- Đường dẫn log/ảnh/video cùng nhận định đạt/chưa đạt và giới hạn.

Đọc [kế hoạch kiểm thử](../docs/09-testing.md). `src/AGVtest.cpp` không được tính là test suite C++ khi chưa phân loại lại nội dung.
