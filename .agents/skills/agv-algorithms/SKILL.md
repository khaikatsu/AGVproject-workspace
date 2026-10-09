---
name: agv-algorithms
description: "Thiết kế và kiểm chứng tìm đường, phân công, pin và mutex cho hai robot AGV trên đồ thị kho. Dùng cho thuật toán core, không dùng riêng cho giao diện hay MQTT client."
---

# Thuật toán đội robot AGV

Đọc `AGENTS.md`, `docs/05-algorithms.md`, bản đồ và yêu cầu liên quan. Tệp chính là `src/MapGraph.hpp`, `src/FLeetmanager.hpp`, `src/Mutexarbiter.hpp`; đường dẫn tính từ gốc workspace.

- Dijkstra là baseline hiện có. Xác minh hướng cạnh, trọng số không âm, đơn vị và node chặn. So tổng trọng số, không so số node cho mục tiêu đường ngắn.
- A star cần heuristic admissible theo đơn vị weight; tọa độ pixel không tự là khoảng cách cm. Giữ baseline để đối chiếu khi đổi planner.
- Kiểm tra lựa chọn trạm sạc, khả năng đi tới và đặt chỗ; không xem ngưỡng pin trong mã là chỉ tiêu phần cứng đã chốt.
- Hai xe có thể xung đột ở node, cạnh ngược chiều, quay và vùng hẹp. Mutex owner tại node không tự chứng minh tránh va chạm vật lý. Đọc kích thước robot và vùng chiếm chỗ từ yêu cầu.
- Mất telemetry không đồng nghĩa xe đã rời zone. Mặc định release khi offline trong mã hiện tại là vấn đề cần đánh giá theo chính sách được chọn.
- Phân công/tie break phải xác định và công bằng theo yêu cầu. Nếu cần nhiều zone, phân tích thứ tự nhận quyền, starvation và deadlock trước khi thêm policy.

Dùng đồ thị nhỏ để kiểm chứng đường không liên thông, one way, đường dài ít node và đường ngắn nhiều node; với điều phối dùng timeline hai xe cùng zone, đổi cạnh, lỗi/offline và pin thấp. Đọc `docs/09-testing.md` để gắn kết quả với yêu cầu. Tham khảo PythonRobotics/RMF tại `docs/11-github-references.md`; không đổi sang MAPF hoặc RMF nếu phạm vi chưa đòi hỏi.
