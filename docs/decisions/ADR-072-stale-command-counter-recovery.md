# ADR-072 — Phục hồi lệch bộ đếm command mà không chạy lại lệnh cũ

Ngày 19/10/2026. Trạng thái: IMPLEMENTED trong source, chưa triển khai lên Core/robot vận hành.

## Nguồn và hiện trạng

`SOURCE`: người dùng gửi log Core lặp `STALE_COMMAND` xen kẽ `diagnostic binh thuong: tu xoa loi Core` và yêu cầu sửa.

`OBSERVED`: ESP32 lưu high-water `cmd_id` trong NVS và từ chối ID cũ không còn trong cache; Core lưu counter ở `command_sequence.json`. Khi Core chạy với file counter cũ/khác đường dẫn, lệnh bị từ chối không có ACK. Handler lỗi cũ lại phát một `STOP` ID kế tiếp, còn diagnostic khỏe tự xóa ERROR, tạo vòng phát lệnh thấp → `STALE_COMMAND` → tự xóa lỗi. ACK giả hoặc xóa NVS có thể làm lệnh cũ bị coi là mới nên không được dùng để phục hồi.

## Quyết định

- Diagnostic firmware v1 thêm `lastCommandId` là số nguyên 0…`INT_MAX`, bằng high-water đã lưu trong NVS. Trường chỉ dùng đồng bộ allocator; không phải ACK và không chứng minh hành động đã hoàn thành.
- `CommandIds` chỉ được tiến lên: khi thấy `lastCommandId` lớn hơn hoặc bằng ID kế tiếp, Core bỏ miền cũ và reserve bền vững block mới phía trên. Không giảm counter, không xóa NVS và không replay command đã bị từ chối.
- Khi nhận đúng lỗi `STALE_COMMAND`, Core xóa pending retry, giữ `AGV_ERROR` và không phát thêm `STOP` có khả năng vẫn thấp. Diagnostic `AGV_IDLE/NONE` không tự xóa riêng lỗi này.
- Sau khi diagnostic đã đồng bộ counter, người vận hành vẫn phải xác minh robot dừng/vị trí hợp lệ và gọi `clear_error`. `RESET_FAULT` mới dùng ID lớn hơn high-water; firmware tiếp tục kiểm tra route, phiên, line, mạng và motor trước khi mở chốt. Không tự phát lại GOTO/task cũ.
- Firmware cũ thiếu `lastCommandId` vẫn tương thích đọc diagnostic, nhưng không thể tự đồng bộ counter; cần nạp firmware mới cùng Core mới để phục hồi đầy đủ.

## Kiểm chứng và giới hạn

`VERIFIED` phần mềm: Core unit kiểm tra counter tiến qua restart, dừng retry/không phát STOP thấp, giữ lỗi qua diagnostic khỏe và chỉ gửi `RESET_FAULT` ID mới sau clear; toàn bộ `core_tests` PASS. Core build riêng PASS. PlatformIO build profile 01 và 02 PASS.

`OPEN`: chưa nạp hai ảnh firmware, chưa thay Core vận hành và chưa quan sát vòng phục hồi qua broker/robot thật. Việc nạp/restart phải thực hiện khi robot kê an toàn hoặc đang dừng; sau triển khai vẫn cần người vận hành xóa lỗi, không tự gửi GOTO để thử.
