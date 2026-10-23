# ADR-072 — Phục hồi lệch bộ đếm command mà không chạy lại lệnh cũ

Trạng thái mới nhất23/10/2026: REQ-FIRMWARE-SERVER-SYNC-01 khôi phục producer `lastCommandId` và chống replay trên nền `NCKHAGV.rar`, theo duyệt người dùng. BuildID01/02 `20261023-robot0x-server-sync-v16`. Quyết định ghi đè nguyên bản21/10 bên dưới là lịch sử, không còn mô tả producer hiện tại.

## Bổ sung 23/10/2026 — Đồng bộ counter trên nền RAR

`OBSERVED`: RAR có nhánh bench chấp nhận ID thấp ngoài cache bằng cách hạ high-water rồi persist, và thiếu `lastCommandId`. `IMPLEMENTED`: phân loại Fresh/Duplicate/Conflict/Stale dùng chung mọi profile; duplicate chỉ ACK, conflict/stale không enqueue và không ghi NVS. `persistCommandId` chỉ cho ID lớn hơn high-water; diagnostic worker công bố giá trị đã persist, Core dùng callback `ensureAbove` hiện có. STOP/WAIT cũ vẫn có thể yêu cầu phanh ưu tiên trước kiểm ID, không chạy lại tác vụ. Không xóa/rewind counter Core hoặc NVS, không thay tuning/mạng. Kiểm chứng tại [tests/README.md](../../tests/README.md); chưa nạp xe.

Ngày 19/10/2026. Trạng thái: IMPLEMENTED trong source, chưa triển khai lên Core/robot vận hành.

## Bổ sung 21/10/2026 — Khôi phục producer diagnostic sau đối chiếu firmware ổn định

`SOURCE`: người dùng báo xe01 tại S1 vẫn `STALE_COMMAND`, không xóa được lỗi và yêu cầu sửa. `OBSERVED`: API gateway xác nhận xe01 online, vị trí S1 hợp lệ, Core giữ ERROR/STALE_COMMAND; `MQTTModule::sendTelemetry` hiện thiếu `lastCommandId`, trong khi parser Core và `CommandIds::ensureAbove` vẫn có. Build/native trước đó chưa kiểm luồng producer MQTT này.

`IMPLEMENTED`: thêm `lastCommandId=highwater_` vào diagnostic v1, đọc ngay trong network worker sở hữu counter. Giá trị lấy từ mã đã persist trong NVS, không lấy route ID hoặc số ACK; có cả khi chưa nhận lệnh (0) và khi MCU báo lỗi. Core dùng trường hiện có để tiến allocator; không đổi dedup hoặc điều kiện `RESET_FAULT`. Hai build ID mới là `20261021-robot01-stale-sync-v5` / `20261021-robot02-stale-sync-v5`. PWM/PD/capture/tuning giữ nguyên.

`VERIFIED`: core_tests PASS, gồm đồng bộ high-water5000, giữ ERROR qua diagnostic khỏe, không retry/STOP mã thấp, `clearError` gửi RESET_FAULT>5000 rồi chờ diagnostic mới. Kết quả build ESP32 và hash tại [hồ sơ kiểm thử](../../tests/README.md). `OPEN`: chưa nạp bản v5 trong lượt sửa, chưa quan sát phục hồi trên xe thật; sau nạp phải xác nhận xe dừng tại RFID trạm, chờ diagnostic rồi bấm Xóa lỗi. Không xóa NVS/counter.

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
