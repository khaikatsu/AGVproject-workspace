# Hồ sơ kiểm thử AGV

Giảm PWM xoay xe01 ngày14/10/2026: probe `tests/motor-host/reverse_speed.cpp` link module motor/movement/line thật PASS01/02 cho pivot trái/phải±200/±230, PWM hai channel, tiến/lùi/còi/STOP. PlatformIO build ESP3201/02 PASS; native01/02 tổng52/52 PASS cho bắt line sau rời junction, NODE_ORDER/quay trạm và chốt mạng/mutex. [ADR-054](../docs/decisions/ADR-054-robot01-slower-pivot.md). Chưa nạp/đo trên sàn.

Hoàn thành đỗ trạm14/10/2026: build/unit `.qa/station-completion-build`; `$env:AGV_CORE_BIN=(Resolve-Path .qa/station-completion-build/agv_core.exe).Path; node tests/heading.integration.js` dùng broker/gateway/DB riêng. RFIDage175278 cùng ready mới phải completed/IDLE/task_id-targetrỗng/CLOSED, lịch sử cùng phiên một record sau lặp diagnostic; chưa quay xong/hướng vào/null RFID/lỗi vẫn giữ task. Fixture `.qa/heading-test-fH7caJ` PASS. [ADR-053](../docs/decisions/ADR-053-station-completion-rfid-anchor.md).

Heartbeat/GDB ngày14/10/2026: `$env:AGV_HEARTBEAT_GDB='1'; node tests/core-heartbeat.integration.js` dùng Core `.qa/network-fix-build/agv_core.exe` (hoặc AGV_CORE_BIN), broker/cổng/map/runtime/counter riêng,1000task terminal và telemetry/diagnostic/encoder5Hz. `_NO_DEBUG_HEAP=1` tương ứng F5, đo20giây, chốt max gap/latency<1500ms, giữ1000task và không GOTO/NODE_ORDER. Fixture `.qa/heartbeat-test-b4iGck` PASS heartbeat99/max278ms, encoder99/max197ms; không phải nghiệm thu xe chuyển động. [ADR-052](../docs/decisions/ADR-052-core-debug-heap-heartbeat.md).

Encoder xe 01 ngày 14/10/2026: `./scripts/test-encoder.ps1` build adapter ISR/codec 01/02, unit Core và Core riêng `.qa/encoder-build`; `node tests/encoder.integration.js --browser` dùng broker/Core/DB/cổng riêng, Edge desktop/mobile. Cấu hình Playwright theo mục browser bên dưới. Phạm vi: GPIO16/4, debounce, distance/speed, STOP/pivot/lùi, RFID rebase, sai session/seq/chặng/schema, stale/error/offline, không hoàn thành task/nhả mutex/checkpoint từ xung. [Báo cáo](../docs/testing-encoder-robot01-2026-10-14.md), [ADR-050](../docs/decisions/ADR-050-robot01-encoder-position-speed.md).

Test Lấy/Hạ độc lập xe01 ngày10/10/2026: `node --test tests/warehouse-actions.test.js`, `node tests/warehouse-actions.browser.js` và `node tests/warehouse-actions.integration.js`. Fixture riêng; integration chạy đúng binary hồi sáng/mode01,5s thời gian thực và kho→gate; không tạoorder. [ADR-045](../docs/decisions/ADR-045-standalone-warehouse-tests-without-orders.md).

Luồng kho5s ngày10/10/2026: `node tests/orders-five-second.integration.js` dùng Core hồi sáng/xeảo/broker/map/DB/counter riêng, kiểm tra5s thời gian thực ở kho lấy/giao và task exit ra gate. Orders unit thêm chỉ hoàn tất sau gate, reservation của đơn chưa phát lệnh và operator resolve; native firmware giữ kiểm tra REVERSE/NODE_ORDER. [ADR-044](../docs/decisions/ADR-044-five-second-warehouse-service-and-gate-exit.md).

Đơn hàng test xe01 ngày10/10/2026: orders.test.js thêm chọn gần nhất theo weight (khác số node), mặc định01 trước dispatch/khi lỗi, preview hai tuyến và queued restart. `node tests/orders-selection.browser.js` dùng fixture Core/DB riêng, kiểm tra hint01/LINE_LOST/tuyến lấy/giao/mobile và không lệnh di chuyển khi lỗi. [ADR-043](../docs/decisions/ADR-043-robot01-warehouse-order-planning.md).

Chia sẻ Internet 10/10/2026: `node tests/simulation.public.test.js` kiểm tra proxy HTTP/WS, Origin, REST đọc/ghi settings và vòng đời phiên, với Core/broker/dữ liệu riêng. `node tests/simulation.public.browser.js` mặc định dùng fixture riêng; đặt `SIM_PUBLIC_URL` để kiểm tra link tunnel thực tế bằng Playwright/Edge (hai xe, heartbeat, reload, mobile). [ADR-042](../docs/decisions/ADR-042-share-simulation-over-internet.md).

Phục hồi web mô phỏng 10/10/2026: `node tests/simulation.connection.browser.js` (Playwright/Edge cấu hình như các test browser bên dưới). Fixture Core/broker/DB/cổng riêng kiểm tra offline/online, HTTP lỗi/backoff, payload thiếu, timeout, chờ Core/MQTT, mất riêng WS, mất phản hồi POST không replay, đóng phiên/mở lại và mobile. Gateway/security có ca cổng trùng không làm hỏng phiên đang chạy. [Quyết định và kết quả](../docs/decisions/ADR-039-simulation-browser-reconnection.md).

Rà soát server GS2 ngày 09/10/2026: Core/unit, fixture MQTT robot02/both, menu, khởi động trùng/counter, recovery và browser Edge PASS. [Bằng chứng và giới hạn S1](../docs/testing-server-gs2-2026-10-09.md). Binary sửa tại `.qa/server-gs2-audit-build`; chưa thay Core vận hành.

Rà soát GS2: `node tests/server-gs2.integration.js`, đặt `AGV_CORE_BIN` tới ảnh Core đã build. Broker/cổng/map/counter/checkpoint riêng trong `.qa`; chạy cả `robot02` và `both`. Kiểm tra path S2→GS2→M3, grant/ACK/duplicate request, không STOP/WAIT hoặc GOTO trùng khi telemetry khỏe; LWT giữ quyền, clear_error giữa tuyến bị từ chối, reset tại S2 giữ ERROR và task chờ đến diagnostic khỏe. Đây là S1 hợp đồng/logic, không xác nhận PWM hoặc hướng bánh thật.

Đã kiểm tra phần mềm web/server/sketch ngày 29/09/2026 bằng GCC, Node, Mosquitto, Edge headless và Arduino CLI. DB/map/broker fixture riêng. Chưa kiểm thử firmware trên board hoặc robot thật.

Kết quả cụ thể tại [hồ sơ chạy kiểm thử](../docs/testing-web-server-2026-09-29.md).

## Bộ kiểm thử

Chẩn đoán bo xe 02 tự reset 09/10/2026: build ESP32 bench 02 với `resetReason/resetReasonCode/firmwareMode/motionEnabled` trong diagnostic PASS, API/enum khớp SDK đang cài. Chỉ thêm thông tin boot/mode; chưa upload hay đo nguồn. MQTT quan sát trực tiếp thấy M3 đã grant/ACK nhưng task bị LWT, uptime xác định boot gần 22:27:52; chưa có resetReason từ bo trước khi nạp ảnh mới.

GS2 ngày 09/10/2026: PlatformIO `test -e native_two_robot_bench` PASS 12/12 sau khi thêm ca S2→GS2→M3→C3: chờ grant, rẽ LEFT, không bắt lại line cũ trước mẫu 000, dừng khi MQTT mất và không suy hướng sau reset tại GS2. `run -e esp32dev_two_robot_bench_02` build ảnh có log `[MOTOR]` GS2 PASS. Chưa upload/thử motor; log Core sống ghi nhiệm vụ xe 02 bị `MQTT LWT offline`.

Kết quả menu 09/10/2026: Core build/unit PASS (`.qa/menu-build`), `core-mode.integration.js` PASS (`.qa/core-mode-test-9rQiWI`), browser PASS (`.qa/browser-test-sNaMX6`), startup PASS (`.qa/startup-test-oT4Apb`), solo 01 cũ PASS (`.qa/solo-shift-test-RDlXXw`), orders 7/7 PASS. Chạy runtime fixture ngoài sandbox; chạy unit trong sandbox từng bị Windows error 5 khi thay file tạm. Không dùng robot/SQLite vận hành.

Menu chọn Core: `tests/core-mode.integration.js` dùng stdin, broker/cổng/map/counter riêng; kiểm tra nhập sai, 0/EOF không ghi dữ liệu, chọn 01/02 chỉ phát GOTO đúng xe, RFID xe không chọn chặn ca khi chưa xác minh, hai xe tranh mutex và counter tiếp nối sau restart. Chạy sau build mới, hoặc đặt `AGV_CORE_BIN` tới binary riêng trong `.qa`. Unit `core_tests.cpp` bổ sung hai ca thử riêng 01/02; browser test kiểm tra nhãn chế độ và vô hiệu xe không chọn. Đây là kiểm chứng logic, chưa thử robot thật.

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

Ưu tiên RFID (13/10/2026): `node --test --test-isolation=none tests/gateway.test.js tests/security.test.js` kiểm tra preempt timer và phản hồi lệnh cũ không ghi đè core_snapshot_seq mới. Build riêng bằng `./scripts/build-core.ps1 -Tests -OutputDirectory .qa/rfid-latency-build`; đặt AGV_CORE_BIN tới executable đó rồi chạy `node tests/straight-through.integration.js` và `node tests/node-order.integration.js`. Fixture đi xuyên có browser WebSocket riêng, đo mqtt_to_web_ms cho mỗi node đổi và gate <1s trên loopback. Độ trễ này bắt đầu tại publish fixture, không phải thời điểm xe thật/RFID tới thẻ. Kết quả và giới hạn ở [ADR-049](../docs/decisions/ADR-049-priority-rfid-snapshots.md).

Đi xuyên node thẳng theo tuyến (13/10/2026): `./scripts/build-core.ps1 -Tests -OutputDirectory .qa/straight-through-build`, sau đó `node tests/straight-through.integration.js`. Fixture tự chọn cổng loopback, broker/map/counter/DB riêng; kiểm tra capability/GOTO metadata, C4→M4→GS2→M3→C3 báo đủ RFID, không NODE_ORDER trung gian khi đang chạy, thiếu M3 thì dừng tại GS2, token sai/offline không trả owner, rẽ ở M4 và firmware cũ vẫn chờ lệnh. Override binary bằng AGV_CORE_BIN. Native: PlatformIO test env native_robot01_bench/native_robot02_bench; parser/dedup/max32 node, runtime grant/line/mạng/STOP, kho/trạm/lùi, clearance đúng token và stop ở đích. Test phần mềm không đo khả năng đọc RC522/phanh/line trên xe thật. [ADR-048](../docs/decisions/ADR-048-route-straight-through.md).

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

## Hướng đầu xe và trễ RFID 10/10/2026

Lịch sử trước đính chính theo ADR-032: Core unit, gateway/security 13/13, native firmware 93/93, route audit 650 cặp PASS. `node tests/heading.integration.js` dùng broker/Core/map/counter/DB riêng, kiểm tra hướng mặc định N tại S1, lùi giữ hướng đầu, heading route/nguồn operator cũ, API manual trả 404, offline UNKNOWN, không phát GOTO/SET_HEADING. Fixture `.qa/heading-test-AIRU8X/verification.json`. Browser PASS `.qa/browser-test-OSqt55`, ảnh `robot-heading-default.png`; không còn panel/chọn hướng. Test firmware gồm tám bay tiến vào/lùi ra và quay đầu qua một/hai nhánh, không đếm lặp centered, timeout dừng. Chưa flash hoặc đo chuyển động thật. [ADR-032](../docs/decisions/ADR-032-fixed-bay-heading.md).

Kiểm thử RFID dùng module thật, SPI/Arduino/MFRC522 stub, không cần board:

```powershell
& 'C:/msys64/ucrt64/bin/g++.exe' -std=c++17 -DAGV_TWO_ROBOT_BENCH_TRIAL=1 -DAGV_ROBOT_PROFILE_01=1 -Itests/rfid-host -Ifirmware/NCKHAGV/NCKHAGV/src tests/rfid-host/test_main.cpp firmware/NCKHAGV/NCKHAGV/src/sensors/RFIDModule.cpp firmware/NCKHAGV/NCKHAGV/src/core/ControlLogic.cpp -o .qa/rfid-scan-check.exe
& .qa/rfid-scan-check.exe
```

PASS cho sự kiện GS1/GS2 trả trước HALT, caller dừng trước cleanup, biên poll 10 ms (đổi từ 20 ms theo yêu cầu mới 10/10/2026), một presence request mỗi miss, 50 lần đọc thẻ đứng yên không tiến node lặp, UID kế tiếp/lạ và collision. Đây là kiểm chứng trình tự phần mềm; chưa chứng minh độ nhạy RF, điểm dừng hay quán tính trên xe. [ADR-030](../docs/decisions/ADR-030-confirm-robot-heading.md), [ADR-031](../docs/decisions/ADR-031-rfid-read-latency.md).

## Ghi kết quả phần cứng về sau

Mỗi kết quả ghi:

- ID ca và yêu cầu liên quan.
- Phiên bản mã, contract và map.
- Thiết bị/simulator, điều kiện mạng và seed nếu có.
- Thao tác, thời điểm, kết quả mong đợi và kết quả thật.
- Đường dẫn log/ảnh/video cùng nhận định đạt/chưa đạt và giới hạn.

Đọc [kế hoạch kiểm thử](../docs/09-testing.md). `src/AGVtest.cpp` không được tính là test suite C++ khi chưa phân loại lại nội dung.

Bản hiện tại theo ADR-033: native 96/96 và audit tuyến 650 cặp PASS. S1/S2 tiến vào, dừng RFID, quay đầu tại S rồi mới ready; xuất trạm Forward; kho tiến vào/lùi ra. Core unit và tests/heading.integration.js dùng broker/counter/DB riêng, giữ task khi đang quay, từ chối hướng vào trong/RFID tuổi 251 ms và chỉ hoàn thành với AGV_IDLE/NONE/STOP đầu hướng ngoài; mẫu ready lượt trước không được dùng lại. ESP32 env esp32dev_two_robot_bench_01 build PASS, chưa flash. Fixture .qa/heading-test-7n6u0V. Bản Core cuối bổ sung từ chối GOTO thủ công khi còn quay tại trạm; unit và fixture .qa/heading-test-2Ghv8D PASS cho 409/không phát command mới.

### Lệnh rẽ tại RFID — ADR-034

`node tests/node-order.integration.js` dùng Core `.qa/node-order-build/agv_core.exe` (hoặc AGV_CORE_BIN), broker/cổng/map/counter/DB riêng. Kiểm tra STOP sau thẻ, lệnh server LEFT/RIGHT, route/tag cũ bị từ chối, retry cùng ID, mutex, không GOTO ghi đè lúc quay và đỗ S1 chờ ready. Native 105/105 PASS; audit `solo_route.cpp` đối chiếu server/MCU 650 cặp tuyến PASS. Motor adapter kiểm tra module MotorModule thật với Arduino stub: hai bánh quay khác dấu tại PWM 245, LEFT/RIGHT đảo dấu và STOP triệt cả bridge. Các kiểm thử không nghiệm thu hướng quay trên xe.

```powershell
& 'C:/msys64/ucrt64/bin/g++.exe' -std=c++17 -DAGV_TWO_ROBOT_BENCH_TRIAL=1 -DAGV_ROBOT_PROFILE_01=1 -Itests/motor-host -Ifirmware/NCKHAGV/NCKHAGV/src tests/motor-host/test_main.cpp firmware/NCKHAGV/NCKHAGV/src/motion/MotorModule.cpp -o .qa/node-pivot-motor.exe
& .qa/node-pivot-motor.exe
```
Hồi quy quét rồi chờ bấm lệnh: NODE_ORDER dùng node/chặng đã xác nhận trong cùng phiên và diagnostic mới STOP, không bắt tuổi thẻ ≤250 ms khi xe đang dừng chờ. Đã có test RFID tuổi 5 giây vẫn nhận STRAIGHT; chưa đọc trong phiên (null) hoặc sai route vẫn bị từ chối. Phục hồi lỗi/mutex và stationReady vẫn cần RFID mới ≤250 ms. RFID adapter đọc cùng thẻ 50 lần mà không tiến tuyến lặp; 6 diagnostic trên bo tại S1 cũng cho tuổi đọc trở lại thấp, chứng minh đọc lặp. Đèn trên ESP32 không được dùng làm bằng chứng số lần đọc.

### PANIC nhận mutex và checkpoint OneDrive — ADR-035

`tests/mqtt-buffer-host` link PubSubClient2.8/ArduinoJson6.21.5 từ `.pio/libdeps` và module CoreProtocol thật. Probe tái hiện pointer zone bị ACK đổi thành chuỗi dài hơn ô8byte; lối nhận grant đã copy qua1.000 callback/đảo field PASS. Không chạy strcpy cũ gây tràn.

Native108/108; Core unit dùng file lock Windows650ms/vượt deadline và giữ file cũ/lưu lại sau mở khóa. `tests/node-order.integration.js` với AGV_CORE_BIN tới `.qa/grant-panic-build/agv_core.exe` gây khóa runtime thật, kiểm tra storage=false/Core unready/command503, mở khóa tự phục hồi và không tự phát GOTO, rồi tiếp tục kiểm thử node-order. Gateway/security/orders21/21 PASS.

ESP32 bench01 build/nạp COM6 đúng MAC ec:c9:ff:ff:09:dc SUCCESS/hash verified; diagnostic bo có firmwareBuild20261010-grant-buffer-fix, S1/IDLE/NONE/headingS/motorSTOP. Đã phục hồi Core/MQTT/storage và mở ca được; chưa chạy kiểm thử quay/line trên sàn. Artifact và giới hạn tại ADR-035.
ADR-036: Core unit và node-order fixture bổ sung reset giữa tuyến ở GS1/M2 trong mode robot01; ACK không mở IDLE, MCU còn lỗi/moving/node khác bị từ chối; healthy STOP mới IDLE, owner/token M2 giữ nguyên. Test01 mở/đóng ca ở M2 dù all_home=false và không tự về S1. Robot02 vẫn từ chối reset giữa tuyến. Chỉ sửa Core, firmware không đổi.
## Vật cản cạnh và giao diện mô phỏng — 10/10/2026

Build riêng: `./scripts/build-core.ps1 -Tests -OutputDirectory build/simulation`; chạy `build/simulation/core_tests.exe`, `node tests/simulation.edges.js`, `node tests/simulation.edges.browser.js`, `node tests/simulation.browser.js` và `node tests/simulation.on-view.js`. Browser dùng cấu hình Playwright/Edge hiện có. Test tạo Core/broker/map/counter/DB riêng trong `.qa/`, kiểm tra 31 đoạn, đường vòng/hết đường, giữ vị trí giữa cạnh, đổi tuyến tại RFID, một form nhiều kho lấy/giao bằng hai xe, desktop/mobile và lease tab. Không phải kiểm thử RC522 hoặc motor thật. Xem [ADR-037](../docs/decisions/ADR-037-simulation-edge-obstacles.md).
## Mũi tên xe mô phỏng — 10/10/2026

`node tests/simulation.direction.test.js` kiểm tra hướng sáu kho/hai trạm, lùi giữ hướng đầu và snapshot hướng. `node tests/simulation.direction.browser.js` kiểm tra hai mũi tên, giao đơn K1→K2 với Bốc hàng/Dỡ hàng, lùi ra kho, đầu hướng cửa khi về trạm và mobile; dùng Playwright/Edge và Core/broker/DB riêng. [ADR-038](../docs/decisions/ADR-038-simulation-direction-arrows.md).
## Sonar xe 01 — 15/10/2026

Bản cuối đã đóng gói ở `build/robot01-dual-sonar/`. Sonar HAL host PASS; native xe01 27/27, xe02 26/26; fixture trước/sau và chấm không bị RFID che PASS ở `.qa/encoder-test-vemfv5`. [Kết quả và cách chạy thử](../docs/testing-sonar-robot01-2026-10-15.md).

Kiểm thử phần mềm sau REQ-ROBOT01-SONAR-01: native profile01 biên dịch thủ công với Unity/ArduinoJson đã cài, **27/27 PASS**, gồm dừng/nhả vẫn giữ route và đóng băng timeout chặng. `tests/encoder_core_tests.cpp` PASS cho cảnh báo/điểm trên cạnh, xóa khi hết vật cản, không ghi vào checkpoint/task/mutex. Core C++ build riêng từ `src/core/AGVserver.cpp` PASS; `tests/core_tests.cpp` PASS với `TEMP/TMP` trong `.qa/sonar-build` để không ghi ngoài workspace. PlatformIO `esp32dev_two_robot_bench_01` build PASS, ảnh ở `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_01/firmware.bin`. Fixture `tests/encoder.integration.js --browser` chạy với Core/broker/DB/map/counter riêng PASS trên Edge desktop/mobile; kiểm tra MQTT diagnostic → Core → gateway → cảnh báo hướng, chấm SVG, màu đỏ cạnh, xóa cảnh báo và hồi quy encoder/task/mutex. Đầu ra `.qa/encoder-test-8amIsK`. Chưa flash, chưa thử trên giá kê/sàn hoặc đo quãng phanh. `scripts/check_workspace.py` vẫn báo link gãy cũ trong docs 13/14/15/19, các artifact `.qa` cũ và README của dependency PlatformIO; JSON Schema chưa cài.
