# ADR-063 — Xe02 dừng và xác nhận line khi xoay trái

22/10/2026, REQ-DUAL-EDGE-LINE-CONFIRM-01: TurnCapture cảhai profile nhận thêm001/100 sau khi một mẫu trước đã chứng minh rời line cũ; mẫu mép không được làm cảhai việc trong cùng vòng. Sau đó cùng phía ổn định100ms/≥3 mẫu/gap≤50ms mới thoát capture. GiữSTOP khi xác nhận, quét≤600ms/chờ≤1500ms, capPWM/grant/mạng/sonar và fault latched. [ADR-076](ADR-076-dual-narrow-line-confirm.md), [kiểm thử](../../tests/README.md). Chưa nạp/thử xe thật.

21/10/2026, REQ-DUAL-NARROW-LINE-CONFIRM-01: cảhai profileLEFT/RIGHTcapture thêm xác nhận010/011/110 liên tục100ms/≥3 mẫu mới/gap≤50ms, không cần mẫu cuối010 trong nhánh mới; giữ đường2/3phiếu010 cũ. STOP trong xác nhận narrow, tổngchờ1500ms nhưng quétmotor≤600ms, chờcòn lạiSTOP; không nới chốt rời vạch cũ/quyềnzone/mạng/sonar/capPWM. Thay deadline600ms của capture và quy tắc chỉ010cuối trong các mục lịch sử dưới. [ADR-076](ADR-076-dual-narrow-line-confirm.md), [kiểm thử](../../tests/README.md). Chưa nạp/thử xe thật.

Ngày: 16/10/2026. Trạng thái: IMPLEMENTED; đã kiểm chứng phần mềm, nghiệm thu trên xe OPEN.

## Cập nhật mới nhất — REQ-ROBOT02-LINE-ALIGN-01

Build env02 PASS: RAM57940byte/flash962613byte. Binary có buildID `20261016-robot02-line-align`; chưa upload.

SOURCE: người dùng xác nhận011/110 là có line nhưng đang căn và đồng ý thay điều kiện010 liên tục100ms. Chỉ đổi LEFT xe02, không nới điều kiện phục hồi mất line tiến/lùi trong ADR-062.

IMPLEMENTED: tách `alignTurnLine` khỏi `recoverLine`. Giữ STOP sớm khi mắt giữa trở lại sau rời line cũ. Phiếu có line gồm010/011/110, lấy tối đa3 mẫu cách ít nhất10ms; dữ liệu gián đoạn>40ms hết hiệu lực. Có≥2 phiếu và **mẫu mới nhất010** mới kết thúc căn; do đó không đòi010 liên tục100ms, cũng không nhảy sang tiến chỉ từ011/110. 011/001 chọn PivotRight,110/100 chọn PivotLeft theo quy ước cảm biến hiện có, xung215/40ms-nghỉ60ms; đổi hướng phải STOP trước.111/101 giữ STOP, không coi là đúng nhánh.000 giữ STOP trước100ms, sau đó chỉ quét theo hướng căn gần nhất (mặc định phải để tìm lại khi trôi qua vạch), không tiến mù. Tổng600ms không đổi; hết hạnLINE_LOST. Các thời gian/PWM là mức bench chưa hiệu chuẩn góc thực.

Kết thúc căn vẫn STOP một vòng, giữ route/index/grant, sonar sau phải lấy mẫu mới trước lùi. Trong200ms đầu pha Following, `settlingTurnLine` giới hạn duty thực mỗi bánh≤215, kể cả trim/chỉnh line; không tăng giới hạn PWM hiện tại của từng bánh. Sau đó về tuning chạy cũ. STOP/mạng/sonar/vật cản/grant vẫn ưu tiên. Encoder không cộng xung căn pivot, chỉ bắt đầu cộng khi thực sự bám tiến/lùi. Không đổi MQTT/schema/ACK hoặc hoàn thành task từ bắt line. BuildID02 `20261016-robot02-line-align`.

VERIFIED native bốn env166/166 PASS (02=43,01=42,trial=41,two-robot=40); hai nhóm mới chỉ chạy logic khi feature được bật. Host GPIO PASS đổi hướng theo011/110, không cộng pivot odometry, STOP khi000, cap215 khi vừa chạy lại, khôi phục PWM thường sau200ms, lùi/còi đúng; motor01/02 hồi quy PASS. Chưa upload/thử sàn. Các phần tiếp theo là lịch sử trước quyết định này.

## Cập nhật tuning mới nhất — REQ-ROBOT02-PIVOT-TORQUE-01

VERIFIED: native xe02 41/41 PASS, build env02 PASS (RAM57940byte/flash962077byte), chưa upload. Host GPIO/motor được mô tả dưới đây.

SOURCE: người dùng báo xe02 xoay trái không nổi và yêu cầu tăng tốc độ xoay. OBSERVED: pivot thường215 và sau rời line hạ180 theo cấu hình cũ; chưa đo lực motor/pin trên xe thật.

IMPLEMENTED: tăng pivot thường lên230 (trái/phải); riêng pha tiếp cận LEFT sau rời line và chỉnh ngược ngắn trong TurnCapture lên215 bằng `AGV_LEFT_TURN_CAPTURE_PWM`. Navigator chọn mức riêng truyền tới MovementModule; tìm line tiến/lùi vẫn180. Giữ nguyên STOP sớm trước RFID khi mắt giữa thấy lại line, xác nhận100ms/≥3 mẫu, xung40ms-nghỉ60ms, giới hạn600/2500ms và mọi chốt an toàn. Không đổi tiến/lùi/chỉnh line hiện có hoặc profile01. BuildID02 `20261016-robot02-turn230-capture215`.

Host GPIO kiểm230→215, chỉnh ngược−215/+215 và STOP, route/odometry PASS; hồi quy motor01/02 PASS, gồm phục hồi180 không đổi. Chưa nạp/đo xe thật; cần thử bàn và đối chiếu quán tính trước chạy task. Các giá trị và kết quả build bên dưới mô tả bản LEFT-CAPTURE trước cập nhật này.

## SOURCE và OBSERVED

SOURCE: người dùng xác nhận lỗi tại G1 khi xoay trái: mắt đã thấy line nhưng xe vẫn xoay tiếp sang trái và lệch line.

OBSERVED từ lịch sử gateway ngày16/10: task `task-57391` đíchK1, RFID C3 lúc17:19:15 → G1 lúc17:19:19 → M5 lúc17:19:25, sau đó `UNEXPECTED_NODE` lúc17:19:26 (giờ Việt Nam). Theo map, C3→G1→K1 là LEFT90°. Lịch sử xác nhận node nhận được khác nhánh cần đi; chưa có Serial lúc xoay để kết luận mẫu cảm biến/góc thực hoặc chiều motor tại thời điểm đó.

OBSERVED từ mã cũ: pha Turning đòi mẫu `000` trước khi nhận `010/101/111`, bỏ qua centered trước `TURN_MIN_MS=100`, sau bắt line chuyển sang tiến ngay trong cùng vòng. Retry ADR-061 kéo dài pivot cùng hướng thêm2500ms. Các điều kiện này có thể làm bỏ qua vạch ngắn hoặc bắt vạch tiếp theo; không dùng kết quả native để khẳng định nguyên nhân vật lý đã được chứng minh.

## Quyết định

Chỉ bật `AGV_LEFT_TURN_CAPTURE_ENABLED` trong profile02 và cho maneuver LEFT. Xe01, RIGHT, TURN_AROUND và xoay đỗ trạm giữ hành vi cũ.

1. Mẫu `000/001/100` chứng minh mắt giữa đã rời vạch cũ, không bắt buộc cả ba mắt trắng. `101` vẫn có thể vắt qua giao điểm nên không đánh dấu rời line. Sau khi rời line, giảm PWM pivot xuống180; mức pivot thường215 hiện có trong profile02 giữ nguyên.
2. Khi mắt giữa sáng trở lại (`010/011/110/111`), chốt ứng viên và STOP ngay dù chưa đủ100ms. `AGVController` quan sát trước RFID/SPI và áp STOP trước Serial log; `CoreNavigator::update` cũng có đường chốt ứng viên cho mẫu đọc cuối vòng.
3. Pha `TurnCapture` giữ route/index/next_node, motor dừng để xác nhận `010` liên tục100ms và≥3 mẫu khác thời điểm. Mắt bên hoặc giao điểm rộng không cho chuyển sang tiến. Xác nhận xong vẫn trả STOP một vòng, sau đó mới tiếp tục chặng cũ; lượt lùi trạm vẫn phải chuyển sonar sau/lấy mẫu mới như trước.
4. Nếu trôi qua vạch sau chốt ứng viên, chỉ chỉnh ngược PivotRight PWM180 theo xung40ms/nghỉ60ms sau khoảng dừng100ms; cả pha xác nhận/chỉnh giới hạn600ms. Hết hạn báo `LINE_LOST` và giữ dừng. Mất mạng/sonar, vật cản, STOP/hủy và grant giữ ưu tiên hiện có. Encoder không cộng pivot vào quãng đường chặng.
5. Nếu LEFT xe02 chưa bắt được ứng viên trong2500ms đầu, STOP và `LINE_LOST`; bỏ retry quay trái dài thêm2500ms của ADR-061 cho riêng trường hợp này. Phục hồi mất line khi bám tuyến theo ADR-062 vẫn giữ.

MQTT/schema/NODE_ORDER/ACK không đổi. Diagnostic trong lúc xác nhận vẫn giữ task đang hoạt động, chưa phát lỗi. Thêm Serial `[TURN] center_capture ...` sau khi STOP ở điểm quan sát sớm và mở log `[MOTOR]` cho nodeG1. BuildID02 `20261016-robot02-left-capture`.

## Kiểm chứng và áp dụng

Native bốn env **158/158 PASS**: profile01=40,02=41,trial=39,two-robot không profile=38. Bốn nhóm mới kiểm G1/C3→K1, chuỗi `010→001→011` không qua000 và trước100ms vẫn STOP, giữ line cũ không kết thúc pivot, xác nhận010, trôi vạch chỉ chỉnh ngược ngắn, bỏ retry dài, mất mạng/sonar/vật cản/STOP/grant. Test route cũ chờ pha xác nhận mới trước khi kiểm Forward/Backward; retry cũ vẫn được kiểm ở các profile còn bật.

Host `tests/motor-host/left_capture.cpp` link navigator/movement/motor/line thật với adapter GPIO: PASS duty215→180 khi rời line, chốt mắt giữa áp duty0/0 trước công việc RFID, countersteer−180/+180, STOP khi xác nhận rồi trở lại PWM tiến hiện tại; route601/index0 giữ nguyên, odometry không cộng xung pivot. Binary `.qa/robot02-left-capture.exe`.

Build `esp32dev_two_robot_bench_02` PASS, RAM57940byte, flash962025byte. Chưa upload hoặc điều khiển xe thật trong lượt sửa này. Cần nạp đúng bo02 rồi đối chiếu Serial G1/line/motor và quán tính sau STOP; góc quay vẫn chưa được đo tuyệt đối bằng encoder/IMU.
