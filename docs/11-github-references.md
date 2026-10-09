# Repo GitHub và phạm vi tham khảo

Tra cứu trực tiếp repo/tài liệu chính chủ ngày 28/09/2026. Danh sách phục vụ lựa chọn thiết kế; chưa clone hay nhập code. Không dùng số sao làm bằng chứng phù hợp. Chưa pin tag/commit vì chưa tích hợp upstream.

| Repo | Dùng để nghiên cứu | Áp dụng cho workspace | License tại thời điểm tra cứu |
| --- | --- | --- | --- |
| [open-rmf/rmf_demos](https://github.com/open-rmf/rmf_demos) | Fleet adapter, nhiệm vụ, demo nhiều robot | Tách điều phối khỏi API robot, kịch bản giao nhau | Apache-2.0 |
| [open-rmf/rmf-web](https://github.com/open-rmf/rmf-web) | Dashboard, API server/client | Tổ chức dữ liệu và giao diện, tham khảo module | Apache-2.0 |
| [AtsushiSakai/PythonRobotics](https://github.com/AtsushiSakai/PythonRobotics) | Dijkstra, A star, tracking, ví dụ robotics | Baseline và test thuật toán; không mặc định production | MIT |
| [ros-navigation/navigation2](https://github.com/ros-navigation/navigation2) | Navigation ROS 2 | Nhánh mô phỏng/định vị khi có nhu cầu | Hỗn hợp theo package.xml; file không đánh dấu có mặc định Apache-2.0 |
| [gazebosim/ros_gz](https://github.com/gazebosim/ros_gz) | Cầu ROS và Gazebo | Bridge trong mô phỏng S3 | Apache-2.0 |
| [ROBOTIS-GIT/turtlebot3_simulations](https://github.com/ROBOTIS-GIT/turtlebot3_simulations) | Robot/world/launch mô phỏng | Cách tổ chức mô hình; robot của đề tài cần thông số riêng | Apache-2.0 |
| [eclipse-mosquitto/mosquitto](https://github.com/eclipse-mosquitto/mosquitto) | Broker và thư viện C/C++ | Phù hợp libmosquitto core đang dùng | EPL-2.0 OR BSD-3-Clause theo LICENSE upstream |
| [espressif/esp-mqtt](https://github.com/espressif/esp-mqtt) | MQTT client, TLS, QoS, reconnect | Lựa chọn nếu dùng ESP-IDF | Apache-2.0 |
| [knolleary/pubsubclient](https://github.com/knolleary/pubsubclient) | MQTT client Arduino | Lựa chọn thử nghiệm Arduino; kiểm tra buffer và QoS | MIT |
| [mqttjs/MQTT.js](https://github.com/mqttjs/MQTT.js) | MQTT client Node.js/browser | Simulator Node.js nếu được chọn | MIT |

## Điểm đọc cụ thể

- RMF demos: `rmf_demos_fleet_adapter`, `docs/faq.md`, demo giao nhiệm vụ. Bắt đầu tại [fleet manager](https://github.com/open-rmf/rmf_demos/blob/main/rmf_demos_fleet_adapter/rmf_demos_fleet_adapter/fleet_manager.py).
- RMF web: `packages/api-server`, `packages/api-client`, `packages/rmf-dashboard-framework`; ngày 01/10/2026 xem [demo/main.tsx](https://github.com/open-rmf/rmf-web/blob/main/packages/rmf-dashboard-framework/examples/demo/main.tsx) để tham khảo phân khu map/robots/tasks, không nhập mã.
- PythonRobotics: `PathPlanning/Dijkstra`, `PathPlanning/AStar`, `PathTracking`; đối chiếu đơn vị và mô hình với graph đề tài.
- Nav2: package/README tương ứng planner/controller/costmap, `package.xml` của package định sử dụng. [LICENSE](https://github.com/ros-navigation/navigation2/blob/main/LICENSE) yêu cầu kiểm tra license theo package.
- ros_gz: `ros_gz_bridge` và bảng tương thích của branch được chọn.
- TurtleBot3 simulations: `turtlebot3_gazebo`, README và branch phù hợp ROS.
- Mosquitto: [README](https://github.com/eclipse-mosquitto/mosquitto/blob/master/README.md), cấu hình listener, client library và [LICENSE](https://github.com/eclipse-mosquitto/mosquitto/blob/master/LICENSE.txt).
- ESP MQTT: README và examples trong repo; PubSubClient: README về QoS/buffer; MQTT.js: README về client và reconnect.

## Cách dùng nguồn khi viết mã

Ghi repo, URL tệp, tag/commit cụ thể, đoạn chức năng dùng, file đích và license khi thực sự chép hoặc sửa code. Giữ notice cần thiết theo nguồn. Không gán một license duy nhất cho repo nhiều package như Nav2; không coi license README là đủ cho mọi dependency.

Các ý tưởng áp dụng trong bộ nền là suy luận thiết kế của workspace. Chúng chưa thể hiện rằng upstream đã triển khai đúng giao thức riêng của dự án, hoặc mọi repo tương thích với nhau trên branch main.

Xem [ghi nhận nguồn](../THIRD_PARTY_REFERENCES.md).
