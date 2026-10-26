# ADR-087 — Hoán đổi tên kho/trạm tại gateway

Ngày: 26/10/2026. Trạng thái: IMPLEMENTED trong source theo REQ-WEB-LOCATION-NAMES-01; kiểm chứng phần mềm tại tests/README.md, chưa thay gateway vận hành hoặc chạy xe thật.

SOURCE: người dùng duyệt phương án giữ nguyên UID, đường line và ID vị trí mà firmware hiểu; server quản lý tên/phân công kho và dịch đích về vị trí cố định. Yêu cầu đặt thao tác tại Chỉnh cấu trúc trong trang Bản đồ.

SOURCE bổ sung 26/10/2026, REQ-WEB-LOCATION-NAMES-02: xóa toàn bộ phần Cấu trúc vị trí cố định gồm form node/cạnh và các handler browser; mục Chỉnh cấu trúc chỉ còn hoán đổi tên kho/trạm. API sửa graph và chức năng snapshot không thuộc yêu cầu xóa này.

SOURCE bổ sung cùng ngày, REQ-WEB-LOCATION-NAMES-03: rút giao diện về tiêu đề, hai ô chọn và nút Cập nhật; xóa hẳn các đoạn giải thích và dòng xem trước. Lý do khóa nằm ở tooltip nút; quy tắc server giữ nguyên. Mô tả form có xem trước trong quyết định ban đầu phía dưới là lịch sử trước yêu cầu này.

Quyết định: một song ánh tên cho K1–K6 và một cho S1/S2; chỉ hoán đổi trong cùng nhóm. Form chọn hai tên, xem trước vị trí rồi Cập nhật. SQLite gateway lưu cấu hình riêng, không ghi graph/snapshot Core. HTTP/WS thêm `location_names:{revision,labels,updated_at}` với labels là ID vật lý → tên sử dụng. HTTP thao tác nhận `location_names_revision` để opt-in hiểu target/pickup/dropoff/warehouse là tên; gateway kiểm revision rồi dịch về ID cố định trước khi gọi dịch vụ/Core. API cũ bỏ trường này tiếp tục nhận ID vật lý. Firmware/MQTT không thay đổi.

Chỉ cập nhật khi Core mới, ca CLOSED, hết task/hàng đợi/lệnh đang chờ, đơn/thao tác kho đã kết thúc hoặc đối soát, không còn owner vùng và xe đang rảnh. CAS revision chặn hai tab ghi đè/retry; bản cũ bị từ chối trước khi giao nhiệm vụ. Lỗi lưu không thay ánh xạ RAM. Giữ kệ, tồn kho, UID, hướng, geometry, đường đi và trạm riêng 01/02 theo ID vật lý. Đổi tên S1/S2 chỉ đổi nhãn: trạm về không chuyển sang vị trí khác.

Tên hiện tại dùng cho map, lựa chọn/giao việc, thẻ xe và tồn kho; tooltip/chi tiết ghi ID vị trí khi tên khác. Đơn và task mới lưu ảnh ánh xạ tại lúc tạo/ghi nhận đầu tiên; lịch sử cũ không đoán đổi tên. Báo cáo kho và log kỹ thuật cũ vẫn dùng ID cố định, ghi rõ cách đọc. Snapshot map không chứa và không tự phục hồi ánh xạ tên.

OPEN: thay vai trò kho thành trạm sạc, chuyển trạm riêng sang vị trí khác hoặc thay graph/line thuộc thiết kế khác. Không có cập nhật cấu hình MQTT trong phương án này. Kết quả kiểm thử ghi ở tests/README.md.
