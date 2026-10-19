# ADR-070 — Vị trí kệ, nhật ký chuyển kệ và báo cáo cuối ca

Ngày 18/10/2026. Trạng thái: IMPLEMENTED phần mềm; thử kệ/servo thật OPEN.

## Nguồn và phạm vi

`SOURCE`: người dùng muốn server cập nhật trạng thái kệ và log theo việc xe đã làm; khi kết thúc ca lưu báo cáo trạng thái kho. Khi được hỏi vị trí ban đầu, người dùng chọn tự nhập trên giao diện. Không tự cấu hình K2–K5 có kệ hoặc K1/K6 trống.

`OBSERVED`: [ADR-069](ADR-069-automatic-orders-use-servo-lift.md) đã nối đơn tự động với DONE servo đúng lệnh. Core lưu cargo ảo cho từng xe, chưa có ID kệ hoặc tồn vị trí. Gateway đã có SQLite, quản lý đơn và quan sát `shift.phase`; `CLOSING` chưa phải kết ca hoàn tất. Core không phát ID ca riêng, nên gateway cấp UUID cho mỗi lần quan sát chuyển sang RUNNING.

## Quyết định

- Gateway quản lý sổ kho trong SQLite; Core tiếp tục quản lý xe/chuyển động. Không đổi payload MQTT hoặc firmware. Trạng thái hiện tại, sự kiện và báo cáo nằm trong ba bảng `warehouse_inventory`, `warehouse_events`, `warehouse_shift_reports`; không thuộc retention log/KPI cũ.
- Người vận hành nhập mã kệ duy nhất tại từng vị trí K1–K6 có trên map; để trống cho ô không có kệ. Chỉ lưu khi ca CLOSED, không có đơn active/thao tác xe đang chạy, xác nhận kiểm tra thực tế và các xe trống. Sửa lại kho là sự kiện đối soát có lưu trạng thái trước; dùng `expected_revision` chống ghi đè thay đổi mới. Dữ liệu tự phục hồi khi mở lại gateway.
- Mỗi vị trí chứa tối đa một kệ. Đơn chuyển kệ chỉ nhận nguồn có kệ, đích trống, một lượt cho mỗi vị trí trong đơn. Một kệ không được chia thành nhiều kiện; không tự lập chuỗi di chuyển để tạo ô trống. Lưu đặt chỗ kệ/nguồn/đích/xe trước khi phát chặng lấy; xung đột giữa hai xe/đơn bị chặn.
- Chỉ sau xác nhận DONE_UP/DONE_DOWN đúng command ID, phiên, RFID, IDLE, diagnostic mới và cargo tương ứng, sổ kho mới chuyển cùng `rack_id` từ ô lên xe hoặc từ xe vào ô. ACK và timer không thay vị trí. Log gồm timestamp UTC, mã kệ, xe, nguồn/đích, move key gắn order/item hoặc action, command ID, Core session và nguồn bằng chứng. Tồn vị trí và log được ghi trong cùng transaction; khóa duy nhất theo session/robot/command chống đếm hai lần.
- Thao tác Lấy/Hạ scope một xe cũng cập nhật sổ kho nếu kho đã được nhập; lúc đó cần servo thật và đặt chỗ. Trước khi cấu hình kho, thao tác test độc lập giữ hành vi cũ và không tự tạo kệ.
- Khi mất kết nối, timeout hoặc gateway restart, lượt chưa chắc kết quả giữ kệ/ô ở trạng thái cần đối soát, không replay. Vị trí hiển thị là lần xác nhận cuối, không tự đoán kệ đã đặt xuống. Đóng đơn cũ không tự xóa trạng thái kệ cần đối soát; kiểm tra thực tế rồi nhập lại kho lúc ca đã đóng.
- Ghi ảnh đầu ca khi thấy RUNNING; khi Core xác nhận CLOSED, chốt một báo cáo gồm ảnh đầu/cuối ca, toàn bộ nhật ký của ca, số lần nâng/hạ, kệ trên xe, kệ cần đối soát và đơn/thao tác chưa xong. Bấm kết ca hoặc CLOSING chưa chốt báo cáo. Báo cáo đã lưu không thay đổi khi nhập lại kho hoặc chạy ca sau; UI đọc lại và tải JSON được.
- Gateway khởi động/đăng ký giữa ca hoặc bị gián đoạn quan sát đánh dấu `observation_gap`; đổi Core session lưu báo cáo gián đoạn `core_session_changed`, không ghi là ca hoàn thành bình thường. Lần đầu nhìn thấy CLOSED khi chưa theo dõi ca không tạo báo cáo giả.

## Giới hạn

Vị trí kệ dựa vào kết quả servo phần mềm và xác nhận ban đầu của người vận hành; chưa có cảm biến tải hoặc đọc ID kệ trên robot. Không phát hiện tự động việc di chuyển kệ bằng tay; cần đối soát lại khi làm việc đó. Các đơn nhiều kiện cùng một ô của mô hình cũ không còn phù hợp với một kệ mỗi vị trí. Báo cáo kho độc lập với KPI thống kê hiện có.

## Kiểm chứng

Kiểm tra bằng DB/Core/robot giả lập riêng: nhập kho, revision, mã trùng, hai xe đặt chỗ, DONE đúng/sai, chống lặp, rollback khi lỗi ghi log, restart/đổi phiên, báo cáo CLOSED bất biến, và tích hợp đơn/action. Trình duyệt desktop/mobile kiểm nhập tùy chọn, tải lại trang, cập nhật theo servo, xem báo cáo và tải JSON. Kết quả cụ thể tại [tests](../../tests/README.md). Không thay DB vận hành, restart gateway thật hoặc thử robot thật trong lần triển khai này.
