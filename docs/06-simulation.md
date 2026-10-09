# Mô phỏng robot và hệ thống

Đã triển khai web clone với hai xe MQTT ảo ngày 02/10/2026, dùng chính Core/gateway và thuật toán hiện có. Chạy `./scripts/run-simulation.ps1`, mở `http://127.0.0.1:3100/simulation`; xem [cách dùng, số liệu và giới hạn](../simulation/README.md), [ADR-010](decisions/ADR-010-isolated-simulation.md). Phần phân loại dưới đây giữ định hướng S1/S2/S3 của bộ nền; runtime hiện tại là S1 có nội suy hiển thị, không phải mô phỏng vật lý.

Ngày 05/10/2026 thêm tác vụ Windows tự khởi động sau khi đăng nhập và giám sát để mở lại phiên nếu dịch vụ dừng; xem [ADR-018](decisions/ADR-018-simulation-autostart.md). Máy tắt nguồn thì web cục bộ không hoạt động.

Ngày 06/10/2026, yêu cầu mới đổi vòng đời: tác vụ đăng nhập chỉ giữ điểm mở trang 3100, mở trang mới khởi chạy mô phỏng và đóng tab cuối sẽ dừng phiên sau khoảng 20 giây. Xem [ADR-020](decisions/ADR-020-simulation-on-view.md).

Cũng trong ngày 05/10/2026, [đơn hàng nhiều kiện](../simulation/README.md) được thêm riêng cho web mô phỏng: một kho lấy, nhiều kho giao/số kiện, hai xe nhận chuyến theo lượt và có thể chạy đồng thời khi Core cho phép. Tiến độ đếm theo task_id hoàn thành và RFID đích; xem [ADR-019](decisions/ADR-019-simulation-orders.md). Chức năng này chưa được thêm vào API/server vận hành hoặc firmware.

## Ba mức mô phỏng

| Mức | Cách làm đề xuất | Chứng minh được | Giới hạn |
| --- | --- | --- | --- |
| S1 Hợp đồng và logic | Robot giả lập trên Node.js hoặc Python, MQTT như firmware | Command/ACK, telemetry, task, pin, lỗi mạng | Không xác nhận motor/cảm biến thật |
| S2 Động học | Mô hình 2D có chiều dài/rộng, vận tốc và quay | Occupancy trên cạnh, xung đột hình học, thời gian | Tham số cần đo; chưa có vật lý cảm biến đầy đủ |
| S3 Vật lý | ROS 2, Gazebo, URDF/SDF và ros_gz | Robot, cảm biến và môi trường chi tiết | Cần bộ phiên bản, tài nguyên máy, cấu hình thực |

Baseline ưu tiên S1. Simulator dùng cùng topic/payload tại [mạng](07-network.md), ACK trùng và trạng thái local giống firmware. Robot giả lập không ghi DB thật hoặc gửi topic vào broker có robot thật.

## Đặc tả simulator S1

- Hai client ID riêng, robot ID `01`/`02` theo fixture; map test riêng.
- Khởi đầu ở node cấu hình, pin cấu hình, thời gian đi cạnh từ weight và tốc độ test.
- Nhận GOTO, loại path sai, ACK hợp lệ, cập nhật node lần lượt.
- STOP dừng; WAIT giữ nguyên vị trí; CHARGE_START chỉ chấp nhận ở trạm đúng rule đã chốt.
- Command cùng ID trong phiên: ACK lại, không chạy lại đường.
- Inject mất ACK, trễ, telemetry đứt quãng, lỗi, vật cản và restart.
- Seed hoặc clock giả có thể tái lập; lưu log input/output và thời điểm.

Vận tốc và tốc độ hao pin test là tham số giả lập, không dùng làm thông số robot thật.

## Kịch bản cần có

1. Một robot hoàn thành nhiệm vụ qua nhiều node.
2. Hai robot có đường không xung đột.
3. Hai robot tranh một giao nhau, một xe phải chờ.
4. Vật cản xuất hiện khi xe đang đi; kiểm tra local stop và core replan riêng.
5. ACK đầu tiên bị mất, command gửi lại, xe chỉ thực hiện một lần.
6. Robot offline khi đang giữ zone; vùng được xử lý đúng chính sách đã chốt.
7. Core restart trong lúc robot còn online; kiểm tra ID/session và lệnh cũ.

## Khi chọn ROS 2 và Gazebo

Đọc tài liệu upstream để chọn ROS distribution, Ubuntu, Gazebo và `ros_gz` tương thích; ghi tag/commit trong ADR. Không trộn Gazebo Classic với Gazebo mới hoặc dùng `main` của mọi repo như một bộ tự động tương thích.

URDF/SDF cần thông số robot thật; TF dự kiến `map -> odom -> base_link` chỉ áp dụng khi mô hình định vị ROS được chọn. Nav2 dành cho navigation của robot; không thay thế mọi quyết định phân công task hay mutex của đội xe.

Nguồn: [ros_gz](https://github.com/gazebosim/ros_gz), [TurtleBot3 simulations](https://github.com/ROBOTIS-GIT/turtlebot3_simulations), [Nav2](https://github.com/ros-navigation/navigation2). Kỹ năng: [agv-simulation](../.agents/skills/agv-simulation/SKILL.md).
