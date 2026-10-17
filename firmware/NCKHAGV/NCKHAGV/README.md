# Firmware NCKHAGV (ESP32 DevKit)

17/10/2026 — Sửa dừng sau trả mutex: buildID01 `20261017-robot01-mutex30-exitfix-v4`,02 `20261017-robot02-mutex30-exitfix-v4`. Đủ điều kiện30 xung và queue release thành công thì đánh dấu đã rời vùng trên chặng hiện tại; xe không xin lại M3 khi đang đi tới G1. Dấu xóa khi đổi RFID/tuyến/lỗi/phiên; mutex tiếp theo vẫn cần token mới. Cần nạp lại đúng env từng xe để áp dụng. Tuning hiện tại và grace7s xe02 giữ nguyên. [ADR-068](../../../docs/decisions/ADR-068-encoder-mutex-tail-clearance.md).

17/10/2026 — Giữ task khi mất line ngắn riêng02: `AGV_LINE_LOSS_GRACE_MS=7000` trong `src/profiles/Robot02Config.h`. Mất000 dừng ngay, quét có giới hạn600ms rồi dừng chờ line;010 ổn định100ms/≥3 mẫu trong7s thì tiếp tục ngay task cũ. Không tìm được mớiLINE_LOST. Không tăng quét lên7s hoặc đổi timeout chặng15s/lỗi mạng/sonar/STOP/xe01. BuildID02 `20261017-robot02-line-grace7s-v4`; chỉ cần nạp firmware02, không đổi Core.

17/10/2026 — Xe02 lùi nhẹ hơn: `src/profiles/Robot02Config.h` đặt reverse/correction200, `AGV_REVERSE_GENTLE_CORRECTION=1`, `AGV_REVERSE_CORRECTION_DELTA=20`; giữ trim lùi+5/−10. `MovementModule.cpp` cho hai bánh cùng lùi khi căn, kể cả settling:010=205/190,011=205/170,110=185/190 (kênhLEFT/RIGHT trong mã). Xe01/tiến/xoay giữ nguyên. BuildID02 `20261017-robot02-gentle-reverse-v4`; chưa upload/thử sàn. Cần xác nhận cả hai bánh vẫn quay ở PWM giảm trước chạy task.

17/10/2026 — Phản ứng bám line: build mới `20261017-robot01-fast-line-v4` / `20261017-robot02-fast-line-v4`, cùng map22node. Áp chỉnh line trước lượt đọc RFID, PWM cùng chiều cập nhật trực tiếp và log chạy giới hạn200ms; hoãn log sonar định kỳ trong pha Following, giữ diagnostic MQTT/log fault. PWM/trim người dùng hiện có không đổi. Hai env nạp vẫn `esp32dev_two_robot_bench_01` / `_02`. Đã kiểm tra phần mềm; chưa upload/đo độ mượt thật. Xem [ADR-066](../../../docs/decisions/ADR-066-fast-line-response.md). BuildID rounded-map-v4 bên dưới là bản trước khi giảm trễ.

17/10/2026 — Map bo góc v4: Core scope 01/02/cả hai dùng 22 node, bỏ C1–C4. G1→M1, M2→G6, G6→M4, M3→G1 là cạnh liên tục 135 cm theo nhãn 85+50, hướng rời/tới khác nhau. Xe bám line qua góc, không pivot/chờ RFID ở góc; giữ PWM/chân dây/timeout. Phải build/nạp đúng hai env bench01/02 trước khi chạy với Core mới. BuildID `20261017-robot01-rounded-map-v4` / `20261017-robot02-rounded-map-v4`. Chưa upload/thử sàn; cần đo bán kính và chiều dài RFID→RFID theo line. Xem [ADR-064](../../../docs/decisions/ADR-064-rounded-map-without-corner-tags.md).

16/10/2026: cân lùi xe02 trong `src/profiles/Robot02Config.h`: `AGV_REVERSE_LEFT_SPEED_TRIM=5` (giảm10 so với trim tiến+15). Với reverse210, duty trái215/phải195; reverse correction220 cho trái225/phải205. Không đổi PWM tiến/xoay hoặc xe01. BuildID `20261016-robot02-reverse-left215`; chưa upload/hiệu chuẩn thực tế.

16/10/2026, bản02 mới `20261016-robot02-line-align`: LEFT phân biệt có line và căn giữa.011/110 giữ phiếu có line, căn theo cảm biến bằng pivot215 có xung;010 không phải giữ100ms.≥2 phiếu010/011/110 trong tối đa3 mẫu cách10ms và mẫu mới nhất010 mới STOP/chuyển bám; gap>40ms bỏ phiếu cũ.111/101 chưa xác nhận nhánh,000 không tiến mù, tổng600ms. Sau bắt line cap duty mỗi bánh≤215 trong200ms rồi về tuning thường. Giữ xoay thường230, xe01 và phục hồi mất line tiến/lùi không đổi. Native/host PASS; chưa nạp/hiệu chuẩn xe thật. Điều kiện010/100ms cho TurnCapture bên dưới là lịch sử.

16/10/2026, tuning mới xe02 vì xoay trái yếu: `AGV_BENCH_TURN_SPEED=230`, `AGV_LEFT_TURN_CAPTURE_PWM=215` trong `src/profiles/Robot02Config.h`. Pivot thường cả hai hướng dùng230; pha dò/bắt line LEFT và chỉnh ngược ngắn dùng215. Quét mất line tiến/lùi vẫn180, không đổi PWM chạy hoặc xe01. Giữ STOP khi mắt giữa bắt lại line. BuildID `20261016-robot02-turn230-capture215`, env `esp32dev_two_robot_bench_02`; chưa upload/thử xe thật. Các mức215→180 bên dưới là lịch sử.

16/10/2026, bản xe02 mới nhất `20261016-robot02-left-capture`: LEFT rời line thì giảm pivot180, mắt giữa bắt lại thì STOP ngay trước RFID, xác nhận010 ổn định100ms rồi mới tiếp tục task. Trôi quá vạch chỉ chỉnh ngược phải bằng xung ngắn trong600ms; bỏ lượt quay trái dài thêm2500ms. Giữ PWM chạy/RIGHT/profile01 hiện có. Native/hostGPIO/build02 PASS; chưa nạp/thử xe thật. [ADR-063](../../../docs/decisions/ADR-063-robot02-left-turn-capture.md).

16/10/2026, REQ-LINE-SEARCH-01 bổ sung tìm line tại chỗ cho cả hai profile: `000` dừng ngay; có hướng chỉnh≤750ms thì STOP100ms, tìm cùng phía PWM180/xung40ms/nghỉ60ms, tối đa600ms và chỉ một lượt/chặng. `010` ổn định100ms/≥3 mẫu mới tiếp tục task; không có hướng mới thì giữ dừng chờ tối đa5s. Chỉnh giới hạn tại `src/Config.h` nhóm `LINE_RECOVERY_*`; đây là giới hạn thời gian, chưa quy đổi góc. BuildID01 `20261016-robot01-line-search`,02 `20261016-robot02-line-search`. Giữ nguyên PWM thường hiện tại của từng profile; chưa nạp/thử xe thật. [ADR-062](../../../docs/decisions/ADR-062-local-line-search.md).

16/10/2026, bản mới cho cả xe01/02 thử bắt lại line sau quẹo: LEFT/RIGHT hết2500ms chưa bắt line sẽ STOP một vòng rồi quay lại cùng hướng tối đa2500ms đúng một lần. Bắt `010/101/111` sau khi đã thấy`000` thì tiếp tục task; thất bại mới phát `LINE_LOST`. Không retry quay đầu/xoay trạm/chạy thẳng và không nới chốt mạng/sonar/vật cản/STOP. BuildID01 `20261016-robot01-turn-line-retry`, buildID02 `20261016-robot02-pwm230-turn-retry`; chưa upload/thử xe thật. [ADR-061](../../../docs/decisions/ADR-061-single-turn-line-retry.md).

16/10/2026, cập nhật PWM02 mới nhất: tiến/lùi/chỉnh line cơ sở230, trim trái−35/phải+15 nên duty thực tế195/245; xoay giữ245/245. Xe01 không đổi. BuildID02 `20261016-robot02-pwm230-l195-r245`, env `esp32dev_two_robot_bench_02`; chưa upload/thử sàn. Cần kê bánh/tháo tải kiểm tra chiều, STOP và quãng dừng trước chạy tuyến. Cấu hình tăng toàn bộ lên245 và các giá trị230 cũ bên dưới là lịch sử.

16/10/2026, [ADR-059](../../../docs/decisions/ADR-059-robot02-firmware-parity.md): xe02 đồng bộ phần cứng/capability với01, giữ PWM02 tiến/lùi/chỉnh230, trim trái−40/phải0, pivot230. Tham số chung ở `src/profiles/SharedRobotConfig.h`; xe02 dùng ID02 và home S2/GS2. Bật encoder16/4, sonar sau0/15/còi21, servo17 hạ0/nâng45 trong5s và lùi về S2; firmwareBuild `20261016-robot02-hardware-parity`. Build env `esp32dev_two_robot_bench_02`; không nạp binary01 cho02. Cả hai dùng cấu hình mạng chung khi không có file override riêng. Các đoạn02 chưa hỗ trợ các phần trên bên dưới là lịch sử; xem tests/README.md cho kết quả mới. Chưa nạp xe thật.

15/10/2026, mới nhất xe01 `20261015-robot01-servo45-test`: một servo GPIO17, hạ0°/nâng45° danh định (quy ước test, chưa hiệu chuẩn), PWM1000–2000µs/50Hz/kênh5, chu kỳ5000ms. Boot duty0 không phát góc. Nút kho hiện có: tới RFID kho, chờ dừng/diagnostic mới, Core LIFT_UP/DOWN, nhúng báo LOADING/UNLOADING rồi DONE; Core/web cập nhật một kiện ảo sau nâng, bỏ sau hạ, mới cấp tuyến lùi gate. Cần cập nhật cả Core/gateway và firmware01. Giữ sonic/encoder/lùiS1/PWMxoay210. Trước khi thử phải cấp nguồn servo hạ áp đúng/chungGND, tháo tải và kiểm tra giới hạn0/45; không cấp8,4V trực tiếp. Chưa nạp/chạy servo thật. [ADR-057](../../../docs/decisions/ADR-057-robot01-servo-lift-test.md).

15/10/2026, bản mới nhất xe01 `20261015-robot01-reverse-home`: về S1 sẽ STOP tại GS1, nhận NODE_ORDER xoay đầu ra ngoài theo line nối dài rồi mới chuyển sonar sau/lùi. Đến RFID S1 dừng và hoàn thành theo stationReady, không xoay tiếp tại S1. Giữ PWM210/timeout2500ms, encoder GPIO16/4, lọc pin và STOP sonic ưu tiên. Cần cập nhật cả Core và firmware; không chỉ nạp MCU với Core cũ. Chưa nạp/thử xe thật. [ADR-056](../../../docs/decisions/ADR-056-robot01-reverse-home-request.md).

15/10/2026, bản mới nhất xe01 `20261015-robot01-priority-sonar-stop`: ưu tiên STOP cục bộ ngay sau xử lý mẫu sonic, trước đọc RFID/SPI và xử lý command. Giữ MQTT, chu kỳ sonic60ms, xử lý xung47,5ms, ngưỡng15/20cm, PWM xoay210 và encoder. Navigator vẫn giữ route/mutex và nhả khi mẫu thông đường đúng điều kiện. Chưa nạp hoặc đo độ trễ/quãng dừng trên xe thật. Dừng hiện tại EN=0 để thả trôi; cần đo khoảng trôi và vị trí sensor để chọn ngưỡng đọc sớm nếu muốn thân xe dừng còn15cm. [ADR-055](../../../docs/decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, bản mới nhất xe01 `20261015-robot01-pivot210`: theo yêu cầu cuối cùng đặt PWM xoay210/255, trim quay0/0; áp dụng trái/phải và quay đỗ S1. Thời hạn vẫn2,5giây, rời line000 rồi nhận010/101/111. Giữ encoder GPIO16/4, sonic47ms-fix/log, bộ lọc pin và tiến/lùi. Không áp phương án180/3,5giây. Env `esp32dev_two_robot_bench_01`, chưa nạp hoặc đo bắt line trên sàn. [ADR-054](../../../docs/decisions/ADR-054-robot01-slower-pivot.md).

15/10/2026, bản mới nhất test01 `20261015-robot01-sonar-47ms-fix`: ảnh log thực tế cho thấy xung47,5ms có đủ cạnh nhưng bị loại bởi hạn45ms. Hạn chờ profile01 đổi thành60ms, cutoff tính khoảng cách vẫn30ms. Xung dài đã kết thúc có `result=NO_TARGET_PULSE valid=1 cm=nan`; đây là ngoài tầm, không phải số đo khoảng cách. HIGH quá60ms vẫn invalid. Giữ log diagnostic và mọi chốt chuyển hướng/vật cản. Regression theo47487/47494us cùng ca vật10cm/HIGH kẹt PASS; chưa nạp hoặc xác nhận với vật ở30cm trên xe thật. Bản38ms/45ms và diagnostic bên dưới là lịch sử.

15/10/2026, bản test01 `20261015-robot01-sonar-diagnostic` bổ sung log ECHO phục vụ REQ-ROBOT01-SONAR-04. Dùng env `esp32dev_two_robot_bench_01`, đọc Serial COM6/115200 sau khi nạp đúng xe. Mỗi giây có hai dòng `[SONAR] side=front/rear`, cùng dòng lúc nhận GOTO và lúc lỗi sonic. Đây là log chẩn đoán, không sửa chốt motor/ngưỡng vật cản/payload MQTT.

- `active=1`: kênh đang dùng; chỉ phân tích cảm biến sau khi kênh sau active (tuyến lùi). `trig`/`echoPin`: GPIO cấu hình; `echo`: mức điện hiện đọc ở GPIO, không phải phép đo điện áp.
- `triggers`: số xung TRIG đã yêu cầu từ boot; `edges`: số cạnh GPIO ISR đã nhận, kể cả khi kênh không chờ. Nếu triggers tăng nhưng edges không tăng, bộ đọc chưa nhận được cạnh; chưa đủ phân biệt dây/nguồn/cảm biến/ISR.
- `waiting/rise/ready`: đang chờ echo/đã thấy sườn lên/có xung hoàn tất chưa được vòng lặp xử lý. Chỉ là ảnh chụp tức thời.
- `pulse_us`: xung hoàn tất gần nhất đã xử lý (timeout=0). `result_age_ms`: tuổi kết quả gần nhất, kể cả timeout; `4294967295` là chưa có kết quả từ lúc boot hoặc đổi kênh. Không đồng nhất tuổi timeout với tuổi số đo khoảng cách hợp lệ.
- `result=RANGE`: có khoảng cách; `NO_TARGET_PULSE`: xung dài đã kết thúc, ngoài tầm; `NO_ECHO_LOW`: timeout chân LOW theo chính sách hiện có; `TIMEOUT_INVALID`: timeout không hợp lệ; `INVALID_PULSE`: xung không hợp lệ; `NONE`: chưa có kết quả. `valid=0` cùng tuổi>300ms có thể là mẫu cũ. `cm=nan` không tự chứng minh cảm biến hỏng, cần đối chiếu result/active/valid.

Ba host probe PASS cho diagnostic không tiêu thụ cờ ISR, không kích thêm xung, tuổi/lý do và hồi quy chuyển hướng/38ms/STOP. Chưa nạp hoặc đọc log mới trên xe thật.

15/10/2026, bản xe01 `20261015-robot01-sonar-no-echo-fix`: sửa cách nhận xung HC-SR04 không thấy vật (~38 ms) để tránh chốt ULTRASONIC_INVALID ở 30 ms khi S1→GS1 chuẩn bị xoay. Profile01 chờ echo tối đa 45 ms, xung khoảng cách vẫn giới hạn 30 ms; xung dài hoàn chỉnh là ngoài tầm, HIGH quá hạn vẫn lỗi. Giữ quét theo hướng và bộ lọc pin. Host tái hiện lỗi cũ/PASS bản sửa; chưa nạp/thử lại xe thật. [ADR-055](../../../docs/decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

Cập nhật 15/10/2026, REQ-ROBOT01-SONAR-02: firmwareBuild xe 01 `20261015-robot01-directional-sonar` chỉ kích cảm biến sau khi chuẩn bị lùi, đang lùi hoặc dừng chờ vật cản phía sau; khi tiến/đứng chờ không có tuyến lùi thì chỉ kích phía trước. STOP hủy tuyến chuyển về trước. Khi đổi hướng, motor dừng, bỏ echo/mẫu cũ, chờ yên 50 ms và chờ mẫu mới tối đa 200 ms; quá hạn vẫn chưa hợp lệ thì lỗi cảm biến. Kênh không được kích có khoảng cách `null`; không phải lỗi mất kết nối. Giữ bộ lọc pin mới, ngưỡng vật cản 15/20 cm và bảng chân đã được người dùng xác nhận. Đây là bản thay thế cơ chế kích luân phiên ở mục bên dưới; chưa nạp hoặc thử chuyển động thật.

15/10/2026: profile xe 01 `20261015-robot01-dual-sonar` dùng hai HC-SR04: trước TRIG12/ECHO13, sau TRIG0/ECHO15; còi active LOW ở GPIO21. Lệnh GOTO đang chạy tạm dừng khi sonar phía di chuyển đọc ≤15 cm, nhả sau >20 cm; cờ telemetry pipe vẫn 0 để Core test 01 giữ tuyến, diagnostic báo hướng/khoảng cách cho web. Cần chia áp ECHO 5 V xuống 3,3 V và kiểm tra GPIO0 boot, không nối GPIO15 với servo; hai servo chung GPIO17 trên ảnh chưa có điều khiển độc lập trong firmware. Chưa nạp hoặc đo xe thật. [ADR-055](../../../docs/decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

14/10/2026: bản xe01 `20261014-robot01-slower-pivot` giảm PWM xoay220→200, trim0/0 cho cả trái/phải/quay đỗ S1. Tiến220/lùi190, điều kiện rời line rồi bắt line và timeout giữ tuning/chốt hiện có. Dùng env `esp32dev_two_robot_bench_01`; ảnh ở `build/robot01-slower-pivot` tính từ gốc workspace. [ADR-054](../../../docs/decisions/ADR-054-robot01-slower-pivot.md). Sau nạp cần thử bắt line và lực xoay trên sàn; giảm PWM chưa chứng minh đã hết bỏ nhánh.

14/10/2026: profile `esp32dev_two_robot_bench_01` có encoder trái GPIO16/phải GPIO4 theo file test của người dùng. Hệ số thử `AGV_ENCODER_PULSES_PER_100CM=161.0f` trong `src/profiles/Robot01Config.h`, firmwareBuild `20261014-robot01-encoder`. Gửi odometry 200 ms để Core menu **1 — thử xe 01** hiển thị vị trí ước tính và tốc độ cm/s. Giữ nguyên motor/line/RFID/STOP/watchdog/mutex và PWM lùi 190; không ghép BOOT hoặc chạy tự do từ sketch test. Build ảnh 01 thành công; chưa upload/hiệu chuẩn trên sàn. [ADR-050](../../../docs/decisions/ADR-050-robot01-encoder-position-speed.md), [hợp đồng](../../../contracts/robot01-odometry.md).

Sau khi nạp đúng ảnh mới, chạy Core mới chọn menu 1, mở gateway/web rồi quét RFID và thử đoạn ngắn. Tốc độ chưa nhận/cũ là `—`; marker nét đứt là ước tính encoder, vòng nhỏ giữ RFID gần nhất. Profile 02/legacy chưa bật encoder. Hệ số 161 cần đối chiếu nhiều lượt 100 cm trên xe thực tế; không lấy độ chính xác fixture làm sai số nghiệm thu.

Khôi phục xe01 ngày 10/10/2026: source trở về `20261010-grant-buffer-fix`, RFID20ms, PWM220/trim trái-40; build có SHA-256 trùng hoàn toàn ảnh đã nạp hồi sáng. Dùng env `esp32dev_two_robot_bench_01`; xem [ADR-041](../../../docs/decisions/ADR-041-restore-morning-robot01-firmware.md). Các mục RFID10ms/PWM245 bên dưới giữ lịch sử trước khôi phục và đính chính, không phải cấu hình hiện tại.

## Tăng nhịp đọc RC522 — 10/10/2026

Theo yêu cầu mới, NCKHAGV giảm khoảng poll RFID từ 20 xuống **10 ms**, giữ một WUPA, dừng trước HALT và đọc lặp không tiến tuyến. `firmwareBuild` mới là `20261010-rfid-poll-10ms`; cần nạp ảnh tương ứng để xe dùng nhịp mới. 10 ms là khoảng tối thiểu giữa lần bắt đầu đọc, không bảo đảm tốc độ UID thực tế gấp đôi hoặc tầm đọc xa hơn. Các thông số motor/mạng hiện có được giữ. Test adapter và build `esp32dev_two_robot_bench_01` PASS, chưa nạp/đo trên xe. Xem [ADR-031](../../../docs/decisions/ADR-031-rfid-read-latency.md). Các ghi nhận 20 ms phía dưới là lịch sử trước lần cập nhật này.

## Chẩn đoán xe 02 dừng ở GS2 — 09/10/2026

OBSERVED lần thử Core `robot02` lúc 22:50:46: diagnostic xe 02 đã có `resetReason=PANIC`, code 4, bench/motion enabled; trạng thái hiện tại SAFE_STOP/INVALID_COMMAND, GS2, không tuyến, PWM 0/0. Log Core ghi giao tuyến lúc 22:42:22/23, LWT lúc 22:42:30, online 22:42:35, xóa lỗi 22:42:43 rồi INVALID_COMMAND 22:42:44; M3 owner 02/grant 48134 đã ACK. PANIC thuộc lần boot gần nhất, không phải khẳng định xe đang panic tại thời điểm đọc. Chưa có backtrace để xác định dòng gây reset hoặc kết luận lỗi nguồn. Sau reset tại GS2, mất previous RFID khiến hướng xuất phát UNKNOWN; tuyến còn lại có thể bị `CoreNavigator::start` từ chối. Xóa lỗi không khôi phục tuyến/hướng cũ.

`esp32dev_two_robot_bench_02` bật `esp32_exception_decoder` cho Serial Monitor. Khi xe đã dừng và chuẩn bị một lần thử mới trên giá kê, mở monitor **trước khi mở ca/giao tuyến**; việc mở cổng có thể reset bo. Dùng đúng COM của xe 02 và đúng ELF của ảnh đã nạp (không build lại trước khi giải mã lỗi):

```powershell
cd 'C:\Users\DELL\OneDrive\noobcoder\AGVproject\firmware\NCKHAGV\NCKHAGV'
& 'C:\Users\DELL\.platformio\penv\Scripts\platformio.exe' device monitor -e esp32dev_two_robot_bench_02 -p COMx -b 115200
```

Giữ đoạn `[ROUTE]`, `[MOTOR]`, `Guru Meditation Error`, `Backtrace` và `[BOOT]` liên tiếp. Bộ giải mã chỉ dịch địa chỉ về file/dòng khi ELF khớp; đây chưa phải bản sửa PANIC hoặc xác nhận đã quay trên phần cứng.

Lần thử tiếp theo: task `task-30721` tới M3 đã được grant 47114/ACK; mất MQTT lúc 22:27:54/58. Uptime đọc lúc 22:30:22 suy thời điểm boot khoảng 22:27:52. Người dùng xác nhận không reset sau khi dừng và ESP32 lấy nguồn 5V từ L298N. Đây là bằng chứng bo tự khởi động lại; sụt/nhiễu nguồn là giả thuyết cần kiểm chứng, chưa kết luận brownout hoặc loại trừ watchdog/panic.

Ảnh chẩn đoán mới gửi `resetReason`, `resetReasonCode`, `firmwareMode`, `motionEnabled` trong diagnostic và `[BOOT]` trên Serial. Không thay PWM, hướng quay hoặc mutex. Để kiểm chứng nguồn trên giá kê: kết thúc ca/chờ motor dừng, tắt nguồn; tháo dây 5V L298N đang cấp ESP32, giữ GND ESP32–L298N chung; cấp ESP32 bằng một nguồn USB riêng, driver/motor vẫn dùng nguồn motor hiện có. Sau khi đặt xe đúng S2/hướng GS2, mở lại ca và người vận hành thử tuyến; so reset/uptime với lần trước. Không cấp đồng thời USB và 5V driver cho ESP32 trong phép thử này. Bo DevKit V1 trong dự án chưa có sơ đồ nguồn chi tiết; không tự đổi jumper hoặc nối pin khác.

Nguồn đối chiếu: [Espressif — brownout và panic](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/fatal-errors.html), [nguồn USB/5V trên DevKitC tham chiếu](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html), đọc 09/10/2026. Hướng dẫn nguồn DevKitC không phải chứng minh sơ đồ của bo clone đang dùng. Reset reason lấy từ SDK ESP32 đã cài, không vô hiệu brownout/watchdog để cho xe chạy.

Core đã ghi task `task-29706` tới C3 gián đoạn vì `MQTT LWT offline` lúc 22:10:50. Sau khi người dùng bấm xóa lỗi, diagnostic tại GS2 báo IDLE/NONE/STOP, không tuyến; không dùng mẫu này để kết luận motor đã quay hoặc lỗi nguồn. Test native tuyến S2→GS2→M3→C3 xác nhận đi thẳng từ S2, chờ grant M3, rẽ LEFT theo tọa độ map, bắt line mới sau mẫu 000 và dừng khi mất mạng. Reconnect tại GS2 không tự suy hướng từ node RFID; cần xác minh/tập kết trước tuyến mới.

Mã có thêm log `[MOTOR] xe=02 node=GS2 action=... left=... right=... line=... next=... zone=...` khi action đổi tại GS1/GS2, áp dụng cả hai bản bench. Không đổi PWM, chiều motor, mutex hoặc watchdog. Log `[ROUTE]`, `[WiFi] Disconnected reason=...`, `[MQTT] Connect failed...`, cùng thông báo boot/reset trên Serial 115200 giúp phân biệt nguyên nhân khi thử trên giá kê. Chỉ build ảnh `esp32dev_two_robot_bench_02`; chưa nạp bản chẩn đoán hoặc thử xe thật trong lượt này.

## Bản thử một xe trên toàn bản đồ 05/10/2026

`src/core/CoreNavigator.cpp` nối GOTO với bám line và quét từng RFID trên đường ngắn nhất do Core tính bằng Dijkstra. Bản thử xe 01 dừng **ít nhất 1 giây** tại mỗi thẻ trước khi đi tiếp hoặc quay (bản thường vẫn 200 ms); mỗi đoạn tối đa 15 s và mỗi lần quay tối đa 2,5 s theo giới hạn phần mềm chưa hiệu chuẩn. Mất line, mất mạng, quá hạn hoặc quét sai thẻ đều dừng và báo lỗi. Topic `agv/01/diagnostic` bổ sung `linePattern` (0–7, bit trái/giữa/phải) để đối chiếu lúc rẽ; đây không phải vị trí RFID. Bản thường còn dừng khi siêu âm lỗi hoặc gặp vật cản; bản trial tạm bỏ chốt này theo mục dưới.

Cập nhật 07/10/2026: trong trial, một mẫu `000` khi bám line làm motor dừng ngay; chỉ sau 120 ms vẫn `000` mới chốt `LINE_LOST`. Mẫu hợp lệ như `010` trở lại trước hạn thì tiếp tục. Serial Monitor ghi `[LINE] blank/recovered/fault` để xác định mẫu lúc xử lý; trạng thái lỗi trên web được giữ lại dù mắt đọc `010` sau khi xe đã dừng. Bản thường không dùng khoảng xác nhận này.

Firmware ước tính pin 2S 18650 từ ADC GPIO36 theo mạch chia áp 30 kΩ/7.5 kΩ (hệ số 5), với dải cấu hình 6.4–8.4 V. Trong giai đoạn thử hiển thị, phần trăm chỉ nằm trong JSON `agv/{id}/diagnostic`; trường pin của telemetry điều khiển luôn là `?`, nên chưa tác động ngưỡng 20%/40% của Core. Đây là nội suy tuyến tính theo điện áp, chưa được đối chiếu đồng hồ trên xe và không thay thế bảo vệ điện áp thấp của BMS. Bản `esp32dev` thường vẫn khóa PWM. Bản `esp32dev_trial`, cùng Core opt-in `AGV_ONE_ROBOT_TRIAL=1`, cho **duy nhất xe 01** đi tới các node có đường trên map qua lệnh GOTO trực tiếp từ web; không giao task tự động và không điều phối hai xe. [ADR-017](../../../docs/decisions/ADR-017-solo-full-map-trial.md) thay phạm vi trial một đoạn của [ADR-016](../../../docs/decisions/ADR-016-single-robot-server-motion-trial.md).

Cập nhật 15/10/2026 sau quan sát pin dao động 65–70%: `BatteryModule` lấy 16 mẫu ADC rải trong khoảng 0,5 giây, bỏ mẫu cao nhất/thấp nhất rồi chia trung bình 14 mẫu. Phần trăm chỉ đổi từng 1% cách nhau ít nhất 5 giây, sau 3 đợt liên tiếp cùng vượt biên 1,5 điểm (riêng sát 0%/100% dùng biên 0,5). Khi duty motor khác 0, giữ %; sau dừng 3 giây, lấy trọn đợt mẫu nghỉ và lọc riêng để tránh sụt áp lúc chạy kéo số hiển thị xuống. Lúc boot chờ khoảng 3,5–4 giây mới có %, chạy liên tục giữ số cũ tới khi nghỉ đủ lâu. Cho phép tăng/giảm chậm khi nghỉ vì chưa có tín hiệu xác nhận sạc. Không chèn `delay` vào vòng điều khiển. `batteryAdcMv` vẫn là trung bình đợt trước khi lọc; `batteryVoltage` vẫn cập nhật khi chạy để giữ chốt điện áp bench, có thể khác điện áp tương ứng với % đang giữ. Telemetry vẫn là `?`; từ REQ-BATTERY-06 ngày16/10/2026, Core dùng `batteryPercentage` calibrated trong diagnostic làm pin điều phối và giữ tương thích payload hiện tại. Vẫn cần so với đồng hồ tại chân chia áp và điện áp pack để kiểm tra sai số thực tế. [ADR-021](../../../docs/decisions/ADR-021-display-only-battery.md).

Đây là project nhúng do nhóm bổ sung, dùng PlatformIO với Arduino framework. `src/main.cpp` khởi tạo `AGVController`; các module trong `src/core`, `motion`, `sensors`, `actuators`, `network` được biên dịch cùng nhau. `AGV_Connection_Test` vẫn là sketch kiểm tra kết nối/bench riêng; không chạy đồng thời hai firmware cho cùng robot ID. Bản thường giữ xin/ACK mutex và chưa tự release. Chỉ bản trial một xe bỏ chờ grant khi qua M1–M6; xe thứ hai phải ở ngoài đường thử.

## Thử xe 01 chạy bằng server tới các node

Đây là bản chạy thử **một xe**, chưa xác nhận vận hành vật lý toàn kho. Cập nhật 07/10/2026: lần chạy thật cho thấy giả định lùi S1→GS1 đưa xe ra khỏi map. Đặt xe 01 ở S1 với **đầu xe hướng về GS1**; đoạn này nay được lệnh đi tiến. Kiểm tra trên giá kê trước: hai bánh cùng quay tiến, sau đó kiểm tra rẽ trái/phải, mẫu line `010/101/111` và nút ngắt nguồn. Đưa xe thứ hai khỏi đường thử. Quét thẻ S1 mới sau khi nạp; hướng đầu xe ở node khác sau reboot chưa được xác nhận và firmware sẽ từ chối tuyến mới.

Tạm thời theo yêu cầu 05/10/2026, bản `esp32dev_trial` **không khởi tạo/đo siêu âm và không dùng nó để cho phép/dừng chuyển động**. Chẩn đoán `ultrasonicCm` sẽ là `null`; giá trị này không khóa lệnh. Bản `esp32dev` thường vẫn chặn khi siêu âm lỗi hoặc gặp vật cản. Chỉ tháo/cắm dây cảm biến khi đã ngắt nguồn; khi thử bản trial, giữ đường trống, nhấc bánh lên giá kê trước và có người trực tiếp ngắt nguồn motor. Muốn dùng cảm biến trở lại phải đổi `Config::ULTRASONIC_MOTION_GUARD` cho bản trial, đo được khoảng cách hợp lệ, kiểm tra ngưỡng dừng 15 cm trên bàn rồi nạp lại. Không dùng bản này để chạy không giám sát.

Mở PowerShell mới từ gốc workspace để chạy đúng **một Core** với công tắc thử (dừng Core cũ trước; giữ `data/command_sequence.json`):

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject'
$env:AGV_ONE_ROBOT_TRIAL = '1'
$env:AGV_HOME_STATIONS = '{}'
./scripts/run-core.ps1 -Build
```

Giữ gateway `AGVwebserver` chạy riêng. Từ thư mục firmware, kiểm tra cổng ESP32 rồi build/nạp **env trial** (thay `COMx` bằng cổng thật):

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject\firmware\NCKHAGV\NCKHAGV'
$pio = "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe"
& $pio run -e esp32dev_trial
& $pio device list
& $pio run -e esp32dev_trial -t upload --upload-port COMx
& $pio device monitor -p COMx -b 115200
```

Đợi web hiện xe `01` tại S1, `online=true`, `position_valid=true`, Core/MQTT kết nối. Chọn **xe 01**, giao đích **GS1** trước; sau khi telemetry GS1 xác nhận hoàn thành, thử M2 rồi tăng dần đến các node khác. Có thể giao một node xa; Core gửi cả đường Dijkstra và firmware dừng/quét từng RFID. Xe 01 mới dùng PWM trial danh định 210/220 khi đi thẳng và 200/200 khi pivot; `LEFT` cấp trái tiến/phải lùi, `RIGHT` cấp trái lùi/phải tiến. Sau khi nạp, xem `[MOTOR]` trên Serial Monitor và xác nhận cả hai bên quay đúng trên giá kê. Chế độ này từ chối lệnh mới khi xe đang chạy, xe 02 xuất hiện hoặc còn owner mutex cũ. Nếu bánh quay sai, lệch line hoặc cảm biến bất thường, ngắt nguồn motor và kiểm tra tại chỗ. ACK chỉ báo nhận lệnh; trạng thái hoàn thành cần RFID đích.

## Build và nạp từ PowerShell

Mở thư mục project chứa `platformio.ini`, rồi gọi trực tiếp PlatformIO đã cài trên máy này (lệnh `platformio` chưa có trong `PATH`):

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject\firmware\NCKHAGV\NCKHAGV'
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev
```

File tạo ra là `.pio/build/esp32dev/firmware.bin`. Đây là ảnh ứng dụng; nếu dùng công cụ nạp `.bin` thủ công còn cần bootloader và bảng phân vùng đúng offset, vì vậy cách nạp dễ nhất là nối USB với ESP32, chọn cổng COM và để PlatformIO nạp đủ các phần:

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" device list
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev -t upload --upload-port COMx
```

Thay `COMx` bằng cổng của ESP32. Lệnh `esp32dev` ở mục này build/nạp **bản thường khóa motor**; muốn thử chạy phải dùng `esp32dev_trial` như mục trên. Điền cấu hình mạng riêng trước khi build/nạp, và dùng ID xe `01` cho trial.

## Kết nối Core

Sao chép `src/LocalNetworkConfig.example.h` thành `src/LocalNetworkConfig.h` rồi điền ID xe `01` hoặc `02`, WiFi và địa chỉ broker LAN. File cục bộ được ignore để tránh đưa mật khẩu vào mã chia sẻ. Cấu hình mới không có file này sẽ giữ xe offline. ESP32 bật tự kết nối lại sau khi AP rớt và còn thử kết nối định kỳ mỗi 5 giây; Serial Monitor in `[WiFi] Disconnected reason=...` và `[WiFi] Got IP` để phân biệt lỗi WiFi với MQTT. Với broker TLS, đặt port của listener, `AGV_MQTT_TLS true`, tài khoản/mật khẩu riêng từng xe và CA PEM; tên/IP broker phải khớp SAN của chứng chỉ. Firmware không dùng `setInsecure()` và chờ NTP trước khi xác minh chứng chỉ; LAN không có Internet cần NTP nội bộ qua `AGV_NTP_SERVER`. Broker/Core phải được cấu hình tương ứng theo [hướng dẫn gia cố](../../../docs/18-hardening.md).

### Xe 02 chỉ kết nối và giám sát

Nếu xe 02 dùng đúng ESP32 DevKit và sơ đồ chân ở `src/Config.h`, build riêng env `esp32dev_robot2_monitor`. Env này lấy WiFi/broker từ `LocalNetworkConfig.h`, đặt ID MQTT thành `02` dù file cục bộ đang dùng ID `01`, và giữ `ENABLE_CORE_MOTION=false`. Không nạp env `esp32dev_trial` cho xe 02. Xác định đúng cổng COM của xe 02 trước khi nạp:

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" device list
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev_robot2_monitor
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev_robot2_monitor -t upload --upload-port COMx
```

Khi Core và broker đang chạy, xe 01/02 phải có MQTT client ID khác nhau (`agv-01-<MAC>` và `agv-02-<MAC>`), cùng xuất hiện trên web sau telemetry. Quét thẻ S2 thật ở xe 02 để xác minh vị trí; trước đó node là `UNKNOWN`. Nếu Core vẫn bật `AGV_ONE_ROBOT_TRIAL=1`, telemetry xe 02 sẽ làm gián đoạn tuyến xe 01 đang chạy và chặn GOTO thử mới. Dừng xe 01 an toàn trước khi kết nối xe 02; chế độ này chỉ dùng để kiểm tra kết nối/giám sát, chưa cho hai xe di chuyển đồng thời. Nếu bo hoặc dây xe 02 khác xe 01, đối chiếu phần cứng rồi sửa cấu hình riêng trước khi nạp.

Core chế độ thường có thể tự gửi `CHARGE_START` cho xe 02 khi kết thúc ca tại S2. Để giữ xe 02 ở vai trò giám sát, bản Core mới hỗ trợ biến môi trường `AGV_MONITOR_ONLY_02=1`: không chọn xe 02 cho nhiệm vụ, từ chối GOTO trực tiếp, không tự đưa xe 02 về sạc. Chỉ áp dụng khi khởi động **một** Core mới sau khi dừng Core cũ và giữ nguyên map/runtime/counter đang dùng; không xóa dữ liệu hay mở hai Core cùng lúc. Firmware xe 02 vẫn khóa PWM độc lập với chế độ Core.

Sau khi đối soát trạng thái hai xe và dừng Core cũ, chạy `./scripts/run-robot2-monitor.ps1 -Build` từ gốc workspace. Script mặc định dùng `src/data` đang chứa map, runtime, shift và counter của Core vận hành; thay `-DataRoot` chỉ khi đã xác định một bộ dữ liệu khác cần tiếp tục.

MQTT dùng `agv/{id}/command` JSON từ Core, `agv/{id}/ack` là cmd_id dạng text, `agv/{id}/telemetry` là `node|battery|obstacle`, `agv/{id}/error` là text, `agv/{id}/status` online/LWT offline retained. Firmware theo dõi `agv/server/heartbeat`; chỉ gửi telemetry khi heartbeat mới và khóa điều khiển khi quá 3 giây, mất WiFi/MQTT, worker mạng ngừng hoặc phiên Core đổi. `cmd_id` cao nhất lưu vào NVS namespace `agv-rfid`, key `last-cmd`, tương thích sketch thử nghiệm; lệnh trùng gần nhất với cùng nội dung chỉ ACK lại, không thực hiện lại. ACK gửi lỗi được thử lại sau reconnect; ACK chỉ xác nhận đã lưu/nhận lệnh, không xác nhận đã tới đích. Không xóa NVS để “sửa” lỗi thứ tự lệnh; giữ file counter của Core.

Diagnostic NCKHAGV mới có `rfidAgeMs`: thời gian từ lần RC522 đọc được thẻ đang nằm dưới đầu đọc trong phiên MQTT; chưa có lần đọc hợp lệ thì `null`. Core thử hai xe dùng tuổi ≤250 ms cùng motor `STOP`, `leftDuty=rightDuty=0`, `taskActive=false` và RFID trạm S1/S2 để đối soát quyền vùng/lỗi mạng theo [ADR-026](../../../docs/decisions/ADR-026-safe-automatic-recovery-at-home.md). Telemetry node cũ, ACK hoặc chỉ MQTT online không kích hoạt phục hồi. Cần build/nạp firmware mới cho cả hai xe và thử trên giá kê trước khi dùng tự phục hồi; bản cũ vẫn cần xác minh thủ công.

Trong bản `esp32dev_trial`, mẫu line `110`/`011` (mắt giữa và một mắt bên cùng thấy vạch) dùng chỉnh nhẹ: cả hai bánh vẫn chạy tiến với duty danh định tương ứng 210/140 hoặc 130/220. Mẫu lệch hẳn `100`/`001` giữ chỉnh mạnh một bánh; `010`, `101`, `111` đi thẳng; `000` dừng ngay và chỉ chốt lỗi sau 120 ms không bắt lại line. Chỉnh khi lùi và bản `esp32dev` thường chưa đổi. Đây là thông số thử phần mềm, chưa được xác minh hướng chỉnh trên xe thật. Kê bánh, đặt lần lượt ba mắt ở trạng thái `110`, `010`, `011`, xem `GENTLE_CORRECT_LEFT`/`FORWARD`/`GENTLE_CORRECT_RIGHT` cùng duty trên Serial/diagnostic; xác nhận bánh tạo hướng kéo xe về vạch trước khi thử một đoạn thẳng ngắn trên mặt đất.

26 UID RC522 đã đồng bộ với bản đồ v3 và sketch thử nghiệm. Khi chưa quét RFID trong phiên kết nối hiện tại, node là `UNKNOWN`; telemetry/hàng đợi của phiên cũ bị loại, không suy ra vị trí từ đích lệnh. Firmware ước tính phần trăm pin từ điện áp ADC theo cấu hình ở trên. Từ REQ-BATTERY-06, Core lấy `batteryPercentage` calibrated trong diagnostic để cập nhật cả hiển thị lẫn `battery`/`battery_measured` điều phối; snapshot ghi nguồn `battery_control_source=diagnostic`. Khi cấu hình hoặc phép đo không hợp lệ, dashboard hiện “Chưa đo” và mode thường khóa giao việc. Telemetry pipe vẫn dùng `?` để tương thích; cần đối chiếu ADC/điện áp thực bằng đồng hồ khi bảo trì.

## Hướng rẽ theo bản đồ và khóa motor

Theo bổ sung của người dùng ngày 04/10/2026, chiều cạnh và tọa độ RFID đã nằm trong bản đồ. `src/core/RoutePlanner.h` dùng `path` của lệnh `GOTO` và đồ thị một chiều để kiểm tra từng cạnh; từ ba node liên tiếp nó phân loại trái/phải/thẳng/lùi. Các nhánh như M1 có thể đi GS1 hoặc G2; **node kế trong path** quyết định nhánh. Điều chỉnh theo quan sát 07/10/2026: xe 01 đi tiến S1→GS1 khi đầu hướng về GS1; các đoạn sau vẫn có thể cần lùi hoặc quay theo path. Hướng trái/phải sau khi lùi được xét theo đầu xe chứ không theo véc-tơ di chuyển lùi. Khi lùi, 010/101/111 giữ thẳng, mắt lệch thì giảm tốc một bên bánh, 000 dừng an toàn; hướng chỉnh thực tế còn phải kiểm tra trên giá kê. Tại RFID đầu tiên khác S1 sau boot/reconnect, nếu chưa thấy RFID trước đó trong cùng phiên, hướng đầu xe là `UNKNOWN_HEADING`; không suy đoán từ mũi tên trên bản đồ. Những tuyến dùng nhánh kho/sạc có thể phải lùi và vẫn cần kiểm chứng thao tác vật lý.

`src/core/RouteMap.generated.h` được sinh từ `data/map_layout.json` bằng `node scripts/generate-firmware-route.js` tại gốc workspace; `--check` phát hiện bản sinh lệch bản đồ. Khi sửa bản đồ vận hành, cần sinh và nạp firmware tương ứng. Core v0 chưa gửi phiên bản map trong `GOTO`, nên không thể xác nhận bản đồ đang chạy có cùng phiên bản chỉ bằng payload lệnh. Không sửa/apply snapshot map trong lúc thử xe nếu chưa đồng bộ lại firmware.

Trong env `esp32dev` thường, `GOTO` được ACK ở mức nhận lệnh, kiểm tra cạnh/hướng, rồi **vẫn dừng** với `MOTION_LOCKED_PENDING_ZONE_CLEARANCE_CALIBRATION`. Cạnh không hợp lệ bị báo `INVALID_COMMAND`; chưa quét RFID trong phiên hiện tại bị báo `NOT_AT_ROUTE_NODE`. `ENABLE_CORE_MOTION=false` và `MotorModule::drive()` chặn PWM. `STOP`/`WAIT` giữ dừng; `CHARGE_START` báo `CHARGER_NOT_CONFIGURED`. Bản thường đã xin/ACK grant vùng M trước khi chạy nhưng chưa tự trả quyền mutex; bản trial một xe bỏ chờ grant khi qua M1–M6 và tuyệt đối không dùng cùng xe thứ hai. Servo chưa được Core điều khiển qua các lệnh này. Ước lượng xe dài 18 cm, rộng 12 cm; đầu đọc nằm giữa ngang xe và cách đuôi khoảng 12 cm. ESP32 DevKit V1 38 chân, L298N và dây theo Config đã được người dùng xác nhận. Line đen đọc 1; `010`, `101`, `111` là nhóm bám thẳng. Sketch độc lập trên xe cũ xác nhận polarity W tiến/S lùi; xe 01 mới cần xác nhận lại trên giá kê. Sau thử GS1, pivot trái trial cấp trái tiến/phải lùi ở 200/200; pivot phải trái lùi/phải tiến. Chỉnh line khi tiến dùng bánh trái để chỉnh trái, bánh phải để chỉnh phải. Khi tới GS1, Serial Monitor in `[MOTOR]` với action, duty và line pattern để phân biệt pivot với chỉnh line một bên. Duty danh định trial đi thẳng là 220/220, với `LEFT_SPEED_TRIM=-30` nên duty trái giảm 30; chế độ thường dùng trim `-40`. Trim mới chưa được đo lại trên xe. Khi quay, firmware chờ mắt rời vạch cũ rồi bắt lại một mẫu bám thẳng; thời hạn quay thử đang giới hạn 2,5 giây và chưa được đo trên mặt đất. Xe không có encoder. Chưa xác nhận biên vùng M1–M6, bám line khi lùi, điện áp pin/B25, thời gian quay hoặc bám line thực. Không tự trả mutex trong bản thường sau khi chỉ đọc một thẻ.

## Chuẩn bị thử một xe với ngưỡng tạm 28 cm

Cập nhật 09/10/2026 cho hướng thử hai xe: xe 02 tại S2 hướng đầu về GS2, nên firmware nhận S2→GS2 là đoạn đi thẳng đầu tiên khi chưa có RFID trước đó. Người dùng đo đầu đọc đến đuôi xe 10 cm (thay ước lượng 12 cm ở đoạn lịch sử phía trên), tâm thẻ M đến mép cuối vùng 10 cm, tâm thẻ kế tiếp ngắn nhất 35 cm, RC522 đọc sớm 8 cm. Tại **lần đọc sớm** thẻ 35 cm, khoảng hở dọc đường danh nghĩa chỉ là 7 cm. Người dùng xác nhận các đoạn 35/50/100 cm trên map là số đo thật và bốn chuỗi 35+35 cm qua G2/G5/G3/G4 đi thẳng. Firmware trong workspace chỉ queue `mutex_release` JSON đúng grant ID khi đọc thẻ dự kiến cách M ≥50 cm hoặc thẻ thứ hai của đúng chuỗi thẳng 35+35 cm. Tuyến khác giữ mutex. Firmware hiện vẫn khóa motor trong `esp32dev` và `esp32dev_robot2_monitor`; binary mới chưa nạp lên xe. Xe 02 chưa có siêu âm; không áp dụng kiểm thử tuyến thật khi chưa có cách dừng vật cản đã kiểm chứng. [ADR-025](../../../docs/decisions/ADR-025-two-robot-supervised-trial.md).

### Bản bench hai xe trên giá kê

Hai env `esp32dev_two_robot_bench_01` và `esp32dev_two_robot_bench_02` mở PWM riêng cho đúng ID 01/02, dùng Core `AGV_TWO_ROBOT_TRIAL=1`, giữ xin/ACK/release mutex. Cập nhật 09/10/2026: hai env bench 01/02 cùng đặt tốc độ cơ sở 230/255; trim trái −40 nên lệnh thẳng/lùi có trái 190, phải 230. Ảnh motor probe riêng của xe 01 vẫn cố định 255/255 và giới hạn 2 giây. Không có ngoại lệ bỏ grant như bản một xe. Bản bench tạm bỏ chốt siêu âm và chốt pin điều khiển chưa hiệu chuẩn, nhưng chỉ cho PWM khi ADC ước lượng pack trong khoảng 6,4–8,6 V; điện áp ra ngoài khoảng này khóa lệnh và dừng khi đang chạy. Đây chỉ là chốt phần mềm, chưa thay phép đo trực tiếp bằng đồng hồ. Chỉ dùng khi **cả hai xe trên giá kê và có thể ngắt nguồn motor ngay**. Vẫn dừng khi mất line, thẻ RFID sai, mất mạng/heartbeat hoặc quá 15 s/đoạn; chỉ mở thử lùi ra kho; lùi khác chưa được hiệu chuẩn. Servo lấy/trả hàng vật lý qua Core chưa được nghiệm thu. Bản `esp32dev`/`esp32dev_robot2_monitor` vẫn khóa PWM.

Nếu `SEGMENT_TIMEOUT` làm ESP32 báo `AGV_SAFE_STOP`, Core/web xóa lỗi phải gửi thêm `RESET_FAULT`; chỉ `WAIT` không mở được chốt firmware. Cần nạp firmware hỗ trợ lệnh này cho cả hai xe và dùng Core mới đồng bộ. Xe phải online, vừa xác minh RFID đúng vị trí, line hợp lệ, không có tuyến đang chạy và chốt điện áp đạt yêu cầu. Sau thao tác xóa lỗi, xem `agv/{id}/diagnostic`: `state=AGV_IDLE`, `error=NONE`, `motor=STOP` trước khi thử lệnh mới. ACK reset chỉ là nhận lệnh. Còi lùi chỉ chứng minh firmware đã đặt duty khác 0; nếu bánh vẫn đứng, đọc duty lúc lệnh đang hoạt động và kiểm tra nguồn motor/ENA/ENB/OUT của L298N trước khi tăng PWM tiếp.

### Cấu hình và nạp riêng xe 01, xe 02

Cập nhật 10/10/2026: quét RFID mỗi tối thiểu 20 ms như sketch test đầu, dùng một WUPA và xử lý node/dừng trước HALT/log để giảm trễ khi đi qua GS2. Module đọc chung áp dụng cả hai profile; giữ UID và PWM đã chọn. Khoảng poll không bao gồm cam kết thời gian SPI/timeout. Host adapter PASS; chưa đo quét/dừng thật sau thay đổi. [ADR-031](../../../docs/decisions/ADR-031-rfid-read-latency.md).

Firmware mới dùng quy ước **kho K: đầu hướng vào để xử lý hàng, tiến vào/lùi ra; trạm S: đầu hướng ra cửa, tiến xuất phát** sau khi đọc RFID thật. S1 ↓ S ra GS1, S2 ↑ N ra GS2, K1–K3 ← W, K4–K6 → E theo map hiện tại. Web chỉ hiển thị mũi tên, không còn bộ chọn hướng tay. Khi về trạm, xe tiến vào S, dừng tại RFID rồi quay đầu tại trạm để hướng ra GS; Core chờ diagnostic sẵn sàng trước khi ghi hoàn thành. Khi lùi, mũi tên giữ hướng đầu xe. Rời kho thì tới cửa xe rẽ/quay đầu nếu cần rồi đi tiến. Quay đầu đếm các lần rời/bắt line theo nhánh vật lý trên map và dừng khi quá timeout; cần kiểm tra line thật khớp map. Chỉ trial mở motor/lùi xuất kho; bản thường vẫn khóa. Chưa đo chiều lùi/correction hoặc góc quay trên xe mới, cần kê bánh kiểm tra trước chạy tuyến. [ADR-033](../../../docs/decisions/ADR-033-stations-face-outward.md).

Chỉ nạp **firmware `.bin` do PlatformIO build**, không nạp trực tiếp `main.cpp`, `Config.h` hay file profile. Hai env bench dùng cùng mã điều khiển và chân GPIO nhưng chọn file thông số riêng:

| Xe | File chỉnh tốc độ/trim/timeout/pin | Env PlatformIO | Ảnh ứng dụng sau build |
| --- | --- | --- | --- |
| 01 | [`src/profiles/Robot01Config.h`](src/profiles/Robot01Config.h) | `esp32dev_two_robot_bench_01` | `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` |
| 02 | [`src/profiles/Robot02Config.h`](src/profiles/Robot02Config.h) | `esp32dev_two_robot_bench_02` | `.pio/build/esp32dev_two_robot_bench_02/firmware.bin` |

Mỗi profile khóa MQTT ID đúng xe. Cập nhật 10/10/2026 theo yêu cầu tăng vừa PWM xe 01: profile 01 tăng cơ sở/quay/chỉnh line từ 230 lên 245; trim trái −40/phải 0 nên đi thẳng là trái 205/phải 245, pivot là 245/245, chỉnh line một bánh là 205/0 hoặc 0/245. Profile 02 giữ cơ sở/quay/chỉnh line 230. Đây là mức thử, chưa xác nhận đủ lực kéo trên sàn; cần nạp đúng env 01 rồi kiểm tra bánh/chiều quay trên giá kê. Bảng chân và UID RFID chung ở `src/Config.h`. Muốn dùng Wi‑Fi/broker hoặc tài khoản MQTT khác nhau, sao chép `src/LocalNetworkConfig01.example.h` thành `src/LocalNetworkConfig01.h`, và `02.example.h` thành `02.h`, rồi điền riêng. Hai file thật bị ignore; nếu chưa tạo, env bench vẫn đọc `src/LocalNetworkConfig.h` cũ. Không đặt mật khẩu trong file profile.

Từ thư mục chứa `platformio.ini`, chọn đúng cổng USB của từng xe rồi build/nạp **từng env**:

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject\firmware\NCKHAGV\NCKHAGV'
$pio = "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe"
& $pio device list
& $pio run -e esp32dev_two_robot_bench_01 -t upload --upload-port COM_XE_01
& $pio run -e esp32dev_two_robot_bench_02 -t upload --upload-port COM_XE_02
```

Thay `COM_XE_01`/`COM_XE_02` bằng cổng COM thực tế; nếu dùng chung một dây USB, chuyển dây sang đúng xe trước lệnh thứ hai. PlatformIO nạp cả bootloader, partition và ứng dụng ở offset phù hợp. Không dùng riêng `firmware.bin` với công cụ nạp thủ công khi chưa có bảng offset của bo. Sau mỗi lần nạp, Serial 115200 và topic `agv/01/...` hoặc `agv/02/...` phải khớp xe; quét lại S1/S2 để xác nhận vị trí trong phiên MQTT mới. Chỉ thử chuyển động khi hai xe trên giá kê, có thể ngắt nguồn motor ngay và đã xác minh hướng quay, line cùng điện áp của từng xe. [ADR-027](../../../docs/decisions/ADR-027-per-robot-firmware-profiles.md).

Theo số đo 17/10/2026, cả hai xe dùng encoder161xung/100cm và cùng kích thước: đầu→RFID7,5cm, RFID→đuôi9,5cm; vùng đọc±3cm quanh tâm và tâm thẻ→biên ra mutex3cm. Profile bench01/02 bắt đầu đếm ở lần đầu đọc đúng M1–M6, chỉ cộng xung khi đang tịnh tiến. Khi **cả hai bánh đủ30 xung**, không đọc lại thẻ ít nhất100ms và route/phiên/vị trí còn khỏe, firmware queue `mutex_release`. Serial hiện `proof=encoder`; nếu xe STOP/quay/tìm line/lỗi/mất mạng thì giữ mutex. RFID rời vùng đã đo vẫn là fallback. Xem [ADR-068](../../../docs/decisions/ADR-068-encoder-mutex-tail-clearance.md).

Để lấy số đo còn thiếu, nạp project rồi mở Serial Monitor115200, thử từng xe/từng mutex có người giám sát và đánh dấu biên ra thật. Đối chiếu dòng `[MUTEX_RELEASE] zone=M... proof=encoder left=... right=... threshold=30` với vị trí đuôi xe. Nếu đuôi chưa qua hẳn biên thì dừng thử và tăng ngưỡng; không giảm chỉ để xe sau chạy sớm. Đưa vật cản ở khoảng cách đã đo để đối chiếu ngưỡng dừng15cm; đặt mắt line trên/vắng vạch để xác nhận mức active. Bề rộng vạch được báo4,5cm, đường kính bánh6,5cm, đầu mắt cách trục bánh trước5cm và bánh sau10cm. Phần trăm pin hiện được nội suy từ dải điện áp cấu hình, nhưng ADC và cầu chia vẫn cần đối chiếu bằng đồng hồ trên xe.

Có thể thử **kết nối và nhận lệnh** với riêng xe `01` trước khi cho motor chạy: dùng ID MQTT `01`, quét RFID S1 thật và kiểm tra heartbeat, ACK, log `[ROUTE]`, telemetry. ACK và log route **không chứng minh xe đã chạy**. Để thử phần trăm pin, kiểm tra `batteryVoltage`, `batteryPercentage`, `batteryCalibrated` trên diagnostic và số hiển thị trên dashboard; telemetry điều khiển vẫn là `?` nên chưa áp dụng ngưỡng 20%/40%. Chế độ trial vẫn giới hạn ở GOTO trực tiếp xe 01. Để vận hành hai xe về sau vẫn cần xác minh pin trên cả hai xe, cơ chế grant/ACK/release mutex và kiểm tra dừng khi mất server.

`platformio run -e esp32dev` build firmware với phiên bản thư viện trong `platformio.ini`; `platformio test -e native` chạy các test logic, gồm `test/test_core_protocol`. Kiểm thử parser độc lập ở `tests/nckhagv_protocol.cpp` dùng g++ và ArduinoJson. `CommandCodec` cũ còn trong source để giữ test logic legacy nhưng không còn được MQTTModule sử dụng. Các bài này không thay thế upload và thử trên xe thật. Xem [giao thức MQTT](../../../docs/07-network.md) và [ADR-014](../../../docs/decisions/ADR-014-nckhagv-core-sync.md).

### Rẽ theo lệnh server tại RFID — 10/10/2026

Theo yêu cầu 12/10/2026, profile bench xe 01 dừng motor ngay khi mắt đọc `000` trong pha bám line nhưng chờ tối đa 5 giây để quét RFID hoặc thấy lại line rồi mới chốt `LINE_LOST`. Xe không chạy mù trong thời gian này; thẻ nằm ngoài tầm tại điểm dừng vẫn cần chỉnh vị trí thẻ/vạch. Xe 02 không đổi. Đây là thay đổi source chưa nạp hoặc kiểm chứng trên xe thật; xem [ADR-046](../../../docs/decisions/ADR-046-robot01-line-loss-rfid-grace.md).

Bản xe 01 mới dùng luồng: **quét RFID → dừng → server gửi NODE_ORDER LEFT/RIGHT → hai bánh ngược chiều xoay tại chỗ → bắt lại line → đi tiếp**. GOTO chỉ mang tuyến; tại đầu tuyến và từng node trung gian xe chờ lệnh riêng khớp cmd_id GOTO/node/chặng, không tự chạy sau dwell. Lệnh vẫn chờ grant mutex, giữ PWM quay 245 và poll RFID 20 ms. Khi đang quay, 000 là rời line cũ; chỉ sau bắt lại nhánh centered mới chuyển sang bám thẳng. Quá 2,5 s chưa bắt lại thì STOP/TURN_TIMEOUT. Không tăng tốc hay tự bỏ lỗi line để chạy mù.

Core mới phải chạy trước khi nạp ảnh này; Core cũ không gửi NODE_ORDER nên xe giữ STOP và sau đó timeout. Debug Serial có `[NODE_ORDER] server: LEFT GS1 -> M2 route=... step=1` hoặc RIGHT tại điểm rẽ; diagnostic `awaitingNodeOrder=true` cho biết xe đang dừng chờ. Lệnh cũ/sai node/chặng bị từ chối, retry cùng ID không quay lại. S1/S2 vẫn hướng ra GS khi đỗ; server gửi STATION_TURN_RIGHT sau RFID trạm và chờ ready trước hoàn thành. Chưa kiểm chứng góc quay hoặc hết mất line trên xe thật. Xem [ADR-034](../../../docs/decisions/ADR-034-server-node-orders-for-pivot-turns.md).
Hồi quy quét rồi chờ bấm lệnh: NODE_ORDER dùng node/chặng đã xác nhận trong cùng phiên và diagnostic mới STOP, không bắt tuổi thẻ ≤250 ms khi xe đang dừng chờ. Đã có test RFID tuổi 5 giây vẫn nhận STRAIGHT; chưa đọc trong phiên (null) hoặc sai route vẫn bị từ chối. Phục hồi lỗi/mutex và stationReady vẫn cần RFID mới ≤250 ms. RFID adapter đọc cùng thẻ 50 lần mà không tiến tuyến lặp; 6 diagnostic trên bo tại S1 cũng cho tuổi đọc trở lại thấp, chứng minh đọc lặp. Đèn trên ESP32 không được dùng làm bằng chứng số lần đọc.

Cập nhật crash/checkpoint10/10/2026: firmware01 buildId=20261010-grant-buffer-fix copy zone/token vào grant riêng trước publish mutex_ack, tránh buffer PubSubClient RX/TX ghi đè pointer JSON. Diagnostic firmwareBuild cho biết đúng ảnh nạp. Core lỗi checkpoint dừng outbound, thử save lại mỗi2s; chỉ sau lưu thành công phục hồi heartbeat. Gateway storage_healthy=false báo core_ready=false, command503 và chặn điều phối đơn. Payload mutex/command/telemetry pipe giữ nguyên. Xem ADR-035.
