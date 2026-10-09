# ADR-018 — Tự khởi động web mô phỏng sau khi đăng nhập Windows

Ngày 05/10/2026. Trạng thái: đã triển khai trên máy hiện tại; chưa kiểm tra bằng chu kỳ tắt/bật máy thật.

Ngày 06/10/2026, phần tự mở và giám sát phiên được [ADR-020](ADR-020-simulation-on-view.md) thay thế theo yêu cầu mới. Tài liệu dưới đây giữ lịch sử quyết định cũ.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn web mô phỏng luôn sẵn sàng, kể cả sau khi tắt rồi bật máy.

`OBSERVED`: web mô phỏng chạy cục bộ ở `127.0.0.1:3100`; runner cũ chạy trong terminal và dừng theo phiên terminal. Mỗi lần chạy tạo một thư mục `simulation/runs/run-*` cùng broker, Core và DB riêng. Không có cơ chế tự khởi động sau Windows login hoặc tự mở lại khi tiến trình dừng. Không thể truy cập localhost khi máy đã tắt nguồn.

## Quyết định

- Tạo Scheduled Task `AGV Simulation Auto Start` cho người dùng Windows hiện tại, trigger khi đăng nhập, chạy PowerShell ẩn một lượt và mở trang mô phỏng trong trình duyệt mặc định. Tác vụ `AGV Simulation Health Check` chạy một lượt mỗi phút, lặp vô thời hạn khi tài khoản đã đăng nhập. Không yêu cầu mật khẩu hoặc đặc quyền quản trị.
- Script kiểm tra cổng 3100 và thử kết nối TCP tới cổng Core/broker riêng của phiên; phiên khỏe được giữ nguyên, cổng web bị ứng dụng khác chiếm không bị can thiệp. Nếu web không có, script mở `simulation/server.js` với Node hệ thống, working directory là gốc workspace và bỏ biến môi trường vận hành `AGV_*` khỏi tiến trình con. Nếu Core/broker không sẵn sàng quá 45 giây, lượt kiểm tra yêu cầu simulator đóng phiên để ghi báo cáo rồi mở phiên sạch. Cách chạy từng lượt tránh phụ thuộc vào một tiến trình giám sát lâu dài.
- Tác vụ chỉ hoạt động sau khi người dùng đăng nhập. Sau reboot, phiên mới bắt đầu ở cấu hình mặc định; không tự tiếp tục trạng thái chuyển động cũ. Dữ liệu các phiên trước được giữ lại. Web vẫn chỉ bind loopback; port 3000/8080/1883 và DB/counter vận hành không bị tác vụ chiếm.
- Bổ sung 05/10/2026 sau sự cố hai xe ảo `OFFLINE` trong khi web/Core/broker vẫn chạy: runtime tự thử kết nối và xác nhận RFID/pin với Core **chỉ** khi xe đã từng online, đang đứng ở trạm riêng, không có tuyến, hàng, grant/vùng hoặc lỗi; chuỗi phục hồi vẫn yêu cầu trạng thái Core và heartbeat mới rồi mới bỏ tạm dừng. Thao tác **Ngắt MQTT** chủ động hoặc **Tạm dừng** không bị tự hoàn tác. Web hiển thị mất kết nối xe ảo thay vì dùng riêng trạng thái broker để báo sẵn sàng. Xe đang chạy dở không tự phục hồi vị trí.

## Kiểm chứng và giới hạn

`VERIFIED`: script một lượt nhận diện phiên 3100 đang chạy mà không tạo phiên trùng; script mở phiên riêng ở 3101 thành công ngoài sandbox. Bản giám sát liên tục ban đầu từng tạo phiên mới ở 3101, nhưng tác vụ Windows dài hạn sau đó bị dừng với mã `0xC000013A`, nên đã thay bằng hai tác vụ chạy một lượt. Tác vụ kiểm tra mới có chu kỳ `PT1M`, không có thời điểm hết hạn, lượt chạy trả mã 0; sau khi chủ động đóng phiên 3100, lượt định kỳ đã tự mở phiên mới và báo hai robot `Sẵn sàng`. Ca thử cổng 3101 dừng đúng PID Core: web còn mở nhưng cổng Core đã đóng; lượt kiểm tra phát hiện rồi tạo phiên mới sẵn sàng. Lần thử mở Core/broker trong sandbox bị `spawn EPERM`; thử ngoài sandbox thành công. Chưa thử restart máy thật.

`VERIFIED` bổ sung: `tests/simulation.mqtt-recovery.js` dùng Core/broker riêng, đóng bất ngờ hai socket MQTT tại S1/S2 rồi xác nhận cả hai xe online, có vị trí hợp lệ và bỏ tạm dừng; thao tác Ngắt MQTT chủ động vẫn giữ xe offline cho tới khi phục hồi thủ công.

`OPEN`: khi máy tắt nguồn, web localhost không thể phục vụ truy cập. Nếu cần truy cập 24/7 từ máy khác, phải chuyển dịch vụ sang máy chủ luôn bật và thiết kế lại truy cập mạng/bảo mật. Đường dẫn Node và workspace hiện cố định cho máy này; khi di chuyển/cài lại cần chạy lại installer hoặc sửa cấu hình. Phiên lỗi bị khởi động sạch nên thao tác đang dở không tiếp tục tự động.
