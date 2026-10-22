# Hồ sơ kiểm thử AGV

## 22/10/2026 — Kiểm tra trước khi đẩy GitHub

Source tại lượt đẩy có tuning mới hơn các hồ sơ v14 bên dưới: Kp01=65/base125; Kp02=65/base120, capture phải02=190, trim quay02=−15/−30, reverse02=165. Giữ các giá trị workspace này. `scripts/test-turn180.ps1` PASS01/02. `scripts/test-fast-line.ps1` FAIL tại static_assert profile01 do fixture còn khóa Kp80; chưa chạy tới profile02, bảng duty tĩnh của fixture cũng cần đối chiếu lại với tuning mới. Không sửa tuning/test để che kết quả. Build01 PASS ở hồ sơ v14 bên dưới thuộc thời điểm Kp01=80; không coi đó là build đã xác minh cho tuning mới nhất. Lượt đẩy GitHub không upload hoặc điều khiển phần cứng.

## 22/10/2026 — Bộ thông số nhúng xe01, v14

REQ-ROBOT01-PROFILE-TUNING-01: `scripts/test-fast-line.ps1` PASS01/02 với bảng PD tĩnh01=100:255/0,110:215/45,010:135/125,011:55/205,001:0/255; kiểm đầu ra driver trước RFID, STOP/000/grant/mạng/sonar. Build `esp32dev_two_robot_bench_01` PASS, RAM58108/327680byte, flash966893/1310720byte, buildID `20261022-robot01-profile02-tuning-v14`; SHA256 firmware.bin `0FBEAAAA458F274BC1E3E18200CDFEF63B8E2CC27F3875F21CC293A0CBCEB468`. Không upload hoặc phát lệnh chuyển động trong lượt này.

Đọc GET gateway và subscribe chỉ đọc `agv/01/diagnostic`: xe01 online, firmwareBuild đúngv14, AGV_SAFE_STOP/BATTERY_UNDERVOLTAGE, nodeS1, rfidAgeMs26, batteryAdcMv1590, batteryVoltage7,859603405V, batteryPercentage0, calibratedtrue, motor0/0, Wi-Fi/MQTTtrue. Đây là trạng thái lúc lấy mẫu, chưa xác minh điện áp đồng hồ hoặc nguyên nhân vật lý; ngưỡng cấu hình3S10,2V giữ nguyên. Kết quả parity lượt trước mới qua các probe MCU, chưa ghi nhận toànscript PASS vì lượt chờ bị người dùng ngắt.

## 22/10/2026 — Bắt line sớm khi quay180°, turn180-v13

REQ-DUAL-TURN180-FAST-CAPTURE-01 / [ADR-077](../docs/decisions/ADR-077-turn180-fast-stop-confirm.md). `scripts/test-turn180.ps1` PASS cả01/02 với navigator→Movement→MotorModule thật và adapterGPIO/time. Kiểm ứng viên010/011/110 ở nhánh cuối gâySTOP trước bướcRFID; giữSTOP100ms/≥3 mẫu mới rồi trảSTOP một vòng trước tiến/lùi. Nhánh90° trung gian không kết thúc quay, không đếm trùng một mảngline; loại001/100/101/111, mẫu trùng timestamp, gap cũ, nhiễu, millis rollover. Kiểm capPWM180 khi đi tiếp, stationReady sau xác nhận, timeout2500ms gốc, mấtgrant và xác nhận lại, STOP/mạng/cấu hình/sonar/vật cản. Nhịp2ms trong probe là thời gian mô phỏng, không phải số đo trênESP32. Mãcontroller đã được rà điều kiện hoãnRFID/logsonar theofastTurnSampling; chưa chạy toàncontroller với thiết bị thật.

`scripts/test-line-confirm.ps1` và `scripts/test-fast-line.ps1` PASS01/02. Native `native_robot01_bench`/`native_robot02_bench` PASS102/102 sau cập nhật ba ca tuyếnG1/vềS1/S2: mẫu cách nhau ít nhất2ms và đủ bước xác nhận100ms. Lần đầu sáu lỗi (ba mỗi profile) do các ca này còn dùng mẫu cách1ms/đi tiếp ngay; cập nhật kỳ vọng theo policy mới, không bỏ assertion chốt an toàn. Không rerun parity/servo vì không sửa các module đó.

BuildESP32 hai env PASS: RAM58108/327680byte mỗi ảnh, flash01=966877/02=966885byte. Đã kiểm chuỗi `20261022-robot01-turn180-v13` / `20261022-robot02-turn180-v13`; SHA25601 `688200152C51051271FEAA701B22DB32E0C22D9553D449B27ED28DAE32AF160E`,02 `9A461657136593106A0FF738B8FFCD190B3F950F6DC8A527854A02290DA3E435`. Chưa upload/điều khiển xe, đo khoảng mẫu hoặc quãngtrôi thật. Khi thử xem `[TURN180] confirmed ... sample_target_ms=2 max_gap_ms=...`; không coi mục tiêu2ms là bảo đảm thời gian thực.

## 22/10/2026 — Bỏ xác nhận mép001/100, narrow-v12

REQ-DUAL-EDGE-LINE-ROLLBACK-01 / [ADR-076](../docs/decisions/ADR-076-dual-narrow-line-confirm.md). Probe hồi quy mới trước sửa thất bại tại `!r.nav.following()` khi001/100 kéo dài sau000, chứng minh policyv10/v11 vẫn nhận mép. Sau sửa `scripts/test-line-confirm.ps1` PASS01/02: chỉ010/011/110 phục hồi;001/100 liên tục trongTurning không khởi độngCapture, liên tục800ms trongCapture không hoàn tất, rẽtrái/phải và rollover; mẫu mép làm lại chuỗi xác nhận mắtgiữa. Kiểm cả timeout, grant, mạng, sonar, STOP, reversecentered-only và capPWM. `scripts/test-fast-line.ps1` PASS01/02, gồm PWM01=110..230 và output xe02 cũ. Native `native_robot01_bench`/`native_robot02_bench` PASS102/102; không rerun parity/servo vì phần đó không thay đổi.

BuildESP32 cảhai env PASS, RAM58092/327680byte mỗi ảnh, flash01=966377/02=966385byte. Đã kiểm chuỗi `20261022-robot01-narrow-v12` / `20261022-robot02-narrow-v12` trong binary; SHA25601 `3CE1CAC4303B9FBCC2FAE2F43E8248A09E31A18C84009923B7EB0B56A9E12A5B`,02 `E78B30909129FF2A077FBBAB33CEE1221491535A8FC5982AEF171D9ED3F5A152`. Diff-check phần thay đổiPASS. Chưa upload hoặc thử rẽ trên xe thật; kết quả kiểm phần mềm không xác nhận đã loại mọi nguyên nhân bắt nhầm vạch.

## 22/10/2026 — Sàn/trần PD chống khựng riêng xe01, pd-floor-v11

REQ-ROBOT01-PD-ANTI-STALL-01 / [ADR-073](../docs/decisions/ADR-073-forward-line-pd-two-robots.md). `scripts/test-fast-line.ps1` PASS01/02, gồm duty tĩnh sau reset và giới hạn mọi nhịp PD: xe01 luôn110..230 khi pattern hợp lệ; xe02 giữ0..255. `scripts/test-line-confirm.ps1` PASS01/02 cho001/010/011/100/110, recovery/capture/deadline/cap và các chốt. `scripts/test-robot-parity.ps1` PASS motor/servo/encoder/sonar hai profile và Core encoder/servo; cập nhật các expected PWM cũ trong probe theo hằng profile hiện tại, không đổi runtime ngoài giới hạn PD xe01.

`platformio test -e native_robot01_bench -e native_robot02_bench` PASS102/102. BuildESP32 hai env PASS, RAM58092/327680byte mỗi ảnh, flash01=966433/02=966473byte. Binary01 chứa `20261022-robot01-pd-floor-v11`, SHA256 `D6948681946A2F32F75DAE54F442C416BB1D10E9CC470A97E497DC5BA070D192`; binary02 vẫn `20261022-robot02-line-edge-v10`, SHA256 sau build `D481FA9175B57D5A53A83935A9C2CE333B7911B3005BF4DABF4B73ECCB15AB3B`. Chưa upload, thử kê bánh, chạy sàn hoặc đo dòng/điện áp thật.

## 22/10/2026 — Xác nhận thêm mép line001/100, line-edge-v10

REQ-DUAL-EDGE-LINE-CONFIRM-01 / [ADR-076](../docs/decisions/ADR-076-dual-narrow-line-confirm.md). `scripts/test-line-confirm.ps1` PASS01/02 với chuỗi thật navigator→Movement→PD→MotorModule: Following tiến và LEFT/RIGHT TurnCapture nhận001/010/011/100/110 ổn định≥100ms/≥3 mẫu mới/gap≤50ms, gồm rollover.001/100 phải giữ cùng phía; ca đảo001↔100 giữSTOP, sau đó chỉ tiếp tục khi một phía đủ bằng chứng. TurnCapture cần một mẫu trước chứng minh đã rời line cũ, không dùng cùng mẫu mép cho cả rời và bắt lại. Lùi vẫn centered-only010;000/101/111, mẫu trùng/stale, mấtgrant/vật cản/mạng/sonar/STOP/fault đều không làm xe tự chạy. Quét motor≤600ms, chờ capture≤1500ms và capPWM giữ nguyên.

`platformio test -e native_robot01_bench -e native_robot02_bench` PASS102/102; lần đầu compile lỗi do test còn tên cờv9, sau cập nhật tên `LINE_RECOVERY_TRACK_CONFIRM` thì chạy lại PASS. `scripts/test-fast-line.ps1` PASS01/02. BuildESP32 hai env PASS, RAM58092/327680byte mỗi ảnh, flash01=966433/02=966473byte trên1310720. Binary chứa đúng `20261022-robot01-line-edge-v10` / `20261022-robot02-line-edge-v10`; SHA25601 `429AF720D2D8433BC794B8DEFC017501827915B784707FE1BA57A4E533D55BB9`,02 `403981538DD619B17379C84CBA55765286DF1A5627A665C1DD7F57AF741BDB43`. Không sửa/rerun servo trong lượt này; giữ cấu hình180/100 đã kiểm ởv9. Chưa upload, điều khiển hoặc đo xe thật.

## 21/10/2026 — Xác nhận line hẹp cảhai xe, line-confirm-v9

REQ-DUAL-NARROW-LINE-CONFIRM-01 / [ADR-076](../docs/decisions/ADR-076-dual-narrow-line-confirm.md). Probe chẩn đoán trước sửa tái hiện011/110 trongTurnCapture chốtLINE_LOST ở600ms cho cả01/02, không cầnserver/reset. Native sau sửa PASS102/102(51mỗi profile). Ba ca legacy side-only còn guard theoGENTLE_FORWARD_CORRECTION của source, không dùng chúng làm bằng chứng kiểm nhánh mới; probe `scripts/test-line-confirm.ps1` kiểm trực tiếp tínhnăng mới.

Host mới nối navigator→Movement→PD→MotorModule vớiGPIO/timeadapter: `scripts/test-line-confirm.ps1` PASS01/02, kiểm010/011/110 ổnđịnh, LEFT/RIGHT/rollover, quét≤600ms/tổngchờ1500ms, dừng sau600ms trước xácnhận, giữroute/index/grant/capPWM, wide/blank/side-only/nhiễu/timestamptrùng/gap>50ms, hazardmạng/cảm biến/STOP/grant/vật cản và faultkhôngreplay. `scripts/test-fast-line.ps1` PASS01/02 sau cập nhật assertions profile01 để khớp nềnPD người dùng đã ghiđè (không đổi runtime motor); vẫn kiểm output trướcRFID, PWM không ngắt khi cùng chiều và mọi chốt. `scripts/test-robot-parity.ps1 -ServoOnly` PASSservo01/02/profiletắtservo/CoreServo01/02, giữ180/100. Native testblankline01 sửa deadline5s cũ thànhgracecấuhình, capturetest dùngtimeoutchờmới vàSTOP khi narrow, không nới motor vì test.

BuildESP32 hai env PASS, RAM58092/327680byte mỗi ảnh, flash01=966361/02=966369byte trên1310720. Binary chứa đúng buildIDline-confirm-v9, SHA25601 `36708100DA8BA861E59AF43B6B47CE117B51F4C1EB5596E6F753175905218F39`,02 `7D29B98830A0BD90FD92CB132BBFB4B7B5DC373E2167DE3629CFC3B0B6FD9BFE`. Diff-check các file sửa trong lượt này PASS. Không thay configmạng/servo/tuningPD/PWM/watchdog/Core/counterNVS; chưa upload/điều khiển hay đo xe thật.

Giới hạn hồi quy mở rộng: `native_trial`(không phải hai envbench đang nạp) ERRORED với1caFAILED `test_mutex_exit_clearance_resets_on_new_route_fault_and_session` dòng375,49PASS/51được thu thập. Test mongForward sau vật cản trong cấu hình khôngsonarsau; policy hiện tại chốtOBSTACLE khiREAR_ECHO=-1. Probe tối thiểu biên dịch cùng cấu hình với CoreNavigator.cpp nguyênbản từ folder người dùng gửi và bản hiện tại đều cho `narrow=0 rear_echo=-1 after_obstacle=STOP error=OBSTACLE`. Đây là lỗi giả địnhtest có từ nềnnguồn, không phải regressiontínhnăngline và không sửa bằng cách bỏchốt vậtcản. Không tuyên bố toàn bộ suiteworkspace PASS.

## 21/10/2026 — Khôi phục mạng xe02 về server máy người dùng

REQ-ROBOT02-LOCAL-NETWORK-RESTORE-01 / [ADR-075](../docs/decisions/ADR-075-sync-validated-two-robot-firmware.md): chẩn đoán đọc-only xác nhận healthCore/MQTT/storage khỏe, scope01+02/caRUNNING, fleet.agvs trống và pending_tasks trống ở thời điểm đọc; broker0.0.0.0:1883 và hotspot192.168.137.1. MQTT02 chỉoffline retained, không bản tin diagnostic mới. Cấu hình/binary02servo-v8 vẫn có SSIDsource khác mạng01/backup trước ghi đè; authMQTT cũng khác. Không lấy kết luận này làm bằng chứng đo Wi-Fi/nguồn trên bo.

Theo duyệt người dùng: backup mạng02 và firmware.bin trước đổi trong `.qa/firmware-overwrite-backups/20261021-170039/network-before-restore02/`, kiểm hash backup, chép mạng01 sang02. VERIFIED network02 bằng hashnetwork01, hai file vàbackup gitignored, hashRobot02Config.h trước/sau không đổi; env02static_assert giữID02. Không sửa payload, counter/NVS, servo180/100, motor/line/watchdog, không chạy suite motor vì chỉ thay private config. Build env02 PASS, RAM58092/327680byte, flash965681/1310720byte; binary đã kiểm chứa SSIDkhôi phục và buildIDrobot02servo-v8. SHA256 `7937F8FCFE9AEB2ED7921D162A2283E651599D72C7CC21386124D20BB788EBAF`. Diff-check các tài liệu thay đổi PASS. Chưa upload hoặc xác nhận xe02 đãonline.

## 21/10/2026 — Đồng bộ servo xe02 giống xe01

REQ-DUAL-SERVO-CALIBRATION-01: cảhai profile dùng hạ180°/nâng100°,544–2400µs/50Hz/16bit/GPIO17/LEDC5/5000ms; buildID01/02 servo180-100-v8. Host yêu cầu cảhai có cùng góc/dải và bảng0/45/90/100/180°=1782/3302/4823/5160/7864. `scripts/test-robot-parity.ps1 -ServoOnly` PASS servo01/02/profile tắt servo và Core servo01/02: boot duty0, khóa motor/ACK khôngDONE/STOP/ngắt thao tác. BuildESP32 hai env PASS, RAM58092byte mỗi ảnh, flash01=965681/02=965689byte. SHA256 binary02 `84A56AA58D3A1C273A8B313F6C91F4FE95B23F812DD96AA5380A0369C2B2F810`;01 không đổi so với lượt trước. Diff-check test/tài liệu PASS; không sửa whitespace tuning profile có từ trước. Chưa upload hoặc chạy servo thật. Các kết quả xe02 dùng1000–2000µs bên dưới là lịch sử trước yêu cầu mới.

## 21/10/2026 — Chốt servo01 hạ180°/nâng100°

Theo xác nhận mới REQ-ROBOT01-SERVO-CALIBRATION-01, thay0/45 tạm bằng hạ180°/nâng100°; dải544–2400µs/50Hz/16bit/GPIO17/LEDC5/5000ms giữ nguyên. Host thêm mốc100°=1575µs/duty5160 và kiểm cấu hình up100/down180 của01, up45/down0 của02;180°=2400µs/duty7864. Boot vẫn chỉ ghi duty0, không tự về180°. BuildID01 `20261021-robot01-servo180-100-v8`. `scripts/test-robot-parity.ps1 -ServoOnly` PASS cảservo01/02/profile tắt servo và Core servo01/02; buildESP32 cảhai env PASS, RAM58092byte mỗi ảnh, flash01=965681/02=965689byte. SHA256 binary01 `3195583360AE1B4EED9A1A2D8A02E6FE28DA5B51E3C3A2A7C8DD3602D0A1457B`. Diff-check các file test/script/tài liệu PASS; profile01 vẫn có whitespace từ thay đổi tuning trước lượt này (dòng control_ms/comment và blankEOF), không sửa ngoài phạm vi servo. Chưa upload, chạy cơ cấu hoặc điều khiển xe.

## 21/10/2026 — Hiệu chỉnh dải xung servo xe01 theo ảnh

REQ-ROBOT01-SERVO-CALIBRATION-01 / [ADR-057](../docs/decisions/ADR-057-robot01-servo-lift-test.md): `scripts/test-robot-parity.ps1 -ServoOnly` PASS servo01, servo02, profile không servo và Core servo01/02. Servo01 giữGPIO17/LEDC5/50Hz/16bit,544–2400µs; kiểm độc lập duty tại0/45/90/180° lần lượt1782/3302/4823/7864. Servo02 vẫn1000–2000µs, duty3276/4095/4915/6553. Kiểm không phát góc lúc boot/chỉ duty0, không ghi kênh motor hoặc kênh servo khác, từ chối góc ngoài0…180/kênh không bật, chu kỳ5s/ngắt thao tác/rollover và Core không coi ACK/kết quả sớm là DONE. Không chạy lại suite line vì thay đổi chỉ servo; không sửa lỗi deadline line vốn có của bản nguồn.

BuildESP32 lần dải xung v7 PASS cảhai env: RAM58092byte mỗi ảnh, flash01=965677/02=965689byte. Chưa upload, phát lệnh nâng/hạ, đổi góc trên xe hoặc restart server. Thời điểmv7 giữhạ0/nâng45 vì ảnh chỉ có dải xung; người dùng đã xác nhận180/100 trong cập nhậtv8 phía trên. Kiểm thử host không phải đo vị trí cơ khí hoặc tải thật.

## 21/10/2026 — Khôi phục mạng xe01 về server máy người dùng

REQ-ROBOT01-LOCAL-NETWORK-RESTORE-01: trước sửa, GET health/shift xác nhận Core/MQTT/storage khỏe, active_robot_ids01+02, phaseRUNNING; broker nghe0.0.0.0:1883 và hotspot có192.168.137.1. Nghe MQTT thụ động6s nhận01/status=offline retained, không có01 telemetry/diagnostic mới;02 online và có6 diagnostic stale-sync-v5. Cấu hình source01 có SSID/mật khẩu và auth MQTT khác cấu hình cục bộ trước ghi đè. Đọc COM6 thụ động7s có log sonar, không đủ xác nhận nguyên nhân Wi-Fi bằng mã disconnect. API WinRT đọc hotspot không khả dụng; không khẳng định đã đo mật khẩu hotspot trực tiếp.

Đã khôi phục `LocalNetworkConfig01.h` từ backup cũ (hash bằng nhau), source01 trước đổi được giữ trong `network-before-restore/` của backup20261021-170039. Đối chiếu61 file: chỉ mạng01 khác source,60 file còn lại khớp. Build `esp32dev_two_robot_bench_01` PASS, RAM58092/327680, flash965677/1310720; binary SHA256 `6CB39B4BD9CF493A5CD6567F79114DFC50A272353BD0623D556DC670DFC43207`, buildID battery-lock-v4 giữ nguyên. Không rerun native vì chỉ phục hồi cấu hình mạng, không sửa mã điều khiển; lỗi test deadline5s/7s của nguyên bản ở hồ sơ dưới vẫn tồn tại. Chưa upload, chưa clear lỗi/hủy task hoặc reset ca: snapshot trước bàn giao còn task-79911→K6 và task-79912→S1 chờ. Người dùng nạp COM6 khi đã kiểm tra xe; chưa tuyên bố01 đã reconnect.

## 21/10/2026 — Ghi đè nguyên bản firmware người dùng gửi

REQ-FIRMWARE-STABLE-SYNC-01 / [ADR-075 quyết định thay thế](../docs/decisions/ADR-075-sync-validated-two-robot-firmware.md): theo yêu cầu mới nhất, lấy toàn bộ61 file project nguồn `C:/Users/DELL/Downloads/firmware/firmware/NCKHAGV/NCKHAGV/` ghi đè project firmware workspace, gồm src/test/lib/config/README/battery.cpp/.gitignore và mạng riêng01/02 (ngoài Git). Không lấy `.pio*` hoặc `.vscode`; sao lưu trước tại `.qa/firmware-overwrite-backups/20261021-170039/`. File mạng fallback cũ đã chuyển vào backup. Kiểm SHA25661/61 file khớp source, không tự sửa các bổ sung v5/v6 vào bản gốc.

Build ESP32 `esp32dev_two_robot_bench_01` và `_02` PASS. RAM58092 byte mỗi ảnh; flash01=965685 byte,02=965713 byte trên1310720. Binary chứa đúng buildID nguồn `20261021-robot01-battery-lock-v4` / `20261021-robot02-battery-lock-v4`; SHA25601 `92A9F325F87626B455272CD2FD14C3FACD566B3953E156AB574DEF950600A0EC`,02 `D12614B9FE2A2E759B3B096CBFE4BC432D4A8B2BED13223041282AD9BF98A44F`.

Native02 PASS51/51. Native01 có50 ca PASS và1 ca FAIL ở `test_robot01_blank_line_waits_for_rfid_without_moving`, source line784: test đợi LINE_LOST tại `LINE_LOSS_CONFIRM_MS=5000`, trong khi profile source đặt `LINE_LOSS_GRACE_MS=7000`, nên tại5s error vẫn NONE (Expected7 Was0); runner báo ERRORED/CTRL_BREAK_EVENT và exit1. Giữ nguyên mã/test nguồn theo yêu cầu ghi đè, không ghi native01 PASS. Cũng giữ nguyên việc source không bật side recovery và không phát lastCommandId. Chưa upload/chạy xe hoặc restart Core, không sửa NVS/counter vận hành.

## 21/10/2026 — Bật lại side recovery xe01, bản v6

REQ-ROBOT01-GENTLE-SIDE-RECOVERY-01 / [ADR-062 bổ sung21/10](../docs/decisions/ADR-062-local-line-search.md): bật `AGV_LINE_RECOVERY_SIDE_CONFIRM=1` riêng01, giữ nền PD/PWM/grace7s/quét600ms/capture và lastCommandId. Ba ca `test_robot01_side_recovery_*` trước đây guard theo gentle nên bị bỏ qua trên nền PD; đã chuyển sang cờ side recovery, assert profile01 phải bật và kiểm lỗi latched tại deadline grace7s.

Native `native_robot01_bench` PASS54/54, ba ca side recovery thực sự chạy trên01: nhóm100/110 và001/011 đủ bằng chứng mới tiếp tục cùng route/index, millis rollover, loại nhiễu/đổi nhóm/timestamp trùng/gap>50ms, không tự clear lỗi, thu hồi grant/vật cản phải xác nhận lại, lùi vẫn centered-only. Hồi quy `native_robot02_bench` PASS54/54; các ca riêng side recovery không áp dụng profile02. Build `esp32dev_two_robot_bench_01` PASS, RAM58092 byte/327680 và flash965765 byte/1310720. Binary có đúng buildID `20261021-robot01-side-recovery-v6` và `lastCommandId`; SHA256 `1F84E90E61AD5F37B2590E91F9603A318DF2920E17727BC007A623520D782D1E`. Chưa upload hoặc thử xe thật trong lượt bật lại.

## 21/10/2026 — Khôi phục diagnostic đồng bộ command, bản v5

REQ-STALE-COMMAND-RECOVERY-01 / [ADR-072 bổ sung](../docs/decisions/ADR-072-stale-command-counter-recovery.md): source `MQTTModule::sendTelemetry` thiếu `lastCommandId`; đã khôi phục từ `highwater_` của network worker. Core unit `tests/core_tests.cpp` được compile lại vào `.qa/stale-command-sync/core_tests.exe` và PASS, gồm ca high-water5000 → giữ ERROR, không phát STOP/retry mã thấp, operator clear → RESET_FAULT>5000, chỉ diagnostic mới mở IDLE. Lượt chạy sandbox đầu bị chặn rename file tạm (Windows error5); chạy lại với quyền phù hợp PASS, không thay counter vận hành.

PlatformIO build `esp32dev_two_robot_bench_01` và `_02` PASS. RAM58092 byte mỗi ảnh; flash01=965725 byte,02=965749 byte trên1310720. Hai binary đã kiểm có `lastCommandId` và đúng buildID `20261021-robot01-stale-sync-v5` / `20261021-robot02-stale-sync-v5`. SHA25601 `3EC9FFDE20B72A051EDF4DBD1C14FF929F76E0E9A27D95C3A8621656D4C5630A`,02 `6B1F66B7CC2ED0EAF91CF115F21AF7284B377B13A63AE29CE580B2A0C04E52AD`. Không thay tham số/mã điều khiển line; native54/54 trước đó không kiểm producer MQTT, không tính là kiểm tra wire thực tế của bản v5. Chưa upload, clear lỗi qua API hoặc đo diagnostic mới trên xe thật trong lượt sửa.

## 21/10/2026 — Đồng bộ firmware hai xe đã chạy ổn

REQ-FIRMWARE-STABLE-SYNC-01 / [ADR-075](../docs/decisions/ADR-075-sync-validated-two-robot-firmware.md): đồng bộ có chọn lọc source từ thư mục người dùng cung cấp, loại trừ `LocalNetworkConfig01.h`/`02.h`. Native `native_robot01_bench` PASS54/54 và `native_robot02_bench` PASS54/54. Ca mới kiểm PD cho010/100/001, cache dưới10ms, cap duty,000 reset, gap dài và millis rollover; capture rẽ phải dùng đúng PWM/countersteer rồi xác nhận010; xe01 STOP ngay ở000, giữ task trước7s và chốtLINE_LOST đúng deadline7s. Các hồi quy node-order/RFID/mutex/sonar/mạng/lùi/capture trái cùng PASS.

Build cuối `esp32dev_two_robot_bench_01` và `_02` PASS. RAM58092 byte mỗi profile; flash01=965677 byte, flash02=965705 byte trên1310720. SHA256 `firmware.bin` xe01 `6CB39B4BD9CF493A5CD6567F79114DFC50A272353BD0623D556DC670DFC43207`, xe02 `3B4824795242586B927E14A505192DD48C537F4B67E2255313C662FE323E58A6`. Chưa upload hoặc thử xe trong lượt đồng bộ.

## 19/10/2026 — Recovery line bên xe01 sau khôi phục firmware

REQ-ROBOT01-GENTLE-SIDE-RECOVERY-01 / [ADR-062](../docs/decisions/ADR-062-local-line-search.md): trước sửa ba ca native mới FAIL khi line bên vẫn dẫn tới pivot hoặc không kết thúc recovery. Sau sửa `platformio test -e native_robot01_bench -e native_robot02_bench` PASS98/98 (49 mỗi profile; ca riêng01 bỏ qua ở02). Kiểm cùng phía001/011,100/110 đủ100ms, millis rollover, giữroute/index, reverse centered-only, đổi phía/center/wide/blank/gap>50ms, đọc lặp cùng timestamp, thu hồi grant/vật cản xóa bằng chứng, deadline600ms và không tựclear lỗi đã chốt.

`scripts/test-fast-line.ps1` PASS01/02: navigator→movement→GPIO/PWM sau000→line bên,STOP trong xác nhận rồi đúng duty/action;110=190/150,011=130/210,hard100/001 giữ một bánh. Không có xe thật/log tại thời điểm báo lỗi; đây là tái hiện logic bằng input giả lập, không khẳng định nguyên nhân duy nhất hoặc đã sửa hoàn toàn khả năng bám cua vật lý. BuildID01 `20261019-robot01-side-recovery-v3`; các hồ sơ PD trước khôi phục phía dưới không mô tả binary hiện tại.

Build ESP32 `esp32dev_two_robot_bench_01` và `_02` PASS. RAM58076byte mỗi profile; flash01=963745byte,02=963921byte. Binary xe01 tại `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_01/firmware.bin` đã kiểm chuỗi buildIDv3; chưa upload hoặc tác động broker/Core vận hành.

## 19/10/2026 — Trợ bám cua riêng xe01, bản v7

REQ-ROBOT01-LINE-ASSIST-01 / [ADR-073 v7](../docs/decisions/ADR-073-forward-line-pd-two-robots.md): host `scripts/test-forward-pd.ps1` PASS5 cấu hình (profile01 assist bật/tắt, rollbackPD và profile02 bật/tắtPD). Đường GPIO→pattern→navigator→Movement→driver kiểm110=180/120,100=180/80,011=120/180,001=80/180,111/101=120/120,010=210/210; đổi phía cùng timestamp phản ứng ngay, wide/010 không giữ lệch/D cũ. Mọi duty assist đều tiến hai bánh; PivotLeft/PivotRight thường, pivot phục hồi180, capture210 và lùi giữ đầu ra cũ. ChặngM2→G6 vẫn cần grant, mất grant/STOP/mạng/sonar/config/servo/000 xuất0/0;000 ở600/1000ms dừng rồi001 đủ bằng chứng ở1300ms mới bám lại cùngroute/index.

PlatformIO native profile01/02 PASS107/107 (54+53), gồm hồi quy node-order/rẽ/capture/RFID/mutex và ca assist chỉ hoạt động profile01. SHA256 trước/sau của `CoreNavigator.cpp`, `AGVController.cpp`, `MovementModule.cpp`, `MotorModule.cpp`, `Robot02Config.h` được đối chiếu không đổi trong lượt v7.

BuildESP32 hai env01/02 PASS; RAM58108byte mỗi ảnh, flash01=964773/02=964845byte trên1310720. Binary chứa đúng buildID `20261019-robot01-curve-assist-v7` / `20261019-robot02-line-confirm-v6`; SHA25601 `57265F6D3B83DDCF57F6001A02B384B04FA30A54428BAFFDED7120EF38872F26`,02 `A8F455F71106D4E241C4F05BD35E2A16AC45C0A4EC1ACB350E22B3143BCCFD17`. `git diff --check` PASS. Chưa upload/chạy xe thật.

## 19/10/2026 — Tách hạn quét và xác nhận mất line, bản v6

REQ-LINE-CONFIRM-GRACE-01 / [ADR-073 v6](../docs/decisions/ADR-073-forward-line-pd-two-robots.md): PlatformIO native hai profile PASS105/105 (53+52). Hai ca mới/profile kiểm ứng viên ở550ms xác nhận650ms,1200ms xác nhận1300ms, cả millis rollover/tiến và010 khi lùi; giữ route301/index0. Quét dừng tại600ms,000/101/111 không phát motor trong phần chờ; hết deadline2s xe01/7s xe02 chốtLINE_LOST, ứng viên muộn không gia hạn và line trở lại không tựclear. Ca hazard mở rộng kiểm mấtWi-Fi/MQTT, sonar/config, STOP, grant và vật cản trong xác nhận sau600ms; các ca rẽ/capture/RFID/mutex/chặng cũ vẫnPASS.

Host `scripts/test-forward-pd.ps1` PASS4 cấu hình (01/02 × PD bật/tắt); nối navigator→Movement→driver tạiM2→G6 kiểm duty0/0 lúc600/1000ms dù task còn active, sau001 ở1200ms đủ100ms thì xuất đúng dutyPD, giữroute700/index0. Không thay tuningPDbase01=210/02=160. Chưa upload, chưaclear_error/GOTO hoặc chạy xe thật.

BuildESP32 `esp32dev_two_robot_bench_01` / `_02` đềuPASS; RAM58108byte mỗi ảnh, flash01=964609/02=964845byte trên1310720. Đã xác minh chuỗi buildID `20261019-robot01-line-confirm-v6` / `20261019-robot02-line-confirm-v6` trong binary. SHA25601 `1BBFEAEA0A7A17BFFA71396FB7F5D870BE2801414ADDE1BA1D599262245A1EAB`,02 `1DDAC1FDCC2F6D49D5A1A0A09C6A41BC84ED2DE7C88DE306D1DE7DFC212B05E7`. Artifact tại `firmware/NCKHAGV/NCKHAGV/.pio/build/<env>/firmware.bin`. `git diff --check` PASS.

## 19/10/2026 — PD nhận lại001 sau000, bảnv5

REQ-PD-SIDE-RECOVERY-01 / [ADR-073](../docs/decisions/ADR-073-forward-line-pd-two-robots.md): native hai profilePASS101/101 (51+50), gồm4 ca mới/profile:001/011/100/110 ởFollowing bình thường không lỗi;000→line bên cùng phía100ms/≥3 mẫu mới phục hồi đúngroute/index; nhiễu/đổi phía/wide/gap>50ms không cộng bằng chứng; deadline và lỗi đã chốt không tự mở; mất mạng/sonar/config/mutex/STOP luôn dừng; lùi vẫn yêu cầu010. Các ca rẽ/capture/settling/RFID/mutex cũPASS. Host `scripts/test-forward-pd.ps1` PASS4 cấu hình, gồm nối đoạnM2→G6 qua000→001 đến duty thật. Probe dùng base/trim hiện hành thay vì đóng đinh160, giữ tuning người dùng01=210/02=160.

PlatformIO buildESP32 cả01/02PASS, RAM58108byte mỗi ảnh, flash01=964601/02=964845byte trên1310720. Binary có buildID `20261019-robot01-pd-side-recovery-v5` / `20261019-robot02-pd-side-recovery-v5`. SHA25601 `CC0FBCA23998778BA8B149C393CC82BD6057C119839669C8ECB57B6BC0EDFC58`,02 `1108218C538F8D20F93B33B6D94C9BE0DE1699108A8A5162A7472FF97C5A4811`. `git diff --check` PASS. Chưa upload/chạy sàn, chưa phátclear-error/GOTO hoặc sửaCore vận hành.

## 19/10/2026 — PD đi tiến cho cảhai xe

Bổ sung sau log `Config::FORWARD_PD_* is not a member`: khôi phục khối khai báoPD bị thiếu trong `Config.h`, giữ tuning profile hiện hành (fallback01=230,02=220; PD160). `scripts/test-forward-pd.ps1` PASS cả4 cấu hình; buildESP32 cả01/02PASS và binary chứa đúng buildIDforward-pd-v4. RAM58108byte mỗi ảnh, flash01=964309/02=964561byte. SHA256 ảnh build lại01 `89B20282AECDED66CB1E1C43374B26CDED5A669DE5262632F1DA71A77D38E475`,02 `B9DEA56428B8DB73CE94DA7C6B18123F69111E6408E19490E539213618BC98C2`. Chưa upload; `git diff --check` PASS.

REQ-FORWARD-PD-01 / [ADR-073](../docs/decisions/ADR-073-forward-line-pd-two-robots.md): `scripts/test-forward-pd.ps1` PASS4 cấu hình (01/02 × bật/tắt). Probe nốiPD→Movement→driver thật với adapter GPIO, kiểm tín hiệuGPIO35 tương đương sketch (80/240 khi rời giữa,110/210 khi ổn định), dt10/20ms, duplicate/subinterval, gap>50ms, rollover, clip0…255, reset route/index/STOP/blank, trạng thái riêng, đầu ra lùi/pivot giữ cũ, không đi khi thiếu/thu hồi grantM2 trên đoạnM2→G6. Hazardwifi/MQTT/sonar/config/servo/STOP thắng cachePD ngay trong interval. Tắt cờ phục hồi bảngPWM ở cảhai profile. `scripts/test-fast-line.ps1` PASS01/02.

PlatformIO `native_robot01_bench` / `native_robot02_bench` PASS93/93 (47+46), gồm RFID/node-order, capture/settling xe02, lùi/trạm, mutex/clearance, sonar/mạng và tìm line. Lần đầu assertion test cũ đòi5s không khớp SharedConfig7s; sửa assertion để kiểm timeout hữu hạn và nhỏ hơn deadline chặng, không đổi runtime. Binary trướcPD lưu `.qa/forward-pd-baseline/robot01-before-pd.bin` và `robot02-before-pd.bin`. Chưa nạp hoặc chạy robot thật trong lượt này; xe02 chưa được đo với hệ sốPD mới.

BuildESP32 `esp32dev_two_robot_bench_01` / `_02` đềuPASS. Mỗi ảnh RAM58108/327680byte (17,7%); flash01=964309byte,02=964561byte trên1310720 (73,6%). Đã kiểm đúng chuỗi buildID `20261019-robot01-forward-pd-v4` / `20261019-robot02-forward-pd-v4` trong binary. SHA25601 `E8E8F6F601D06A82C3A7F7293350DBA21A74A9DB20B2B39633ADA64AE815308B`,02 `69CAF71FB08C412306B65E7375242E3D719C151DB02DEE90C6E4A5E317200682`. Artifact tại `firmware/NCKHAGV/NCKHAGV/.pio/build/<env>/firmware.bin`; chưa upload. `git diff --check` PASS.

## 19/10/2026 — PWM cua xe01 M2 → G6

Bổ sung REQ-ROBOT01-ROUNDED-LINE-01 / [ADR-066](../docs/decisions/ADR-066-fast-line-response.md): `scripts/test-fast-line.ps1` PASS cả01/02. Driver profile01 xuất110=205/150,011=155/200, mỗi kênh không vượt đi thẳng205/200, tổng duty giảm và chênh≥40PWM; STOP/000/grant/mạng/sonar vẫnPASS. PlatformIO build cảhaiPASS;01 RAM58076byte/flash963553byte,02 RAM58076byte/flash963801byte. Binary01 `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` buildID `20261019-robot01-curve-pwm-v3`, SHA256 `8D5B7A296EF3BA770AAA766CF73CE2AA0348EACA1AD43AD61D7EF4885BAE896C`. Chưa upload hoặc đo khả năng ôm cua trên sàn; chỉ xác minh logic và đầu ra driver bằng host.

## 19/10/2026 — Phục hồi STALE_COMMAND

REQ-STALE-COMMAND-RECOVERY-01 / [ADR-072](../docs/decisions/ADR-072-stale-command-counter-recovery.md): `core_tests.exe` PASS, gồm `CommandIds::ensureAbove` qua restart, xóa pending retry, không phát STOP ID thấp, giữ ERROR qua diagnostic khỏe và `RESET_FAULT` sau clear có ID lớn hơn high-water. Core build riêng tại `.qa/stale-command-fix/agv_core.exe` PASS. PlatformIO `esp32dev_two_robot_bench_01` và `_02` PASS với diagnostic `lastCommandId`; RAM đều58076/327680 (17,7%), flash lần lượt963569/963801 trên1310720 (73,5%). Binary01 SHA256 `68DFD7C115EC3682F8F9852BA9E54E7FF18E1CEEB51F7E5C2EF282EF465FD303`, binary02 `DE0B32603971E563207894136F7ABE7B85814D7D14E1CD1B08D00B455B69361A`; chưa upload/restart Core vận hành hoặc thử robot thật.

## 19/10/2026 — Xe01 bám line qua cung bo

REQ-ROBOT01-ROUNDED-LINE-01 / [ADR-066](../docs/decisions/ADR-066-fast-line-response.md): `scripts/test-fast-line.ps1` PASS cả hai profile, gồm xác nhận profile01 giữ hai bánh có duty ở `110/011` và profile02 giữ hành vi cũ. `scripts/test-rounded-map.ps1` PASS cả hai profile với 22 node, 4 cung và 60 bộ ba Core/MCU. PlatformIO native `native_robot01_bench` + `native_robot02_bench` PASS93/93 (47+46), gồm hồi quy mất line, safety và gentle correction mới.

Build `esp32dev_two_robot_bench_01` PASS: RAM58076/327680 byte (17,7%), flash963521/1310720 byte (73,5%). Binary `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` chứa buildID `20261019-robot01-rounded-line-fix-v1`, SHA256 `671BEB9F1A73CC406043FD409A08CD0A4DEDC38BF1B67903DD53339AFD56ABFA`. Không sửa/build server vì rà soát xác nhận server chỉ phản ứng sau `LINE_LOST`; chưa upload hoặc kiểm thử phần cứng.

18/10/2026, bổ sung REQ-WAREHOUSE-UI-01: `node tests/warehouse.browser.integration.js` PASS với Edge/DB/Core giả lập riêng tại `.qa/warehouse-browser-jMlObp`. Kiểm tiêu đề **Trạng thái kho hàng**; đơn needs_attention vẫn hiện, bấm đối soát làm đơn closed biến khỏi danh sách/tiến độ, checkbox xem lại đúng số lượng, reload trở về bộ lọc gọn mặc định. Đơn hoàn thành khác vẫn hiện; API còn nguyên bản ghi closed và báo cáo JSON đã lưu không đổi. Các ca hai xe/servo/kho/báo cáo/desktop/mobile trước đó cùng PASS. Không xóa DB hoặc restart gateway vận hành.

## 18/10/2026 — Giao diện tuyến xe và kho trong ca

REQ-WAREHOUSE-UI-01 / [ADR-071](../docs/decisions/ADR-071-warehouse-operations-overview.md): `npm test` PASS47/47 sau thêm `warehouse.shift.initial_slots`. `node tests/warehouse.browser.integration.js` PASS với Edge desktop1440/mobile390: tạo đơn bằng form mới không nhập quantity, xe01 đang giao K2→K1 và xe02 đang đi lấy K3 để giao K6, RFID/đích lệnh riêng, ô kho đầu ca/hiện tại/giữ chỗ, xe02 lỗi thì dừng chỉ báo bước chạy và hiện đối soát, mất MQTT ghi dữ liệu cũ/khóa form, DONE nâng/hạ và báo cáo CLOSED/tảiJSON giữ đúng. Ảnh tại `.qa/warehouse-browser-IBgCYs/desktop-operations.png`, `mobile-operations.png`, `transfer-form.png`; file báo cáo riêng cùng thư mục. `node tests/browser.integration.js` PASS các view, mapv4/dock hai cột, tuyến/heading/history, ca/scope, stale/reconnect và mobile tại `.qa/browser-test-UYTE6f`. Cấu hình Playwright đi kèm runtime/Edge như mục phía dưới. Syntax và `git diff --check` PASS; không restart gateway thật, không sửa DB vận hành hoặc nạp firmware.

## 18/10/2026 — Trạng thái kệ và báo cáo kho cuối ca

REQ-WAREHOUSE-STATE-01 / [ADR-070](../docs/decisions/ADR-070-warehouse-racks-and-shift-reports.md): `npm test` trong `AGVwebserver` PASS47/47 (gateway15, security4, orders13, warehouse-actions8, inventory7). Kiểm nhập vị trí tùy chọn/mã kệ trùng/revision/xác nhận thực tế/ca CLOSED; hai xe đặt chỗ nguồn–đích, xe đang có kệ không lấy thêm; DONE tương quan và chống lặp; lỗi ghi log rollback cả tồn vị trí; file DB riêng mở lại giữ trạng thái và yêu cầu đối soát; đổi Core session tạo báo cáo gián đoạn; ca CLOSING chưa tạo báo cáo, CLOSED tạo đúng một bản bất biến; tích hợp đơn tự động và Lấy/Hạ scope02.

Trình duyệt `node tests/warehouse.browser.integration.js` PASS với Edge headless, desktop1440 và mobile390: tự nhập bốn kệ tại vị trí chọn, tải lại trang giữ dữ liệu, servo nâng/hạ cập nhật ô↔xe, báo cáo và tải JSON; ảnh/file tại `.qa/warehouse-browser-DAMxPp`. `node tests/browser.integration.js` hồi quy dashboard PASS, ảnh tại `.qa/browser-test-LmSOMn`. Dùng `AGV_PLAYWRIGHT_MODULE` trỏ Playwright đi kèm runtime và `AGV_BROWSER_CHANNEL=msedge`; npm script `test:warehouse:browser` chạy riêng bài mới. Syntax các JS sửa và `git diff --check` PASS. Tất cả dùng Core giả lập/DB riêng; chưa restart gateway thật, không sửa DB vận hành và chưa thử phần cứng.

## 18/10/2026 — Đơn hàng tự động dùng servo hai xe

REQ-ORDER-AUTO-LIFT-01 / [ADR-069](../docs/decisions/ADR-069-automatic-orders-use-servo-lift.md): `node --test tests/orders.test.js tests/warehouse-actions.test.js` PASS21/21. Ca mới kiểm capability bắt buộc, hai xe nhận hai lượt không trùng robot, `warehouse_lift` đúng ID/action, ACK/trạng thái đang chạy không cấp chặng giao, DONE sai command_id bị bỏ, DONE đúng cargo mới đi tiếp, hạ xong mới lùi gate, offline/restart giữ đối soát. `npm test` trong `AGVwebserver` PASS18/18 (gateway14 + security4) khi chạy với loopback; lần chạy sandbox đầu bị EACCES loopback, không tính là lỗi sản phẩm. `node --check` các tệp JS sửa và `git diff --check` PASS. Chưa restart gateway/Core vận hành, chưa nạp firmware hoặc thử servo/kệ thật. Chu kỳ vẫn5s; 3s còn OPEN.

## 17/10/2026 — Sửa dừng/xin lại mutex sau30 xung

REQ-MUTEX-EXIT-CONTINUE-01 / [ADR-068](../docs/decisions/ADR-068-encoder-mutex-tail-clearance.md): native profile01/02 PASS92/92. Ba ca hồi quy mới nối clearance với navigator: sáu mutex trả đúng token rồi bám tiếp/fast-line không dừng hoặc xin lại; token sai/lặp/tới muộn bị từ chối; M5 đã trả không miễn grantM6 và vẫn đi xuyên khi đủ quyền; dấu clearance xóa khi đổi RFID, STOP/tuyến mới, lỗi mạng hoặc reset phiên. `scripts/test-fast-line.ps1` PASS cảhai, gồm thu hồi grant thường vẫn STOP và mạng/sonar/mất line giữ chốt.

Build ESP32 cảhai PASS, RAM58076byte mỗi ảnh; flash01=963701byte,02=963717byte. BuildID01 `20261017-robot01-mutex30-exitfix-v4`, SHA256 `54283E642CA445B780F91054D4ACF1E338967D9FB67AF9464490554FEE9A9CA8`; buildID02 `20261017-robot02-mutex30-exitfix-v4`, SHA256 `43AB628E4C2E9FC55583B8853D2FFAC986724EAF27968DB916C95D8647376B39`. Binary chứa đúng buildID đã kiểm tra. Giữ tuning profile hiện tại và grace7s của02; không sửa Core/gateway/map. Chưa upload/chạy xe thật trong lượt này.

## 17/10/2026 — Nhả mutex theo encoder30 xung, hai xe

REQ-MUTEX-ENCODER-01 / [ADR-068](../docs/decisions/ADR-068-encoder-mutex-tail-clearance.md). PlatformIO `native_robot01_bench` và `native_robot02_bench` PASS tổng86/86. Ca mutex kiểm tra số đo17cm/18,5cm, 29/30 xung, bắt buộc cả hai bánh, đọc lại thẻ đặt lại cửa sổ100ms, STOP/trạng thái không an toàn giữ khóa, đúng zone/token, queue một lần và counter wrap. Test riêng robot01 chỉ chạy đúng profile01; test grace7s chạy profile02.

`scripts/test-encoder.ps1 -SkipCoreBuild` PASS cả profile01/02:161xung/100cm, debounce300µs, tổng xung tịnh tiến không tăng trong STOP/pivot, đổi RFID/route/session, lùi và wrap; Core odometry regression PASS và không tự sửa task/mutex/checkpoint. Đây là mô phỏng ISR/GPIO và logic host, không đo trượt bánh thật.

Build ESP32 cuối PASS: cả hai dùng RAM58076byte; flash01=963589byte,02=963621byte. BuildID01 `20261017-robot01-mutex30-v4`, SHA256 `00A1D74B4E68712B374FEB0BDD5D328B76B26313F2E0CF11CC1DAC49BBF56255`; buildID02 `20261017-robot02-mutex30-line-grace7s-v4`, SHA256 `D276F92A5AB78FF6AC30FAB0FAF06D50D623DB0EC46943F9E8199185D6F50931`. Chưa upload hoặc phát lệnh xe. OPEN: thử từng xe/từng hướng có đánh dấu biên, quan sát `[MUTEX_RELEASE] proof=encoder left/right`, đo vị trí đuôi và tăng ngưỡng nếu cần.

17/10/2026 — REQ-ROBOT02-LINE-GRACE-7S-01: build `esp32dev_two_robot_bench_02` PASS, RAM57948byte/flash962913byte, binary xác nhận buildID `20261017-robot02-line-grace7s-v4`. Kiểm tra host cơ bản `tests/motor-host/line_grace.cpp` PASS: tiến/lùi, có/không hướng tìm, STOP ở600ms và giữ route/index/cmd ở5–6,5s, bắt lại010 ở6,6s rồi tiếp tục sau xác nhận100ms, hết7s mớiLINE_LOST, mạng/STOP ưu tiên. Cập nhật expectation timeout các fixture native liên quan; không chạy toàn bộ native/integration theo yêu cầu người dùng. Chưa upload/thử xe thật. Xe01/TurnCapture/timeout chặng15s giữ nguyên.

17/10/2026 — REQ-ROBOT02-REVERSE-GENTLE-01: chỉ build và kiểm tra motor cơ bản `tests/motor-host/reverse_gentle.cpp` profile01/02 PASS; không chạy native/integration toàn bộ. Kiểm duty lùi02=205/190,011/001=205/170,110/100=185/190 (cả hai bánh lùi), settling cùng hướng/duty, PWM adapter, còi và STOP000;01 giữ chỉnh một bánh, tiến/xoay theo tuning hiện tại không đổi. Build `esp32dev_two_robot_bench_02` PASS RAM57948byte/flash962909byte, buildID `20261017-robot02-gentle-reverse-v4`. Đây là host GPIO, chưa đo khả năng hội tụ line và chưa upload/chạy xe thật.

## 17/10/2026 — Giảm trễ bám line hai xe, fast-line-v4

REQ-LINE-RESPONSE-01 / [ADR-066](../docs/decisions/ADR-066-fast-line-response.md). `scripts/test-fast-line.ps1` PASS hai profile với driver/movement/navigator thật và GPIO/PWM stub. Kiểm tra pattern010/100/001/110/011 được áp ra motor trước bước giả lập RFID25ms, không đổi cmd/index, không khởi động sớm khi chờ grant, revocation giữ STOP cả lượt sớm/thường,000 dừng rồi trả quyền xử lý cho recovery, mất WiFi/MQTT/vật cản/sonarinvalid/configinvalid dừng. Kiểm tra ghi PWM: duty lặp không ghi lại, cùng chiều thay duty không xen0, đổi chiều có0 trước cấp duty, STOP hai channel0. Đây là kiểm tra thứ tự/đầu ra phần mềm, không phải đo thời gian trên ESP32. Binary `.qa/fast-line/fast-line-01.exe` và02.

PlatformIO native_robot01_bench/native_robot02_bench PASS85/85. `scripts/test-rounded-map.ps1` PASS22node/4cạnh bo/60bộ ba Core–MCU mỗi profile và navigator đi liên tục. `node scripts/generate-firmware-route.js --check` và `git diff --check` PASS.

Build ESP32 `esp32dev_two_robot_bench_01` và02 PASS. RAM57948byte mỗi xe; flash01=962901byte,02=962913byte. File nạp ở `.pio/build/<env>/firmware.bin` trong project firmware. SHA25601 `F457B69E28B3468DC0D6AAAAE4E74A1056D58AEB91BD35A3DD8D98A91D7A0AA8`;02 `15751A65C4AC8BCDFABAF011AF24920D30400A6985A6FAFA3856DA090DA6AC54`. BuildID `20261017-robot01-fast-line-v4` / `20261017-robot02-fast-line-v4`. Chưa upload, không thay Core sống hoặc phát lệnh xe. OPEN: người dùng nạp đúng hai env và đối chiếu độ trễ/bám góc trên sàn; chưa chứng minh hết văng/độ mượt hoặc chọn được PWM tối ưu thực tế.

## 17/10/2026 — Khôi phục checkpoint khi đổi map v3 → v4

[ADR-065](../docs/decisions/ADR-065-rounded-map-checkpoint-recovery.md), REQ-MAP-RECOVERY-01. Build `scripts/build-core.ps1 -Tests -OutputDirectory .qa/runtime-map-recovery` PASS, toàn bộ unit Core PASS.

`tests/runtime-map-migration.cpp` biên dịch g++17 với `-I include`, chạy độc lập và với tham số `data/runtime_state.json` v3 trước triển khai đều PASS. Kiểm tra giữ lịch sử, owner/token, chuyển vị trí C thành chưa xác minh/hai đầu cạnh, ngắt pending, roundtrip restore lần2, backup và từ chối thay graph/obstacle ngoài phạm vi. Chỉ tạo file thử trong `.qa/runtime-migration-*`, không ghi dữ liệu đầu vào.

`AGV_CORE_BIN=.qa/runtime-map-recovery/agv_core.exe node tests/runtime-map-migration.integration.js <checkpoint-v3>` PASS với bản sao checkpoint thực tế,764task. Chạy/start lại scope active01/active02/active-both bằng broker/cổng/map/counter/runtime/shift riêng, nhận22node/35cạnh/4geometry qua WS, giữ nguyên owner/grant và đủ task, backup nguyên byte chỉ một bản mỗi scope, không phát command/grant cho robot. Artifacts `.qa/runtime-migration-integration-8TN0cG`.

Triển khai theo xác nhận người dùng: sao lưu binary/map/runtime/shift/counter trong `.qa/runtime-map-recovery/deployment`, đóng đúng GDB7944 giữ Core lỗi23748 rồi thay `build/agv_core.exe`. SHA256 mới `5E46628B663A348AF0B1CF76A1C34DAB668285D77D8875F9F4FAF1C7EC499BB8`. Core mới PID12908 chạy nền `--mode active-both`, cwd gốc workspace. API3000 báo agv_connected/core_ready/protocol_supported/mqtt_connected/storage_healthy=true, mapv4 22node/35cạnh/4geometry. Cả hai xe OFFLINE/vị trí chưa xác minh, ca CLOSING/can_start=false. Giữ M1/02/token79263, M5/02/79292, M6/01/79261 và764task; không xác minh vùng hay mở ca thay người vận hành. Backup `data/runtime_state.json.before-rounded-map-v4-0.json` khớp SHA256 file cũ `50D7ACC02F0A80C34F34327971E641AFD864319BFD49DA4BCB6D73F191CA7B69`. Counter tiếp nối qua startup, không rewind. Chưa nạp firmware hoặc chạy xe thật.

## 17/10/2026 — Map bo góc v4, bỏ C1–C4

REQ-MAP-03 / [ADR-064](../docs/decisions/ADR-064-rounded-map-without-corner-tags.md). Graph vận hành/fallback/bảng MCU cùng 22 node, 35 cạnh có hướng, 6 mutex. Bốn cạnh ngoài 135 cm theo giả định cộng nhãn 85+50; tọa độ là sơ đồ, chưa đo bán kính/chiều dài dọc line thật.

- `scripts/build-core.ps1 -Tests -OutputDirectory .qa/rounded-map-v4`: Core build và toàn bộ unit PASS; kiểm map mặc định khớp JSON, loại bốn ID cũ, cost, chiều cạnh, mọi kho đi/về hai home, mutex/shift/checkpoint.
- Source hoàn chỉnh build/unit lại **PASS** tại `.qa/rounded-map-v4-final/agv_core.exe` và `core_tests.exe`, gồm validation geometry hai chiều và không mất geometry khi sửa cạnh.
- `scripts/test-rounded-map.ps1`: PASS cả 01/02, mỗi profile đối chiếu 60 tổ hợp hướng Core/MCU, 4 cạnh bo/tiếp tuyến vào-ra, bám line liên tục không có thẻ góc, dừng ở đích, geometry lỗi giữ map cũ, serialize giữ geometry.
- PlatformIO native_robot01_bench + native_robot02_bench: **85/85 PASS**, gồm RFID, NODE_ORDER, đi xuyên/grant, lùi S1/S2, line search/capture, STOP/mạng/sonar. Fixture cũ C4→…→C3 chuyển thành G6→…→G1; hướng đầu tại G6=S và tới M4=W.
- ESP32 bench01/02 build **PASS**, RAM 57940 byte mỗi ảnh; flash 01=962593 / 02=962605 byte. BuildID `20261017-robot01-rounded-map-v4` / `20261017-robot02-rounded-map-v4`.
- `tests/encoder_core_tests.cpp`: PASS cả hai xe và hai stream cùng boot/session/seq; cạnh bo dùng weight135, x/y theo hai đoạn, giữ saturation/stale/task/mutex. Binary `.qa/rounded-map-v4/encoder_core.exe`.
- MQTT/Core menu scope01/02/cảhai: PASS tại `.qa/core-mode-test-O685q4`, kiểm 22 node/4 geometry, từ chối C1–C4, hồi quy pin/scope/mutex/counter restart.
- `tests/map.integration.js`: PASS tại `.qa/map-test-1w7tR5`; save file và snapshot/apply giữ bốn geometry, hai xe tranh/quyền mutex, task hoàn tất đúng RFID.
- `tests/straight-through.integration.js`: PASS tại `.qa/straight-through-test-Ep5mCz`; đi xuyên M4/GS2/M3, vùng bận/offline giữ token, đúng hướng tới sau bo góc. `tests/node-order.integration.js`: PASS tại `.qa/node-order-test-9EQZ97`, gồm lùi kho rồi rẽ G1→M1 và đỗ trạm.
- `tests/browser.integration.js`: PASS Edge desktop/mobile + snapshot preview tại `.qa/browser-test-GH3IJX`, bốn SVG path bo và 22 node, không tràn ngang. Tên ảnh trong lượt này còn `warehouse-map-v3.png`, nhưng nội dung/title là mapv4; runner hiện đã đổi tên ảnh v4.
- `tests/encoder.integration.js --browser`: PASS tại `.qa/encoder-test-zifqVL`; marker di chuyển dọc cạnh bo, cảnh báo đỏ đúng SVG path, stale/saturation và desktop/mobile. Web/orders/warehouse unit **24/24 PASS**.
- Bảng route `generate-firmware-route.js --check`, JS syntax và whitespace PASS. `scripts/check_contracts.py` chưa chạy được vì cả Python mặc định và bundled runtime thiếu `jsonschema`; không cài thêm dependency trong yêu cầu đổi map.

Tất cả fixture dùng broker/cổng/map/DB/counter riêng. Lượt sandbox đầu của PlatformIO/Edge bị chặn cache/profile/loopback; chạy lại với quyền phù hợp PASS. **Chưa upload firmware, chưa restart Core hoặc thử robot thật.** Build vào `build/agv_core.exe` bị script chặn vì Core vận hành PID6284 đang chạy; cần dừng đúng phiên và F5/build lại trước dùng map mới. Không xóa/rewind counter hoặc chuyển snapshot/checkpoint vận hành.

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

## Mở ca kèm kiểm tra kho — 21/10/2026

`node tests/shift-start.browser.integration.js` (hoặc `npm run test:shift:browser` trong AGVwebserver) PASS Edge headless với Core giả lập, hai xe và SQLite riêng: hủy không ghi, xác nhận thực tế bắt buộc, mã trùng không mở ca, thứ tự lưu/mở, Core từ chối hoặc HTTP504/unknown sau khi lưu vẫn giữ form và không tự retry, prefill, xung đột revision/phiên Core, khóa MQTT, chống submit trùng và ảnh đầu ca đúng dữ liệu vừa lưu. Lần cuối PASS tại `.qa/shift-start-browser-XkbtLd`; desktop1440/mobile390 đã chụp, ảnh mobile đã xem trực tiếp ở lần đầu `.qa/shift-start-browser-54rClT`. Đặt `AGV_PLAYWRIGHT_MODULE`/`AGV_BROWSER_CHANNEL` như hướng dẫn trong tài liệu này. Bốn bài browser mô phỏng dùng nút mở ca được cập nhật thêm bước xác nhận và kiểm tra syntax, chưa chạy lại toàn bộ mô phỏng; không đổi lệnh API hoặc luật mô phỏng.

`npm test` trong AGVwebserver: 47/47 PASS. `tests/browser.integration.js` PASS (`.qa/browser-test-e1zUgc`); `tests/warehouse.browser.integration.js` PASS (`.qa/warehouse-browser-hcVAz1`). Sandbox ban đầu chặn kết nối localhost bằng EACCES; kết quả PASS là lần chạy có quyền localhost phù hợp. Không chạy server/DB vận hành hoặc thử phần cứng.
