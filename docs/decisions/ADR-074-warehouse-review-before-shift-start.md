# ADR-074 — Nhập và kiểm tra vị trí kệ ngay khi mở ca

Ngày 21/10/2026. Trạng thái: IMPLEMENTED giao diện; kiểm chứng bằng Core giả lập và SQLite riêng, chưa thử xe thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn nhập trực tiếp dữ liệu kho trong bước mở ca và đã duyệt phương án kiểm tra/sửa vị trí kệ → lưu kho → bắt đầu ca.

`OBSERVED`: [ADR-070](ADR-070-warehouse-racks-and-shift-reports.md) đã có API cấu hình kho khi CLOSED, kiểm tra mã kệ/revision/xác nhận thực tế và báo cáo đầu/cuối ca. Trước thay đổi này, nút bắt đầu ca gửi lệnh trực tiếp; form nhập kho ở một phần mở rộng khác, dễ bị bỏ sót.

## Quyết định

- Nút **Mở ca** mở hộp kiểm tra vị trí K1–K6 có trong map. Điền sẵn dữ liệu đã lưu; lần đầu để trống, không gán kệ mặc định. K là vị trí cố định, R là ví dụ mã kệ riêng; ô rỗng nghĩa không có kệ.
- Mỗi lần mở hộp đều phải xác nhận đã kiểm tra vị trí kệ thực tế và các xe trống. **Xác nhận và bắt đầu ca** lưu qua `/api/warehouse/configure`, chỉ sau thành công mới gọi `/api/shift/start`. Trong bước nhập/lưu, ca vẫn CLOSED; không cho chỉnh kho trong RUNNING/CLOSING.
- Hủy trước xác nhận không lưu và không mở ca. Khi request đang chờ, khóa chỉnh sửa, nút xác nhận, nút đóng và Escape để tránh thao tác chồng. Lỗi lưu không phát lệnh mở ca. Lưu thành công nhưng mở ca lỗi/không rõ kết quả giữ nội dung và báo riêng hai kết quả; không rollback kho hoặc tự gửi lại lệnh.
- Dữ liệu cũ/mất MQTT khóa xác nhận. Đổi Core session hoặc revision kho trong lúc nhập yêu cầu đóng rồi mở lại, không tự tải đè nội dung đang nhập. Server vẫn là nơi kiểm tra cuối cùng các chốt hiện có; giao diện không nới quyền.
- Giữ form đối soát riêng khi CLOSED và toàn bộ API/Core/MQTT/firmware hiện có. Không biến việc nhập kho thành điều kiện mới cho mọi client gọi `/api/shift/start`.

## Giới hạn và kiểm chứng

Hai request lưu kho/mở ca không phải transaction xuyên gateway–Core. Một client khác có thể đổi trạng thái giữa hai request; Core/gateway tiếp tục kiểm tra điều kiện hiện tại. Không tuyên bố khóa nguyên tử ảnh kho với lệnh mở ca trên mọi client. Timeout/unknown yêu cầu đối chiếu ca và kho trước khi thử lại.

Kiểm thử Edge headless desktop/mobile: hủy không ghi, bắt buộc xác nhận, mã kệ trùng không mở ca, thứ tự lưu/mở, Core từ chối mở sau khi lưu, dữ liệu điền sẵn, revision/phiên thay đổi, mất MQTT, chống submit trùng, ảnh đầu ca lấy từ dữ liệu đã lưu. Hồi quy dashboard và kho dùng DB riêng. Vị trí kệ vẫn là xác nhận của người vận hành/servo, chưa có cảm biến tải thật.
