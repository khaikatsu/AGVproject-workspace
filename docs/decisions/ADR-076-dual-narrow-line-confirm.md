# ADR-076 — Xác nhận line bám được và tách thời gian chờ khỏi quét motor

Ngày21/10/2026, cập nhật22/10/2026. Trạng thái: IMPLEMENTED phần mềm, kiểm chứng/build tại tests/README.md; nạp và thử xe thật OPEN.

## Quyết định thay thế22/10/2026 — Bỏ xác nhận mép001/100, narrow-v12

`SOURCE`: người dùng quan sát khi rẽ xe có thể bắt nhầm vạch khác với mẫu001/100 và yêu cầu bỏ phần vừa thêm. `OBSERVED`: test hồi quy mới chạy trên bản trước sửa thất bại tại điều kiện edge-only không được vềFollowing, tái hiện firmware vẫn dùng mép làm bằng chứng phục hồi. Đây là xác nhận policy phần mềm quá rộng, chưa phải đo hình học thực tế của vạch trên sàn.

`IMPLEMENTED`: gỡ001/100 khỏi candidate phục hồi sau000, bộ xác nhận ổn định TurnCapture và điều kiện khởi động TurnCapture. Chỉ010/011/110 có mắt giữa được xác nhận;001/100 làm mất liên tục của bằng chứng xác nhận. Chúng vẫn có thể chứng minh đã rời vạch cũ để khi mắt giữa trở lại mới vàoCapture; khi Following bình thường vẫn dùng PD chỉnh hướng. Không đổi quy tắcphiếu010 cũ, cửa sổ100ms/≥3mẫu/gap≤50ms, quay/quét≤600ms, chờcapture1500ms, grace9s/7s, sàn/trầnPD01=110..230, servo/mạng/watchdog hoặc hợp đồngMQTT. BuildID01/02 `20261022-robot01-narrow-v12` / `20261022-robot02-narrow-v12`.

`VERIFIED`: hostline-confirm PASS cảhai profile, gồm mép liên tục khiTurning không khởi độngCapture, mép liên tục800ms trongCapture không hoàn tất, rollover/rẽtrái/rẽphải, mép làm lại xác nhận và phục hồi sau000 không nhận001/100. Giữ test010/011/110, chốtgrant/mạng/sonar/STOP/timeout và capPWM. Fast-line hai profile/native102/102 PASS; kết quảbuild tại tests/README.md. `OPEN`: nạp/thử rẽ trên xe thật. Chính sách nhận001/100 trong các phần v10 phía dưới là lịch sử đã được thay thế.

## SOURCE

Người dùng báo hai xe xoay hoặc tiến vẫn thấy010/011/110 nhưng server báo mất line, yêu cầu kiểm tra xung đột rồi đồng ý cải thiện. Ngày22/10 người dùng yêu cầu nhận thêm100 và001 để xe ổn định hơn. Mục tiêu là giảm chốt LINE_LOST khi tín hiệu line hợp lệ đã trở lại, không phải vô hiệu hóa STOP/watchdog hay tự chạy lại sau reset.

## OBSERVED

Core nhận lỗi MCU qua topicerror hoặc diagnostic đã bind, sau đó interrupt task và gửiSTOP. Firmware chốt LINE_LOST trong CoreNavigator. Probe host trên cảhai profile tái hiện011/110 liên tục trong TurnCapture vẫn lỗi ở600ms, không cần server/reset; sau lỗi010 không tự xóa chốt. Đây là tái hiện một nguyên nhân khả dĩ, không chứng minh mọi lần khựng ở xe thật do cùng nhánh. Following hiện phục hồi centered-only; phần grace01=9000ms/02=7000ms không áp cho TurnCapture. Servo180/100 và mạng riêng hai xe đã được khôi phục trong các yêu cầu trước, cần giữ nguyên.

## Quyết định và IMPLEMENTED

- Bật `AGV_LINE_RECOVERY_TRACK_CONFIRM` ở SharedRobotConfig cho cảhai profile. Bộ xác nhận nhận nhóm có mắt giữa010/011/110 và hai mẫu mép001/100;000/101/111 không phải bằng chứng line bám được.
- Khi Following đi tiến đã vào phục hồi sau000: yêu cầu line hợp lệ liên tục≥100ms,≥3 mẫu có timestamp mới cách≥5ms và gap≤50ms. Nhóm010/011/110 có thể chuyển qua lại vì mắt giữa vẫn bám line;001 và100 phải giữ đúng cùng một phía. Đảo001↔100 làm lại bộ đếm. STOP trong xác nhận, giữ route/index/grant. Đi lùi vẫn centered-only010. Các thời gian grace9s/7s, quét180PWM/600ms và timeout chặng giữ nguyên.
- TurnCapture vẫn phải có một mẫu trước chứng minh đã rời vạch cũ;001/100 không được vừa chứng minh rời line vừa được tính là bắt lại line trong cùng mẫu. Sau đó001/010/011/100/110 ổn định100ms/≥3 mẫu/gap≤50ms có thể kết thúc căn. Giữ quy tắc010 có≥2 phiếu trong3 mẫu và mẫu cuối010 của bản cũ để không làm chậm đường căn đã xác nhận.
- Khi bắt đầu thấy bất kỳ mẫu line hợp lệ nào thì dừng để lấy bằng chứng, không tiếp tục pivot trong lúc xác nhận.000 chỉ được quét bằng xung40ms/nghỉ60ms trong ngân sách600ms cũ;101/111 không xác nhận line mới và giữSTOP.
- Tách tổng thời gian chờ TurnCapture thành1500ms. Từ600ms trở đi luônSTOP, chỉ quan sát line; không kéo dài thời gian quay/quét. Hết1500ms chưa xác nhận mới LINE_LOST; thiếu grant/vật cản xóa bằng chứng cũ, phải xác nhận lại khi an toàn.
- Sau xác nhận capture vẫnSTOP một vòng, giữ route/cmd_id/index/grant, handoff sonar trước/sau và cap PWM200ms đầu theo mức capture từng hướng đã có. Không tăng tuning motor/PD/servo.
- STOP, mất mạng/phiên, cảm biến lỗi, thu hồi grant vẫn thắng. Sau fault hoặc reset MCU không replay route hoặc tự clear lỗi. Server vẫn chỉ phản ứng với lỗi MCU thực sự phát, không thêm nhánh tự hồi phục ởCore, không nới quyền mutex.
- Thêm Serial `[LINE] capture_recovered`, `capture_timeout`, `turn_timeout` với pattern/elapsed và bằng chứng tương ứng để chẩn đoán hiện trường. Không thêm dữ liệu nhạy cảm, topic/payload/ACK mới.
- BuildID01/02 `20261022-robot01-line-edge-v10` / `20261022-robot02-line-edge-v10`. Mạng riêng, servo hạ180/nâng100, watchdog, counter/NVS không đổi.

## Kiểm chứng và giới hạn

Probe `scripts/test-line-confirm.ps1` dùng navigator→Movement→PD→MotorModule thật với adapter GPIO/time; kiểm LEFT/RIGHT/rollover, ổn định001/010/011/100/110, đảo001↔100 không xác nhận, phục hồi sau600ms, mọi output sau600ms trong capture chưa xác nhận bằng0, deadline1500ms, capPWM, blank/wide/nhiễu/timestamp trùng/mẫu cũ, grant/vật cản cần xác nhận lại, mạng/cảm biến/STOP/fault không tự chạy lại. Đây là bench phần mềm, không phải đo chuyển động thật. Native01/02 và kết quả hồi quy/build ghi trong tests/README.md.

Chưa nạp hoặc điều khiển xe trong lượt sửa. Khi thử thật cần kê bánh/cô lập tải trước, rồi tuyến ngắn có giám sát, không đặt tay vào bánh hoặc cơ cấu servo khi đang nhận lệnh. Nếu có reset/sụt nguồn vẫn phải xử lý nguyên nhân riêng; bản này không sửa nguồn điện và không bảo đảm hết mọi lỗi line vật lý.

VERIFIED v10: hostline-confirm01/02 vàfast-line01/02 PASS, native01/02 PASS102/102, buildESP32 hai env PASS. Kết quả hostservo/CoreServo của v9 vẫn còn giá trị vì v10 không sửa servo; không tính là đã chạy lại trong lượt này. Hồi quyngoàiphạmvi `native_trial` của v9 còn1testmutex/vậtcản lỗi nền như đã ghi trong tests/README.md.

Tham chiếu: REQ-DUAL-NARROW-LINE-CONFIRM-01; REQ-DUAL-EDGE-LINE-CONFIRM-01; ADR-062; ADR-063; ADR-075.
