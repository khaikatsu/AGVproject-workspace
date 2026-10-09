# ADR-013: Nạp sẵn 26 UID RFID cho hai xe

Ngày: 03/10/2026. Trạng thái: triển khai phần mềm, chưa đo RC522 trên xe thật.

## Bối cảnh

Người dùng cung cấp UID của 26 thẻ tương ứng mọi node trên map v3 và yêu cầu firmware nhận ngay node khi quét. Trước đó sketch chỉ biết node sau khi gõ `learn NODE` cho từng ESP32 và lưu mapping theo key tên node trong Preferences. Nếu giữ mapping cũ, một UID đã học sai có thể ghi đè danh sách mới khi khởi động.

## Quyết định

Đặt 26 cặp `{node, UID}` trong sketch, dùng chung cho xe 01 và 02. Firmware nạp bảng vào RAM trước khi đọc NVS. Mapping thủ công mới dùng key `uid-NODE` trong namespace `agv-rfid`; `learn NODE` ghi đè một node, `forget NODE` bỏ ghi đè và trở về UID mặc định. Các key NVS cũ chỉ là `NODE` bị bỏ qua, không xóa; key `last-cmd` của chống phát lại command vẫn được giữ. Nếu override mới sai hoặc trùng UID, bỏ toàn bộ override và dùng bảng mặc định.

Boot vẫn là UNKNOWN đến khi RC522 đọc thẻ thật. Firmware gửi tên node qua telemetry pipe v0 hiện có, nên Core/gateway/web không đổi payload hay bảng node. Hai board chỉ khác `ROBOT_ID`; trạm riêng vẫn 01→S1, 02→S2. Các UID và cách kiểm tra ở [README firmware](../../firmware/AGV_Connection_Test/README.md).

## Kiểm chứng và giới hạn

Host probe đối chiếu cả 26 cặp với danh sách người dùng, quét S1/S2 không cần `learn`, bỏ qua mapping NVS cũ, kiểm tra override/forget, boot UNKNOWN và giao thức Core. Kiểm thử tích hợp dùng Core/MQTT riêng. Chưa nạp ESP32 hoặc xác minh UID/điện áp/độ tin cậy RC522 trên xe thật. Thẻ đặt sai vị trí vật lý vẫn cho node sai; cần đối chiếu nhãn thẻ với vị trí trước vận hành.

## Hiệu chỉnh UID ngày 08/10/2026

Người dùng báo UID cũ của hai node ghi sai và cung cấp giá trị thay thế: `C3 = A79A0695`, `K1 = 57D42295`. Đã cập nhật bảng mặc định trong cả hai firmware, README và fixture. Với sketch Arduino có override `uid-C3` hoặc `uid-K1` trong NVS, cần chạy `forget C3` / `forget K1` sau khi nạp để dùng UID mặc định mới. Đây là hiệu chỉnh bảng UID; không đổi giao thức hay luồng định vị.
