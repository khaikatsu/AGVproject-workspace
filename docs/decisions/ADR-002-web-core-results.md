# ADR-002 — Core xác nhận thao tác web và sở hữu trạng thái nhiệm vụ

Ngày: 29/09/2026. Trạng thái: IMPLEMENTED theo yêu cầu trực tiếp sửa web/server trong lúc chờ firmware.

## Bối cảnh

Gateway ghi thành công khi gửi socket và đoán hoàn thành theo target. Web làm mất kiểu array của response, chưa hiển thị route/history đầy đủ. Snapshot tự khôi phục có thể ghi đè graph mà không được core xác nhận.

## Quyết định

Giữ C++17, CommonJS/Express/ws/better-sqlite3 và HTML/JS. Tách gateway factory để test DB riêng; server.js vẫn là entrypoint. Core WS phiên bản 1 có core_session_id và request_id/result; task_id chỉ dùng giữa web/gateway/core/DB, không đổi payload MCU. Core là nguồn graph/task/fleet/mutex.

Gateway chờ result trước HTTP thành công và chỉ lưu obstacle/snapshot của thao tác core đã xác nhận. Timeout/mất socket báo unknown; không retry tự động lệnh di chuyển. Snapshot do người vận hành áp dụng; reconnect không tự apply.

Trong lúc chưa có xác nhận dừng từ firmware, sửa cấu trúc map cần fleet/queue/zone owner trống. Mất mạng/ACK/lỗi giữ vùng và ngắt nhiệm vụ, không tự giao lại. Counter command lưu trên đĩa trước khi cấp block, cùng hợp đồng ACK số đang có. SQLite thêm định danh/trạng thái/event; giữ bản ghi lịch sử cũ.

## Hệ quả và kiểm chứng

Phải chạy core mới cùng gateway mới; core cũ chỉ đọc. Có bộ kiểm thử liên kết tại [hồ sơ](../../tests/README.md). UI báo nhiệm vụ core chấp nhận, không suy ra robot đã hoàn thành từ HTTP. Cần xác minh thực nghiệm với firmware và thiết kế phục hồi sau reboot robot/core trước tích hợp phần cứng. Không xem logic mutex đã test là chứng minh tránh mọi va chạm vật lý.
