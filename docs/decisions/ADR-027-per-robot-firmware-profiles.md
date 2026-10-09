# ADR-027 — Profile firmware riêng cho xe 01 và xe 02

Ngày 09/10/2026. Trạng thái: IMPLEMENTED trong mã, chưa nạp hai xe.

## Bối cảnh

`SOURCE`: người dùng hỏi ảnh nào cần nạp cho từng xe và muốn tách thông số để chỉnh hai xe độc lập.

`OBSERVED`: hai môi trường PlatformIO đã có tên riêng nhưng cùng lấy phần lớn tốc độ, trim, timeout và hiệu chuẩn pin từ `src/Config.h`; xe 02 được chọn bằng cờ `AGV_ROBOT_2_BENCH`. Cấu hình mạng cục bộ chỉ có một `LocalNetworkConfig.h` dùng chung.

## Quyết định

- `esp32dev_two_robot_bench_01` chọn `src/profiles/Robot01Config.h`, khóa MQTT ID `01`; `esp32dev_two_robot_bench_02` chọn `Robot02Config.h`, khóa ID `02`. Mỗi file chứa PWM cơ sở, trim trái/phải, PWM quay/chỉnh line, thời gian xác nhận mất line, timeout đoạn, dwell RFID và hệ số đo pin. Giá trị ban đầu giữ đúng hai môi trường bench trước khi tách.
- Sơ đồ chân, bảng UID RFID, khóa điều khiển và điều kiện dừng vẫn trong `Config.h` dùng chung. Kiểm tra biên duty ở compile time; chọn đồng thời hai profile hoặc chọn profile ngoài chế độ bench bị từ chối.
- Cấu hình mạng/mật khẩu có thể tách bằng `LocalNetworkConfig01.h` và `LocalNetworkConfig02.h`, đều bị ignore. Nếu file riêng chưa có, mỗi môi trường dùng `LocalNetworkConfig.h` cũ để không đổi mạng đang cấu hình. File `.example.h` chỉ là mẫu, không chứa bí mật.
- PlatformIO tạo hai ảnh `.pio/build/esp32dev_two_robot_bench_01/firmware.bin` và `.pio/build/esp32dev_two_robot_bench_02/firmware.bin`; nạp bằng `run -e ... -t upload --upload-port COMx` để PlatformIO đặt cả bootloader, partition và ứng dụng đúng offset.

## Kiểm chứng và giới hạn

Host g++ đã biên dịch profile 01/02 riêng và kiểm tra ID cùng giá trị tuning được chọn. Build PlatformIO trong môi trường hiện tại bị `PermissionError` khi ghi `C:\Users\DELL\.platformio\platforms.lock`; chưa xác nhận hai ảnh ESP32 mới và chưa nạp bo. Cần xác định đúng cổng COM từng xe và thử trên giá kê trước khi cho motor chạy. Việc tách profile không xác nhận PWM 230, hướng quay hoặc hệ số pin đã phù hợp cả hai xe thật.

