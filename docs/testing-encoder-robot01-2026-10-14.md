# Kiểm thử encoder xe 01 — 14/10/2026

Phạm vi theo yêu cầu: tích hợp sketch encoder hai bánh trước vào firmware 01, vị trí ước tính liên tục trong Core mode `robot01` và tốc độ cm/s trên web. [ADR-050](decisions/ADR-050-robot01-encoder-position-speed.md), [contract](../contracts/robot01-odometry.md).

## Kết quả phần mềm

| Phần | Kết quả | Bằng chứng |
| --- | --- | --- |
| Adapter encoder/codec thật, profile 01 | PASS | GPIO16/4, lọc cạnh >300 µs, 161 xung →100 cm, speed, STOP/pivot, RFID rebase, lùi, wrap counts/time, đổi session, giới hạn buffer |
| Adapter profile 02 | PASS | Không attach interrupt khi encoder disabled |
| Unit encoder Core | PASS | Schema/seq/session/chặng; 98% trước đích; stale/error/mode isolation; không đổi task/mutex/durable snapshot |
| Core build + unit hiện có | PASS | `.qa/encoder-build/agv_core.exe`, `.qa/encoder-build/core_tests.exe` |
| PlatformIO ESP32 01/02 | PASS | `esp32dev_two_robot_bench_01`, `esp32dev_two_robot_bench_02` |
| Native regression 01/02 | 52/52 PASS | Parser/NODE_ORDER/line-loss/mutex/route/pass-through/STOP/watchdog |
| Gateway/security | 18/18 PASS | HTTP/WS/delta/session/checkpoint/storage/auth |
| MQTT/Core/gateway/Edge desktop-mobile | PASS | `.qa/encoder-test-6pqzhv/verification.json`, screenshots và layout.json; dừng giữa chặng giữ marker, desktop cuộn xem đủ footer |
| Dashboard regression hai xe | PASS | `.qa/browser-test-LK9lZL`; routes/history/stale/reconnect/editor/shift/obstacle/mobile |
| Straight-through regression | PASS | `.qa/straight-through-test-wTRnaK/verification.json`; contention/offline giữ đúng token, RFID từng node, turn/legacy fallback |

Fixture encoder đi từ C4 tới M4, nhận distance/speed mà node vẫn C4/task dispatched/M4 vẫn owner01; distance vượt đoạn chỉ giới hạn marker 98%. Kiểm tra duplicate, sai Core/MCU connection/route, số âm và kiểu dữ liệu sai; mẫu cũ không tăng tuổi dữ liệu mới. Qua RFID M4 đổi mốc sang M4→GS2. Phiên kết nối mới bỏ mẫu cũ, lùi K5→G5 giữ hướng đầu, lỗi/offline loại speed/estimate. Mẫu quá 1000 ms thì web hiện `— cm/s` và giữ marker; WAIT/hủy giữa đoạn không làm marker nhảy về RFID mốc. Browser không có pageerror, viewport 390×844 không tràn ngang; desktop footer không bị root overflow cắt.

Độ trễ MQTT fixture đến WebSocket trong các lần chạy là khoảng vài chục ms (chi tiết từng mẫu trong verification.json), không phải phép đo Wi-Fi/encoder xe thật. Tất cả broker/Core/DB/map/counter/port của fixture đều riêng; không phát command sang broker robot vận hành. Kiểm thử loopback/PlatformIO cần quyền truy cập socket/cache trên Windows, đã chạy thành công sau sandbox hạn chế được xử lý bằng escalation.

Kiểm tra riêng tài liệu encoder: 6 liên kết nội bộ và JSON ví dụ PASS. `scripts/check_workspace.py` toàn repo còn lỗi liên kết cũ tới cấu trúc src trước di chuyển, artifact audit cũ đã thiếu và README vendor `.pio`; không có lỗi liên kết ở tài liệu encoder mới. Python hiện chưa có jsonschema để chạy validation schema tùy chọn; parser C++ và codec đã được kiểm thử trực tiếp.

## Artifact và vận hành

- Core mới: `build/agv_core.exe`, SHA256 `93A7899039C7A5E4568D71ACB81ACEEB36602237C93CAFD1E454A0CDE5DE2A67`; bản trước giữ ở `.qa/encoder-build/previous_operational_core.exe`. Copy chỉ sau kiểm tra binary đích không chạy; hash trùng binary fixture đã build.
- Firmware 01: `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_01/firmware.bin`, buildId `20261014-robot01-encoder`, SHA256 `0D900F7D32ECBB5170303F869556CE3ED1CE0DA58079393C4919E9E5D65F8A76`. RAM 57.204 byte, Flash 953.973 byte.
- Bản giữ ổn định ở `build/robot01-encoder/`: firmware.bin/firmware.elf/bootloader.bin/partitions.bin và manifest.json để ảnh build sau không ghi đè hồ sơ encoder này; chưa upload.
- Khi kiểm tra runtime chỉ broker1883 đang nghe; Core8080/gateway3000 đang dừng. Chưa khởi động Core vận hành hoặc upload firmware trong lượt này.
- Sau nạp đúng firmware01: mở Core bằng F5/chọn menu **1**, chạy `npm start` trong AGVwebserver, mở web và quét RFID hợp lệ trước giao việc. Hệ số thử ở `src/profiles/Robot01Config.h`; không đổi hệ số trực tiếp bằng web.

Runner từ gốc: `./scripts/test-encoder.ps1`, `node tests/encoder.integration.js --browser`. Playwright/Edge cấu hình theo [tests/README.md](../tests/README.md). Build firmware dùng PlatformIO `run -e esp32dev_two_robot_bench_01 -e esp32dev_two_robot_bench_02`; native dùng `test -e native_robot01_bench -e native_robot02_bench`.

## Chưa nghiệm thu

Chưa nạp ảnh mới hoặc đo trên xe thật. Hệ số 161 xung/100 cm lấy từ sketch, cần nhiều lượt đo và đối chiếu hai bánh; sai số do trượt/nhiễu/mốc RFID đọc sớm vẫn còn. Encoder một OUT không xác nhận độc lập chiều quay. Node/task/mutex tiếp tục theo RFID và chốt hiện có; kết quả trên không chứng minh độ chính xác vị trí/tốc độ vật lý.
