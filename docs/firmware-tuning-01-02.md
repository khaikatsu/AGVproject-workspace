# Bảng chỉnh thông số xe 01 và xe 02

Cập nhật 20/10/2026. OBSERVED: đối chiếu source hiện tại trong workspace, sau khôi phục firmware và sửa gentle/recovery xe01. Đây là bảng tra cấu hình, không phải kết quả đo xe thật. Khi sửa code về sau, các giá trị trong profile và `Config.h` là nguồn chính xác; không dùng các bộ PD/v5/v6/v7 trong tài liệu lịch sử để suy ra cấu hình hiện tại.

## Mở đúng file

| Muốn chỉnh | Nơi chỉnh |
| --- | --- |
| PWM, trim, cờ riêng xe01 | [Robot01Config.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/Robot01Config.h) |
| PWM, trim, cờ riêng xe02 | [Robot02Config.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/Robot02Config.h) |
| Thời gian chặng/RFID, hiệu chuẩn encoder/pin chung | [SharedRobotConfig.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/SharedRobotConfig.h) — sửa ảnh hưởng cả hai xe |
| PWM/thời gian recovery, giới hạn quay, sonar, GPIO | [Config.h](../firmware/NCKHAGV/NCKHAGV/src/Config.h) — các hằng dùng chung ảnh hưởng cả hai xe |

`L/R` trong bảng dưới là kênh phần mềm `LEFT_ENA` GPIO14 / `RIGHT_ENB` GPIO25. Comment cũ của người dùng gọi hai kênh này ngược với tên bánh vật lý; cần xác minh trên xe trước khi suy ra bánh trái/phải. Pattern cũng theo thứ tự bit của Config: GPIO39 / GPIO34 / GPIO35, không tự suy ra phía lắp cảm biến thực tế.

PWM có độ lớn 0…255; số lớn hơn là duty lớn hơn, không phải vận tốc cm/s. Trong bảng duty: dấu `+` là lệnh tiến, dấu `−` là lệnh lùi theo cấu hình motor; `0` là kênh dừng.

## Thông số riêng từng xe

Các tên `AGV_*` dưới đây nằm trong profile tương ứng; “mặc định” nghĩa là profile không khai báo macro đó, `Config.h` chọn giá trị fallback.

| Macro | Xe01 | Xe02 | Tác dụng |
| --- | ---: | ---: | --- |
| `AGV_BENCH_MOTOR_SPEED` | 230 | 240 | PWM nền đi thẳng tiến, trước trim |
| `AGV_BENCH_LEFT_SPEED_TRIM` | −20 | +5 | Cộng vào kênh L khi tiến thẳng và chỉnh line tiến |
| `AGV_BENCH_RIGHT_SPEED_TRIM` | 0 | 0 | Cộng vào kênh R khi tiến thẳng và chỉnh line tiến |
| `AGV_BENCH_TURN_SPEED` | 230 | 230 | PWM nền pivot tại node |
| `AGV_BENCH_TURN_LEFT_SPEED_TRIM` | +25 | +25 | Trim kênh L cho cả hai hướng pivot |
| `AGV_BENCH_TURN_RIGHT_SPEED_TRIM` | +25 | +25 | Trim kênh R cho cả hai hướng pivot |
| `AGV_BENCH_CORRECTION_SPEED` | 240 | 240 | PWM chỉnh mạnh; kênh còn lại bằng0 |
| `AGV_GENTLE_FORWARD_CORRECTION` | 1 | 0 mặc định | Chọn gentle cho110/011 khi tiến |
| `AGV_BENCH_GENTLE_OUTER_SPEED` | 210 | 240 mặc định | PWM nền kênh nhanh khi gentle; xe02 fallback về CORRECTION_SPEED |
| `AGV_BENCH_GENTLE_CORRECTION_SPEED` | 150 | 240 | PWM nền kênh chậm khi gentle; xe02 hiện không chọn nhánh này lúc bám line tiến |
| `AGV_REVERSE_SPEED` | 200 | 200 | PWM nền lùi thẳng, dùng trim lùi riêng |
| `AGV_REVERSE_LEFT_SPEED_TRIM` | +5 | +5 | Trim kênh L khi lùi; trong source đang viết `05`, giá trị bằng5 |
| `AGV_REVERSE_RIGHT_SPEED_TRIM` | −10 | −10 | Trim kênh R khi lùi |
| `AGV_REVERSE_CORRECTION_SPEED` | 240 | 240 | PWM nền chỉnh line khi lùi, độc lập tốc độ lùi thẳng |
| `AGV_REVERSE_GENTLE_CORRECTION` | 0 mặc định | 1 | Khi bật, giảm một kênh lùi thay vì dừng hẳn |
| `AGV_REVERSE_CORRECTION_DELTA` | 0 mặc định | 30 | Trừ khỏi kênh chậm của chỉnh line lùi, sau cộng trim |
| `AGV_LINE_RECOVERY_SIDE_CONFIRM` | 1 | 0 mặc định | Xe01 tiến được nhận lại line bên ổn định sau000 |
| `AGV_LINE_LOSS_GRACE_MS` | 0 mặc định | 7000 ms | Hạn xác nhận mất line, không phải thời gian cho motor quay liên tục |
| `AGV_LEFT_TURN_CAPTURE_ENABLED` | 0 mặc định | 1 | Bắt line sau quay trái ở node |
| `AGV_LEFT_TURN_CAPTURE_PWM` | 180 mặc định | 230 | PWM pivot chính xác cho nhánh capture; xe01 không bật capture |

`TURN_LEFT_SPEED_TRIM` không phải “chỉ chỉnh lúc quay trái”; nó chỉnh kênh L trong cả `turnLeft()` và `turnRight()`. Tương tự với trim kênh R.

## Duty thực tế sau trim

Giá trị dưới đây là đầu ra nhánh chuyển động thường, khi đã được navigator cho phép; chưa vào settling/capture/tìm line. Các chốt dừng hoặc thiếu grant có thể đưa cả hai kênh về0 dù mắt đọc khác000.

| Trạng thái / pattern | Xe01 L / R | Xe02 L / R |
| --- | --- | --- |
| Tiến010,101,111 | +210 / +230 | +245 / +240 |
| Tiến110 | +190 / +150 | +245 / 0 |
| Tiến011 | +130 / +210 | 0 / +240 |
| Tiến100 | +220 / 0 | +245 / 0 |
| Tiến001 | 0 / +240 | 0 / +240 |
| Lùi010,101,111 | −205 / −190 | −205 / −190 |
| Lùi100,110 | 0 / −230 | −215 / −230 |
| Lùi001,011 | −245 / 0 | −245 / −200 |
| `PivotLeft` thường | +255 / −255 | +255 / −255 |
| `PivotRight` thường | −255 / +255 | −255 / +255 |
| Vừa phát hiện000 / STOP | 0 / 0 | 0 / 0 |

Sau000, recovery có thể xuất xung pivot L/R = +180/−180 hoặc−180/+180 theo hướng nhớ, xen STOP. Capture quay trái xe02 dùng mức230 riêng; 200ms settling sau capture có thể cap duty theo230, nên không áp nguyên bảng chuyển động thường cho pha này.

Công thức xe01 khi tiến:

```text
010: L = MOTOR_SPEED + LEFT_TRIM           R = MOTOR_SPEED + RIGHT_TRIM
110: L = GENTLE_OUTER + LEFT_TRIM          R = GENTLE_INNER + RIGHT_TRIM
011: L = GENTLE_INNER + LEFT_TRIM          R = GENTLE_OUTER + RIGHT_TRIM
100: L = CORRECTION_SPEED + LEFT_TRIM      R = 0
001: L = 0                               R = CORRECTION_SPEED + RIGHT_TRIM
```

`GENTLE_INNER` trong công thức là `AGV_BENCH_GENTLE_CORRECTION_SPEED`; các tên rút gọn chỉ dùng để đọc công thức, không phải macro mới. Ví dụ110 hiện bằng `(210−20)/(150+0)` =190/150;011 bằng `(150−20)/(210+0)` =130/210.

## Thông số chung và recovery

| Nơi chỉnh | Tên | Giá trị hiện tại | Ý nghĩa |
| --- | --- | --- | --- |
| SharedRobotConfig.h | `AGV_FAST_LINE_RESPONSE` | 1 | Áp bám line sớm trước thao tác RFID khi đang Following được phép |
| SharedRobotConfig.h | `AGV_LINE_RECOVERY_ENABLED` | 1 | Cho phép bộ tìm lại line có giới hạn |
| SharedRobotConfig.h | `AGV_BENCH_LINE_LOSS_CONFIRM_MS` | 5000 ms | Hạn chờ nếu không có hướng tìm hoặc đã dùng lượt quét; grace riêng xe02 ưu tiên7000ms |
| SharedRobotConfig.h | `AGV_BENCH_SEGMENT_TIMEOUT_MS` | 15000 ms | Giới hạn thời gian một chặng |
| SharedRobotConfig.h | `AGV_BENCH_RFID_STOP_DWELL_MS` | 1000 ms | Thời gian dừng tại RFID theo máy trạng thái |
| SharedRobotConfig.h | `AGV_TURN_RETRY_ENABLED` | 1 | Cho phép thử quay lại theo policy navigator |
| SharedRobotConfig.h | `AGV_ENCODER_PULSES_PER_100CM` | 161.0 | Hệ số encoder xung/100cm |
| SharedRobotConfig.h | `AGV_BENCH_BATTERY_VOLTAGE_SCALE` / `OFFSET` | 5.0 / 0.0 | Hệ số và offset hiệu chuẩn điện áp |
| Config.h | `MOTOR_PWM_HZ` / `MOTOR_PWM_BITS` | 5000 Hz / 8 bit | Tần số và độ phân giải PWM motor |
| Config.h | `LINE_SENSOR_ACTIVE_STATE` | 1 | Mức tín hiệu được chuẩn hóa thành mắt thấy vạch |
| Config.h | `LINE_RECOVERY_PWM` | 180 | Độ lớn duty hai kênh khi quét tìm line, không cộng trim pivot thường |
| Config.h | `LINE_RECOVERY_STOP_MS` | 100 ms | Dừng ban đầu trước khi quét |
| Config.h | `LINE_RECOVERY_TIMEOUT_MS` | 600 ms | Ngân sách thời gian quét tính từ lần000 đầu, gồm dừng/nghỉ |
| Config.h | `LINE_RECOVERY_PULSE_MS` / `REST_MS` | 40 / 60 ms | Thời gian xung xoay / nghỉ |
| Config.h | `LINE_RECOVERY_CENTER_MS` | 100 ms | Thời gian line ứng viên phải ổn định; còn cần≥3 mẫu khác thời điểm |
| Config.h | `LINE_RECOVERY_MEMORY_MS` | 750 ms | Tuổi tối đa của hướng chỉnh gần nhất để chọn phía quét |
| Config.h | `LINE_RECOVERY_SAMPLE_GAP_MS` | 50 ms | Gap tối đa giữa các mẫu xác nhận xe01 tiến |
| Config.h | `TURN_MIN_MS` / `TURN_TIMEOUT_MS` | 100 / 2500 ms | Giới hạn của nhánh quay; không phải PWM |
| Config.h | `LEFT_CAPTURE_SAMPLE_MS` / `MAX_GAP_MS` / `SETTLE_MS` | 5 / 50 / 200 ms | Chu kỳ lấy mẫu/gap/settling của capture |
| Config.h | `OBSTACLE_DISTANCE_CM` / `OBSTACLE_CLEAR_CM` | 15 / 20 cm | Ngưỡng dừng vì vật cản / cho thông lại |

Hạn lỗi recovery không giống nhau giữa hai xe:

| Tình huống | Xe01 | Xe02 |
| --- | --- | --- |
| Có hướng mới và còn lượt quét | Chốt lỗi khi hết600ms nếu chưa xác nhận line | Quét tối đa600ms; sau đó STOP chờ xác nhận, tổng hạn7000ms |
| Không có hướng hoặc đã quét một lần trong chặng | STOP chờ, hạn5000ms | STOP chờ, hạn7000ms |
| Mẫu được nhận lại khi tiến | 010 hoặc100/110 cùng nhóm hoặc001/011 cùng nhóm | Chỉ010 |
| Mẫu được nhận lại khi lùi | Chỉ010 | Chỉ010 |

Mỗi chặng chỉ có một lượt quét. Mẫu ứng viên phải đủ100ms/≥3 mẫu trước deadline; xe01 tiến còn yêu cầu gap≤50ms. Trong xác nhận motorSTOP. Không trộn nhóm trái/phải/giữa;101/111 không đủ chứng minh bắt lại line. Hết hạn đã chốtLINE_LOST thì mắt trở lại cũng không tự xóa lỗi. Thông số này theo source hiện tại, không phải grace2s của nhánh PD lịch sử.

## Chọn đúng thông số khi muốn điều chỉnh

| Mục đích | Thông số cần xem |
| --- | --- |
| Đổi tốc độ tiến thẳng | `AGV_BENCH_MOTOR_SPEED` và duty thực sau hai trim tiến |
| Cân hai kênh khi đi thẳng | `AGV_BENCH_LEFT_SPEED_TRIM` / `RIGHT_SPEED_TRIM`; trim cũng đổi duty chỉnh line tiến |
| Chỉnh cung nhẹ110/011 xe01 | `AGV_BENCH_GENTLE_OUTER_SPEED` và `AGV_BENCH_GENTLE_CORRECTION_SPEED` |
| Chỉnh mạnh100/001 | `AGV_BENCH_CORRECTION_SPEED`; xe02 cả110/011 cũng dùng giá trị này |
| Đổi lực pivot tại node | `AGV_BENCH_TURN_SPEED` và hai trim quay; hiện230+25=255 đã tới mức tối đa |
| Đổi tốc độ/cân bằng lùi | `AGV_REVERSE_SPEED`, hai trim lùi |
| Đổi chỉnh line lùi xe02 | `AGV_REVERSE_CORRECTION_SPEED` và `AGV_REVERSE_CORRECTION_DELTA` |
| Điều chỉnh bắt lại vạch sau000 | Thời gian/PWM recovery trong Config.h và cờ nhận line bên xe01; tách khỏi tuning cua khi còn Following |

Với xe01, giảm `AGV_BENCH_MOTOR_SPEED` xuống dưới210 phải giảm cả `AGV_BENCH_GENTLE_OUTER_SPEED` cho phù hợp, nếu không static_assert sẽ chặn build. Mọi duty sau trim phải nằm trong0…255. Gentle phải giữ cả hai kênh>0 và chênh duty đúng phía sau trim; host fast-line hiện còn kiểm chênh thực tế≥40PWM và mỗi kênh không vượt duty đi thẳng. PWM không thể chứng minh bánh còn quay dưới tải.

Thay từng nhóm thông số và ghi lại trước/sau để dễ đối chiếu. Sửa profile là cấu hình compile-time: cần build/nạp lại đúng xe; chỉnh trên dashboard không tự thay các macro này. Cấu hình nguồn không chứng minh ảnh đang nạp trên ESP32 đã giống nhau.

## Build/nạp đúng xe

| Xe | Môi trường PlatformIO | Build ID hiện tại trong source |
| --- | --- | --- |
| 01 | `esp32dev_two_robot_bench_01` | `20261019-robot01-side-recovery-v3` |
| 02 | `esp32dev_two_robot_bench_02` | `20261017-robot02-mutex30-exitfix-v4` |

Chạy từ gốc workspace nếu PlatformIO đã nằm trong PATH:

```powershell
pio run -d firmware/NCKHAGV/NCKHAGV -e esp32dev_two_robot_bench_01 -t upload
pio run -d firmware/NCKHAGV/NCKHAGV -e esp32dev_two_robot_bench_02 -t upload
```

Mỗi lần chỉ chạy dòng của xe đang cắm. Khi có nhiều cổng, thêm `--upload-port COMx` với cổng đã xác định. Khi `pio` không có trong PATH, thay `pio` bằng `& "C:\Users\DELL\.platformio\penv\Scripts\platformio.exe"`. Lệnh bỏ `-t upload` chỉ build. Cả hai profile đều lấy `SharedRobotConfig.h`; sửa file chung cần kiểm tra lại cả hai profile.
