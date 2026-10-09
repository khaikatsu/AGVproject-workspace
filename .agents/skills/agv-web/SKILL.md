---
name: agv-web
description: "Phát triển dashboard AGV, bản đồ kho, REST client và WebSocket trình duyệt trong workspace này. Dùng cho phần giao diện, không dùng riêng cho firmware hoặc thuật toán core."
---

# Phát triển dashboard AGV

Đọc `AGENTS.md`, `docs/03-web.md` và yêu cầu liên quan trong `docs/requirements/register.md`. Các đường dẫn này tính từ gốc workspace, ba cấp phía trên thư mục kỹ năng.

- Bắt đầu ở `AGVwebserver/index.html`; xác minh endpoint trong `server.js`. Nền hiện tại HTML/JS, không tự thay bằng framework mới.
- Theo dõi state `map/fleet/mutex/logs`; `agv_connected` chỉ nói liên kết gateway-core. Tuổi dữ liệu robot cần metadata riêng, không dùng trạng thái socket làm online của mọi xe.
- Vị trí node/RFID là số đo rời rạc. Nếu animate trên cạnh, giải thích đó là ước lượng và giữ tọa độ bản đồ độc lập với pixel.
- HTTP 202 chưa xác nhận core đã nhận hoặc robot đã hoàn thành. Đọc `docs/04-server.md` khi sửa thao tác task/map; dùng kết quả core và ID tương quan nếu được triển khai.
- Chưa có handler core cho `load_map_data`; không tuyên bố áp dụng snapshot thành công chỉ từ API hiện tại.
- Hiển thị log/node bằng text an toàn, xử lý state thiếu trường, mất WS và reconnect. Giữ các thao tác điều khiển bám khả năng robot thật đã được mô tả.

Kiểm tra các trạng thái liên quan bằng fixture/mock hoặc môi trường test: hai xe, lỗi, offline, pin thấp, payload thiếu và resize. Không mở app đang ghi DB thật chỉ để thử giao diện khi có thể dùng môi trường riêng. Cập nhật tài liệu theo thay đổi và nêu kiểm tra thực tế đã làm.

Nguồn ý tưởng dashboard: `docs/11-github-references.md`, mục rmf-web. Tham khảo tổ chức module, không nhập stack RMF vào workspace nếu chưa có căn cứ.
