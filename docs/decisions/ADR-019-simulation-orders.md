# ADR-019 — Đơn hàng nhiều kiện trên web mô phỏng

Ngày 05/10/2026. Trạng thái: đã triển khai và kiểm chứng bằng mô phỏng S1.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn nhập một đơn lấy nhiều kiện ở một kho rồi giao tới nhiều kho; server dùng cả hai xe, đi thay phiên hoặc đồng thời nếu không cản đường.

`OBSERVED`: simulator đang có chuỗi một kiện `cargoJob(robot,pickup,dropoff)`; Core C++ tự tìm đường Dijkstra và quản lý đường đang chiếm/mutex. Simulator chỉ mô phỏng xe điểm, hàng ảo và RFID ảo. API/lược đồ của server vận hành chưa có đơn hàng nhiều kiện.

## Quyết định

- Chỉ mở rộng server và giao diện mô phỏng. Đơn hàng gồm kho lấy, danh sách `{dropoff, quantity}` theo thứ tự nhập; mỗi kiện thành một chuyến lấy/giao bằng `cargoJob` hiện có. Mỗi xe ảo chở tối đa một kiện mỗi chuyến. Giới hạn 20 kiện/đơn để giao diện và phiên thử hữu hạn.
- Lớp đơn hàng xét các kiện theo thứ tự ưu tiên trong danh sách, nhiều nhất hai chuyến đang hoạt động. Nếu xe phụ trách kho đích của kiện sớm hơn đang bận, xét tiếp kiện ở kho đích khác để xe rảnh có thể làm song song. Trong các xe rảnh, ưu tiên xe đã nhận ít chuyến hơn trong đơn; khi cân bằng, chọn tổng chi phí Dijkstra từ vị trí hiện tại tới kho lấy và từ kho lấy tới đích nhỏ nhất, hòa thì chọn ID xe. Một kho đích giữ cùng xe phục vụ trong suốt đơn để xe khác không bị chặn tại bến giao; không cấp việc cho xe nếu xe kia đang đứng ở kho đích đó. Core tiếp tục lập tuyến thực tế và giữ quyền đường/mutex. Nếu Core từ chối một chặng vì đường tạm bị xe khác chiếm, kiện vẫn chờ và thử lại sau một giây, trong khi kiện ở kho khác vẫn có thể được xét; không báo lỗi vĩnh viễn hoặc tạo hàng ảo. Không tự giả định tránh va chạm vật lý.
- Chỉ ghi một kiện đã giao khi cả task Core và RFID đích được chuỗi hàng xác nhận. Nếu chuyến lỗi/gián đoạn, giữ kiện ở trạng thái chưa giao và ngừng cấp việc mới cho đơn để người vận hành đối soát; không tự chạy lại kiện có thể đang trên xe.
- Danh sách điểm giao quy định thứ tự **ưu tiên**. Giữ thứ tự kiện trong cùng một kho giao; việc cấp và hoàn tất kiện ở các kho khác nhau có thể lệch thứ tự nhập để hai xe cùng làm. Quy tắc bắt buộc giao xong điểm A trước khi giao điểm B là yêu cầu khác, chưa được chốt.

## Giới hạn

`VERIFIED`: `tests/simulation.orders.js` dùng Core C++/broker/DB riêng trong `.qa` giao đủ năm kiện K1→K2×2,K4×3 bằng cả hai xe; từng chuyến có task_id hoàn thành và quan sát được **hai xe cùng ở trên cạnh đường trong một đơn**. Hai đơn khác kho cũng chạy chồng thời gian trên hai xe. Một xe hết pin làm đơn cần đối soát và không tăng số kiện giao. `tests/simulation.orders.browser.js` chạy Edge headless xác nhận form, tiến độ 5/5, không lỗi JS và không tràn ngang trên màn hình 390 px. Chưa chạy xe thật.

`OPEN`: kích thước/tải trọng hàng thật, vị trí lấy/trả chính xác, thời gian thao tác và khả năng chạy đồng thời an toàn của hai robot thật chưa có số đo. Mô phỏng S1 không nghiệm thu va chạm hình học hoặc motor thật.
