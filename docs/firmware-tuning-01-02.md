# Bảng chỉnh thông số xe 01 và xe 02

## 23/10/2026 — Bộ thông số tích hợp vào thư mục main

Nguồn giá trị: bản lưu cuối `output/turn180-confirm-both-20261022/staged` của workspacechat, theo lựa chọn người dùng. Chuyển giá trị hợp lệ; giữservo/ID/mạng/hiệu chuẩn củađích. Xe02 giữ `AGV_BENCH_MOTOR_SPEED=160` và `AGV_RIGHT_TURN_CAPTURE_ENABLED=1` hợp lệ củađích vì bảnlưu thiếu hai khai báo này, có macro `AGV_BENCH_MOTOR_SPEED_ENABLED` khôngđượcđọc và dòngPWMphải trùng. Không sao chép các dòng lỗi đó hoặc vô hiệu hóa chức năng hiệncó.

| Thông số | Xe01 | Xe02 |
| --- | ---: | ---: |
| PD Kp / Kd | 60 / 30 | 65 / 25 |
| PD base / chu kỳ | 125 /10ms | 125 /15ms |
| Đi thẳng dự phòng / trimENA / trimENB | 255 /0 /0 | 160 /0 /0 |
| PWM bắtline trái / phải | 220 /220 | 200 /200 |
| Quay tại node / trimENA / trimENB | 220 /0 /0 | 200 /+5 /+10 |
| G1 PWM trướctrim | 190 | 170 |
| Xác nhận line sau quay180 | 100ms | 100ms |
| Lùi thẳng / trimENA / trimENB | 170 /+20 /−40 | 135 /+15 /−40 |
| Lùi correction / delta | 150 /30 | 130 /50 |

Đi thẳng dự phòng không phải tốc độPD. ENA/ENB lùi thẳng tương ứng01=190/130,02=150/95; quaynode02=205/210, riêngG1=175/180 sautrim. `TURN_AROUND_CONFIRM_MS=100` là thời gian STOPxácnhận sau khi quay đủnhánh, khôngphải thời gianquay; chỉ thêm khả năng chỉnh riêng, không rútngắn100ms vốn có. Các mục bên dưới là hồ sơ cũ. Chi tiết trước→sau/backup ở báo cáo tích hợp trong workspacechat; chưa nạp/thửxe thật.

Cập nhật22/10/2026 theo source hiện tại: xe01 profile02-tuning-v14 dùng bộ thông số mới người dùng gửi, PD0..255; xe02 giữ turn180-v13 và tuning riêng đang có. Nền firmware do người dùng gửi đã có các ngoại lệ được ghi tại [ADR-075](decisions/ADR-075-sync-validated-two-robot-firmware.md), nên không còn khớp nguyên bản61/61 file. Bảng này không thay thế phép đo trên xe.

## Mở đúng file

| Muốn chỉnh | Nơi chỉnh |
| --- | --- |
| PD, PWM, trim, capture riêng xe01 | [Robot01Config.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/Robot01Config.h) |
| PD, PWM, trim, capture riêng xe02 | [Robot02Config.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/Robot02Config.h) |
| Encoder, thời gian chặng/RFID và dải pin chung | [SharedRobotConfig.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/SharedRobotConfig.h) |
| Hiệu chuẩn ADC pin riêng từng xe | [BatteryCalibration.h](../firmware/NCKHAGV/NCKHAGV/src/profiles/BatteryCalibration.h) |
| Thời gian recovery, GPIO và giới hạn chung | [Config.h](../firmware/NCKHAGV/NCKHAGV/src/Config.h) |

`L/R` là kênh phần mềm ENA/GPIO14 và ENB/GPIO25. Tên bánh trong comment cũ có thể khác kênh phần mềm; không suy ra dây vật lý nếu chưa kiểm trên xe. Pattern theo thứ tự bit GPIO39/GPIO34/GPIO35.

## PD đi tiến

PD chỉ được gọi trong Following đi tiến sau khi navigator đã cấp quyền và không có hazard. STOP,000,lùi,pivot,tìm line, mất grant hoặc đổi chặng đều reset lịch sử PD.

| Macro | Xe01 | Xe02 |
| --- | ---: | ---: |
| `AGV_LINE_PD_ENABLED` | 1 | 1 |
| `AGV_LINE_PD_BASE_SPEED` | 125 | 125 |
| `AGV_BENCH_LINE_KP` | 80 | 80 |
| `AGV_BENCH_LINE_KD` | 25 | 25 |
| `AGV_LINE_PD_MIN_RUNNING_PWM` | 0 (mặc định) | 0 (mặc định) |
| `AGV_LINE_PD_MAX_RUNNING_PWM` | 255 (mặc định) | 255 (mặc định) |
| `AGV_LINE_PD_CONTROL_MS` | 10 ms | 10 ms |
| `AGV_LINE_PD_RESET_GAP_MS` | 100 ms | 100 ms |
| `AGV_LINE_PD_STEERING_SIGN` | +1 | +1 |

Với mẫu đầu hoặc sau reset, bỏ D. Công thức trong source dùng error `100=-2`, `110=-1`, `010/101/111=0`, `011=+1`, `001=+2`, sau đó áp steering sign. Cả hai xe clip0…255 khi pattern hợp lệ. Mẫu000 trả không hợp lệ và motor phải STOP qua luồng điều khiển.

Duty PD tĩnh dưới đây là sau trim, chưa gồm D khi pattern vừa đổi:

| Pattern | Xe01 ENA/ENB | Xe02 ENA/ENB |
| --- | ---: | ---: |
| 010,101,111 | 135 / 125 | 135 / 125 |
| 110 | 215 / 45 | 215 / 45 |
| 100 | 255 / 0 | 255 / 0 |
| 011 | 55 / 205 | 55 / 205 |
| 001 | 0 / 255 | 0 / 255 |
| 000 | 0 / 0 | 0 / 0 |

## PWM fallback, quay và lùi

| Macro | Xe01 | Xe02 |
| --- | ---: | ---: |
| `AGV_BENCH_MOTOR_SPEED` | 160 | 160 |
| `AGV_BENCH_LEFT_SPEED_TRIM` | +10 | +10 |
| `AGV_BENCH_RIGHT_SPEED_TRIM` | 0 | 0 |
| `AGV_BENCH_TURN_SPEED` | 220 | 230 |
| `AGV_BENCH_TURN_LEFT_SPEED_TRIM` | 0 | 0 |
| `AGV_BENCH_TURN_RIGHT_SPEED_TRIM` | 0 | 0 |
| `AGV_G1_TURN_AROUND_PWM` | 190 | 200 |
| `AGV_LEFT_TURN_CAPTURE_PWM` | 220 | 220 |
| `AGV_RIGHT_TURN_CAPTURE_PWM` | 220 | 220 |
| `AGV_REVERSE_SPEED` | 170 | 180 |
| `AGV_REVERSE_LEFT_SPEED_TRIM` | +15 | +10 |
| `AGV_REVERSE_RIGHT_SPEED_TRIM` | −40 | −40 |
| `AGV_REVERSE_CORRECTION_SPEED` | 150 | 150 |
| `AGV_REVERSE_CORRECTION_DELTA` | 30 | 30 |

Duty thực tế chính:

| Trạng thái | Xe01 ENA/ENB | Xe02 ENA/ENB |
| --- | ---: | ---: |
| Tiến fallback thẳng | +170 / +160 | +170 / +160 |
| Pivot thường | 220 / 220 | 230 / 230 |
| Lùi thẳng | −185 / −130 | −190 / −140 |
| Chỉnh lùi, chậm ENA | −135 / −110 | −130 / −110 |
| Chỉnh lùi, chậm ENB | −165 / −80 | −160 / −80 |

Hai profile bật reverse gentle, nên chỉnh lùi vẫn giữ cả hai kênh quay. Capture rẽ trái/phải dùng PWM riêng theo hướng; settling sau capture cap duty bằng đúng PWM capture của hướng đó. Quay đầu G1 chỉ giảm PWM ở thao tác TurnAround tại G1, không đổi rẽ thường ở node khác.

## Mất line và bắt lại line

| Thông số | Giá trị |
| --- | ---: |
| Grace giữ task xe01/02 | 7000 / 7000 ms |
| Quét motor tối đa | 600 ms |
| Dừng trước quét | 100 ms |
| Xung/nghỉ quét | 40 / 60 ms |
| Xác nhận line | 100 ms và ít nhất3 mẫu |
| Nhớ hướng chỉnh gần nhất | 750 ms |
| Timeout chặng | 15000 ms |

Khi gặp000, motor STOP ngay. Nếu còn hướng hợp lệ và lượt tìm chưa dùng, xe chỉ phát xung pivot trong tối đa600ms; phần còn lại của grace7s là chờ đứng yên để xác nhận line. Hết grace mới chốt `LINE_LOST`. Lỗi đã chốt không tự mở khi line trở lại.

Hai profile bật `AGV_LINE_RECOVERY_TRACK_CONFIRM`: Following tiến phục hồi sau000 chỉ nhận010/011/110 ổn định100ms/ít nhất3 mẫu mới, gap≤50ms.001/100/000/101/111 xóa liên tục xác nhận. Lùi vẫn cần010. KhiFollowing bình thường,001/100 vẫn dùngPD chỉnh hướng.

Capture sau rẽ giữ nhánh phiếu010 cũ và nhận010/011/110 ổn định100ms sau bằng chứng đã rời line cũ.001/100 không khởi độngCapture hoặc xác nhận đã bắtline; chúng chỉ có thể chứng minh rời vạch cũ. Trong lúc xác nhận line cómắtgiữa motorSTOP; quét motor≤600ms, tổng chờ≤1500ms.111/101 không cho phép chuyển sang Following. Mất mạng, sonar, STOP hoặc thiếu grant vẫn ưu tiên dừng.

## Quay180° và nhịp quan sát line

Quay180° có `AGV_TURN_AROUND_CAPTURE_ENABLED=1` trongSharedRobotConfig. `TURN_LINE_SAMPLE_MS=2` tạiConfig là khoảng quan sát mục tiêu/tối thiểu, không phải chu kỳ thời gian thực bảo đảm; xem log `[TURN180] confirmed ... max_gap_ms=...` trên xe. Hoãn đọcRFID khi quay180/xác nhận; nhánh cuối010/011/110 làmSTOP rồi đợi100ms/≥3 mẫu mới/gap≤50ms,001/100/101/111 không xác nhận. Đếm nhánh vẫn theo map và cần000 giữa các nhánh; deadline quay2500ms gồm cả xác nhận. Sau quaySTOP một vòng/capPWM180 trong200ms đầuFollowing. Không đổiPWM quay trong lượt sửa; [ADR-077](decisions/ADR-077-turn180-fast-stop-confirm.md).

## Pin 3S

Hai profile dùng dải tổng áp10,2–12,9V, full12,6V. Hệ số ADC hiện tại:

- Xe01: `11.9 / 2.421`.
- Xe02: `12.5 / 2.388`.

Đổi ESP32, cầu chia áp hoặc bộ pin phải đo lại đồng thời điện áp đồng hồ và `batteryAdcMv`. Khóa tổng áp không thay thế BMS bảo vệ từng cell.

## Build đúng xe

| Xe | Environment | Build ID source |
| --- | --- | --- |
| 01 | `esp32dev_two_robot_bench_01` | `20261022-robot01-profile02-tuning-v14` |
| 02 | `esp32dev_two_robot_bench_02` | `20261022-robot02-turn180-v13` |

Build từ gốc workspace:

```powershell
& "C:\Users\DELL\.platformio\penv\Scripts\platformio.exe" run -d firmware/NCKHAGV/NCKHAGV -e esp32dev_two_robot_bench_01
& "C:\Users\DELL\.platformio\penv\Scripts\platformio.exe" run -d firmware/NCKHAGV/NCKHAGV -e esp32dev_two_robot_bench_02
```

Chỉ thêm `-t upload --upload-port COMx` sau khi build/test xanh và đã xác định đúng xe/cổng. Không commit `LocalNetworkConfig01.h` hoặc `LocalNetworkConfig02.h`.
