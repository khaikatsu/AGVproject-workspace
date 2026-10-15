# Tích hợp HC-SR04 xe 01 — 15/10/2026

REQ-ROBOT01-SONAR-05, ảnh người dùng từ bản diagnostic: front active1, triggers225/242/258, edges451/484/517, pulse_us47487/47494/47489, tuổi0/0/31ms, result INVALID_PULSE. Rear inactive/triggers0 là đúng hướng. Có tín hiệu ECHO tới bộ đọc, nhưng xung47,5ms vượt hạn45ms của bản trước; không phải mẫu stale trong ảnh. Hạn mới60ms, cutoff tính khoảng cách vẫn30ms; không biến xung dài thành số cm, HIGH quá hạn vẫn lỗi. Regression đúng độ rộng log fail trước/PASS sau; 3 host probe PASS. Bản mới `20261015-robot01-sonar-47ms-fix`; chưa nạp/đo lại với vật ở30cm. Cần xác nhận log RANGE khi có vật và NO_TARGET_PULSE khi ngoài tầm trước chạy tuyến.

Xác minh cuối bản `20261015-robot01-sonar-no-echo-fix`: regression S1→GS1, HAL, directional host PASS; native01 27/27, native02 26/26 và build ESP32 01/02 PASS. RAM xe01 57300 byte, flash 956949 byte. Chưa nạp/thử lại xe thật.

REQ-ROBOT01-SONAR-03: người dùng báo xe tiến từ S1 tới GS1 dừng chuẩn bị xoay rồi ULTRASONIC_INVALID dù không có vật. Log mới nhất của lần lỗi là direct_goto 12:46:21 → ULTRASONIC_INVALID 12:46:24; diagnostic sau reboot có build directional-sonar và IDLE/NONE/UNKNOWN, không là trace lỗi cũ. Regression `tests/sonar-host/no_echo_gate.cpp` tái hiện xung no-target 38 ms bị chốt invalid ở 30 ms, fail trước sửa/PASS sau sửa; kiểm tra cùng tuyến/cùng ID tiếp tục pivot và vật thật 10 cm vẫn dừng, HIGH quá 45 ms vẫn lỗi. Bản mới `20261015-robot01-sonar-no-echo-fix` ở thư mục `.pio/build/esp32dev_two_robot_bench_01`; chưa nạp/đo trên xe, chưa khẳng định nguyên nhân vật lý duy nhất. Bộ đóng gói cũ bên dưới không tự cập nhật.

Cập nhật REQ-ROBOT01-SONAR-02: bản mới `20261015-robot01-directional-sonar` chỉ kích sau khi chuẩn bị/đang lùi hoặc dừng chờ cản sau, các trường hợp khác kích trước. Đổi hướng dừng motor, xóa echo/mẫu cũ, chờ yên 50 ms và chờ mẫu mới tối đa 200 ms. Chưa nạp/thử xe thật. Host scheduler/HAL, native 01 27/27 và 02 26/26, build ESP32 01/02 PASS. Ảnh mới tại `firmware/NCKHAGV/NCKHAGV/.pio/build/esp32dev_two_robot_bench_01/firmware.bin`; thư mục đóng gói/manifest của bản dual-sonar cũ bên dưới chưa cập nhật và không đại diện cho bản mới. [Chi tiết](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

## Bản đã chuẩn bị

FirmwareBuild `20261015-robot01-dual-sonar`, env `esp32dev_two_robot_bench_01`; bộ ảnh và Core đã kiểm thử ở `build/robot01-dual-sonar/`. `manifest.json` ghi hash SHA-256, chân, ngưỡng và kết quả kiểm thử; `uploaded=false`, `physical_test_verified=false`.

| Thiết bị | Chân ESP32 |
| --- | --- |
| HC-SR04 trước | TRIG12 / ECHO13 |
| HC-SR04 sau | TRIG0 / ECHO15 |
| Còi active LOW | GPIO21 |
| Servo nâng hạ theo ảnh | GPIO17; điều khiển servo vẫn chưa cấu hình |

Xe tiến dùng sonar trước, xe lùi dùng sonar sau. Khoảng cách ≤15 cm tạm dừng, giữ nguyên GOTO/task/grant; >20 cm hoặc hết echo với chân LOW cho phép tiếp tục khi các chốt khác hợp lệ. Khoảng 15–20 cm giữ dừng để tránh bật/tắt liên tục. ECHO kẹt HIGH và mẫu cũ gây lỗi cảm biến. Timeout LOW không phân biệt được dây ECHO đứt với ngoài tầm đo; cần kiểm tra cảm biến trước ca.

Core menu 1 ghi cảnh báo phía trước/sau và khoảng cách; web hiện cảnh báo chữ, tô đỏ cạnh và chấm đỏ trên lớp trên cùng. Chấm dùng tọa độ map cm và encoder cùng route/hướng, còn mới. Tọa độ là ước tính từ mốc xe cộng khoảng cách sonar; khoảng lệch vị trí gắn cảm biến, trượt bánh và quãng phanh cần đo trên xe.

## Kết quả phần mềm

- Firmware ESP32 profile01 build PASS; RAM 17,5%, Flash 72,9%.
- Native profile01 **27/27 PASS**, profile02 **26/26 PASS**. Ca mới kiểm tra tạm dừng lâu hơn timeout chặng vẫn giữ route rồi tiếp tục; STOP hủy tuyến đúng luồng cũ.
- `tests/sonar-host/test_main.cpp` cùng module cảm biến thật PASS: pinout, hai ISR độc lập, khoảng cách 15 cm, ngoài tầm LOW, kẹt HIGH và mẫu cũ.
- Core unit PASS; encoder core unit PASS, bao gồm từ chối chiếu vật cản từ route hoặc phía khác với mẫu encoder, cảnh báo/xóa và không lưu vào checkpoint.
- `tests/encoder.integration.js --browser` PASS trên Edge desktop/mobile với Core/broker/map/counter/SQLite riêng, gồm cảnh báo trước/sau, tọa độ lùi K5→G5, màu đỏ cạnh, chấm không bị RFID che, xóa cảnh báo, hồi quy task/mutex/encoder/session/offline. Artifact cuối: `.qa/encoder-test-vemfv5/`, ảnh `sonar-front.png`, `sonar-rear.png`, kết quả `verification.json`.

Kiểm tra workspace vẫn báo các link gãy cũ và thiếu thư viện JSON Schema; xem `tests/README.md`. Chưa flash hoặc thử xe thật.

## Dùng bản thử

Từ PowerShell tại gốc workspace, sau khi đã kiểm tra chia áp/chuyển mức ECHO 5 V xuống 3,3 V, dây còi GPIO21, GPIO15 không còn nối servo và boot GPIO0:

1. Kê bánh xe để thử cảm biến/motor; kiểm tra phía trước và sau đọc đúng Serial/diagnostic. Xác định đúng cổng USB của **xe 01** bằng `platformio device list`, rồi thay `COMx` bên dưới bằng cổng đó.
2. Nạp đúng profile:

   ```powershell
   & 'C:/Users/DELL/.platformio/penv/Scripts/platformio.exe' run -d firmware/NCKHAGV/NCKHAGV -e esp32dev_two_robot_bench_01 -t upload --upload-port COMx
   ```

3. Khi Core cũ đã dừng, chạy Core bản mới từ gốc workspace; ở menu chọn **1 — thử xe 01**:

   ```powershell
   ./scripts/run-core.ps1 -OutputDirectory build/robot01-dual-sonar
   ```

   Script giữ vị trí counter hiện có. Không xóa counter, không chạy hai Core cùng counter hoặc cùng cổng.

4. Khởi động gateway bằng `npm --prefix AGVwebserver start`, rồi mở `http://127.0.0.1:3000`. Nếu gateway đang chạy, tải lại trang để lấy JS/HTML mới.
5. Khi firmware đã xác nhận RFID/hướng và mọi chốt hợp lệ, thử đoạn ngắn: đưa vật cản phía di chuyển đến ngưỡng, xác minh hai duty về 0, cảnh báo đúng phía và cạnh đỏ; dời vật cản vượt20 cm hoặc ra ngoài tầm, xác minh đi tiếp **cùng route command ID**. Thử cả tiến và lùi, STOP và mất mạng. STOP/mất mạng/lỗi cảm biến có thể hủy hoặc chốt lỗi tuyến; dời vật cản không tự xóa các lỗi đó.

15 cm là ngưỡng **khoảng cách cảm biến đọc được**. Khoảng cách xe dừng thật còn phụ thuộc chu kỳ đo, tốc độ và quán tính; cần đo với tải thật trước nghiệm thu.
