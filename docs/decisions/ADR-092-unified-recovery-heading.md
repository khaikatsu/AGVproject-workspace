# ADR-092 — Khôi phục xe và xác nhận hướng trước khi chạy

Ngày 29/10/2026. SOURCE: REQ-UNIFIED-RECOVERY-HEADING-20261029, người dùng duyệt sau rà soát các nút trùng và hướng mặc định tại kho/trạm.

## Quyết định

- Thẻ xe có một nút Khôi phục xe. Dialog hiển thị RFID thật, điều kiện dừng, xác nhận thân xe ra hết vùng cũ và lựa chọn xe đã được nhấc/xoay bằng tay. Khôi phục lỗi và tiếp tục nhiệm vụ là hai quyết định riêng.
- Hướng mất/không chắc chắn hoặc người vận hành đã di chuyển xe làm khóa hướng tại Core. Hướng bay_default không gỡ khóa này. Tất cả nguồn cấp GOTO/NODE_ORDER/CHARGE_START đều chịu khóa, kể cả đơn tự động.
- Sau reset, xe phải IDLE/STOP, RFID mới, hết task/tuyến/lift và diagnostic cùng boot/kết nối mới được nhận CONFIRM_HEADING. Hướng N/E/S/W là phía đầu xe theo bản đồ; ở kho/trạm yêu cầu đúng quy ước ra/vào hiện có, người vận hành phải đặt xe đúng hướng trước khi xác nhận.
- MQTT thêm CONFIRM_HEADING gồm cmd_id/node/heading. MCU không quay xe, chỉ ghi nhận hướng đã kiểm tra khi dừng. Diagnostic trả capability, headingSource=operator và headingCommandId. Core chỉ gỡ khóa khi đúng lệnh, node, hướng, boot/kết nối và uptime sau yêu cầu; ACK không đủ. Reconnect vô hiệu xác nhận cũ.
- Firmware cũ vẫn reset lỗi được nhưng không thể xác nhận hướng mới; UI báo cần nạp firmware hỗ trợ. Giữ endpoint cũ cho tương thích; dashboard dùng luồng mới. Không sửa config riêng, mạng, pin, DB/counter hoặc tự restart dịch vụ.

## Kiểm chứng

Bổ sung kiểm tra xung đột cùng ngày: đã sửa lệnh chen ngang, phản hồi cũ sau ngắt/kết ca, lỗi giả khi mất ACK, phân công xe khóa hướng, resume đồng thời và FIFO cũ khi reconnect. Bộ cuối21/21 ca Core,65/65 web/server và browser PASS; build mới riêng tại `.qa/recovery-conflicts-20261029/core/agv_core.exe`, SHA256 `70873BBCE7D19F78EA759A7C9E567A5BB5FB2F6EE2D4512AFDFB8A6194168EE2`. Firmware không sửa/build/test/nạp trong lượt audit. [Báo cáo đầy đủ](../testing-recovery-conflicts-2026-10-29.md). Các đoạn dưới ghi mốc triển khai trước audit.

IMPLEMENTED trong source. Core build PASS riêng tại `.qa/recovery-heading-20261029/core/agv_core.exe`; SHA256 `0DEF37D22680F25301BFBFF3A8FBABF9FF27C2252FAEAA113F88A51F6402D8F6`. Chưa thay executable hoặc restart Core/gateway vận hành.

Hồi quy server `tests/shift_recovery.cpp` PASS cho01/02, K1–K6, reset và xác nhận hướng đúng lệnh; ACK, uptime cũ, cmd_id sai, sai phiên, motor còn chạy, hướng trạm sai, MCU thiếu capability và gửi lặp đều không mở khóa. Reconnect khóa lại. Gateway/đơn/đối soát27/27 PASS với DB/Core giả riêng; browser khôi phục/reset→chờ hướng→tiếp tục→kết ca→hoàn tác→đối soát và thẻ xe desktop/mobile PASS. Bằng chứng trong tests/README.md.

Firmware chỉ rà source/diff và đồng bộ callsite producer/parser/dedup, không build/test/nạp theo AGENTS.md; người dùng tự thử thực tế. BuildID01/02 `20261029-robot01-recovery-heading-v24` / `20261029-robot02-recovery-heading-v24`. Không coi kết quả giả lập web/server là kiểm chứng firmware hoặc xe thật.
