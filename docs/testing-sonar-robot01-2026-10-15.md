# Tích hợp HC-SR04 xe 01 — 15/10/2026

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
