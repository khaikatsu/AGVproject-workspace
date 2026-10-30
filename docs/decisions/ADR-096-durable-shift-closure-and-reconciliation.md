# ADR-096 — Giữ kết ca và bằng chứng đối soát qua restart

Ngày30/10/2026. SOURCE: người dùng yêu cầu khắc phục lỗi kết ca/đối soát và đơn cũ tái xuất hiện mỗi khi mở server. Trạng thái: triển khai source, kiểm chứng ghi ở cuối tài liệu.

## Nguyên nhân đã xác minh

- Đơn legacy đã `closed_by_shift` thiếu `shift_closed_at` dùng `updated` làm mốc kết ca. `interruptAll` vẫn sửa đơn này khi mất Core/dữ liệu cũ; startup còn sửa các pha item legacy. Mốc bị dời qua bằng chứng nhập kho và vô hiệu hiệu lực đối soát đã có.
- Sau `configure`, kho publish trước khi xóa cache bằng chứng đối soát, nên snapshot đầu tiên vẫn có thể hiện đơn cũ dù lần lưu kho đã thành công.
- ShiftController luôn khởi động lại ở CLOSING và có thể chuyển CLOSED về CLOSING khi xe mất telemetry/lỗi/vùng chưa xác minh. Điều này trộn trạng thái ca đã kết với điều kiện sẵn sàng cho ca mới.

## Quyết định

- Ca đã lưu CLOSED giữ CLOSED qua restart và lỗi/telemetry sau đó. Khôi phục RUNNING/CLOSING vẫn về CLOSING, hủy quyền chạy việc cũ; không tự mở ca hoặc phát lại task/servo cũ. `can_start` tiếp tục kiểm RFID/kết nối/lỗi/hướng/kệ/mutex/pin hiện hành. Kết ca chưa hoàn tất vẫn chờ proof và về trạm như trước.
- `interruptAll` không ghi lại đơn đã kết ca. Startup không sửa đơn đã kết ca/đã có bằng chứng đối soát. Mọi lần save khác ghim mốc legacy trước khi cập nhật `updated`. Item legacy chưa chuyển pha không được giữ xe nếu đơn đã có bằng chứng đối soát hợp lệ.
- Xóa cache đối soát trong transaction cấu hình kho, trước publish sau commit. Lần lưu hợp lệ đầu tiên phải phát ngay trạng thái đơn đã đóng theo bằng chứng, không cần chờ snapshot Core kế tiếp.
- Giữ audit/lịch sử/đơn đã xóa/báo cáo, kho thực tế, counter và mọi chốt robot. Không xóa DB hoặc suy xe trống từ mất kết nối. Đơn cũ đã bị dời mốc từ trước cần một lần kiểm tra và lưu kho hợp lệ; sau đó restart không được vô hiệu bằng chứng đó.
- Web phân biệt Ca đã đóng/Đang kết ca và hướng dẫn xem lý do từng xe. Bấm mở form đối soát không phải đã xác nhận kho; vẫn cần kiểm tra và lưu có xác nhận thực tế.

## Phạm vi áp dụng

Sửa Core/gateway/web, không sửa firmware01/02. Cần build/chạy Core và khởi động gateway mới rồi tải lại web. Không tự xác nhận tồn kho hoặc đổi dịch vụ vận hành khi chưa đối chiếu trạng thái xe.

## Kiểm chứng

VERIFIED30/10/2026: Node38/38 (orders/inventory), gateway + tích hợp restart24/24 PASS. Tích hợp mới `tests/shift-restart.integration.js` dùng SQLite/cổng/Core giả riêng, ngắt Core, đổi phiên và mở lại gateway/DB3lần; đơn đã đối soát vẫn đóng, snapshot sau configure có proof ngay, kho/lịch sử giữ nguyên và không phát command. Fixture `.qa/shift-restart-AtGGqZ`. C++ `tests/shift-lifecycle.cpp` PASS cho01/02 × saved CLOSED/RUNNING/CLOSING: giữ pha, chặn mở ca khi lỗi/mutex/thiếu proof, không tự GOTO/CHARGE_START và đọc lại CLOSED từ đĩa. JavaScript parse, kiểm tra nhãn ca/lý do lỗi/hướng bằng Node VM và diff PASS.

Build Core mới thành công tại `.qa/durable-shift-20261030/agv_core.exe`. Bộ `core_tests` tổng hợp dừng ở `fleet.directGoto("01","M2","after-mid-route-reset").empty()` trước các ca kết ca; đối chứng với ShiftController gốc từ HEAD cũng lỗi đúng assertion này. Vì vậy không tuyên bố toàn bộ Core suite PASS; lỗi phục hồi giữa tuyến cần xử lý riêng.

APPLIED30/10/2026 08:42+07 sau khi người dùng xác nhận cảhai xe đứng yên/trống: backup SQLite bằng SQLite backup API, binary cũ và các file map/runtime/shift/counter tại `.qa/durable-shift-20261030/operational-backup-20261030-014104`. Dừng đúng Core20176/gateway18544 sau kiểm tra PID/đường dẫn/không task hoặc cargo, thay `build/agv_core.exe` SHA256 `DEF64496FF8AA4DBE2308056A5F307DDF94F04310BE51E6120D4FB458BF2F3D4`, chạy lại scope active-both và gateway DB cũ. Phiên Core mới `1793324555161960300-4279368604`, PID17136, gateway4328; API Core/MQTT/storage khỏe, web phục vụ script mới. Kho revision450/K1=kệ1/K6=kệ2 giữ nguyên; counter chỉ tăng cấp phát, không xóa/rewind. Ca trước đang CLOSING tiếp tục CLOSING vì xe01 OFFLINE/thiếu proof; không cưỡng ép CLOSED hoặc tự xác nhận kho. Chưa lưu đối soát kho thực tế. Trạng thái runtime có thể đổi sau mốc này.
