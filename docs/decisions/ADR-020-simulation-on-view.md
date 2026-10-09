# ADR-020 — Chạy mô phỏng khi có người xem

Ngày 06/10/2026. Trạng thái: triển khai theo REQ-SIM-07.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn web mô phỏng chỉ bật khi vào xem, đóng trình duyệt thì dừng. Yêu cầu mới thay phần tự chạy lại liên tục của REQ-SIM-03.

`OBSERVED`: tác vụ Windows cũ mở Core, broker, gateway, hai xe sau đăng nhập và kiểm tra mỗi phút. Đóng tab không đóng tiến trình; dừng thủ công sẽ bị tác vụ mở lại.

## Quyết định

- Tác vụ đăng nhập chỉ mở `simulation/launcher.js` trên loopback 3100. Không tự mở trình duyệt, không chạy Core/broker. Gỡ tác vụ `AGV Simulation Health Check`.
- Khi trang 3100 được mở, launcher tạo một phiên mô phỏng tại cổng loopback riêng và chuyển tab tới đó. Nhiều tab cùng dùng một phiên. Khi phiên dừng, lần mở 3100 tiếp theo tạo phiên và dữ liệu riêng.
- Mỗi tab gửi tín hiệu còn xem mỗi 4 giây và báo rời trang bằng beacon. Server hết hạn tín hiệu sau 12 giây, dừng phiên sau khoảng 20 giây kể từ tín hiệu cuối nếu không còn tab. Đây cũng xử lý tắt trình duyệt đột ngột khi sự kiện rời trang không gửi được. Dừng phiên dùng `close()` hiện có để ghi report và đóng gateway, Core, broker.
- Điểm mở trang 3100 vẫn chạy nhẹ khi không có tab, để URL cố định có thể khởi động mô phỏng bằng trình duyệt. Đóng trình duyệt không tắt điểm mở trang.

## Giới hạn

`VERIFIED`: `tests/simulation.on-view.js` dùng Core/broker và thư mục `.qa/` riêng: hai tab chia sẻ phiên, tab còn lại giữ phiên, hết tab ghi report và dừng, mở lại tạo phiên mới. Tác vụ Windows `AGV Simulation Auto Start` đã cập nhật ở trạng thái Ready; tác vụ `AGV Simulation Health Check` cũ không còn. Điểm mở trang 3100 báo `active:false` khi không có tab. Chưa thử đóng Edge bằng tay hoặc reboot máy; bộ browser test cũ chưa chạy vì môi trường thiếu Playwright.


Trình duyệt ngủ hoặc tạm dừng JavaScript đủ lâu cũng làm phiên dừng. URL cổng phiên trong thanh địa chỉ chỉ tồn tại đến khi phiên dừng; luôn mở lại URL 3100. Dừng khi đóng tab là dừng mô phỏng S1 cục bộ, không mang ý nghĩa STOP cho robot thật. Không thay payload MQTT hoặc API vận hành.
