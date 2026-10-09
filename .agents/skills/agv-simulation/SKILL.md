---
name: agv-simulation
description: "Xây robot giả lập MQTT, mô phỏng động học hoặc ROS 2 Gazebo cho workspace AGV. Dùng khi kiểm chứng hệ thống bằng mô phỏng, không coi kết quả giả lập là kiểm thử phần cứng thật."
---

# Mô phỏng AGV

Đọc `AGENTS.md`, `docs/06-simulation.md`, `docs/07-network.md` và mục tiêu kiểm chứng. Tài liệu tính từ gốc workspace. Code mô phỏng đặt `simulation/`.

- Chọn mức đủ trả lời câu hỏi: S1 hợp đồng/logic, S2 hình học/động học, S3 vật lý/cảm biến. Nền khuyến nghị S1; không cài ROS chỉ vì có từ “robot”.
- Robot giả lập phải subscribe/publish giống firmware hiện tại: telemetry text, command JSON, ACK text, mutex grant text. Fixture tại `contracts/examples/current-mqtt.json`.
- Test dùng broker/DB/map riêng. Có hai client ID, vị trí và pin cấu hình, command dedup, clock/seed tái lập và log input/output.
- Tạo inject lỗi đúng câu hỏi: mất ACK, trễ telemetry, restart, vật cản, offline trong zone. Một message ACK không đại diện hoàn thành task.
- Khi mô phỏng chiếm chỗ, dùng kích thước/tốc độ/turning đã cung cấp, ghi rõ giả định. Không rút quãng dừng vật lý từ mô phỏng event thuần túy.
- ROS/Gazebo cần chọn bộ distribution/branch tương thích từ upstream và ghi ADR. Không trộn Classic/mới hoặc clone main của mọi repo rồi giả định tương thích.

Kiểm tra luồng web-core-robot bằng một và hai robot, command trùng, zone và phục hồi. Báo rõ mức mô phỏng và giới hạn. Các nguồn Nav2/ros_gz/TurtleBot3 nằm `docs/11-github-references.md`; mô hình TurtleBot không thay thông số robot của đề tài.
