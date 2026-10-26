# ADR-086 — Nhiệm vụ dưới bản đồ và một khối trạng thái kho

Ngày 26/10/2026. Trạng thái: IMPLEMENTED. Điều chỉnh bố cục của [ADR-071](ADR-071-warehouse-operations-overview.md), [ADR-083](ADR-083-fixed-vehicle-cards-in-warehouse-overview.md) và [ADR-085](ADR-085-unified-mission-results-board.md).

## Nguồn và hiện trạng

`SOURCE`: người dùng yêu cầu đặt Nhiệm vụ và kết quả dưới bản đồ kho và gộp hai phần trạng thái kho bị trùng trong ảnh.

`SOURCE` bổ sung cùng ngày: người dùng yêu cầu xóa trang Nhiệm vụ do các chức năng đã có ở Tổng quan. Đã bỏ tab/view `tasks`, giữ ba tab Tổng quan/Bản đồ/Báo cáo, chuyển tạo đơn và mở lịch sử xe về `overview`. Bảng nhiệm vụ, hàng đợi, tạo đơn và lịch sử xe tiếp tục hoạt động tại Tổng quan.

`OBSERVED`: bảng nhiệm vụ đang nằm trước bản đồ. Bảng Trạng thái kệ ở sidebar và thẻ Kho trong ca cùng hiển thị trạng thái hiện tại từ `state.warehouse`; bảng còn chứa form đối soát và kệ trên xe nên không thể chỉ xóa toàn bộ khối mà bỏ các thông tin này.

## Quyết định

- Chuyển `missionBoard` vào grid chính, ngay sau bản đồ theo thứ tự đọc. Desktop/tablet dùng hàng toàn chiều ngang bên dưới bản đồ; điện thoại đặt trước các công cụ điều khiển. Giữ cả chế độ bản đồ mở rộng/thu gọn. Tab Báo cáo ẩn bản đồ, vẫn có bảng nhiệm vụ.
- Giữ một **Trạng thái kho** trong **Trạng thái xe và kho**: sáu thẻ K1–K6, trạng thái đầu ca, đặt chỗ/đối soát, danh sách kệ trên xe và form nhập/đối soát khi CLOSED. Bỏ bảng trạng thái hiện tại ở sidebar. Khối thống nhất cũng xuất hiện ở Báo cáo; bảng trạng thái trong báo cáo ca đã lưu vẫn giữ vì đó là dữ liệu lịch sử.
- Một renderer/cache dựng trạng thái kho hiện tại, được dùng cả sau phản hồi lưu kho và cập nhật WS. Summary ghi tổng kệ, số ô sẵn sàng nhận kệ (trống và không có giữ chỗ), kệ trên xe và số vị trí cần đối soát. Ô trống chưa xác minh hiện **Chờ xác minh vị trí** thay vì gợi ý sẵn sàng. Kệ trên xe cần đối soát có nhãn riêng.
- Không thay API, sổ kho, lệnh điều khiển, chốt mở ca/đối soát hoặc logic điều phối.

## Kiểm chứng

Fixture Edge/Core giả/SQLite riêng kiểm vị trí bảng ở desktop mở rộng/thu gọn, tab Nhiệm vụ, tablet/mobile; chỉ một khối kho; nhập và reload, kệ chuyển kho→xe→kho sau servo, giữ chỗ/hai xe/đối soát, dữ liệu cũ và Báo cáo. Hồi quy nhiệm vụ, dashboard và mở ca được ghi tại [hồ sơ kiểm thử](../../tests/README.md). Không sử dụng DB hay robot vận hành.
