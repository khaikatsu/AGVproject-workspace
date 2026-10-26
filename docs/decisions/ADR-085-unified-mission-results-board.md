# ADR-085 — Một bảng Nhiệm vụ và kết quả cho đơn kệ và di chuyển

Ngày 26/10/2026. Trạng thái: IMPLEMENTED. Bổ sung [ADR-071](ADR-071-warehouse-operations-overview.md), [ADR-083](ADR-083-fixed-vehicle-cards-in-warehouse-overview.md) và [ADR-084](ADR-084-delete-individual-dashboard-records.md).

Bố cục cập nhật theo [ADR-086](ADR-086-map-first-missions-and-single-inventory.md): bảng chung nằm dưới bản đồ; hai phần trạng thái kho được gộp thành một khối.

## Nguồn và hiện trạng

`SOURCE`: người dùng đồng ý gộp Các lượt lấy–giao với Nhiệm vụ và kết quả thành một bảng duy nhất; yêu cầu dễ nhìn và đẹp mắt. Phương án được duyệt gồm hàng chính theo đơn, chi tiết từng lượt/chặng, lệnh di chuyển riêng, bộ lọc và các thao tác Chi tiết/Hủy/Xóa.

`OBSERVED`: mỗi item của đơn có thể tạo ba task Core (tới lấy, tới giao, ra cửa). `item.task_id` bị thay/xóa khi chuyển sang nâng/hạ hoặc hoàn tất, nên các đơn cũ không đủ dữ liệu để gán mọi task lịch sử. Các dòng lịch sử hiện dùng cả phiên Core và task ID; khi checkpoint phục hồi, DB có thể chuyển cùng dòng lịch sử sang phiên mới và giữ nguyên khóa số của dòng.

## Quyết định

- Bỏ cả hai khối HTML/renderer cũ; thêm một bảng **Nhiệm vụ và kết quả** toàn chiều ngang, dùng được ở Tổng quan/Nhiệm vụ/Báo cáo. Cột gồm Công việc, Xe, Nguồn→Đích, Trạng thái/Tiến độ, Thao tác. Có bộ lọc Tất cả/Đang xử lý/Hoàn thành/Cần đối soát với số lượng theo công việc, tìm theo tuyến/xe/mã và giữ checkbox xem đơn đã đối soát.
- Một đơn là một hàng chính, kể cả nhiều kệ và hai xe. Chi tiết mở ra từng item, tiến độ năm bước, mã kệ, lý do chờ/lỗi, đường dự kiến và các task liên quan. Lệnh Core không có liên kết được giữ thành hàng di chuyển riêng. Không gộp theo xe, đích hoặc tiền tố mã; dữ liệu cũ thiếu bằng chứng vẫn có thể còn dòng riêng.
- Sau `direct_goto` thành công, gateway lưu vào `item.task_links[]` các trường `task_id`, `core_session_id`, `phase` (`pickup/dropoff/exit`), `history_id` nếu tìm thấy đúng dòng DB, `from`, `target`, `robot`, `status`, thời gian và lý do. Link nằm trong JSON đơn hiện có, không thêm bảng/cột mới cho feature này. Trạng thái task theo Core được cập nhật vào link khi xử lý chặng; link không tự hoàn tất đơn.
- Gateway enrich `task_history`/GET lịch sử bằng `order_id`, `order_item`, `order_phase`, `order_deleted_at`. Match bằng phiên+task ID đầy đủ; khóa DB đã ghi kèm task ID chính xác cho phép giữ liên kết khi DB adopt nhiệm vụ sang phiên Core mới. Liên kết mâu thuẫn bị bỏ, không đoán. Bản ghi parent được bổ sung vào state nếu nó được lịch sử tham chiếu nhưng nằm ngoài50 đơn gần nhất.
- Task có liên kết nằm dưới parent và không hiện lại thành dòng độc lập khi parent bị lọc hoặc xóa. Xóa parent vẫn là xóa logic khỏi danh sách, không hủy Core, không đổi KPI/sổ kho. Xóa riêng lịch sử một chặng dùng ID dòng DB như ADR-084; không dựng lại chặng đã xóa từ link kỹ thuật. Raw link vẫn giữ để không mất chứng cứ ghép đơn.
- Nút **Hủy** cho nhiệm vụ độc lập hoặc từng chặng đang pending/dispatched trong phần chi tiết, chỉ khi phiên Core khớp và controls ready. Không tạo API hủy toàn đơn hoặc suy ra đã dừng xe. Sau hủy một chặng, scheduler/đối soát cũ tiếp tục quyết định trạng thái đơn. Nút **Xóa** tách riêng, giữ xác nhận và persistence như trước.
- Chỉ dựng nội dung mở rộng khi mở Chi tiết; bộ lọc/từ khóa và trạng thái mở được giữ qua cập nhật. Cache projection bỏ thay đổi riêng encoder, xác nhận bằng DOM identity. Desktop dùng bảng và header cố định; mobile chuyển từng hàng thành thẻ trong cùng bảng, không tạo bản sao renderer. Trang cuộn được khi map thu gọn hoặc xem báo cáo.
- Sửa nhánh đọc phản hồi API: HTML404 `Cannot DELETE` hiện rõ gateway cần khởi động lại, thay vì báo nhầm mất kết nối. JSON array và các chính sách auth/Origin/CSRF vẫn giữ.

## Kiểm chứng và giới hạn

Model/unit kiểm exact ID/phiên, liên kết tồn tại sau khi item xóa task hiện tại, ba chặng hoàn tất, adoption qua khóa DB, không ghép theo robot/target, dữ liệu mâu thuẫn, parent bị xóa/closed, nhiều xe, search/filter và không suy hoàn tất từ task con. Browser kiểm một bảng, các chặng không lặp, Chi tiết hai xe, bộ lọc/tìm kiếm, Hủy đúng chặng, Xóa/persistence, stale khóa Hủy, XSS, encoder giữ DOM, desktop/mobile và hồi quy dashboard/kho/thẻ xe. Kết quả tại [hồ sơ kiểm thử](../../tests/README.md).

Không restart gateway/Core vận hành, không sửa DB hoặc counter thật, không đổi MQTT, firmware, session hoặc audit. Muốn lưu liên kết mới cần nạp lại gateway khi không có đơn đang chạy; các task cũ đã mất liên kết không được tái dựng bằng suy đoán. Chưa benchmark CPU/FPS toàn dashboard.
