# ADR-006: Ca làm việc và trạm sạc cố định

Ngày: 29/09/2026. Trạng thái: **ACCEPTED** cho server/web; firmware hoãn theo yêu cầu mới nhất.

## Nguồn quyết định

Người dùng yêu cầu hoàn thành task cuối trước tan ca rồi về sạc, ngày mới tại trạm và vị trí dựa RFID. Xác nhận 01 → S1, 02 → S2; nút kết thúc ca; C3 → G1 → C1 khép vòng. Chỉ đạo mới nhất: chưa sửa firmware, chỉ sửa tự về sạc và mặc định trạm khi kết nối/khởi động server.

## Quyết định

Thêm ShiftController trên core, gọi dưới khóa điều phối hiện có. Core sở hữu CLOSING/CLOSED/RUNNING; gateway chuyển HTTP thành WS có request_id, chỉ báo nhận sau command_result. FleetManager có gate nhận/phân công và cấu hình trạm riêng. Kết thúc ca không hủy task đang chạy hoặc tự hoàn thành hàng đợi.

Khởi động tập kết sau telemetry mới; không tạo robot giả hoặc dùng S1/S2 làm vị trí đo. CLOSED cần cả hai xe đúng trạm, telemetry mới, hết kế hoạch, không lỗi và không giữ mutex. Mở ca cần pin đo ≥40% và MQTT hoạt động. MQTT giữ CHARGE_START/WAIT/GOTO và payload cũ để không sửa một phía.

Lưu chế độ ca bằng thay file tạm; không lưu tọa độ hoặc khôi phục kế hoạch từ SQLite. Giữ counter command. Trạm riêng áp dụng cả pin thấp; lỗi/offline/ACK và ưu tiên xử lý pin thấp hiện có vẫn hiệu lực.

## Hệ quả

Hai cạnh trái đảo hướng cho mọi kho/trạm đi tới/về được. Mở ca cần đủ hai ID topic 01/02. RC522/pinout/learn UID đã chọn nhưng **DEFERRED**, sketch giữ nguyên. Kiểm chứng bằng unit, broker thật với robot giả lập và browser; chuyển động/docking/sạc điện thật và phục hồi task/mutex sau core reboot giữa chuyển động còn OPEN.

## Cập nhật 08/10/2026: thử từng xe

Theo REQ-SHIFT-02, điều kiện mở ca xét những xe đã xuất hiện trong fleet: một xe ở đúng trạm riêng có thể mở ca; khi cả hai xe đã xuất hiện, cả hai phải đủ điều kiện như trước. Xe đã xuất hiện rồi offline/lỗi vẫn được tính, không bỏ qua. Khi telemetry pin của một xe duy nhất là `?`, ca có thể mở để thử hiển thị; pin đã đo dưới 40% vẫn khóa. Với một xe chưa đo pin, Core chờ RFID tại trạm và không tự gửi GOTO/CHARGE_START trong lúc tập kết. Nếu xe duy nhất đã ở trạm và có pin đo đủ, Core cũng không gửi CHARGE_START chỉ vì xe còn lại vắng mặt; khi xe có pin đo ở ngoài trạm, luồng tự về trạm vẫn hoạt động. `beginWorkFromHome` chỉ gửi WAIT cho xe tham gia; thuật toán tìm đường, phân công task, mutex và ngưỡng pin cho task không đổi. Vì vậy mở ca không tự cho phép giao task khi pin điều khiển còn `?` ngoài chế độ GOTO thử riêng đã có.
