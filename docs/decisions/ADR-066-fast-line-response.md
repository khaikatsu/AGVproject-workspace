# ADR-066 — Ưu tiên phản ứng bám line cho hai xe

Bổ sung17/10/2026, REQ-REVERSE-TRIM-02: người dùng yêu cầu tách rõ trim lùi trái+5/phải−15 cho cảhai profile. Thêm hai macro01 và macro phải02 (macro trái02 đã có). Giữ chỉnh tay correction/gentle/reverseCorrection240 hiện tại và reverse01=200/02=210. Duty lùi01=205/185,02=215/195; chỉnh lùi245/0 hoặc0/225, giữ chiều điều khiển và cap settling. Trim tiến/PWMxoay không đổi. Các số PWM trong phần quan sát lần sửa fast-line bên dưới là lịch sử trước bổ sung này. Chỉ kiểm tra biên dịch Config.h hai profile cho lần thêm macro; chưa build lại ảnh ESP32 hoặc upload.

Ngày17/10/2026. SOURCE: người dùng báo hai xe không bám mượt qua góc bo và xác nhận “chỉnh lái chậm, không theo kịp đường cong”. Phạm vi firmware profile01/02, không đổi map hoặc chính sách điều phối.

OBSERVED: vòng AGVController trước đây chỉ áp navigator/motor sau RC522 và hàng lệnh. Mỗi thay đổi action có thể in hai dòng Serial; sonar in hai dòng mỗi giây. MotorModule L298N ghi PWM0 trước mọi cập nhật, kể cả cùng chiều/cùng duty. Các yếu tố này có thể tăng độ trễ hoặc tạo ngắt duty; chưa đo được chu kỳ/thời gian trên bo thực để khẳng định nguyên nhân duy nhất.

IMPLEMENTED: hai profile bật AGV_FAST_LINE_RESPONSE trong SharedRobotConfig.h. CoreNavigator::refreshFollowing chỉ cập nhật khi đã Following, không trong chờ grant/NODE_ORDER, quay, căn line, settling, hoặc đang phục hồi/mới pause vật cản. Dùng nguyên update hiện có để xử lý line/safety, không tự tiến path/RFID. AGVController gọi và áp motor trước RFID sau kiểm tra priority/network/sonar/phiên/vị trí/lift/control lỗi. Lượt cập nhật đầy đủ sau RFID vẫn giữ để phản ứng với node/lệnh mới. Thiếu grant trong Following giữ STOP ở cả lượt sớm và thường.

Driver L298N bỏ ghi lặp cùng duty; nếu cùng dấu thì đổi duty trực tiếp. Chuyển chiều hoặc về0 vẫn đi qua đường ghi disable0 và chân hướng như trước. Dừng vẫn triệt PWM hai bên. Cấu hình legacy không bật cờ giữ cách ghi cũ; BTS7960 không đổi.

Log MOTOR/BUZZER đổi action giới hạn200ms, STOP vẫn có log ngay. Hoãn log sonar định kỳ trong Following; log fault/nhận GOTO và diagnostic MQTT còn nguyên. Không thay thời gian poll RFID20ms, timeout/sonar/ngưỡng vật cản/mất line, PID hoặc PWM/trim/luật ánh xạ pattern. Đây là ưu tiên thứ tự xử lý, không phải vòng điều khiển thời gian thực: RC522, Serial và công việc còn lại vẫn có thể chặn vòng lặp.

PWM hiện có được giữ nguyên theo file profile tại lúc sửa:01 base220/trim−30,+15/correction235/reverse200/reverseCorrection220/pivot230/capture230;02 base230/trim+15,−15/correction235/reverse210/reverseLeftTrim+5/reverseCorrection220/pivot230/capture230. Các giá trị trong ghi chú lịch sử cũ không ghi đè các chỉnh tay hiện tại của nhóm.

BuildID mới20261017-robot01-fast-line-v4 /20261017-robot02-fast-line-v4, giữ mapv4. VERIFIED phần mềm và artifact tại tests/README.md. OPEN: chưa upload hoặc đo độ trễ/bán kính/quãng trôi/khả năng theo góc thực. Nếu còn chậm sau nạp, cần đối chiếu pattern/duty và bố trí mắt/bán kính trên sàn để chọn bước tuning PWM hoặc điều khiển tiếp theo.
