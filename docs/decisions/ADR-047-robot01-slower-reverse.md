# ADR-047 — Giảm riêng tốc độ lùi xe 01

Ngày 13/10/2026. IMPLEMENTED và VERIFIED phần mềm; chuyển động vật lý OPEN.

`SOURCE`: người dùng báo xe 01 lùi K5→G5 quá nhanh, dễ lệch line hoặc rơi hàng, và yêu cầu ưu tiên giảm tốc độ lùi trước.

`OBSERVED`: MovementModule dùng BASE_SPEED cho lùi thẳng, CORRECTION_SPEED cho hai nhánh chỉnh line khi lùi. Profile 01 đặt hai mức này là 220, trim trái −40/phải 0, nên duty lùi thẳng là −180/−220. Firmware đúng profile 01 là esp32dev_two_robot_bench_01; esp32dev_trial không đọc Robot01Config.h.

`IMPLEMENTED`: thêm AGV_REVERSE_SPEED và AGV_REVERSE_CORRECTION_SPEED riêng profile 01, cùng đặt 190. Lùi thẳng là −150/−190; chỉnh trái giữ bánh trái 0, bánh phải −190; chỉnh phải giữ bánh trái −150, bánh phải 0. Config.h dùng fallback BASE_SPEED/CORRECTION_SPEED khi profile không khai báo, nên xe 02 và legacy giữ tuning cũ. Đi tiến, pivot, hướng correction, line-loss, watchdog, mutex, RFID và payload giữ nguyên. Diagnostic profile 01 nhận diện firmwareBuild=20261013-robot01-slower-reverse.

`PROPOSED`: 190 là mức thử phần mềm giảm 30 PWM, không phải tốc độ cm/s hay thông số đã hiệu chuẩn. Chưa thêm ramp PWM, chưa giải quyết phục hồi sau đặt tay tại G5 hoặc đi xuyên RFID; các phần này ngoài phạm vi lượt chỉnh lùi.

`VERIFIED`: tests/motor-host/reverse_speed.cpp chạy MovementModule/MotorModule/LineFollowModule thật với Arduino shim, kiểm tra duty/PWM/sign của lùi và correction, tiến/pivot, còi và STOP; profile 01 và 02 đều PASS. Native profile 01/02 42/42 test PASS. Build ESP32 esp32dev_two_robot_bench_01 PASS (RAM55996/Flash948117). SHA256 firmware.bin: A9AE091837A2C0D8016291C0F605DB33C5CBE4F03BBEE0EA4DEDEEDC62DCDA60. Binary host và manifest nằm trong .qa/robot01-reverse-20261013; firmware tại firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_01/firmware.bin. Chỉ build/test trên host, không upload, không gửi lệnh robot.

`OPEN`: chưa nạp vào xe. Cần người vận hành nạp đúng profile rồi thử ngắn không hàng, sau đó có tải, quan sát lực kéo, line và độ ổn định K5→G5. Nếu giảm PWM gây kẹt bánh hoặc không bắt được line, đo duty/nguồn/driver và chỉnh riêng mức lùi; không suy test host chứng minh không rơi hàng.
