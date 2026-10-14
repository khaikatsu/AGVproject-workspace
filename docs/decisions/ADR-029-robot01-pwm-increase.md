# ADR-029 — Tăng vừa PWM chạy và quẹo xe 01

Ngày 10/10/2026. Trạng thái: IMPLEMENTED trong profile; kiểm chứng phần mềm, chưa nạp/đo chuyển động thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng báo xe 01 nhận lệnh server nhưng đứng yên, kêu è; yêu cầu tăng PWM vừa phải khi đi và quẹo.

`OBSERVED`: profile `Robot01Config.h` đặt PWM cơ sở/quay/chỉnh line 230; trim chạy trái −40/phải 0, nên chạy thẳng danh định 190/230, pivot 230/230. Tiếng è do người dùng báo chưa đủ xác định nguyên nhân là duty thấp; chưa có số đo mới tại driver.

## Quyết định

Chỉ profile `esp32dev_two_robot_bench_01`: tăng cơ sở, quay, chỉnh line và chỉnh nhẹ từ 230 lên 245 (+15, khoảng 5,9% thang 255). Giữ trim nên chạy thẳng 205/245, pivot 245/245; chỉnh một bánh 205/0 hoặc 0/245. Giữ chiều motor, GPIO, timeout, chốt điện áp/line/mạng và mutex. Profile xe 02 giữ 230; legacy solo và probe không đổi.

## Kiểm chứng và giới hạn

`VERIFIED`: host g++ chọn đúng MQTT ID 01, PWM cơ sở 245, trái 205/phải 245 và GPIO không trùng. `native_two_robot_bench` với cờ `AGV_ROBOT_PROFILE_01=1`: 12/12 PASS, gồm chốt điện áp, clearance/mutex, quay bắt lại line và dừng mất mạng. Đây là mô phỏng logic, chưa chứng minh lực kéo thật.

`VERIFIED`: build ESP32 `esp32dev_two_robot_bench_01` PASS, tạo `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` (RAM 16,8%, Flash 72,1%). Chưa upload. Kiểm tra workspace tổng thể còn báo link hỏng ngoài phần thay đổi và thiếu thư viện `jsonschema`; không ghi nhận lỗi link ở tài liệu mới.

`OPEN`: nạp đúng ảnh env 01, thử hai bánh và chiều quay trên giá kê; quan sát duty/điện áp driver nếu vẫn đứng yên. Chưa khẳng định mức 245 đủ kéo xe trên sàn hoặc đã giải quyết nguyên nhân tiếng è.
