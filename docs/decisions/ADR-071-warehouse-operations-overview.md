# ADR-071 — Theo dõi tuyến từng xe và trạng thái kho trong ca

Ngày 18/10/2026. Trạng thái: IMPLEMENTED giao diện.

Bổ sung 26/10/2026: [ADR-085](ADR-085-unified-mission-results-board.md) thay hai danh sách Các lượt lấy–giao và Nhiệm vụ và kết quả bằng một bảng chung, mở chi tiết theo đơn/chặng và giữ các thao tác hiện có.

Bổ sung 26/10/2026: [ADR-083](ADR-083-fixed-vehicle-cards-in-warehouse-overview.md) đổi tiêu đề hiện hành thành **Trạng thái xe và kho**, gộp bảng Đội xe vào hai thẻ cố định01/02 và giữ thẻ sau khi gỡ khỏi Core. Các quyết định tên gọi bên dưới ghi lại yêu cầu tại thời điểm18/10.

## Nguồn và hiện trạng

`SOURCE`: người dùng gửi ảnh form đơn hàng và yêu cầu hiển thị rõ xe nào đi lấy/giao tại kho nào, nguồn → đích của từng lượt, và trạng thái các kho trong ca. Cho phép làm lại giao diện cho dễ nhìn.

`OBSERVED`: form cũ ưu tiên mã đơn dài và ô số lượng luôn bằng một; nguồn/giao, xe thực hiện và tồn vị trí nằm ở các bảng khác nhau. Khi dock hai cột bên bản đồ, người vận hành phải cuộn mới xem hết thông tin. [ADR-070](ADR-070-warehouse-racks-and-shift-reports.md) đã có trạng thái kho và ảnh đầu ca lưu ở gateway.

## Quyết định

- Bổ sung theo ảnh người dùng: tiêu đề chính là **Trạng thái kho hàng**. Mặc định ẩn các đơn `closed` đã đóng sau đối soát khỏi danh sách/tiến độ; checkbox **Hiện đơn đã đối soát (N)** cho xem lại. Đây là bộ lọc hiển thị, không xóa lịch sử hoặc báo cáo; đơn còn cần đối soát luôn hiện. Tải lại trang trở về danh sách gọn mặc định.

- Đặt bảng theo dõi toàn chiều ngang phía trên bản đồ ở Tổng quan/Nhiệm vụ, không nằm trong hai cột cuộn của map. Mỗi xe có thẻ nguồn → đích, pha hiện tại, RFID gần nhất, đích của lệnh đang chạy, mã kệ và năm bước tới lấy/nâng/chuyển/hạ/ra cửa.
- Ghép thẻ xe với item theo robot và task ID. Task Core khác không được gắn nhầm với lượt giao cũ; thao tác kho scope một xe chỉ hiển thị lấy hoặc hạ tại kho, không tự suy ra đủ tuyến. Khi lỗi hoặc đơn cần đối soát, dừng chỉ báo bước đang chạy và hiện lý do. Dữ liệu cũ/mất MQTT được ghi rõ và làm mờ bảng; không suy hoàn tất theo target hoặc vị trí xe.
- Sáu ô K1–K6 hiển thị mã kệ/ô trống, giữ chỗ, xe lấy/nhận, kệ đang tới, đối soát và mã kệ ở đầu ca. Gateway snapshot/delta bổ sung `warehouse.shift.initial_slots` từ ảnh đầu ca đã lưu; không đổi dữ liệu gốc, MQTT, Core hoặc firmware. Sự kiện gần nhất chỉ dùng mô tả và có lọc ca hiện tại, không đếm số lượt cả ca từ mảng20 log.
- Form chuyển kệ giữ REST hiện có. Ẩn quantity cố định1, gắn mã kệ/trạng thái vào option, vô hiệu nguồn trống/đích có kệ/ô giữ chỗ. Có khung LẤY TỪ → GIAO ĐẾN; đơn nhiều nguồn/đích ghi rõ server ghép sau khi tạo, không trình bày ghép đoán là kết quả. Nêu lý do nút giao việc bị khóa. Giữ lựa chọn người dùng khi tồn vị trí đổi, yêu cầu chọn lại thay vì âm thầm đổi kho.
- Danh sách đơn ưu tiên tuyến kho và xe thực hiện, mã đơn đầy đủ ở tooltip; đường dự kiến thu gọn. Tiến độ đơn đã tạo chuyển xuống phần mở rộng, nhãn dùng kệ. Giữ hỗ trợ nhiều kho, phân xe theo thuật toán cũ và các view/map controls đang có.

## Kiểm chứng và giới hạn

`VERIFIED`: fixture gateway/DB riêng và trình duyệt Edge kiểm hai xe trên hai tuyến, form tạo đơn, đổi ô sau DONE, trạng thái đầu ca/giữ chỗ, lỗi và stale, desktop/mobile. Kiểm tra hồi quy theo [tests](../../tests/README.md). Mã frontend dùng DOM/textContent cho dữ liệu, không nội suy HTML từ mã kệ hoặc log.

Không đổi lệnh điều khiển robot hoặc thuật toán ghép đơn. Chưa restart gateway vận hành, không tác động DB thật và chưa thử robot/kệ vật lý.
