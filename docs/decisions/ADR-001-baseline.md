# ADR 001 Nền kiến trúc cho workspace

Ngày: 28/09/2026. Trạng thái: lựa chọn nền cho bộ tài liệu; chưa chốt mọi công nghệ triển khai.

## Bối cảnh

Đề tài cần hai robot đi tuyến định trước và web giám sát. Workspace có core C++, Node.js gateway, SQLite, HTML/JS, MQTT và graph RFID/node; chưa có firmware hoặc simulator được xác minh.

## Lựa chọn nền

Bộ tài liệu bám stack hiện có. Markdown là nguồn tài liệu dễ sửa; Word là bản xuất để đọc/chia sẻ. Kỹ năng cài ở `.agents/skills` của dự án. Nhánh mô phỏng đầu tiên đề xuất robot giả lập MQTT; ROS 2/Gazebo mở khi có mục tiêu kiểm chứng vật lý.

## Hệ quả

Tiếp tục trên mã hiện tại giảm việc đổi kiến trúc khi chưa có yêu cầu. Phải xử lý các GAP build/contract trước tích hợp. Cảm biến, bo mạch, cấu hình mạng, điều phối vật lý và chỉ tiêu realtime vẫn OPEN.

## Điều kiện xem lại

Yêu cầu mới về SLAM, nhiều robot hơn, cloud, điều khiển vật lý, công nghệ nhóm đã chọn hoặc giới hạn phần cứng có thể thay lựa chọn nền. Ghi ADR tiếp theo và cập nhật tài liệu; không ghi đè lịch sử quyết định cũ.
