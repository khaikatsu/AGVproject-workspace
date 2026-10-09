---
name: agv-server
description: "Phát triển gateway Express, SQLite, core C++ và vòng đời nhiệm vụ của workspace AGV. Dùng khi sửa API, lưu trữ hoặc điều phối server; giao diện dùng agv-web."
---

# Phát triển server AGV

Đọc `AGENTS.md`, `docs/02-current-system.md`, `docs/04-server.md` và yêu cầu liên quan. Đường dẫn tài liệu tính từ gốc workspace.

- Tách core owner map/fleet/mutex với gateway owner lịch sử. `AGVwebserver/server.js` CommonJS, `db.js` mở SQLite ngay khi import; test cần DB riêng trước khi khởi chạy.
- Kiểm tra endpoint và handler core thành cặp. Ưu tiên căn cứ implementation, không dùng comment “đã sửa” như kết quả kiểm thử.
- GAP hiện tại: gateway gửi `load_map_data` chưa có handler, suy luận hoàn thành từ `target`, seq/cmd counter reset theo tiến trình. Giải quyết đúng phạm vi yêu cầu; không gom mọi GAP vào một thay đổi nhỏ.
- Với task/map, thiết kế request ID, kết quả core và thứ tự cập nhật DB để không báo hoàn thành khi chỉ gửi socket. Không thêm migration hoặc sửa DB vận hành để thử tác vụ.
- C++ source nằm `src/`; tên header có sai khác case. `AGVtest.cpp` là JavaScript; không compile tất cả cpp vào một target. Core cần working directory gốc cho `data/map_layout.json`.
- Trước khi sửa đa luồng, kiểm tra lock order và callback publish/notify; tránh gọi lại vào module đang giữ lock. Phần điều phối đường đọc thêm `docs/05-algorithms.md`.

Đọc `docs/07-network.md` nếu thay payload, ACK hoặc WS. Kiểm tra lỗi đầu vào, core mất kết nối, duplicate/hủy/restart và dữ liệu history theo thay đổi. Chỉ báo build/runtime thành công khi đã chạy kiểm tra tương ứng; tài liệu hiện trạng là khảo sát tĩnh.
