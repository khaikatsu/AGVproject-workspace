# ADR-050 — Vị trí và tốc độ encoder xe 01

Cập nhật17/10/2026: [ADR-068](ADR-068-encoder-mutex-tail-clearance.md) thay giới hạn “encoder không nhả mutex” cho hai profile sau khi người dùng cung cấp kích thước/vùng đọc/biên mutex và xác nhận161xung/100cm. Các quyết định hiển thị odometry bên dưới vẫn giữ.

Ngày: 14/10/2026. Quyết định theo yêu cầu người dùng; triển khai phần mềm, nghiệm thu vật lý còn mở.

`SOURCE`: encoder ở hai bánh trước xe 01; sketch `C:/Users/DELL/Downloads/testencoder (1).ino` dùng trái GPIO16/phải GPIO4, INPUT_PULLUP/RISING, lọc cạnh >300 µs, hệ số thử 161 xung/100 cm. Người dùng yêu cầu tích hợp firmware01, vị trí liên tục Core test 01 và tốc độ trên web. Hệ số chưa được xác nhận là số đo hiệu chuẩn.

`OBSERVED`: firmware dùng RFID/line; GPIO0 dành cho còi, thứ tự chân line khác sketch. Hai OUT không đo chiều quay độc lập; trước thay đổi chưa đọc encoder.

## Quyết định

- Chỉ profile `esp32dev_two_robot_bench_01` bật hai encoder; kiểm tra trùng chân lúc compile. Giữ motor/line/RFID/PWM/chốt an toàn hiện tại. Không ghép BOOT/Serial chạy tự do của sketch.
- ISR đếm raw ticks, khóa giữa ISR/vòng điều khiển. Quãng đường chặng chỉ cộng lúc navigator bám line tiến/lùi; chờ/STOP/pivot không tiến marker. Chiều lùi lấy từ navigator. Raw ticks không reset; distance chặng reset khi đổi RFID/route/session.
- Topic `agv/01/odometry`, JSON schema 1/QoS0/không retained, mỗi200 ms. Queue một mẫu, bỏ mẫu>500 ms hoặc khác connection session, xóa queue khi reconnect/Core restart. Worker gửi telemetry/diagnostic trước odometry. Diagnostic thêm encoderSupported/encoderBootId/encoderCoreSessionId, dùng session/uptimeMs hiện có để gắn phiên.
- Chỉ Core mode `robot01` (menu 1) dùng odometry hiển thị; cần telemetry mới/RFID hợp lệ/xe khỏe, đúng Core/boot/connection session, seq/uptime/route/index/from/to. Không đổi lastSeen/node/task/mutex/phục hồi, không checkpoint số đo.
- Distance = trung bình xung hai bánh × 100/161; speed = distance trong cửa sổ ≥200 ms/thời gian. Map test 01 dùng tọa độ cm; tiến độ = distance/chiều dài đoạn thẳng, giới hạn98% trước RFID đích. Xung không xác nhận hoàn thành hoặc rời vùng.
- Web có speed cm/s ở đầu/bên dưới bản đồ và dòng xe 01; marker nét đứt là ước tính, vòng nhỏ giữ RFID gần nhất. Animation180 ms nối hai mẫu đã nhận, không dự đoán vượt mẫu cuối. Sau1000 ms hoặc mất Web/Core/MQTT/storage lỗi/robot lỗi, speed `—`, dừng và giữ marker cuối có nhãn dữ liệu cũ. RFID hoặc mẫu mới hợp lệ cập nhật tiếp.
- WAIT/hủy giữa chặng giữ vị trí ước tính cuối; mẫu dừng tốc độ0 không chứng minh xe ở RFID mốc. Node trong bảng/lịch sử vẫn là RFID, và estimate không cấp quyền cho chặng mới.

## Kiểm chứng và giới hạn

Runner `scripts/test-encoder.ps1`, build PlatformIO01/02, native regression, gateway tests, `node tests/encoder.integration.js --browser`. Fixture broker/Core/map/counter/DB/cổng riêng kiểm tra sai session/seq/chặng/schema, saturate, RFID mới, STOP/pivot/lùi, stale/lỗi/offline và Edge desktop/mobile. Kết quả thực tế ở sổ yêu cầu/báo cáo kiểm thử; không coi fixture là nghiệm thu xe thật.

`OPEN`: hiệu chuẩn hai bánh qua nhiều lượt100 cm, mức điện áp OUT, mất/nhiễu xung khi motor chạy, trượt bánh, sai số RFID đọc sớm và ảnh đã nạp. Encoder chưa dùng PID motor hoặc nhả mutex. [Hợp đồng](../../contracts/robot01-odometry.md).
