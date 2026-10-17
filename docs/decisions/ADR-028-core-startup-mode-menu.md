# ADR-028 — Chọn chế độ thử Core tại terminal

> SUPERSEDED cho menu F5 từ 16/10/2026: [ADR-058](ADR-058-unified-core-active-robot-scope.md) đổi lựa chọn 1/2/3 thành scope của cùng Core với luật vận hành thật. Các mode trial tại đây chỉ còn tương thích hồi quy.

Ngày: 09/10/2026
Trạng thái: đã chốt theo REQ-CORE-MENU-01

## Bối cảnh

Người dùng chạy F5 và muốn chọn xe 01, xe 02 hoặc hai xe khi mở server. Bản solo cũ chỉ hỗ trợ 01 và firmware có đường tắt mutex; không dùng cơ chế đó cho hai firmware bench hiện tại.

## Lựa chọn

Core mở menu trên stdin tương tác; F5 truyền `--select-mode` để luôn hiện menu. Chọn `1/2/3` tương ứng `robot01/robot02/both`; `4` là vận hành thường, `0` hoặc EOF thoát trước khi giữ cổng/ghi counter/kết nối MQTT. Nhập sai hỏi lại. `--mode robot01|robot02|both|normal` dùng cho kiểm thử/tự động, không cần nhập. Tiến trình không có stdin tương tác vẫn dùng cấu hình môi trường cũ.

Chế độ chọn một xe là thử có mutex, riêng với legacy `AGV_ONE_ROBOT_TRIAL`. Core chỉ giao task/GOTO cho ID được chọn; vẫn nhận telemetry và giữ vị trí/đường chưa xác minh của xe khác. Xe khác đã xuất hiện phải online, dừng/rảnh tại trạm riêng, hết task/đường/vùng trước khi mở ca một xe. Không tự chuyển động để đưa xe đó về trạm. Cả ba chế độ thử không dùng pin điều khiển để phân công và không tự về sạc, giữ giới hạn của thử bench hiện hành.

## Hệ quả và kiểm chứng

Giữ Dijkstra, FIFO, MQTT command/telemetry, file map/counter/runtime/shift và cấu trúc thư mục. Đổi chế độ bằng cách dừng rồi mở lại Core; không reset dữ liệu hoặc chạy hai Core chung counter. Menu không tự mở ca hoặc gửi GOTO. Các trường WS bổ sung `test_mode`, `trial_robot_id`, `supervised_trial` giúp web hiển thị đúng và giới hạn lựa chọn xe.

Kiểm thử unit và broker/Core thật với stdin piped: menu/nhập sai/EOF, một xe 01/02, từ chối GOTO xe khác, auto-task đúng ID, xe khác chưa xác minh chặn mở ca, hai xe và mutex, restart giữ counter. Kết quả phần mềm không thay thử hai xe thật; firmware vẫn phải khớp hợp đồng mutex và điều kiện chuyển động hiện tại.
