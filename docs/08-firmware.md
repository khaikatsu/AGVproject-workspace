# Giao tiếp vi điều khiển và điều khiển robot

17/10/2026, REQ-MUTEX-EXIT-CONTINUE-01: sửa nhịp dừng sau khi trả mutex30 xung nhưng RFID gần nhất vẫn là node M. Sau queue release thành công và token khớp, navigator ghi đã ra khỏi vùng cho riêng chặng hiện tại, tiếp tục bám line tới RFID kế tiếp mà không xin lại khóa cũ. Quyền vùng tiếp theo, thu hồi grant thường và STOP/lỗi/mạng giữ chốt; dấu đã ra bị xóa khi đổi node/tuyến/phiên. BuildID01/02 `20261017-robot01-mutex30-exitfix-v4` / `20261017-robot02-mutex30-exitfix-v4`. [ADR-068](decisions/ADR-068-encoder-mutex-tail-clearance.md).

17/10/2026, REQ-MUTEX-ENCODER-01: profile01/02 nhả M1–M6 sớm theo số đo người dùng. Mốc từ lần đầu đọc thẻ M; yêu cầu cả hai bánh≥30 xung tịnh tiến (18,5cm theo161xung/100cm), thẻ không đọc lại100ms, route/phiên/vị trí/mạng khỏe và đang Following. STOP/pivot/tìm line không cộng; lỗi giữ khóa. RFID rời vùng cũ vẫn là fallback. BuildID01 `20261017-robot01-mutex30-v4`,02 `20261017-robot02-mutex30-line-grace7s-v4`; chưa upload/thử sàn. [ADR-068](decisions/ADR-068-encoder-mutex-tail-clearance.md).

17/10/2026, REQ-ROBOT02-LINE-GRACE-7S-01: profile02 thêm `AGV_LINE_LOSS_GRACE_MS=7000`, tách thời gian giữ task khỏi quét600ms.000 khi bám tiến/lùi vẫn STOP, tìm có hướng≤600ms rồi STOP chờ;010 ổn định100ms/≥3 mẫu trong7s thì tiếp tục task/chặng, hết hạn mớiLINE_LOST. Không quét liên tục7s, không nới xoay/TurnCapture hoặc bỏ timeout chặng15s/mạng/sonar/grant/STOP. Core không trì hoãn lỗi đã chốt; không đổi MQTT hoặc xe01. BuildID02 `20261017-robot02-line-grace7s-v4`; chưa upload/thử sàn.

17/10/2026, REQ-ROBOT02-REVERSE-GENTLE-01: riêng02 bật chỉnh lùi hai bánh bằng delta20, cơ sở reverse/correction200; giữ trim+5/−10 hiện có. Duty kênhLEFT/RIGHT:010=205/190,011/001=205/170,110/100=185/190. Không đảo bảng hướng hoặc đổi01; áp cả settling, giữ còi/STOP/sonar/tìm line. Thông số thử chưa hiệu chuẩn xe thật. BuildID `20261017-robot02-gentle-reverse-v4`. [ADR-067](decisions/ADR-067-robot02-gentle-reverse-line.md).

17/10/2026: [ADR-066](decisions/ADR-066-fast-line-response.md), profile01/02 fast-line-v4 áp motor/line sớm trước RFID trong pha Following đã cho phép, đổi duty cùng chiều không ngắt PWM0 và giảm log lặp khi chạy. Giữ PWM/trim hiện tại theo file profile, sonar/RFID/STOP/mutex. Cần nạp lại cảhai; chưa kiểm chứng độ mượt trên sàn hoặc đo độ trễ điều khiển.

16/10/2026, REQ-ROBOT02-REVERSE-BALANCE-01: riêng xe02 thêm `AGV_REVERSE_LEFT_SPEED_TRIM=5`, thay dùng trim tiến+15 cho lùi. Giữ reverse210/correction220 và phải−15 hiện có: lùi trái215/phải195; chỉnh lùi trái225/phải205 (chỉ bánh cần chỉnh hoạt động). Pha cap sau bắt line cũng dùng trim lùi riêng. Tiến/xoay/xe01/STOP/sonar giữ nguyên. BuildID `20261016-robot02-reverse-left215`, chưa nạp/thử thực tế.

16/10/2026, REQ-ROBOT02-LINE-ALIGN-01 thay xác nhận010/100ms riêng LEFT xe02:011/110 giữ bằng chứng có line và căn pivot theo phía mắt, không xóa xác nhận. Phiếu010/011/110 tối đa3 mẫu cách≥10ms, bỏ mẫu cũ khi gap>40ms;≥2 phiếu và mẫu mới nhất010 thì STOP một vòng rồi bám tiếp.111/101 không xác nhận nhánh;000 không tiến mù; tổng căn600ms, xung215/40ms-nghỉ60ms. Vừa bám lại cap mỗi bánh≤215 trong200ms, sonar/grant/STOP giữ hiệu lực; encoder không cộng pivot. Xe01 và phục hồi mất line tiến/lùi180/100ms không đổi. BuildID02 `20261016-robot02-line-align`; native166/166, hostGPIO PASS, chưa upload/thử sàn. [ADR-063](decisions/ADR-063-robot02-left-turn-capture.md).

16/10/2026, REQ-ROBOT02-PIVOT-TORQUE-01 thay tuning LEFT-CAPTURE bên dưới: xe02 pivot thường230; sau rời line/chỉnh ngược trong TurnCapture215, tách khỏi phục hồi mất line tiến/lùi180. Giữ nhận line/STOP sớm, xung40ms-nghỉ60ms và mọi timeout/chốt an toàn. Tiến/lùi/chỉnh line và xe01 không đổi. BuildID02 `20261016-robot02-turn230-capture215`; chưa nạp/thử lực xoay thật. [ADR-063](decisions/ADR-063-robot02-left-turn-capture.md).

16/10/2026, REQ-ROBOT02-LEFT-CAPTURE-01: sửa riêng LEFT xe02 dừng bắt line tạiG1. Không đòi000 toàn bộ hoặc chờ100ms mới nhận line; mắt giữa rời qua000/001/100 thì pivot180, mắt giữa trở lại STOP sớm trước RFID rồi xác nhận010 ổn định100ms/≥3 mẫu. Nếu trôi vạch chỉ chỉnh ngược phải bằng xung180/40ms-nghỉ60ms trong600ms; bỏ retry trái dài và báoLINE_LOST khi hết hạn. Giữ task/chặng và chốt an toàn; buildID02 `20261016-robot02-left-capture`. Native158/158, hostGPIO và build02 PASS; chưa upload/thử xe thật. [ADR-063](decisions/ADR-063-robot02-left-turn-capture.md).

16/10/2026, REQ-LINE-SEARCH-01: profile01/02 phục hồi mất line trong lúc tiến/lùi: dừng100ms, nhớ hướng chỉnh cuối≤750ms, quét cùng phía với PWM180/xung40ms/nghỉ60ms trong tối đa600ms, một lần/chặng. Mắt giữa `010` ổn định100ms/≥3 mẫu mới tiếp tục task/chặng cũ; không có hướng mới thì STOP chờ tối đa5s. Hết hạn báo `LINE_LOST`; encoder không cộng pivot và các chốt mạng/sonar/vật cản/STOP/grant giữ hiệu lực. BuildID01/02 `20261016-robot01-line-search`/`20261016-robot02-line-search`; chưa upload/hiệu chuẩn góc thật. [ADR-062](decisions/ADR-062-local-line-search.md).

16/10/2026, REQ-TURN-LINE-RETRY-01: hai profile bench khi LEFT/RIGHT hết timeout2500ms mà chưa bắt line sẽ STOP một vòng rồi pivot lại cùng hướng đúng một lần. Bắt lại line sau khi đã rời line cũ thì tiếp tục task; lần hai thất bại mới chốt/phát `LINE_LOST`. Route và mutex được giữ; mất mạng/sonar/vật cản/STOP vẫn ưu tiên. Không áp cho quay đầu, xoay trạm hoặc mất line khi chạy thẳng. BuildID01 `20261016-robot01-turn-line-retry`, buildID02 `20261016-robot02-pwm230-turn-retry`; chưa nạp/thử xe thật. [ADR-061](decisions/ADR-061-single-turn-line-retry.md).

16/10/2026, REQ-ROBOT02-PWM-230-ASYM-01 thay cấu hình PWM02 mới nhất: tiến/lùi và chỉnh line dùng mức cơ sở230 với trim trái−35/phải+15, nên duty thực tế trái195/phải245; pivot giữ245/245. Xe01, chân dây, timeout và chốt an toàn không đổi; buildID02 `20261016-robot02-pwm230-l195-r245`. Chưa nạp/thử xe thật; cấu hình tăng toàn bộ lên245 bên dưới là lịch sử trước yêu cầu này.

16/10/2026, cập nhật mới hơn REQ-ROBOT02-PARITY-01: REQ-ROBOT02-PWM-245-01 tăng toàn bộ mức PWM02 từ230 lên245, giữ trim−40/0 nên tiến/lùi205/245, xoay245/245. Xe01 và các thông số khác giữ nguyên; buildID02 `20261016-robot02-pwm245`. Chưa nạp/thử xe thật; các mức230 bên dưới là lịch sử trước lần tăng này.

16/10/2026, REQ-ROBOT02-PARITY-01: xe02 dùng cấu hình/capability giống01 (encoder16/4, sonar sau0/15, còi21, servo17 chu kỳ5s, pin/timeout), giữ nguyên PWM02 tiến/lùi/chỉnh230/trim−40/0/xoay230 và ID02/trạmS2. Cấu hình chung ở SharedRobotConfig.h, lùi về S2 tương tự01 vềS1; buildID02 `20261016-robot02-hardware-parity`. Các mô tả02 chưa có encoder/sonar sau/servo bên dưới là lịch sử. [ADR-059](decisions/ADR-059-robot02-firmware-parity.md).

16/10/2026, REQ-CORE-UNIFIED-01: firmware không còn được ghép với một binary Core test riêng. Core thống nhất chọn scope 01/02/cả hai và đọc capability từ diagnostic; MQTT/payload firmware không đổi, nên không cần nạp lại chỉ vì đổi scope. Việc nạp vẫn cần khi chính mã điều khiển/capability firmware thay đổi. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

16/10/2026, REQ-BATTERY-06: không đổi thuật toán lọc hoặc firmware payload. Người dùng chấp nhận `batteryPercentage` đã lọc trong diagnostic làm số chuẩn; Core mới dùng nó cho `battery/battery_measured/needs_recharge` và ngưỡng20/40. Telemetry NCKHAGV vẫn gửi `?` để tương thích. Mode thường cần diagnostic calibrated còn mới; mode trial vẫn giữ ngoại lệ không chặn pin. Chưa restart Core vận hành hoặc nghiệm thu `CHARGE_START`/bộ sạc thật. [ADR-021](decisions/ADR-021-display-only-battery.md).

15/10/2026, bản xe01 `20261015-robot01-servo45-test`: một servo GPIO17/kênhLEDC5,50Hz/16bit; chỉ mở kênh này, xe02 vẫn disabled. Người dùng xác nhận một servo/quay45° để test kết nối; quy ước test hạ0°/nâng45° dùng dải1000–2000µs, chưa hiệu chuẩn góc cơ khí/thông số mẫu servo. Không tự quay khi boot (duty0); lệnh mới tại kho/đúng phiên/xeIDLE không tuyến mới phát targetPWM, giữ chu kỳ5000ms không blocking. LiftCycle báo đang nâng/hạ và DONE sau5s, không phải feedback vị trí/tải. ServoModule cũ hai kênh giữ cho cấu hình khác. STOP/mất mạng/đổi node/lỗi ngắt kết quả và không replay; không tự chuyển góc khi ngắt, PWM giữ target cuối vì tự thả xung có thể làm rơi tải. Không coi STOP là đo/khóa được trục servo. Cần nguồn hạ áp phù hợp/chungGND, chưa xác nhận nguồn thực tế/chưa nạp hoặc chạy cơ cấu. Giữ encoder/sonar/PWMxoay210/lùiS1. [ADR-057](decisions/ADR-057-robot01-servo-lift-test.md).

15/10/2026, REQ-ROBOT01-REVERSE-HOME-01: bản `20261015-robot01-reverse-home` thay riêng GS1→S1 của xe01: xoay tại GS1 để đầu hướng S, dùng line nối dài đã được người dùng xác nhận; dừng một vòng khi bắt nhánh mới rồi chuyển sonar sau/lấy mẫu mới trước khi lùi. Đọc RFID S1 khi đang bám lùi thì STOP/IDLE/stationReady, không quay tại S1. Giữ PWM xoay210, timeout2500ms, encoder, lọc pin, sonar và STOP cục bộ. Core NODE_ORDER cần cờ `reverse_after_turn:true` khớp route/góc/capability; không có cờ đúng thì giữ dừng. Xe02/S2/kho K không đổi. Native/host và integration riêng PASS; chưa nạp hoặc thử sàn. [ADR-056](decisions/ADR-056-robot01-reverse-home-request.md).

15/10/2026, REQ-ROBOT01-STOP-LATENCY-01: bản `20261015-robot01-priority-sonar-stop` áp STOP sau cập nhật sonic trước RFID/SPI/command, thay vì chờ áp motor cuối vòng. Chốt vật cản vẫn ở ESP32, không phụ thuộc server STOP; MQTT và sonic60ms không đổi. Giữ PWM xoay210, encoder, ngưỡng15/20cm và driver EN=0 khi dừng. Chưa nạp/đo quãng trôi; ngưỡng15cm chưa bảo đảm thân xe dừng còn15cm. [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, REQ-ROBOT01-PIVOT-210-01: người dùng yêu cầu cuối cùng tăng PWM xoay xe01 lên210; trim quay0/0, timeout vẫn2500ms, chốt rời/bắt line giữ nguyên. Encoder/sonic/log/battery và tiến/lùi giữ nguyên. Bản mới `20261015-robot01-pivot210`, env01; chưa nạp/thử sàn. [ADR-054](decisions/ADR-054-robot01-slower-pivot.md).

15/10/2026, REQ-ROBOT01-SONAR-05: ảnh [SONAR] thực tế bắt được xung47487–47494us, đủ cạnh, nhưng bản cũ loại bởi hạn45ms. Bản xe01 `20261015-robot01-sonar-47ms-fix` chờ60ms; xung dài đã kết thúc coi ngoài tầm, không giả số cm, HIGH quá60ms vẫn invalid. Giữ cutoff khoảng cách30ms, chu kỳ60ms, stale300ms và ngưỡng15/20cm. Host tái hiện/PASS bản sửa; chưa nạp/thử vật có khoảng cách biết trước. [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, REQ-ROBOT01-SONAR-04: bản test01 `20261015-robot01-sonar-diagnostic` thêm Serial `[SONAR]` mỗi giây, trước GOTO và khi lỗi sonic, gồm mức ECHO, số cạnh, số xung TRIG, độ rộng xung, tuổi kết quả và lý do hợp lệ/không hợp lệ. Chưa thay cách đọc hay chốt motor; đây là bước chẩn đoán sau khi monitor/MQTT báo null trước cả GOTO. [Cách đọc log](../firmware/NCKHAGV/NCKHAGV/README.md), [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, REQ-ROBOT01-SONAR-03: sửa lỗi phần mềm có thể gây ULTRASONIC_INVALID khi xe 01 tới GS1 chuẩn bị xoay dù đường trống. HC-SR04 có xung không thấy vật khoảng 38 ms; bản trước chốt HIGH ở 30 ms. Profile01 mới chờ tối đa 45 ms nhưng giữ ngưỡng tính khoảng cách 30 ms; xung dài hoàn chỉnh được coi ngoài tầm, HIGH kẹt vẫn lỗi. FirmwareBuild `20261015-robot01-sonar-no-echo-fix`; chưa nạp/đối chiếu xung trên xe thật. [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, REQ-ROBOT01-SONAR-02: xe 01 chỉ kích sonar trước khi tiến/đứng chờ, sonar sau khi chuẩn bị/đang lùi và khi tạm dừng vì vật cản trên tuyến lùi. Đổi hướng dừng motor, xóa mẫu/echo cũ, chờ yên 50 ms rồi chờ mẫu mới tối đa 200 ms; hết hạn vẫn chưa hợp lệ thì lỗi cảm biến. FirmwareBuild `20261015-robot01-directional-sonar`, chân 12/13 và 0/15 giữ nguyên. Người dùng đã xác nhận dây khớp bảng chân; chưa nạp/thử chức năng mới trên xe thật. [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

14/10/2026: giảm nhẹ PWM xoay xe01 từ220 xuống200, trái/phải duty ngược chiều±200 để thử bắt line khi quay. Giữ tiến220/lùi190 và chốt line/timeout; cần nạp env01 và kiểm chứng trên sàn. [ADR-054](decisions/ADR-054-robot01-slower-pivot.md).

14/10/2026: riêng `esp32dev_two_robot_bench_01` đọc encoder bánh trước trái GPIO16/phải GPIO4, RISING/INPUT_PULLUP, lọc >300 µs, hệ số thử 161 xung/100 cm. ISR đếm raw ticks, quãng đường chặng chỉ cộng khi navigator bám line tiến/lùi; gửi odometry 200 ms. Chưa PID/nhả mutex theo encoder. [ADR-050](decisions/ADR-050-robot01-encoder-position-speed.md), [contract](../contracts/robot01-odometry.md) cập nhật việc loại encoder trước đây; profile 02/legacy chưa bật.

Cập nhật 01/10/2026: mutex đã chuyển sang JSON có grant_id/ACK/retry, sketch có LWT, TLS tùy cấu hình, watchdog và cmd_id trong NVS. [Thiết kế hiện tại và cấu hình](18-hardening.md) thay thế mô tả giao thức/phục hồi cũ bên dưới. Motor/phanh thật vẫn OPEN.

Người dùng đã xác nhận **ESP32 DevKit + Arduino IDE + PubSubClient + RC522**. [Sketch](../firmware/AGV_Connection_Test/README.md) có 26 UID node nạp sẵn, `learn` để thay thẻ, boot UNKNOWN đến khi quét; core/web đồng bộ Chờ quét RFID. [ADR-013](decisions/ADR-013-preloaded-rfid-uid-map.md). Bảng chân mới 03/10/2026 thay RC522 SS21 bằng SS5 để dành GPIO21 cho servo; L298N/line/HC-SR04 đã có module driver và liên động với MQTT/route nhưng motor khóa mặc định. [ADR-012](decisions/ADR-012-motor-firmware-integration.md). Chưa upload/đo board thật; đo pin, sạc và điều khiển rẽ vẫn OPEN.

## Thông số phải bổ sung

- MCU và framework: ESP-IDF, Arduino/PlatformIO, STM32 hoặc loại khác.
- Motor/driver, encoder, nguồn, pin, bộ đo pin và mạch sạc.
- Định vị: RFID, line sensor, odometry, LiDAR hoặc tổ hợp; bảng node ↔ tag.
- Cảm biến vật cản, nút dừng, hành vi an toàn khi mất mạng.
- Kích thước, tải trọng, vận tốc, khoảng dừng, sơ đồ chân và mức điện áp.

Không tự điền pinout, PWM hay điện áp từ một repo tham khảo; dùng datasheet và sơ đồ nhóm cung cấp.

## Tách các tác vụ

1. Điều khiển motor và đọc encoder/sensor theo nhịp phù hợp phần cứng.
2. Máy trạng thái local: khởi động, chờ, di chuyển, chờ vùng, sạc, lỗi, dừng.
3. Network worker: WiFi/MQTT reconnect, receive queue, telemetry, ACK.
4. Command parser: giới hạn độ dài, kiểu dữ liệu, path hợp lệ, cmd_id/session.

Callback MQTT không chạy vòng motor lâu hoặc delay chờ hết hành trình. Chuyển command hợp lệ vào queue/state rồi ACK; công việc điều khiển đọc trạng thái riêng. STOP có đường xử lý ưu tiên theo chính sách đã xác định.

## Vòng trao đổi hiện tại

Robot subscribe `agv/{id}/command` và `mutex_grant`; publish telemetry dạng `node|battery|obstacle`, ACK text và các event. Giữ đúng [hợp đồng v0](07-network.md) khi tích hợp core hiện có.

Với GOTO, path gồm node còn lại. Firmware dùng bảng chuyển node/hướng phù hợp đường thực; node ID không trực tiếp là lệnh motor. Khi đến RFID/node, publish số đo đã xác nhận. Vị trí khởi động phải được xác minh; không mặc định robot đang ở trạm sạc chỉ vì server hiển thị như vậy.

Trước vùng chung, dừng ở điểm chờ và xin mutex. Chỉ đi khi được grant cho đúng zone; release khi thân robot rời hết vùng. Nếu mất grant, request lại có thể được owner hiện tại xác nhận trong core; cần kiểm thử tránh chiếm hai lần.

## Retry và phục hồi

- Cùng command trong phiên: không lặp hành động, vẫn ACK lại.
- Payload sai hoặc quá dài: báo lỗi theo hợp đồng; không ACK như đã nhận lệnh hợp lệ.
- Mất MQTT: local watchdog chuyển sang hành vi đã chốt; không tự nhận path cũ sau reconnect.
- Server/robot reboot: thiết kế session ID hoặc lưu vết; đây là khoảng trống v0 cần xử lý.
- Sạc chỉ ở node/điều kiện xác nhận; `charging_done` gửi pin đo thật nếu có.

Thời gian watchdog, debounce sensor và tần suất telemetry chưa được chốt. Dùng timeout core 10 s làm dữ kiện, không tự suy ra robot được phép chạy không liên lạc 10 s.

## Thử nghiệm bàn và tích hợp

Đầu tiên log command/ACK/sensor khi motor chưa chạy; kiểm tra buffer cho đường dài và deduplicate. Sau đó thử motor với điều kiện bàn phù hợp, đo local stop, mất WiFi và cảm biến lỗi. Tích hợp hai robot chỉ sau khi đã xác minh vị trí, vùng dùng chung và chính sách mất mạng.

Tham khảo [ESP MQTT](https://github.com/espressif/esp-mqtt) hoặc [PubSubClient](https://github.com/knolleary/pubsubclient) theo framework. Điểm đặt code sau khi chốt: [firmware](../firmware/README.md). Kỹ năng: [agv-firmware](../.agents/skills/agv-firmware/SKILL.md).
## Project NCKHAGV bổ sung 04/10/2026

Mã ESP32 PlatformIO tại [NCKHAGV](../firmware/NCKHAGV/NCKHAGV/README.md) đã nhận hợp đồng MQTT v0 của Core, bảng 26 UID và fail-safe kết nối. Theo bổ sung 04/10, hướng rẽ được tính từ path và đồ thị có hướng; motor vẫn khóa vì điều kiện rời M1–M6, quyền mutex và hiệu chuẩn chuyển động chưa hoàn chỉnh. Pin ADC chưa hiệu chuẩn nên không gửi giá trị 0% giả cho Core. Xem [ADR-014](decisions/ADR-014-nckhagv-core-sync.md).

Theo thông số phần cứng bổ sung 04/10, project NCKHAGV đã bỏ module encoder và trường xung/quãng đường chẩn đoán; bánh 6,5 cm chỉ là kích thước cơ khí. Line đen active HIGH, mẫu đi thẳng `010/101/111`; siêu âm thử dừng ≤15 cm. Sketch độc lập cùng pinout xác nhận HIGH/LOW ở cả hai cầu là tiến, LOW/HIGH là lùi; REQ-FW-NCKH-19 sửa các cờ đảo cực tính vốn khiến GOTO S1→GS1 phát pattern lùi. Bám line trên mặt đất vẫn OPEN. Xe 01 trước đó được thử lùi S1→GS1; logic bám line khi lùi và dừng khi mất vạch có trong mã. Hai cell 18650/B25 chưa có điện áp đo nên Core chưa nhận pin thật. [ADR-014](decisions/ADR-014-nckhagv-core-sync.md) giữ rõ giới hạn mở motor và ngưỡng thử 28 cm.

Bản điều khiển cơ bản sau đó nối GOTO vào `CoreNavigator` theo từng RFID và xin/ACK quyền vùng M; chưa tự trả quyền vì chưa xác minh thân xe rời vùng. `node|?|obstacle` giúp Core hiện vị trí thật dù pin chưa đo; trường pin không được coi là số đo và Core tiếp tục chặn task. PWM vẫn khóa mặc định; host/Core test đã chạy, PlatformIO và xe thật chưa được kiểm chứng. [ADR-015](decisions/ADR-015-basic-rfid-line-motion.md).

Người dùng yêu cầu thử xe chạy bằng server: env `esp32dev_trial` mở PWM cho **duy nhất xe 01 lùi S1→GS1**, tối đa 8 giây, với Core opt-in `AGV_ONE_ROBOT_TRIAL=1`. Env `esp32dev` mặc định vẫn khóa motor. Build ESP32 và fixture Core/broker/gateway đã PASS; chưa chứng minh chiều lái, siêu âm khi lùi hoặc thử bánh trên giá kê. [ADR-016](decisions/ADR-016-single-robot-server-motion-trial.md).

Cập nhật 05/10/2026: theo yêu cầu mới, chính env/công tắc trial trên đã mở GOTO xe 01 theo đường Dijkstra tới mọi node có đường trên map; firmware đi từng RFID và bỏ chờ grant riêng trial một xe. Bản thường vẫn khóa motor. Mỗi đoạn dừng sau tối đa 15 giây, mỗi lần quay sau 2,5 giây; chưa hiệu chuẩn ngoài đời. Core chặn task tự động, xe khác và owner mutex cũ; telemetry xe khác trong lúc chạy làm gián đoạn trial. Kiểm chứng 650 cặp node và fixture phần mềm, chưa nạp hoặc chạy xe thật. [ADR-017](decisions/ADR-017-solo-full-map-trial.md).

Cập nhật 07/10/2026: quan sát xe 01 lùi ra khỏi map khi nhận S1→GS1 đã bác bỏ giả định hướng xuất phát cũ. Quy tắc trial mới yêu cầu đầu xe đặt tại S1 hướng về GS1 và chọn tiến cho đoạn này. Các đoạn mô tả lùi S1→GS1 ở trên là lịch sử thiết kế, không còn là hướng dẫn vận hành. Ca trial được kết thúc và lỗi vị trí được giữ để người vận hành kiểm tra trước lần thử tiếp theo. [ADR-017](decisions/ADR-017-solo-full-map-trial.md).

Cập nhật 07/10/2026: sketch W/S/A/D đã xác nhận polarity cũ của firmware đảo hai bên, khiến S1→GS1 chạy lùi. REQ-FW-NCKH-19 đặt hai cờ đảo cực tính về `false`. Sau khi pattern trái lùi/phải tiến vẫn làm xe quay phải tại GS1, REQ-FW-NCKH-23 đặt pivot trái trial về trái tiến/phải lùi ở 255/255, khớp phép thử sketch D quay trái. Serial Monitor ghi `[MOTOR]` cùng duty và line pattern để phân biệt pivot với chỉnh line một bên. Bản `esp32dev_trial` build thành công; chưa nạp/kiểm chứng chuyển động vật lý của bản mới.

Cập nhật xe 01 mới 07/10/2026: theo REQ-FW-NCKH-24, xe thay thế giữ nguyên chân nhưng dùng quy ước quay chuẩn: `LEFT` là trái lùi/phải tiến, `RIGHT` là trái tiến/phải lùi; cả hai bên PWM 255 trong trial. Chỉnh line khi tiến cũng đổi bên motor để `CorrectLeft` thực sự hướng trái và `CorrectRight` hướng phải. Mapping theo sketch xe cũ ở đoạn trên là lịch sử, không dùng cho xe mới. Duty danh định chạy thẳng trial 220/220, trim trái trial `-30` và chế độ thường `-40`; trim mới cùng khả năng bám line còn chờ đo trên xe mới.

Cập nhật sau thử GS1 07/10/2026: xe mới quay phải khi nhận mapping `LEFT` trên và pivot 255/255 quá nhanh. REQ-FW-NCKH-25 đổi `LEFT` thành trái tiến/phải lùi, `RIGHT` đối xứng, đồng thời giảm PWM pivot trial còn 200/200. Chỉnh line khi tiến khớp lại bên motor theo chiều quay thực tế. Mục REQ-FW-NCKH-24 ở trên là lịch sử; bản mới chưa nạp và chưa xác nhận bắt lại line.

Cập nhật pin 08/10/2026: firmware dùng GPIO36 và mạch chia 30 kΩ/7.5 kΩ để ước tính pin 2S trong dải 6.4–8.4 V. Để người dùng thử hiển thị trước, phần trăm gửi qua JSON diagnostic; telemetry điều khiển vẫn gửi `?`, nên Core chưa áp dụng ngưỡng 20%/40%. Điện áp và phần trăm cần được đối chiếu bằng đồng hồ trên xe trước khi cho phép ngưỡng điều phối sử dụng số liệu này.

Sau quan sát pin 65–70% trên xe, `BatteryModule` mới lấy 16 mẫu ADC không chặn trong mỗi đợt, bỏ mẫu cao nhất/thấp nhất, lọc giữa các đợt và thêm biên 1,5 điểm cho phần trăm. Bản `esp32dev_trial` đã build thành công ngày 08/10/2026; chưa nạp hoặc đo xe thật với bộ lọc này.

Cập nhật 15/10/2026 theo REQ-BATTERY-04: phần trăm chỉ đổi 1% sau ít nhất 5 giây và 3 đợt cùng xu hướng; giữ số khi duty motor khác 0, chờ motor nghỉ 3 giây và lấy trọn đợt mẫu nghỉ trước khi cập nhật. Bộ lọc % nghỉ tách riêng bộ lọc điện áp đang dùng cho chốt bench; điện áp vẫn cập nhật khi xe chạy. Chưa có tín hiệu sạc thật nên cho phép số tăng/giảm chậm lúc nghỉ. Lần đầu boot chờ khoảng 3,5–4 giây mới có %; chạy liên tục giữ số cũ tới khi nghỉ. Chi tiết và kiểm chứng trong [ADR-021](decisions/ADR-021-display-only-battery.md).

Cập nhật REQ-FW-NCKH-26: `010` là mẫu bám line hợp lệ; web báo `LINE_LOST` do lỗi firmware đã chốt từ một mẫu `000` trước đó, còn marker GS1 là RFID cuối. Để tránh chốt lỗi vì một mẫu thoáng qua, bản trial dừng motor ngay ở `000`, xác nhận mất line liên tục 120 ms rồi mới báo lỗi; Serial ghi mẫu `[LINE]` tại lúc xử lý. Cần nạp và quan sát xe thật để biết nguyên nhân gốc.
## Xe 01: sonar trước/sau (15/10/2026)

Profile `esp32dev_two_robot_bench_01` nhận HC-SR04 trước TRIG/ECHO GPIO12/13, sau GPIO0/15; còi active LOW dùng GPIO21 theo ảnh người dùng. Firmware kích xen kẽ cách 50 ms, timeout echo 30 ms; sonar phía di chuyển ≤15 cm giữ STOP và tuyến, chỉ nhả khi >20 cm hoặc timeout với ECHO LOW (ngoài tầm theo sketch thử). ECHO kẹt HIGH/dữ liệu cũ dừng lỗi cảm biến; dây ECHO đứt LOW không thể phân biệt với ngoài tầm đo. FirmwareBuild `20261015-robot01-dual-sonar`; xe 02 giữ pin và behavior cũ. Xem [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md), [hợp đồng](07-network.md). Chưa nạp/đo quãng phanh thực tế; ECHO 5 V phải qua chia áp/chuyển mức 3,3 V và cần kiểm tra GPIO0 lúc boot trước thử motor.
