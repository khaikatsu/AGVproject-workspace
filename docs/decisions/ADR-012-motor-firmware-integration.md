# ADR-012: Tích hợp driver L298N vào sketch MQTT/RFID

Ngày: 03/10/2026. Trạng thái: áp dụng cho kiểm thử phần mềm; vận hành motor thật chờ kiểm thử bàn.

## Bối cảnh

Người dùng bổ sung `driver.cpp`, `TestConfig.h` và bảng chân toàn xe để đồng bộ firmware với Core. Driver cũ là chương trình Arduino độc lập có `setup/loop`, bật motor bằng nút BOOT, dùng API `ledcSetup/ledcAttachPin` của ESP32 Arduino core cũ và phụ thuộc `Config.h` chưa có trong workspace. Bảng chân mới chỉ định GPIO0 cho còi, GPIO21 cho servo 3 và RC522 SS5, khác SS21 trong sketch trước đó. Bảng là SOURCE do người dùng cung cấp; dây và điện áp thật chưa được đo.

## Quyết định

Đặt `Config.h`, `TestConfig.h`, `driver.h`, `driver.cpp` cùng `.ino` trong một thư mục sketch Arduino IDE. Tạo `Config.h` theo bảng chân, dùng API LEDC pin-based của ESP32 core 3.x; chọn L298N theo `TestConfig`. Sketch vẫn dùng MQTT v0 và mutex JSON hiện tại, không đổi Core/gateway/topic/payload. Phần nhận MQTT chỉ xếp lệnh; vòng lặp cục bộ gọi driver. `STOP/WAIT`, hết heartbeat, mất MQTT/WiFi, sai thứ tự RFID hoặc tới mỗi RFID sẽ dừng motor và bỏ arm đoạn hiện tại. Nếu node hiện tại hoặc node kế là mutex, firmware xin grant và chỉ cho phép chạy sau khi có đủ token; chỉ release bằng thao tác đã xác minh toàn thân xe rời vùng.

Motor tắt khi boot và `ENABLE_AUTONOMOUS_MOTION=false` mặc định. Sau kiểm thử bàn có thể bật và dùng `motor arm` trên Serial cho **từng đoạn**; `motor off` dừng. Servo quét khi xe đứng, về 90° trước khi chạy; HC-SR04 chặn nếu vật cản <30 cm, mất echo hoặc dữ liệu cũ; line 000/101 dừng. Không dùng GPIO0 làm nút BOOT vì bảng chân dành nó cho còi. Pin/sạc/hàng/encoder chưa ghép vào điều khiển.

## Giới hạn trước khi chạy thật

- Bảng chân cần đối chiếu dây/mức điện áp trên xe. GPIO5/12/15 là chân strapping; ECHO phải hạ về mức ESP32.
- Driver line hiện chưa có bảng rẽ theo tuyến, khoảng dừng theo tốc độ/tải, hay bằng chứng thân xe rời mutex. RFID chỉ xác nhận vị trí đầu đọc. Vì vậy chưa cho phép tự chạy trọn `GOTO`, tự release zone hoặc tự bật motor khi nhận lệnh.
- `battery` và `charging_done` vẫn là dữ liệu test. Không kết luận pin 15% đo thật hoặc đã sạc điện.
- Kết quả host kiểm tra logic interlock và GPIO shim; MQTT/ca dùng broker/Core riêng. Chưa có ESP32 build sau tích hợp, upload, kiểm thử điện, phanh hoặc xe thật.

## Kiểm chứng

`scripts/test-firmware.ps1` chạy probe `.ino` và `driver_bench.cpp` với shim GPIO/PWM: motor tắt khi không được phép, line mất và siêu âm gần đều dừng. `node tests/firmware.integration.js` kiểm tra Core/MQTT v0 với sketch host. `node tests/shift.integration.js --sketch` kiểm tra hai sketch trong luồng ca. Log fixture dưới `.qa/`; không dùng DB/map/counter vận hành.
