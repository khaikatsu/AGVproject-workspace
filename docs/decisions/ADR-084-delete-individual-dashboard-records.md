# ADR-084 — Xóa riêng từng đơn và dòng lịch sử trên dashboard

Ngày 26/10/2026. Trạng thái: IMPLEMENTED. Bổ sung [ADR-071](ADR-071-warehouse-operations-overview.md) và [ADR-083](ADR-083-fixed-vehicle-cards-in-warehouse-overview.md).

Bố cục hiện hành theo [ADR-085](ADR-085-unified-mission-results-board.md): nút Xóa nằm trong bảng chung, ở hàng đơn hoặc lịch sử độc lập; các chặng liên kết nằm trong Chi tiết. Quy tắc xóa logic và giữ dữ liệu vận hành dưới đây vẫn áp dụng.

## Nguồn và hiện trạng

`SOURCE`: người dùng gửi ảnh danh sách Các lượt lấy → giao và Nhiệm vụ và kết quả, yêu cầu thêm nút Xóa sau mỗi mục để tự chọn mục muốn xóa.

`OBSERVED`: đơn được giữ trong bộ nhớ scheduler và `warehouse_orders`; task được Core gửi lại thường xuyên, gateway đồng bộ theo phiên/task ID vào `task_log`. Xóa vật lý dòng task có thể khiến snapshot tiếp theo tạo lại dòng và làm thay đổi nguồn KPI. Đơn active/cần đối soát còn giữ thông tin phân xe, kệ và chốt chưa hoàn thành.

## Quyết định

- Nút **Xóa** nằm ngay cuối từng tiêu đề đơn và cuối từng dòng lịch sử, kể cả đơn active/cần đối soát và task đang chờ/chạy. Có xác nhận **xóa khỏi danh sách**, nêu rõ không hủy nhiệm vụ và vẫn giữ sổ kho/báo cáo. Không xóa hàng loạt hoặc xóa dữ liệu vận hành khi triển khai mã.
- Xóa logic bền vững: đơn lưu `deleted_at` trong JSON hiện có; task thêm cột `deleted_at TEXT NULL` bằng migration cộng thêm. `syncTasks`, adoption qua phiên và cập nhật trạng thái không xóa dấu này. Đọc danh sách task lọc trước LIMIT; đọc đơn mặc định bỏ mục đã xóa.
- `DELETE /api/orders/:id` dùng ID đơn đầy đủ, `DELETE /api/tasks/history/:id` dùng khóa số nguyên `task_log.id` của dòng. Không dùng mã Core đã cắt ngắn, target hay vị trí xe để chọn mục. Kết quả `{ok:true,id,scope:"dashboard"}`, đơn thêm `deleted_at`; cùng ID đã xóa trả thành công lặp lại, không tồn tại404, ID sai400. Các endpoint chịu auth/Origin/CSRF hiện có, không yêu cầu Core online và không phát command tới Core/robot.
- Scheduler vẫn đọc mọi đơn, kể cả đã xóa; chốt nhập lại kho và báo cáo cuối ca cũng dùng toàn bộ đơn. WS giữ đơn đã xóa còn `active/needs_attention` kèm dấu `deleted_at` để thẻ xe tiếp tục có tuyến/kệ/cảnh báo; danh sách đơn và bộ chọn tiến độ lọc dấu này. Đơn đã xóa cần đối soát có mục cảnh báo kèm nút **Đã đối soát** dưới khối trạng thái xe và kho, kể cả khi không còn robot để gắn thẻ.
- Task bị xóa khỏi lịch sử vẫn có trong theo dõi task mở và nguồn KPI; Core thực thi như cũ. Sổ kho, sự kiện nâng/hạ, báo cáo đã lưu và các counter không đổi. Retention hiện hành vẫn áp dụng; thao tác này không xóa vật lý dữ liệu nguồn và chưa có UI hoàn tác.
- UI chỉ bỏ mục khi DELETE thành công. Hủy confirm hoặc HTTP lỗi giữ mục; chặn gửi trùng khi request pending. Xóa đơn trong `<summary>` không làm đóng/mở mục. Broadcast đồng bộ danh sách mới cho các client.

## Kiểm chứng

Unit/API kiểm đúng ID, retry, auth/CSRF, persistence/reopen, snapshot không tạo lại lịch sử, giữ task mở/KPI, giữ hold của xe có kệ, giám sát đơn active đã xóa và đưa đơn chưa xong vào báo cáo CLOSED. Test browser kiểm confirm/cancel/lỗi API, chọn từng mục, đơn đã đối soát, pending task, xử lý đối soát cho đơn đã xóa, reload/restart và desktop/mobile. Kết quả tại [hồ sơ kiểm thử](../../tests/README.md).

Không thay giao thức MQTT, Core, firmware, session hoặc audit; thử bằng DB và cổng riêng.
