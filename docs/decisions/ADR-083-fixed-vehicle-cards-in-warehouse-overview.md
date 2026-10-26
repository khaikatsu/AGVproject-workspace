# ADR-083 — Gộp đội xe vào trạng thái xe và kho, giữ hai thẻ cố định

Ngày 26/10/2026. Trạng thái: IMPLEMENTED giao diện. Bổ sung [ADR-071](ADR-071-warehouse-operations-overview.md).

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn gộp bảng Đội xe vào bảng kho, mặc định luôn có hai thẻ ID01/02; sau khi gỡ một xe offline khỏi Core, thẻ xe vẫn hiện và nhận lại dữ liệu khi xe kết nối. Người dùng đồng ý thực hiện phương án giữ các thao tác trên từng thẻ.

`OBSERVED`: bảng Đội xe và thẻ tuyến cũ cùng hiển thị trạng thái/RFID/đích. Thẻ tuyến chỉ được tạo từ `fleet.agvs`, nên biến mất khi Core xóa bản ghi. `DELETE /api/agvs/:id` gửi `remove_agv` tới Core; đây là thao tác xóa bản ghi theo dõi, có kiểm tra OFFLINE và mutex, không phải một bộ lọc giao diện.

## Quyết định

- Đổi tiêu đề thành **Trạng thái xe và kho**; bỏ HTML, CSS và `renderFleet()` của bảng Đội xe chi tiết. Giữ KPI đội xe ở phần tóm tắt.
- Hai thẻ `01`/`02` tồn tại cố định, theo thứ tự này. Placeholder chỉ ở browser; không thêm robot giả vào snapshot, bản đồ, bộ chọn task hoặc danh sách Core. Chưa có dữ liệu thì RFID/pin là `—`; Core hoặc dữ liệu cũ thì chờ dữ liệu, không suy ra xe online.
- Mỗi thẻ gom pin đo/diagnostic, tốc độ encoder, tuổi telemetry, RFID, trạm riêng, đích, tuyến/pha, kệ trên xe, lỗi, chờ proof dừng/RFID và giới hạn boot encoder. Nút Lịch sử, Chọn xe, Xóa lỗi, **Gỡ khỏi Core** dùng API hiện có; placeholder chỉ có Lịch sử. Xe ngoài scope, chưa xác minh, đang chờ proof hoặc dữ liệu cũ không được chọn giao task.
- Mất kết nối chỉ hiển thị OFFLINE và giữ thông tin cuối; không tự gỡ. Chỉ sau kết quả DELETE thành công mới ghi nhớ nhãn **Đã gỡ khỏi Core**, chỉ dùng khi snapshot không còn xe. Thất bại/không rõ kết quả không gắn nhãn này. Telemetry online mới xóa nhãn và cập nhật thẻ cũ. Ghi nhớ chỉ trong trang và phiên Core hiện tại; tải lại hoặc đổi phiên không suy ra lịch sử gỡ, hiển thị Chưa kết nối khi chưa có xe.
- Không đổi chốt OFFLINE/mutex/xác nhận đưa xe khỏi kho ở Core, không xóa lịch sử SQLite hoặc sổ kệ. Nếu sổ kho còn kệ trên một xe không có bản ghi Core, placeholder vẫn báo cần đối soát.
- Cache nội dung riêng từng thẻ theo dữ liệu hiển thị; tuổi/tốc độ/số giây chờ cập nhật tại chỗ. Encoder/tuổi thay đổi không dựng lại nội dung thẻ; xe01 đổi pin không dựng lại xe02. Ô kho có cache riêng. Đây là giảm DOM dư đã kiểm bằng identity, chưa phải benchmark CPU/FPS của toàn dashboard.

## Kiểm chứng và giới hạn

Fixture `tests/vehicle-cards.browser.integration.js` dùng Core giả, SQLite và cổng loopback riêng; kiểm tra hai ID từ lúc chưa có telemetry, offline không tự gỡ, từ chối gỡ, gỡ thành công, lịch sử sau gỡ, telemetry quay lại, scope, lỗi và khóa khi stale, đổi phiên/tải lại, XSS, DOM giữ nguyên và desktop/mobile. Hồi quy dashboard và kho theo [hồ sơ kiểm thử](../../tests/README.md).

Không thay session phía server hoặc audit; không build/deploy Core hoặc điều khiển robot thật.
