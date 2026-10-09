# ADR-026 — Tự phục hồi lỗi và mutex khi RFID trạm được đọc mới

Ngày 09/10/2026. Trạng thái: IMPLEMENTED trong mã, chưa nạp Core/firmware mới lên hệ thống thật.

## Bối cảnh

`SOURCE`: người dùng muốn xe gửi telemetry khi đã trở lại bình thường để Core tự gỡ lỗi; mutex của xe lỗi cần được trả để tránh khóa hệ thống. Hai ảnh người dùng cho thấy 01/02 mất telemetry, ca `CLOSING`, quyền M2/M3 còn giữ. Snapshot Core đọc tại thời điểm chẩn đoán cho thấy gateway–Core–broker vẫn kết nối nhưng hai xe `OFFLINE`.

`OBSERVED`: firmware vẫn phát telemetry pipe khi đang `AGV_SAFE_STOP`; trường node có thể là RFID đã lưu. LWT, ACK và node cũ không chứng minh toàn bộ xe đã rời mutex. Diagnostic hiện có `state/error/motor/taskActive/wifi/mqtt`, nhưng chưa cho biết lần đọc RFID gần nhất thực sự cách hiện tại bao lâu.

## Quyết định

- Chỉ bật tự phục hồi trong `AGV_TWO_ROBOT_TRIAL=1` với 01→S1, 02→S2. Core vẫn giữ quyền owner ngay khi xe lỗi/offline; xe lỗi được loại khỏi hàng đợi xin vùng.
- Firmware NCKHAGV thêm `rfidAgeMs` vào diagnostic JSON, bằng thời gian từ lần RC522 đọc được **thẻ đang nằm dưới đầu đọc** trong cùng phiên MQTT. Không có số đo thì trường này là `null`. Telemetry pipe và command giữ v0.
- Core chỉ đối soát tự động khi nhận telemetry mới ở đúng trạm và diagnostic cùng node với `rfidAgeMs ≤250 ms`, MQTT/Wi‑Fi nối, motor `STOP`, hai duty PWM bằng 0, không task hoạt động hay đường/đích Core còn chạy. Mỗi vùng còn giữ phải cách trạm ít nhất 50 cm theo cả đường ngắn nhất **bỏ chiều cạnh** và khoảng cách thẳng giữa tọa độ map đã đo; thiếu đường hoặc khoảng cách nhỏ hơn thì giữ quyền. Sau khi đủ điều kiện, Core trả đúng grant ID và cấp quyền cho xe chờ kế tiếp.
- Với lỗi mạng (`NETWORK_UNAVAILABLE`, `WIFI_TIMEOUT`, `MQTT_TIMEOUT`), Core đợi ít nhất 300 ms sau diagnostic đầu tiên đủ điều kiện rồi gửi `RESET_FAULT` tối đa một lần, để lệnh `WAIT` khi reconnect được firmware xử lý trước. Firmware vẫn tự kiểm tra line, RFID, mạng và chốt chuyển động trước khi nhận reset. Core chỉ đổi `ERROR→IDLE` sau diagnostic mới `AGV_IDLE/NONE/STOP`; lỗi cảm biến/chuyển động khác không được tự gửi reset.
- Task đã gián đoạn không tự chạy lại. Nếu thiếu diagnostic mới, firmware cũ, RFID không đọc lại, xe chưa về trạm hoặc một vùng không đủ khoảng cách, luồng xác minh thủ công `confirm-clear` và `clear-error` vẫn còn.

## Kiểm chứng và giới hạn

Core C++ build và unit PASS với: LWT giữ M2; RFID M2 không trả quyền; telemetry S1 một mình không trả quyền; diagnostic dừng và RFID mới ở S1 trả M2; reset lỗi mạng một lần; diagnostic khỏe mới xóa lỗi; `LINE_LOST` không tự reset. Đây là fixture logic, chưa chứng minh vị trí/thân xe thật.

Fixture MQTT riêng `tests/two-robot-recovery.integration.js` đã tạo nhưng không chạy xong trong sandbox: lệnh `net.connect` tới listener loopback trả `EACCES`. PlatformIO build firmware cũng bị chặn ghi vào `C:\Users\DELL\.platformio`; chưa nạp hai ESP32. Trước khi thay Core đang chạy và nạp firmware, cần chạy fixture MQTT, build hai env bench và thử bàn: quét thẻ S1/S2 liên tục, đưa thẻ ra khỏi đầu đọc, ngắt mạng ở trong vùng và ở trạm, kiểm tra motor dừng cùng quyền mutex. Chưa xác nhận tính tin cậy của RC522 và biên khoảng cách trên xe thật.
