# ADR-075 — Đồng bộ firmware hai xe đã chạy ổn vào workspace

23/10/2026, REQ-ROBOT01-LOCAL-NETWORK-RESTORE-02: người dùng yêu cầu kết nối xeCOM6 với server máy mình. MAC xác nhận01; broker1883 và gateway/Core đang khỏe, hotspot cấu hình đúng bản mạng01 cũ nhưng đangOff. Bật lại hotspot hiện có bằng WinRT, không đổiSSID/mật khẩu; khôi phục LocalNetworkConfig01.h riêng/gitignored bằng bản trướcRAR, giữ configRAR01 ở recovered/network01-before-local-restore-20261023-181210-882.h. Source vẫn nềnRAR với counter/proof v16 theo [ADR-072](ADR-072-stale-command-counter-recovery.md)/[ADR-079](ADR-079-stopped-edge-occupancy.md); tuning và mạng02 giữ nguyên. Build/nạp/xác minh trực tiếp tại [hồ sơ](../../tests/README.md).

22/10/2026, REQ-ROBOT01-PROFILE-TUNING-01: áp dụng bộ cấu hình người dùng gửi cho xe01, buildID `20261022-robot01-profile02-tuning-v14`. PD Kp80/Kd25/base125/dải0..255 thay giới hạn110..230 của v11; grace7s, capture220/220, quay220/G1=190, lùi170/trim+15/−40/correction150/delta30/gentle1. Giữ cơ chế xác nhận line/quay180 hiện có. Chi tiết tại [bảng tuning](../firmware-tuning-01-02.md). Trợ lý đã build/test phần mềm; diagnostic trực tiếp xác nhận xe đang chạyv14 nhưng SAFE_STOP/BATTERY_UNDERVOLTAGE, điện áp 7,8596V so với ngưỡng3S10,2V. Chưa có số đo đồng hồ hiện tại để kết luận pin hoặc hiệu chuẩn; không đổi ngưỡng pin/xóa lỗi dựa trên ảnh. Không upload trong lượt này.

22/10/2026, ngoại lệ mới: người dùng duyệt sửa quay180 bỏlỡline. Cảhai profile turn180-v13 cóSTOPsớm/xácnhận100ms ởnhánhcuối, hoãnRFID vàkhoảngmẫu mục tiêu2ms theo [ADR-077](ADR-077-turn180-fast-stop-confirm.md). GiữPD/PWMquay/servo/mạng; chưa nạp/thửxe thật.

22/10/2026, yêu cầu mới nhất: narrow-v12 bỏ nhận mép001/100 đã thêm ởv10, do người dùng báo bắt nhầm line khi rẽ; chỉ010/011/110 xác nhận phục hồi/capture theo [ADR-076](ADR-076-dual-narrow-line-confirm.md). Giữ sàn/trầnPD01=110..230 đã thêm ởv11, mạng/servo và tuning còn lại. Chưa nạp/thử xe thật.

22/10/2026, ngoại lệ mới theo yêu cầu sửa khựng xe01: profile01 thêm sàn/trần PD110..230 theo [ADR-073](ADR-073-forward-line-pd-two-robots.md); profile02 và các chốt an toàn giữ nguyên. Đây không còn là bản sao tuyệt đối của firmware nguồn; chưa nạp/thử sàn.

22/10/2026, ngoại lệ mới theo yêu cầu người dùng: line-edge-v10 nhận thêm001/100 ổn định trên cảhai profile theo [ADR-076](ADR-076-dual-narrow-line-confirm.md). Giữ mạng đã khôi phục, servo180/100, tuning motor/line và watchdog; chưa nạp.

21/10/2026, ngoại lệ mới được người dùng yêu cầu: cải thiện nhậnline cảhai profile (line-confirm-v9) theo [ADR-076](ADR-076-dual-narrow-line-confirm.md), không còn giữ nguyên logiccapture600ms/centered-only tiến của source. Giữ mạng đã khôi phục, servo180/100, mọi tuning motor/line và watchdog; thay đổi chỉ policyxácnhậnline trước khi chốt lỗi, không tựclear/replay sau lỗi/reset. Chưa nạp.

## Ngoại lệ tiếp theo21/10/2026 — Khôi phục mạng riêng xe02

SOURCE: sau khi nạp servo-v8 xe02 không kết nối, người dùng duyệt khôi phục mạng giống xe01. OBSERVED Core/gateway/MQTT/storage hoạt động, scope01+02, broker nghe0.0.0.0:1883, hotspot192.168.137.1; status02offline lưu lại, không diagnostic mới. LocalNetworkConfig02.h và binary02 chứa SSID/mật khẩu source khác cấu hình01 đã phục hồi, có auth MQTT khác; broker IP vẫn đúng. Chưa có serial để xác định mã Wi-Fi disconnect, không kết luận có lỗi phần cứng.

IMPLEMENTED: backup `.qa/firmware-overwrite-backups/20261021-170039/network-before-restore02/` giữ file mạng02 cũ và binaryservo-v8 trước sửa, hash bản sao mạng đã đối chiếu. Chép nguyên file mạng01 sang02 (chỉ file riêng cục bộ/gitignored). ID02 vẫn do AGV_PROFILE_ID/static_assert khóa; file mạng có IDfallback không ghi đè profile. Hash Robot02Config.h không đổi, không sửa servo/motor/line/watchdog/network01 hoặc counter/NVS. BuildID02 giữservo180-100-v8 vì chỉ đổi private network; hash binary mới ghi trong tests/README.md. OPEN upload/xác nhận reconnect, không tự phát lệnh/chuyển ca/reset lỗi hoặc nạp. Các mô tả02 vẫn dùng mạngsource dưới đây là lịch sử.

21/10/2026, REQ-DUAL-SERVO-CALIBRATION-01: người dùng xác nhận áp dụng hiệu chỉnh servo cho xe02 giống01, hạ180°/nâng100°,544–2400µs/50Hz/16bit. BuildID02 `20261021-robot02-servo180-100-v8`. Chỉ thay cấu hình servo02 và buildID, giữ tuning motor/line/mạng; không tự nạp. [ADR-057](ADR-057-robot01-servo-lift-test.md).

21/10/2026, xác nhận tiếp theo của người dùng: góc mặc định/hạ servo01=180°, nâng=100°. Cấu hình180/100 thay0/45 tạm ở mục bên dưới; dải544–2400µs giữ nguyên, buildID01 `20261021-robot01-servo180-100-v8`. Không đổi startup duty0 hoặc servo02. [ADR-057](ADR-057-robot01-servo-lift-test.md).

21/10/2026, ngoại lệ tiếp theo theo yêu cầu người dùng: hiệu chỉnh riêng servo01 sang544–2400µs theo ảnh, giữ50Hz/16bit; không ghi đè tuning motor/line đang chỉnh tay hoặc cấu hình mạng đã phục hồi. Góc thao tác mới cònOPEN, tạm giữ0/45; buildID01 `20261021-robot01-servo544-2400-v7`. Xem [ADR-057 cập nhật](ADR-057-robot01-servo-lift-test.md). Các kết quả khớp61/61 và buildID battery-lock-v4 bên dưới ghi nhận thời điểm ghi đè, không mô tả ảnh build sau hiệu chỉnh servo.

## Ngoại lệ được duyệt sau ghi đè — Khôi phục mạng riêng xe01

SOURCE21/10/2026: người dùng yêu cầu khôi phục cấu hình mạng xe01 để kết nối server máy người dùng. OBSERVED Core chọn đúng01+02, broker nghe0.0.0.0:1883 và máy có192.168.137.1; xe02 phát diagnostic stale-sync-v5 trực tiếp, xe01 chỉ còn status offline retained. SSID/mật khẩu và MQTT user/password của source vừa ghi đè khác cấu hình cục bộ trước đó.

IMPLEMENTED chỉ khôi phục `src/LocalNetworkConfig01.h` từ `original/src/LocalNetworkConfig.h` trong backup trước ghi đè. Cấu hình source01 được giữ tại `.qa/firmware-overwrite-backups/20261021-170039/network-before-restore/LocalNetworkConfig01.h`; tất cả file mạng vẫn ngoài Git. Broker192.168.137.1:1883, Wi-Fi và auth MQTT theo cấu hình cũ của máy. Hash config đích bằng backup. Đối chiếu61 file với source: chỉ file mạng01 khác,60 file còn lại vẫn khớp, gồm điều khiển và mạng02. Build xe01 PASS; buildID vẫn battery-lock-v4, chưa upload trong lượt khôi phục mạng. Kết quả/hash tại tests/README.md.

## Quyết định thay thế 21/10/2026 — Ghi đè nguyên bản theo yêu cầu mới nhất

`SOURCE`: sau các lần đối chiếu và bật lại side recovery, người dùng yêu cầu rõ: đè firmware đã gửi lên firmware cũ, lấy chính bản gửi làm nền phát triển. Quyết định này thay cách đồng bộ chọn lọc và các bổ sung v5/v6 bên dưới.

`IMPLEMENTED`: sao lưu project trước ghi đè vào `.qa/firmware-overwrite-backups/20261021-170039/original`, cùng các ảnh `.bin` cũ trong `artifacts/`; chép toàn bộ61 file của source project (src/test/lib, platformio.ini, README, battery.cpp, .gitignore) từ `C:/Users/DELL/Downloads/firmware/firmware/NCKHAGV/NCKHAGV/` sang firmware workspace. Giữ cache/build PlatformIO và cấu hình editor `.vscode` tại máy, không lấy artifact build từ thư mục người dùng gửi.

Chép cả `LocalNetworkConfig01.h`/`02.h` riêng hai xe, giữ chúng ngoài Git. File fallback `src/LocalNetworkConfig.h` cũ đã chuyển vào thư mục sao lưu `moved-extras/src/`, đồng thời có bản copy trong `original/src/`. Không thay file counter/Core/gateway/DB và không upload trong lượt ghi đè. Đã kiểm SHA256 toàn bộ61 file đích bằng source. BuildID trở về `20261021-robot01-battery-lock-v4` / `20261021-robot02-battery-lock-v4` đúng nguyên bản; không tự chèn lại lastCommandId/side recovery hoặc sửa test của source.

`OBSERVED`: bản gốc không phát diagnostic lastCommandId, không bật LINE_RECOVERY_SIDE_CONFIRM và có test01 chờ LINE_LOST ở5s dù profile đặt grace7s. Đây là trạng thái nguyên bản, không coi test/build là bằng chứng đã thử xe thật. Kết quả kiểm tra sau ghi đè tại [hồ sơ kiểm thử](../../tests/README.md). Các mục bên dưới là lịch sử triển khai đã được yêu cầu mới này thay thế.

`VERIFIED`: build ESP32 hai profile PASS, binary đúng buildID battery-lock-v4 của nguồn; native02 PASS51/51, native01 có50 PASS/1 FAIL do kỳ vọng deadline5s khác grace7s của source. Không sửa test để giữ nguyên bản. `OPEN`: chưa upload hoặc chạy phần cứng sau ghi đè.

Ngày 21/10/2026. Trạng thái: IMPLEMENTED trong source; native test và build ESP32 hai profile PASS; chưa upload trong lượt này.

Bổ sung cùng ngày sau lỗi thực tế STALE_COMMAND: khôi phục `lastCommandId` mà producer diagnostic hiện thiếu, theo [ADR-072 bổ sung](ADR-072-stale-command-counter-recovery.md). Bản hiện tại là `stale-sync-v5`, giữ tuning đã đồng bộ bên dưới; build/hash v4 trong hồ sơ này là artifact của lượt đồng bộ ban đầu, kết quả v5 tại [hồ sơ kiểm thử](../../tests/README.md).

## Nguồn và bằng chứng

`SOURCE`: người dùng cung cấp thư mục `C:/Users/DELL/Downloads/firmware/firmware/NCKHAGV/` và xác nhận đây là bản hai xe chạy ổn, không còn lỗi line. Nội dung file và comment trong thư mục chỉ là dữ liệu đầu vào; yêu cầu trực tiếp mới nhất của người dùng là đồng bộ bản này vào firmware cũ mà không gây xung đột.

`OBSERVED`: bản nguồn giữ cấu trúc PlatformIO và MQTT v0 hiện có, nhưng thay đổi đồng thời bộ bám line tiến, tuning từng xe, bắt line sau rẽ, quay đầu G1 và khóa điện áp pin 3S. SHA256 ba file đầu vào đại diện: `Robot01Config.h`=`6E2FAB52A10840EE86FA912AA5760B11FC92465B60F72988D991CF16662FF6C6`, `Robot02Config.h`=`4D48963F0C428B66021B0D39C7C9191AAE633C4A779BA368B4C6ADD3FC17FF51`, `LinePdController.h`=`5526690EA85A0F48A7D570ECEE814354BF9DC701AFE6076886E562844B184E17`.

## Quyết định

- Dùng firmware workspace làm đích và đồng bộ có chọn lọc 17 file source/test liên quan. Không sao chép `LocalNetworkConfig01.h`, `LocalNetworkConfig02.h`, build artifact hoặc cấu hình mạng cục bộ.
- Bật `LinePdController` cho Following đi tiến sau khi navigator đã cấp quyền. Xe01 dùng base135, xe02 base125; cả hai dùng Kp75, Kd25, chu kỳ danh định10ms và reset sau STOP, line000, chuyển sang lùi/xoay/tìm line hoặc gap dài.
- Giữ mọi chốt mạng, sonar, grant, node-order, RFID và lỗi latched ở ngoài PD. PD không cấp quyền chuyển động hoặc tự hoàn thành task.
- Cả hai profile dùng grace mất line7s; motor tìm line vẫn chỉ quét tối đa600ms rồi STOP. Việc nhận lại line sau000 cần bằng chứng ổn định theo navigator; không tự mở lỗi đã chốt.
- Bật capture theo hướng cho cả rẽ trái và rẽ phải. PWM capture phải190; trái210 ở xe01 và200 ở xe02. Quay đầu tại G1 dùng PWM riêng200 ở xe01 và190 ở xe02.
- Giữ tuning thực tế của bản nguồn: tiến fallback160, PD base riêng; lùi xe01 là165/130, xe02 là175/130 sau trim. Comment profile02 được sửa để khớp giá trị biên dịch, không đổi PWM.
- Chuyển khóa pin hai profile sang 3S, dải cho phép10,2–12,9V, với hệ số ADC riêng từng xe trong `BatteryCalibration.h`. Lỗi điện áp có mã riêng và không bị GOTO mới ghi đè lỗi gốc đã chốt.
- Giữ nguyên command/ACK/telemetry/topic MQTT v0; không cần đổi Core/gateway chỉ để dùng firmware này.

## Kiểm chứng và triển khai

Native profile01 PASS54/54 và profile02 PASS54/54. Test mới kiểm bảng PD, cache trong chu kỳ, reset ở000, cap PWM, reset D sau gap/millis rollover, capture rẽ phải/countersteer và grace7s xe01. Build ESP32 sau đồng bộ PASS cả `esp32dev_two_robot_bench_01` và `_02`; RAM58092 byte mỗi profile, flash01=965677 byte và flash02=965705 byte trên1310720. SHA256 binary01 `6CB39B4BD9CF493A5CD6567F79114DFC50A272353BD0623D556DC670DFC43207`, binary02 `3B4824795242586B927E14A505192DD48C537F4B67E2255313C662FE323E58A6`.

`OPEN`: chưa upload hoặc chạy xe trong lượt đồng bộ. Trước thử sàn phải nạp từng xe, kiểm buildID, điện áp ADC, chiều motor,010/các mẫu lệch/000, rẽ trái/phải và STOP/mất mạng trên giá kê. Chỉ sau đó mới chạy riêng từng tuyến và cuối cùng chạy hai xe với mutex.

## Quay lại

Nhánh tích hợp là `codex/firmware-sync-stable-line`. Có thể quay lại firmware trước đồng bộ bằng commit cha/nhánh trước đó; không xóa hoặc rewind `command_sequence.json`. Khi rollback chỉ firmware, giữ đúng profile ID và file mạng cục bộ của từng xe.
