# ADR-054 — Giảm nhẹ PWM xoay xe01 để bắt line

Ngày14/10/2026.

## Cập nhật 15/10/2026 — REQ-ROBOT01-PIVOT-210-01

`SOURCE`: xe đã xoay nhưng người dùng quan sát bắt line không kịp; yêu cầu cuối cùng là PWM xoay210. Áp dụng riêng profile01: TURN_SPEED200→210, hai bên trim0/0, pivot trái+210/−210, phải−210/+210, gồm quay đỗ trạm. Giữ TURN_TIMEOUT2500ms, TURN_MIN100ms, rời000 rồi nhận010/101/111; không áp phương án180/3500ms đã bị yêu cầu mới thay thế. Không suy ra PWM210 sẽ sửa được lỗi bắt line nếu chưa có trace line/góc thực tế.

Giữ encoder GPIO16/4, sonic hạn60ms/log47ms, bộ lọc pin và tuning tiến/lùi. FirmwareBuild `20261015-robot01-pivot210`; env `esp32dev_two_robot_bench_01`. Artifact mới ở `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` trong project NCKHAGV, không phải bộ đóng gói slower-pivot cũ bên dưới. Chưa upload hoặc chạy xe thật.

Kiểm thử driver host profile01/02 PASS: xe01 pivot trái/phải duty±210, hai PWM channel210; xe02 giữ±230. Tiến/lùi/chỉnh line/còi và STOP PASS. Compile-time kiểm tra timeout cả hai profile vẫn2500ms. Script probe: `tests/motor-host/reverse_speed.cpp`; binary trong `.qa/turn-line-tuning-build/`. Chưa chứng minh khả năng bắt line ngoài sàn.

`SOURCE`: người dùng yêu cầu xe01 xoay chậm hơn một chút; quan sát xe không nhận line khi xoay và tiếp tục quay180°.

`OBSERVED`: profile01 dùng TURN_SPEED220, trim quay0/0. Pivot trái/phải chạy hai bánh ngược chiều; điều kiện bắt nhánh là đã rời line cũ (`000`) rồi thấy centered (`010/101/111`), TURN_MIN100ms và timeout2500ms. S1/S2 chủ động quay đầu180° để hướng ra GS; quay180° ở trạm là hành vi đỗ theo ADR-033. Chưa có trace IR khi xe bỏ lỡ nhánh rẽ để chứng minh nguyên nhân duy nhất là PWM.

## Quyết định

Giảm riêng AGV_BENCH_TURN_SPEED của profile01 từ220 xuống200 (giảm PWM khoảng9,1%; không suy ra tốc độ góc giảm đúng9,1%). Trái:+200/−200, phải:−200/+200; trim0/0. Áp dụng các pivot, gồm quay ở node rẽ và quay đỗ S1. FirmwareBuild mới `20261014-robot01-slower-pivot`.

Giữ tiến220/trim trái−40, lùi190/trim trái−40, điều kiện bắt line, góc hướng server, thời hạn quay/mạng/mutex, encoder và diagnostic còi. Xe02 giữ TURN_SPEED230. Chưa hiệu chỉnh điều kiện rời line hoặc dùng encoder đo góc vì chưa có dữ liệu thực tế để chọn ngưỡng mới.

## Kiểm chứng

Bản210 ngày15/10/2026: PlatformIO profile01 build PASS, RAM57332 byte/17,5%, flash957789 byte/73,1%. Các artifact đóng gói và số±200 bên dưới mô tả bản14/10 cũ; bản mới dùng ảnh trong `.pio/build/esp32dev_two_robot_bench_01`. Chưa upload/thử sàn.

Host module MotorModule/MovementModule/BuzzerModule/LineFollowModule thật với I/O stub PASS: profile01 pivot trái/phải có duty±200 và PWM hai channel200, STOP triệt PWM; profile02 pivot±230. Hồi quy tiến/lùi/chỉnh line/còi PASS bằng probe hiện có. Build ESP3201/02 PASS; native26case mỗi profile, tổng52/52 PASS, gồm rời junction trước bắt line, NODE_ORDER trái/phải, quay đỗ trạm, timeout/mất mạng/mutex.

Artifact `build/robot01-slower-pivot/{firmware.bin,firmware.elf,bootloader.bin,partitions.bin,manifest.json}`. SHA256 firmware070D4C1A93E848C3F5EBFB0626AFBCEEC909451B0A0AE0E77AABF8A023DD8AD9. Chưa upload hoặc điều khiển xe thật trong lượt sửa; Core/gateway vận hành không cần thay cho tuning PWM này.

`OPEN`: chưa nạp hoặc đo góc quay trên sàn. Cần thử lượt rẽ ngắn, đủ lực quay dưới tải và bắt lại line trước timeout; nếu vẫn bỏ nhánh, đối chiếu chuỗi IR/thời điểm thay vì tiếp tục giảm PWM theo suy đoán.
