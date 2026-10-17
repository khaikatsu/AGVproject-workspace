# Hồ sơ kiểm thử AGV

16/10/2026 — REQ-ROBOT02-REVERSE-BALANCE-01: chỉ chạy kiểm tra cơ bản motor-host profile02 PASS (lùi215/195, chỉnh lùi225/205, trim lùi trong pha cap, tiến/xoay/STOP/còi), không chạy bộ native/integration. Build `esp32dev_two_robot_bench_02` PASS, RAM57940byte/flash962661byte, buildID `20261016-robot02-reverse-left215`. Chưa upload/thử phần cứng. Giữ các tuning mới người dùng đã chỉnh: reverse210/correction220/capture230.

## 16/10/2026 — Xe02 tách bắt line và căn giữa

BuildID02 `20261016-robot02-line-align`. Native bốn env **166/166 PASS** (02=43,01=42,trial=41,two-robot=40). Test mới:011/110 được tính phiếu có line, hướng căn phải/trái tương ứng; mẫu mới nhất010 và≥2 phiếu trong3 mẫu thì tiếp tục trước100ms, không xác nhận từ111/101, không thêm phiếu cùng thời điểm, mẫu cũ>40ms hết hiệu lực. Giữ timeout600ms, STOP/grant/mạng/sonar/vật cản, không nhận line cũ và hồi quy route/lùi trạm/tìm line.

Build `esp32dev_two_robot_bench_02` **PASS**, RAM57940byte/flash962613byte; ảnh `.pio/build/esp32dev_two_robot_bench_02/firmware.bin` có đúng buildID mới. Chưa upload.

Host `left_capture.cpp` **PASS**: GPIO STOP trước RFID, pivot theo từng mắt±215, cap duty215 sau bắt line rồi về tuning thường sau200ms; blank000 dừng, giữ route/index và odometry không cộng pivot. Reverse correction trong pha cap giữ dấu/zero/còi đúng; host motor01/02 hồi quy **PASS**. Chỉ mô phỏng GPIO, không phải kiểm thử quán tính/nguồn/góc thật. Chưa upload. Các báo cáo bên dưới là bản trước cập nhật này.

## 16/10/2026 — Tăng lực xoay xe02

Native `native_robot02_bench`: **41/41 PASS**. Build `esp32dev_two_robot_bench_02`: **PASS**, RAM57940byte/flash962077byte, ảnh `.pio/build/esp32dev_two_robot_bench_02/firmware.bin`. Chỉ kiểm chứng phần mềm, chưa upload.

Host `left_capture.cpp` PASS PWM230→215 khi rời line, chốt mắt giữa STOPGPIO0/0 trước RFID, chỉnh ngược−215/+215, xác nhận line rồi tiếp tục task/index cũ; odometry không cộng pivot. Host `reverse_speed.cpp` profile01/02 PASS tiến/lùi/chỉnh line giữ nguyên, pivot02=230, phục hồi mất line thông thường vẫn180 và STOP/còi đúng. Chưa nạp hoặc đo lực xoay thật. BuildID02 `20261016-robot02-turn230-capture215`; báo cáo LEFT-CAPTURE bên dưới là bản trước tuning mới.

## Xe02 xoay trái bắt line thì dừng — 16/10/2026

REQ-ROBOT02-LEFT-CAPTURE-01/[ADR-063](../docs/decisions/ADR-063-robot02-left-turn-capture.md). Native01/02/trial/two-robot không profile: **158/158 PASS** (40/41/39/38). Ca G1 C3→G1→K1 tái hiện `010→001→011` không có000 và trướcTURN_MIN_MS vẫn chốtSTOP; xác nhận010100ms/≥3 mẫu rồi STOP trước Forward. Kiểm không nhận lại line cũ, trôi vạch chỉ PivotRight ngắn, timeoutLINE_LOST không retry trái dài, giữ task/index/grant và ưu tiên mạng/sonar/STOP/vật cản. Hồi quy RIGHT/quay đầu/đỗ trạm/lùi S1/S2/tìm line/encoder context PASS.

Host [left_capture.cpp](motor-host/left_capture.cpp) link CoreNavigator/ControlLogic/MovementModule/MotorModule/LineFollowModule thật với adapter GPIO: **PASS**, đầu ra `.qa/robot02-left-capture.exe`. Kiểm PWM215→180, STOPGPIO0/0 ngay khi chốt mắt giữa trước công việc RFID, chỉnh ngược−180/+180, giữ quãng đường odometry khi pivot và trở lại PWM tiến hiện có sau xác nhận. Không phải phép đo dạng sóng hoặc góc quay thật.

`platformio run -e esp32dev_two_robot_bench_02`: **PASS**, RAM57940byte/flash962025byte, ảnh `.pio/build/esp32dev_two_robot_bench_02/firmware.bin`, buildID `20261016-robot02-left-capture`. Chưa upload hoặc điều khiển xe thật. Chạy host: g++ C++17, define AGV_TWO_ROBOT_BENCH_TRIAL=1/AGV_ROBOT_PROFILE_02=1, include firmware/src và tests/motor-host, link các module nêu trên; không cần MQTT/broker thật.

## Tìm lại line tại chỗ và tiếp tục chặng — 16/10/2026

REQ-LINE-SEARCH-01/[ADR-062](../docs/decisions/ADR-062-local-line-search.md): `platformio test -e native_robot01_bench -e native_robot02_bench -e native_trial -e native_two_robot_bench` **142/142 PASS** (36/37/35/34). Sáu nhóm ca mới mô phỏng mất line theo mắt trái/phải khi tiến/lùi, STOP100ms, xung/nghỉ/timeout600ms (kể cả vòng lặp chậm vẫn phải STOP/nghỉ trước xung kế), mẫu giữa `010` ổn định100ms/≥3 mẫu khác thời điểm, nhiễu/bắt giao điểm không cho tiến, giữ route_cmd_id/index/next_node, một lượt quét/chặng, bộ nhớ hướng cũ/không có hướng, RFID đúng/sai, STOP/hủy/mạng/sonar/cấu hình/vật cản/grant, trànmillis và timeout chặng. Khi đang tìm, following=false và không pass-through/prefetch zone kế. Native legacy tắt tìm line vẫn giữ hành vi cũ.

Host `tests/motor-host/reverse_speed.cpp` link motor/movement/line thật: **PASS01/02**, kiểm pivot tìm trái/phải ±180 và PWM hai channel, nghỉ/STOP0/0, còi tắt khi pivot; hồi quy tiến/lùi/chỉnh/pivot thường theo tuning đang có. Xe02 hiện tiến245/215, lùi235/205, chỉnh250/220, chỉnh lùi245/215, pivot215; đây là cấu hình có sẵn lúc bắt đầu yêu cầu tìm line, không sửa lại thành tuning cũ của các hồ sơ bên dưới. Đầu ra `.qa/line-search-motor-01.exe`/`line-search-motor-02.exe`.

Build ESP32 env01/02 **PASS**; RAM57940byte mỗi ảnh, flash01=961641/02=961649byte; buildID01 `20261016-robot01-line-search`,02 `20261016-robot02-line-search`. Lần chạy PlatformIO trong sandbox bị chặn platforms.lock/cache; chạy lại với quyền cache PASS. Hồi quy phát hiện static_assert áp lên profile cũ đang tắt tính năng, đã sửa chỉ kiểm PWM≤TURN_SPEED khi bật tìm line rồi chạy lại toàn bộ bốn env PASS. Chưa upload, restart Core hoặc thử xe thật; thời gian xung chưa được hiệu chuẩn thành góc quét.

## Thử bắt lại line sau quẹo — 16/10/2026

REQ-TURN-LINE-RETRY-01/[ADR-061](../docs/decisions/ADR-061-single-turn-line-retry.md): native profile01 **30/30 PASS**, profile02 **31/31 PASS**. Ca mới kiểm LEFT timeout lần đầu chỉ STOP/không lỗi/giữ route, lần thử lại vẫn PivotLeft, bắt `000→010` thì tiếp tục Forward; timeout lần hai dừng `LINE_LOST`; mất MQTT trong retry dừng `NOT_CONNECTED`. Hồi quy `native_trial` **29/29 PASS** và `native_two_robot_bench` **28/28 PASS**, capability tắt giữ hành vi timeout cũ.

Build ESP32 env01 **PASS** RAM57900/flash960953byte, env02 **PASS** RAM57900/flash960961byte. Build ID01 `20261016-robot01-turn-line-retry`, ID02 `20261016-robot02-pwm230-turn-retry`. Chưa upload hoặc thử motor thật; cần kiểm tra trên giá kê rồi đoạn cua ngắn, quan sát log `turn_retry=1`, STOP giữa hai lần và góc quét thân xe trước khi chạy cùng robot khác.

## Xe02 tiến/lùi230, duty trái195/phải245 — 16/10/2026

REQ-ROBOT02-PWM-230-ASYM-01: probe host motor cho cả hai profile **PASS**; xe02 tiến195/245, lùi−195/−245, chỉnh line/lùi cùng trim mới, pivot vẫn±245, còi lùi và STOP đúng; xe01 hồi quy PASS. `platformio test -e native_robot02_bench`: **30/30 PASS**, gồm parser, route, mutex, mất mạng/line, interlock điện áp và lùi về S2. Đây là kiểm thử lệnh PWM phần mềm, không phải phép đo bánh xe thật.

`platformio run -e esp32dev_two_robot_bench_02`: **PASS**; RAM57900byte, flash960697byte. Ảnh `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_02/firmware.bin` chứa buildID `20261016-robot02-pwm230-l195-r245`. `git diff --check` không có lỗi whitespace. Chưa upload hoặc thử trên giá kê/sàn; cần xác minh đúng bo02, chiều bánh, STOP và quãng dừng trước chạy tuyến.

## Xe02 PWM245 — 16/10/2026

Giới hạn hồi quy mở rộng: `scripts/test-robot-parity.ps1` PASS motor/servo/encoder MCU/sonar của cả hai profile, nhưng dừng tại `encoder_core.exe` (exit−1073741819). Chạy GDB: hai ca encoder đơn PASS, sau đó nlohmann type_error305 trong ca hai xe. Fixture `twoRobots()` có vòng duyệt `f.toJson()["agvs"]` tham chiếu vào JSON tạm; cần xử lý riêng lifetime fixture. Không ghi nhận toàn bộ script parity PASS, chưa chạy tới servo_core trong lượt này. Không sửa Core/thuật toán encoder trong yêu cầu PWM.

REQ-ROBOT02-PWM-245-01: tăng tiến/lùi/chỉnh/xoay245, trim chạy−40/0, trim xoay0/0. `platformio test -e native_robot01_bench -e native_robot02_bench`: **59/59 PASS**, gồm interlock điện áp, STOP, mất mạng/line và mutex. Host motor cả hai profile PASS: xe02 tiến205/245, lùi−205/−245, chỉnh line/lùi, pivot±245, STOP về0; xe01 vẫn tiến180/220, lùi150/190, pivot210. Không phải phép đo motor vật lý.

`platformio run -e esp32dev_two_robot_bench_02`: **PASS**; RAM57900byte, flash960681byte. BuildID `20261016-robot02-pwm245`, ảnh `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_02/firmware.bin`. Lượt PlatformIO đầu bị sandbox chặn platforms.lock, chạy lại với quyền ghi cache PASS. Chưa upload/kiểm thử bàn hoặc sàn; không đổi ngưỡng sonar, timeout, servo hay PWM01. Cần kiểm tra chiều/STOP/quãng dừng trước chạy tải thật.

## Tạm bỏ chốt pin, sạc thủ công — 16/10/2026

REQ-BATTERY-TEST-BYPASS-01/[ADR-060](../docs/decisions/ADR-060-temporary-battery-bypass-manual-charge.md). `./scripts/build-core.ps1 -Tests -OutputDirectory .qa/battery-bypass-build`: **build Core và full unit PASS**. Ca mới kiểm cả01/02: pin chưa đo hoặc0/29/30/39% vẫn mở ca/GOTO; queue tự giao task, pin0% không lấy mất tuyến đi sạc; kết thúc ca về trạm/IDLE, không CHARGE_START; mất mạng vẫn chặn, bật lại cờ pin khôi phục chốt.

`$env:AGV_CORE_BIN=(Resolve-Path .qa/battery-bypass-build/agv_core.exe).Path; node tests/core-mode.integration.js`: **PASS** main thật ởscope01/02/cảhai, pin`?` và0%, lọc xe, mutex owner/waiter và counter tiếp tục sau restart. Artifact `.qa/core-mode-test-bqVfke`. Lượt đầu `.qa/core-mode-test-evu20t` timeout kết nối trong sandbox; chạy lại với quyền loopback PASS. Broker/cổng/map/runtime/counter riêng, không dùng broker1883 hay điều khiển xe thật.

`node --test tests/dashboard-battery.test.js`: **4/4 PASS**, mock UI hiển thị bypass/sạc thủ công cho ba scope, giữ vô hiệu xe ngoài scope và tương thích khi Core cũ thiếu cờ. `node --test tests/orders.test.js tests/warehouse-actions.test.js`: **20/20 PASS**. JS syntax và `git diff --check` PASS. Chưa thay `build/agv_core.exe`, chưa restart Core vận hành hoặc upload/thử xe thật; F5 sẽ build lại source mới trước khi chạy.

## Xe02 đồng bộ phần cứng xe01, giữ PWM — 16/10/2026

REQ-ROBOT02-PARITY-01/[ADR-059](../docs/decisions/ADR-059-robot02-firmware-parity.md). `platformio test -e native_robot01_bench -e native_robot02_bench`: **59/59 PASS** (29 profile01,30 profile02), gồm xoay/lùi S2, vật cản/mạng/mất line/grant/dedup. Build ESP32 hai env **PASS**, RAM57900byte mỗi profile; flash01=960689byte,02=960697byte. Ảnh02: `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_02/firmware.bin`, buildID `20261016-robot02-hardware-parity`.

`scripts/test-robot-parity.ps1`: **PASS** motor driver (01 tiến180/220,lùi150/190,xoay210;02 tiến/lùi190/230,xoay230), encoder ISR/codec, sonar hai hướng, servo GPIO17/50Hz/0–45°/5s cho cả hai profile; Core servo01/02 và encoder01/02 PASS. Encoder Core bổ sung mẫu có ID khác stream bị từ chối, cùng boot/session/seq của hai xe độc lập. Core build/full unit PASS tại `.qa/robot02-parity-build`.

`tests/servo.integration.js` dùng `AGV_TEST_ROBOT=02`, Core mới và host `servo_02.exe`, broker/cổng/map/counter/DB riêng: **PASS**, Edge headless/browsertrue tại `.qa/servo-integration-UfgiLg/verification.json`. Kiểm tra nâng/hạ5s, ACK đầu bị mất/retry không ghiPWM lại, GOTO bị khóa, kiện đúng02 và chỉ lùi gate sau DONE. Hồi quy01 PASS tại `.qa/servo-integration-MYQyVs/verification.json`. Gateway/order/warehouse **20/20 PASS**. Chưa upload, restart Core vận hành hoặc dùng xe thật.

Fixture `core-mode.integration.js` bản mới **PASS** tại `.qa/core-mode-test-qwmiua`: scope01/02/hai xe nhận odometry qua subscription `agv/+/odometry`, cùng boot1234/connection1/seq1 vẫn riêng từng ID; gói robot_id khác topic bị bỏ qua. Bài thử phát hiện và sửa subscription cũ chỉ `agv/01/odometry`. Tất cả cổng/broker/data tách vận hành.

Xác nhận binary cuối sau sửa subscription: full unit Core **PASS**; servo02 + Edge **PASS** tại `.qa/servo-integration-08ZLVV/verification.json`. Các cổng serial hiện có COM3/COM4/COM5/COM7 chưa được gán danh tính xe02, không chọn cổng/nạp tự động.

## Core thống nhất theo phạm vi xe — 16/10/2026

REQ-CORE-UNIFIED-01/[ADR-058](../docs/decisions/ADR-058-unified-core-active-robot-scope.md): `build-core.ps1 -Tests -OutputDirectory .qa/unified-core-build` PASS toàn bộ unit, gồm scope/lọc GOTO/tự phân công/participating/encoder/ca/pin. `core-mode.integration.js` với broker/map/counter/cổng riêng PASS menu `active01`, `active02`, `active-both`, xe ngoài scope, pin thật, mutex hai xe và restart giữ counter; artifact cuối `.qa/core-mode-test-210bQo`. `orders.test.js` + `warehouse-actions.test.js` 19/19 PASS; ba tệp JS liên quan qua `node --check`. Chưa thay Core vận hành hoặc chạy phần cứng.

## Pin diagnostic dùng cho điều phối — 16/10/2026

REQ-BATTERY-06/[ADR-021](../docs/decisions/ADR-021-display-only-battery.md): Core unit kiểm tra diagnostic calibrated cập nhật `battery=67`, `battery_measured=true`, `battery_control_source=diagnostic`; telemetry `?` kế tiếp không xóa giá trị còn mới và mode normal nhận GOTO khi ≥40%. Telemetry số cũ đổi nguồn về `telemetry`; diagnostic không hợp lệ/không calibrated không được dùng. Build Core/toàn bộ unit Core PASS tại `.qa/battery-control-build-final2`; `node --check AGVwebserver/public/dashboard.js` PASS. Gateway suite thử trong sandbox bị EACCES loopback và đã dừng, nên không ghi PASS/FAIL sản phẩm. Chưa restart Core vận hành hoặc thử về trạm/`CHARGE_START` trên xe thật.

## Một servo GPIO17 xe01, 0/45° và kiện ảo — 15/10/2026

[ADR-057](../docs/decisions/ADR-057-robot01-servo-lift-test.md), REQ-ROBOT01-SERVO-LIFT-01. Host [servo-host/test_main.cpp](servo-host/test_main.cpp) link ServoModule/CoreProtocol/LiftCycle thật, Arduino GPIO/time adapter: PASS01 kênh5/GPIO17 duy nhất, boot duty0/không góc,50Hz/16bit, nâng45 danh định PWM4095 (1250µs), hạ0 PWM3276 (1000µs), đủ5000ms, busy/STOP/mất an toàn/trànmillis/parser/dedup identity. Profile02 PASS servo disabled. Dải1000–2000µs và0/45 là quy ước test, không số đo góc ngoài đời.

[servo_core.cpp](servo_core.cpp) PASS: ID/node/capability/dừng tại kho, ACK không hoàn tất, kết quả sai ID/đến sớm/4999ms không chốt, khóa GOTO, đủ5s nâng/hạ đổi cargo, gián đoạn không tự replay. Core unit hiện có PASS, native01 **28/28**, native02 **27/27**, buildESP env01/02 PASS (`20261015-robot01-servo45-test` cho01). Core build riêng `.qa/servo45-build/agv_core.exe`, không ghi đè executable vận hành.

Gateway/orders/warehouse **31/31** PASS: API/history/WS/đơn cũ giữ nguyên; mới kiểm tra timer gateway/ACK/sai ID/stale không tạo cargo hoặc lùi, gián đoạn/đổi phiên/rời kho dừng lượt, và race RFID trước diagnostic phải chờ `lift.ready/fresh/observed_node` đúng kho. Lượt gateway chạy sandbox đầu gặp EACCES loopback, đã dừng và chạy lại với quyền loopback PASS; không diễn giải là lỗi sản phẩm. Regression Edge/mobile firmware cũ `.qa/warehouse-actions-browser-PULXqS` PASS, không tạo thêm order.

[servo.integration.js](servo.integration.js) chạy broker/Core/gateway/DB/map/counter riêng với receiver host thật: nâng/hạ từ hai nút kho, mấtACK đầu rồi retry cùngcmd_id không tăng số lần phátPWM; đủ5s mới cargo/mới cấp tuyến lùi. Có Edge headless xác minh nhãn nâng/hạ, đúng một kiện trên marker sau nâng, bỏ sau hạ, không pageerror; screenshot đã xem. `.qa/servo-integration-lFbYwZ/verification.json`: PASS/browsertrue, độ trễ lệnhnâng→lùigate5332ms, hạ→gate5129ms (loopback, không độ trễ thực của xe); writes PWM lần lượt2/3 kể cả boot/dedup. RFID/chạy đường của fixture được cấp giả, không nghiệm thu AGVController với cảm biến thật. Hai lượt đầu phát hiện race RFID–diagnostic, đã sửa và thêm unit hồi quy rồi chạy PASS.

Xác nhận cuối với Core mới: `.qa/servo-integration-6rNTrH/verification.json` PASS/browsertrue; thêm assert lý do trong khung ca là Đang nâng/Đang hạ, không nhầm Đang về S1 khi đang chạy servo. Core unit hiện có PASS sau build cuối; servo_core mở rộng PASS đổi boot/sai node/servo chạy ngoài lệnh hiện tại đều ERROR, không nhận cargo. Không thay artifact live trong quá trình build lại.

Chạy lại: compile Core entrypoint src/core/AGVserver.cpp với include/ và libs như scripts/build-core.ps1 vào `.qa/servo45-build`; servo_core.cpp g++17/include; host test cần `-Itests/servo-host`, include src MCU/ArduinoJson, macro bench/profile tương ứng, link ServoModule.cpp/CoreProtocol.cpp/ControlLogic.cpp. Đặt AGV_CORE_BIN/AGV_SERVO_HOST tới binary riêng; `node tests/servo.integration.js`; thêm AGV_SERVO_BROWSER=1 và AGV_PLAYWRIGHT_MODULE tới bundled playwright để kiểm UI. Gateway tests: `node --test --test-isolation=none tests/gateway.test.js tests/orders.test.js tests/warehouse-actions.test.js`.

Chưa nạp COM6, chạy servo/motor, restart Core/gateway thật hoặc sửa DB/counter vận hành. Cần nguồn hạ áp/chungGND và tháo tải/kiểm tra giới hạn cơ khí trước thử. DONE chỉ xác nhận chu kỳ lệnh PWM5000ms, không vị trí/có hàng thật; STOP giữ targetPWM cuối, không đảm bảo trục đã dừng hoặc tải được khóa.

## Xe01 xoay tại GS1 rồi lùi S1 — 15/10/2026

REQ-ROBOT01-REVERSE-HOME-01/[ADR-056](../docs/decisions/ADR-056-robot01-reverse-home-request.md): native01 **28/28**, native02 **27/27**, Core unit, audit **650 tuyến/profile**, route-map check và host [reverse_home.cpp](sonar-host/reverse_home.cpp) PASS. Host sonar dùng SensorModule/DirectionalSonar/CoreNavigator thật với GPIO stub: xoay dùng trước → STOP khi bắt nhánh → xóa mẫu cũ/chờ sonar sau → cản sau giữ route → thông đường lùi → RFID S1 dừng/không xoay trạm. Native kiểm tra E/W/N/S, cờ thiếu/sai/parser/dedup, rời/bắt line, RFID khi chờ không đến đích, STOP/mất mạng/line/timeout/mẫu hỏng; giữ hồi quy grant/mutex/kho/S2.

Build ESP32 env `esp32dev_two_robot_bench_01` và `_02` PASS. Profile01 RAM57364 byte/flash958693 byte; profile02 RAM56884 byte/flash950369 byte. Bản xe01 `20261015-robot01-reverse-home`. Core mới `.qa/reverse-home-build/agv_core.exe`, không ghi đè/restart Core vận hành. Script build chính bị sandbox từ chối CIM; đã compile đúng entrypoint/include/libs bằng g++ trực tiếp, chạy Core unit với TEMP/TMP trong `.qa/reverse-home-build/tmp` PASS.

`tests/node-order.integration.js` PASS hai lượt với AGV_CORE_BIN trỏ Core mới: mặc định capability mới, RIGHT + reverse_after_turn từ heading E tại GS1; chưa hoàn thành khi chỉ xoay/lùi hoặc mới có thẻ mà MCU chưa IDLE/ready; S1 ready hoàn thành một lần/đóng ca, không STATION_TURN_RIGHT. Với AGV_TEST_REVERSE_HOME=0, firmware cũ vẫn LEFT tiến vào/quay tại S1. Artifact `.qa/node-order-test-rSJTiB/verification.json` (mới), `.qa/node-order-test-6zNN1c/verification.json` (cũ). Lượt sandbox đầu không kết nối được broker riêng; hai lượt PASS sau cấp quyền loopback. Cổng/map/counter/DB riêng, không dùng broker1883 hoặc dữ liệu vận hành. Chưa nạp, chạy motor hoặc nghiệm thu sàn; kết quả phần mềm không chứng minh góc/quãng phanh thực, độ ổn định line/RFID hoặc khoảng trống thân xe khi xoay.

Chạy lại: build Core riêng bằng `./scripts/build-core.ps1 -Tests -OutputDirectory .qa/reverse-home-build`, đặt AGV_CORE_BIN rồi chạy `node tests/node-order.integration.js` hai lần (lần cũ đặt AGV_TEST_REVERSE_HOME=0). Firmware build PlatformIO env01/02 không target upload. Audit solo_route.cpp cần include `include` và `firmware/NCKHAGV/NCKHAGV/src`, macro AGV_TWO_ROBOT_BENCH_TRIAL=1/profile tương ứng. Host reverse_home.cpp dùng include tests/sonar-host cùng src MCU, link SensorModule.cpp/CoreNavigator.cpp/ControlLogic.cpp, macro bench/profile01.

Ưu tiên STOP sonic xe01 ngày15/10/2026: 3 host probe HAL/no_echo_gate/directional_test PASS; review Controller đảm bảo STOP sau cập nhật mẫu trước RFID/SPI/command. Build profile01 `20261015-robot01-priority-sonar-stop` PASS (RAM57332byte, flash957949byte). Chưa kiểm thử thời gian thực hoặc quãng trôi, chưa nạp; không xem host probe là phép đo giảm trễ trên xe. [ADR-055](../docs/decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

PWM xoay210 xe01 ngày15/10/2026: `tests/motor-host/reverse_speed.cpp` link MotorModule/MovementModule/LineFollowModule/ControlLogic thật với adapter motor-host PASS01/02. Xe01 pivot duty±210/PWM210 hai channel, xe02 giữ±230; hồi quy tiến/lùi/chỉnh line/còi/STOP PASS, static_assert timeout2500ms cả hai profile. Đầu ra `.qa/turn-line-tuning-build/motor_01.exe`/`motor_02.exe`. FirmwareBuild xe01 `20261015-robot01-pivot210`, chưa nạp/thử line thực tế. [ADR-054](../docs/decisions/ADR-054-robot01-slower-pivot.md).

Lọc pin 15/10/2026: adapter `tests/battery-host/battery_smoothing.cpp` link `BatteryModule` và `ControlLogic` thật, giả lập ADC/clock theo từng ms. Kiểm tra trung bình bỏ mẫu biên, sụt áp ngắn, giữ % lúc motor chạy nhưng điện áp vẫn sống, nghỉ ngắn không mở giữ số, 3 đợt xác nhận, tối đa 1%/5 giây, hồi pin, hai đầu 0%/100%, tràn millis và khởi tạo lại. Chạy từ gốc workspace (thư mục `.qa/battery-smoothing-build` cần tồn tại):

```powershell
g++ -std=c++17 -Wall -Wextra -Itests/battery-host -Ifirmware/NCKHAGV/NCKHAGV/src tests/battery-host/battery_smoothing.cpp firmware/NCKHAGV/NCKHAGV/src/sensors/BatteryModule.cpp firmware/NCKHAGV/NCKHAGV/src/core/ControlLogic.cpp -o .qa/battery-smoothing-build/battery_smoothing.exe
& .qa/battery-smoothing-build/battery_smoothing.exe
```

Host và build ESP32 `esp32dev_two_robot_bench_01`/`02` PASS; chưa nạp hoặc quan sát độ mượt trên xe thật. Artifact/firmwareBuild ghi tại REQ-BATTERY-04/[ADR-021](../docs/decisions/ADR-021-display-only-battery.md).

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

REQ-ROBOT01-SONAR-05: mở rộng no_echo_gate với xung thực tế47487/47494us và update lúc HIGH46ms, fail bản hạn45ms/PASS bản hạn60ms. Giữ regression vật10cm dừng/nhả cùng route và HIGH quá hạn vẫn lỗi; test HAL/directional dùng Config timeout thay vì mốc45ms cũ. Ba host probe PASS; bản xe01 `20261015-robot01-sonar-47ms-fix`, chưa nạp/thử lại phần cứng.

Build bản `20261015-robot01-sonar-diagnostic` profile01 PASS, RAM17,5%, flash73,1%; chưa upload hoặc xác minh log mới trên phần cứng.

REQ-ROBOT01-SONAR-04: HAL `test_main.cpp` kiểm tra snapshot diagnostic đọc hai lần vẫn giữ ISR ready, không phát thêm TRIG, đếm cạnh/trigger, pulse_us, lý do RANGE/NO_ECHO_LOW/TIMEOUT_INVALID, result_age_ms tăng và sentinel UINT32_MAX sau clearReading. HAL, directional_test và no_echo_gate PASS. Bản Serial `20261015-robot01-sonar-diagnostic` chưa nạp/thử trên bo.

Xác minh cuối bản `20261015-robot01-sonar-no-echo-fix`: 3 host probe PASS; native01 27/27, native02 26/26 PASS; build PlatformIO 01/02 PASS. Đầu ra trong `.qa/directional-sonar-build`, ảnh xe01 ở `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` trong project NCKHAGV. Chưa upload hoặc thử lại xe thật.

REQ-ROBOT01-SONAR-03: thêm `tests/sonar-host/no_echo_gate.cpp`, cùng flags/include/link như directional_test. Tái hiện S1 tiến GS1, dwell/grant rồi pivot gặp xung no-target 38 ms; fail bản cũ tại `front.valid()`, PASS sau sửa. Vật 10 cm vẫn STOP/nhả cùng route; HIGH quá45ms vẫn SensorInvalid. Binary `.qa/directional-sonar-build/no_echo_gate.exe`. HAL/directional host cũng PASS sau sửa hạn chờ (profile01 45 ms, cutoff khoảng cách vẫn30ms). Chưa nạp/thử xe thật.

REQ-ROBOT01-SONAR-02: `tests/sonar-host/directional_test.cpp` PASS khi link `SensorModule.cpp`, `ControlLogic.cpp`, `CoreNavigator.cpp` với adapter `tests/sonar-host`, include `firmware/NCKHAGV/NCKHAGV/src`, C++17 và cờ `-DAGV_TWO_ROBOT_BENCH_TRIAL=1 -DAGV_ROBOT_PROFILE_01=1`. Kiểm tra chỉ kích một kênh theo hướng tuyến, yên 50 ms, không tái dùng echo/mẫu cũ, dừng/nhả vật cản lúc lùi cùng route ID, STOP, kẹt HIGH hết 200 ms và mẫu cũ không được gia hạn thu mẫu. Binary tại `.qa/directional-sonar-build/directional_test.exe`; HAL cũ `sonar_hal.exe` PASS. Native 01 27/27, 02 26/26 và build ESP32 01/02 PASS. FirmwareBuild xe01 `20261015-robot01-directional-sonar`; chưa nạp/thử xe thật. [ADR-055](../docs/decisions/ADR-055-robot01-dual-sonar-edge-warning.md). Với PowerShell, bọc dấu nháy các đối số `-I.../.pio/...` khi biên dịch native thủ công.

Bản cuối đã đóng gói ở `build/robot01-dual-sonar/`. Sonar HAL host PASS; native xe01 27/27, xe02 26/26; fixture trước/sau và chấm không bị RFID che PASS ở `.qa/encoder-test-vemfv5`. [Kết quả và cách chạy thử](../docs/testing-sonar-robot01-2026-10-15.md).

Kiểm thử phần mềm sau REQ-ROBOT01-SONAR-01: native profile01 biên dịch thủ công với Unity/ArduinoJson đã cài, **27/27 PASS**, gồm dừng/nhả vẫn giữ route và đóng băng timeout chặng. `tests/encoder_core_tests.cpp` PASS cho cảnh báo/điểm trên cạnh, xóa khi hết vật cản, không ghi vào checkpoint/task/mutex. Core C++ build riêng từ `src/core/AGVserver.cpp` PASS; `tests/core_tests.cpp` PASS với `TEMP/TMP` trong `.qa/sonar-build` để không ghi ngoài workspace. PlatformIO `esp32dev_two_robot_bench_01` build PASS, ảnh ở `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_01/firmware.bin`. Fixture `tests/encoder.integration.js --browser` chạy với Core/broker/DB/map/counter riêng PASS trên Edge desktop/mobile; kiểm tra MQTT diagnostic → Core → gateway → cảnh báo hướng, chấm SVG, màu đỏ cạnh, xóa cảnh báo và hồi quy encoder/task/mutex. Đầu ra `.qa/encoder-test-8amIsK`. Chưa flash, chưa thử trên giá kê/sàn hoặc đo quãng phanh. `scripts/check_workspace.py` vẫn báo link gãy cũ trong docs 13/14/15/19, các artifact `.qa` cũ và README của dependency PlatformIO; JSON Schema chưa cài.
