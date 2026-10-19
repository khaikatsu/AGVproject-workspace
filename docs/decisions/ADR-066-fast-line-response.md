# ADR-066 — Ưu tiên phản ứng bám line cho hai xe

## Bổ sung 19/10/2026 — PWM bo cua xe01 sau thử M2 → G6

SOURCE bổ sung REQ-ROBOT01-ROUNDED-LINE-01: người dùng xác nhận xe01 vẫn không ôm cua M2→G6 trong scope01+02 và yêu cầu chỉnh PWM theo chẩn đoán.

OBSERVED trực tiếp trước sửa: diagnostic xe01 đang nạp `20261019-robot01-rounded-line-stale-sync-v2`, `AGV_SAFE_STOP/LINE_LOST`, pattern000, motorSTOP/duty0/0, Wi‑Fi/MQTT khỏe; task tớiK6 gián đoạn sau nodeM2. Mã profile01 dùng đi thẳng205/200 nhưng gentle110=255/240 và011=245/250: tăng tốc khi chỉnh và chênh hai kênh chỉ15/5PWM. Đây là bất hợp lý trong tuning; chưa có trace pattern/duty ngay trước mất line để khẳng định nguyên nhân cơ khí duy nhất.

IMPLEMENTED: profile01 thêm `AGV_BENCH_GENTLE_OUTER_SPEED=200` và hạ `AGV_BENCH_GENTLE_CORRECTION_SPEED`240→150. Mẫu110 nay205/150,011=155/200,010 vẫn205/200. Tách bánh nhanh gentle khỏi hard correction250 để100/001 giữ255/0 và0/250. Chiều motor, trim, lùi, pivot, timeout, tìm line, mutex và giao thức giữ nguyên. Profile khác thiếu macro mới fallback vềCORRECTION_SPEED nên xe02 giữ hành vi cũ. BuildID01 `20261019-robot01-curve-pwm-v3`, bao gồm sửa stale-counter trước đó.

Kiểm tra host phải xác nhận gentle hai bánh tiến, mỗi kênh không vượt duty đi thẳng, tổng duty giảm và chênh thực tế≥40PWM sau trim; đồng thời hồi quy STOP/000/grant/mạng/sonar và xe02. Kết quả build/kiểm tra cuối tại [tests](../../tests/README.md). OPEN: chưa nạp hoặc thử sàn;150PWM là mức tuning thử, cần xác minh bánh chậm còn quay dưới tải và chiều chỉnh hội tụ trên cung thực.

## Bổ sung 19/10/2026 — REQ-ROBOT01-ROUNDED-LINE-01

SOURCE: khi chạy Core xe01+02 trên map bo, xe01 chỉ quẹo nhẹ rồi dừng và báo mất line; chương trình nhúng thử dò line chạy bình thường. Người dùng yêu cầu sửa và kiểm tra xung đột server.

OBSERVED: không có lệnh Core dừng giữa cung. Firmware01 đi qua `000`, hết phục hồi rồi phát `LINE_LOST`; Core/server nhận lỗi sau đó mới gửi STOP. Profile01 đã sao chép nhầm grace mất line7s, LEFT turn capture và reverse gentle riêng xe02. Ngoài ra chế độ tích hợp ánh xạ `110/011` thành chỉnh gắt chỉ một bánh, khác cách thử gentle giữ cả hai bánh chạy, nên có thể bẻ quá mạnh trên cung bo. Đây là nguyên nhân ở firmware có bằng chứng mã nguồn; hiệu quả cơ khí thật vẫn cần đo trên xe.

IMPLEMENTED: thêm cờ riêng `AGV_GENTLE_FORWARD_CORRECTION` cho profile01. `110/011` dùng `GentleCorrectLeft/Right` với duty/trim hiện tại; `100/001` vẫn là hard correction, `000` vẫn STOP/phục hồi giới hạn và mọi chốt mạng, sonar, grant, mutex giữ nguyên. Xóa ba cờ xe02 khỏi profile01. Không đổi xe02, Core, server, map, MQTT hoặc các trị số PWM/trim người dùng đang dùng. BuildID01 là `20261019-robot01-rounded-line-fix-v1`.

VERIFIED: fast-line host và rounded-map PASS; PlatformIO native hai profile PASS93/93; build ESP32 profile01 PASS. Binary có SHA256 `671BEB9F1A73CC406043FD409A08CD0A4DEDC38BF1B67903DD53339AFD56ABFA`.

OPEN: chưa upload/chạy xe thật. Trước khi chạy lại hai xe, cần kê bánh xác nhận chiều/duty/STOP, rồi thử riêng xe01 trên một cung ngắn có người giám sát; chỉ sau đó mới thử scope01+02.

Bổ sung17/10/2026, REQ-REVERSE-TRIM-02: người dùng yêu cầu tách rõ trim lùi trái+5/phải−15 cho cảhai profile. Thêm hai macro01 và macro phải02 (macro trái02 đã có). Giữ chỉnh tay correction/gentle/reverseCorrection240 hiện tại và reverse01=200/02=210. Duty lùi01=205/185,02=215/195; chỉnh lùi245/0 hoặc0/225, giữ chiều điều khiển và cap settling. Trim tiến/PWMxoay không đổi. Các số PWM trong phần quan sát lần sửa fast-line bên dưới là lịch sử trước bổ sung này. Chỉ kiểm tra biên dịch Config.h hai profile cho lần thêm macro; chưa build lại ảnh ESP32 hoặc upload.

Ngày17/10/2026. SOURCE: người dùng báo hai xe không bám mượt qua góc bo và xác nhận “chỉnh lái chậm, không theo kịp đường cong”. Phạm vi firmware profile01/02, không đổi map hoặc chính sách điều phối.

OBSERVED: vòng AGVController trước đây chỉ áp navigator/motor sau RC522 và hàng lệnh. Mỗi thay đổi action có thể in hai dòng Serial; sonar in hai dòng mỗi giây. MotorModule L298N ghi PWM0 trước mọi cập nhật, kể cả cùng chiều/cùng duty. Các yếu tố này có thể tăng độ trễ hoặc tạo ngắt duty; chưa đo được chu kỳ/thời gian trên bo thực để khẳng định nguyên nhân duy nhất.

IMPLEMENTED: hai profile bật AGV_FAST_LINE_RESPONSE trong SharedRobotConfig.h. CoreNavigator::refreshFollowing chỉ cập nhật khi đã Following, không trong chờ grant/NODE_ORDER, quay, căn line, settling, hoặc đang phục hồi/mới pause vật cản. Dùng nguyên update hiện có để xử lý line/safety, không tự tiến path/RFID. AGVController gọi và áp motor trước RFID sau kiểm tra priority/network/sonar/phiên/vị trí/lift/control lỗi. Lượt cập nhật đầy đủ sau RFID vẫn giữ để phản ứng với node/lệnh mới. Thiếu grant trong Following giữ STOP ở cả lượt sớm và thường.

Driver L298N bỏ ghi lặp cùng duty; nếu cùng dấu thì đổi duty trực tiếp. Chuyển chiều hoặc về0 vẫn đi qua đường ghi disable0 và chân hướng như trước. Dừng vẫn triệt PWM hai bên. Cấu hình legacy không bật cờ giữ cách ghi cũ; BTS7960 không đổi.

Log MOTOR/BUZZER đổi action giới hạn200ms, STOP vẫn có log ngay. Hoãn log sonar định kỳ trong Following; log fault/nhận GOTO và diagnostic MQTT còn nguyên. Không thay thời gian poll RFID20ms, timeout/sonar/ngưỡng vật cản/mất line, PID hoặc PWM/trim/luật ánh xạ pattern. Đây là ưu tiên thứ tự xử lý, không phải vòng điều khiển thời gian thực: RC522, Serial và công việc còn lại vẫn có thể chặn vòng lặp.

PWM hiện có được giữ nguyên theo file profile tại lúc sửa:01 base220/trim−30,+15/correction235/reverse200/reverseCorrection220/pivot230/capture230;02 base230/trim+15,−15/correction235/reverse210/reverseLeftTrim+5/reverseCorrection220/pivot230/capture230. Các giá trị trong ghi chú lịch sử cũ không ghi đè các chỉnh tay hiện tại của nhóm.

BuildID mới20261017-robot01-fast-line-v4 /20261017-robot02-fast-line-v4, giữ mapv4. VERIFIED phần mềm và artifact tại tests/README.md. OPEN: chưa upload hoặc đo độ trễ/bán kính/quãng trôi/khả năng theo góc thực. Nếu còn chậm sau nạp, cần đối chiếu pattern/duty và bố trí mắt/bán kính trên sàn để chọn bước tuning PWM hoặc điều khiển tiếp theo.
