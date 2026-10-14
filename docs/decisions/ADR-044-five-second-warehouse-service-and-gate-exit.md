# ADR-044 — Dừng 5 giây trong kho và lùi ra gate

Ngày 10/10/2026. Source và kiểm thử hoàn tất; đang chờ gateway nạp lại.

`SOURCE`: người dùng hỏi vì sao xe bận dù chỉ muốn lấy/giao hàng; yêu cầu tới kho, dừng5s, lùi ra gate, xoay theo chặng tiếp; giao hàng tương tự. Người dùng xác nhận xe trống và cho phép đóng đơn cũ giữ xe để chạy test mới.

`OBSERVED`: xe01 S1/IDLE, vị trí/hướng trạm đã xác nhận, không tuyến/task. Đơn cũ `order-1791574998978-f5688158`, item interrupted/task40963 giữ robot01, nên đơn mới queued. Hai đơn mới K5→K3 hiện cùng chờ xe. Firmware hồi sáng đã có REVERSE kho→gate và NODE_ORDER xoay tại gate; không cần nạp/build lại firmware/Core cho thay đổi thời gian và thứ tự task này.

## Luồng mới

1. Core cấp tuyến tới kho lấy. Chỉ sau task completed và RFID/IDLE đúng kho mới tính5s.
2. Sau5s gửi tuyến tới kho giao: chặng đầu lùi kho→gate, rồi Core/firmware xoay theo NODE_ORDER ở gate và bám tuyến.
3. Tới kho giao, chờ task completed và RFID/IDLE, dừng5s.
4. Gửi task riêng lùi kho giao→gate tương ứng. Chỉ sau task exit completed và RFID/IDLE tại gate mới delivered/kết thúc lượt. Chặng tiếp theo xoay theo hướng được xác nhận tại gate; không tự đoán hướng khi chưa có tuyến tiếp.

Đứng chờ mô phỏng bốc/dỡ, không thay điều khiển servo/cảm biến tải thật. ACK hoặc hết timer không chứng minh đã tới gate. Mất mạng/lỗi hoặc rời kho trong lúc chờ vẫn needs_attention; mutex/counter giữ nguyên.

## Server và giao diện

- SERVICE_MS=5000; item thêm ready_exit/dispatching_exit/to_exit và exit_gate; route_plan thêm exit.path/cost. APIhealth thêm warehouse_service_ms để biết backend đã nạp cấu hình mới chưa.
- Đơn chưa từng phát lệnh (queued) không giữ xe khi needs_attention; chuyến thực sự bắt đầu vẫn giữ tới đối soát. Lý do chờ nêu đúng ID đơn giữ xe.
- Danh sách đơn sắp xếp theo created giảm dần, kể cả gateway đọc lại SQLite, để đơn mới không bị lẫn dưới lịch sử.
- Đóng đúng đơn cũ giữ robot01 chỉ sau gateway mới chạy và theo xác nhận xe trống của người dùng. Không xóa DB/history hoặc reset counter.

## Kiểm chứng và giới hạn

`VERIFIED`: orders/gateway/security26/26 PASS; bao gồm timer5s, exit task/đúng gate, không tính delivered khi còn trong kho, không phát lại chuyến sau restart, reservation queued và operator resolve. Edge/mobile fixture PASS.

`VERIFIED`: firmware native20/20 PASS, gồm reverse kho→gate và pivot trái/phải sau RFID. Không sửa binary đã nạp.

`VERIFIED`: `tests/orders-five-second.integration.js` chạy chính binary Core hồi sáng với broker/map/DB/counter/xeảo riêng ngoài OneDrive, ghi đủ5s thời gian thực ở cả hai kho, tuyến giao bắt đầu G5 từK5, exit vềG3 sauK3, xe không còn cargo/path. Bằng chứng `.qa/warehouse-five-second-RROXM0/verification.json`. Mô phỏng không chứng minh hướng motor/độ ổn định quét RFID trên xe thật.

`OPEN`: backend sống vẫn chưa nạp source mới; thao tác restart gateway trước đó bị auto-review chặn `blocked by policy`. Đã đưa lệnh cho người dùng chạy. Chưa đóng đơn cũ để tránh queued dispatch theo cấu hình10s cũ. Chưa thử lượt lấy/giao vật lý mới.

Đã chuẩn bị `.qa/apply-five-second-cargo.ps1` cho người dùng chạy một lần: kiểm tra01 S1/IDLE và không tuyến/task/chuyến đang chạy, khởi động lại đúng gateway nếu cần, chờ health warehouse_service_ms=5000/Core sẵn sàng/giữ phiên, kiểm tra lại xe rồi resolve đúng đơn cũ mà người dùng đã xác nhận. Script không tự được thực thi bởi agent. Sau đóng đơn cũ, các đơn queued hiện có có thể tự dispatch; hiện có hai đơnK5→K3 riêng.
