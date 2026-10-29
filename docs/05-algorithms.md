# Thuật toán tìm đường và điều phối

28/10/2026: tuyến dự kiến đầy đủ là ưu tiên né mềm; đặt chỗ cứng chỉ gồm phần tiếp cận/hành lang mutex kế tiếp và node thoát. Node thực, đích được đặt chỗ, footprint lỗi/offline và hành lang đang chiếm vẫn chặn. Core kiểm request mutex đúng node hiện tại/đoạn kế hoặc một đoạn nhìn trước được straight-through cho phép; không cấp vùng xa tùy ý. Proof ADR-079 vẫn thu hẹp về node/hai đầu cạnh; stale/offline không tự nhả. Phục hồi ở RFID cũ cần xác nhận vật lý/heading và proof mới; home tự giải phóng khi đủ khoảng cách50cm và STOP đúng phiên. [ADR-090](decisions/ADR-090-shift-recovery-and-binary-inventory.md).

23/10/2026, REQ-STOPPED-OCCUPANCY-01: proof đứng yên hợp lệ thu hẹp chặn về node A hoặc A/B của đoạn xe lỗi; planner và arbiter cùng bảo vệ vùng đó, tự trả mutex ngoài vùng. Giữ đường cũ để dự phòng khi stale/offline/restart; xe lỗi vẫn ERROR. Không cộng đệm hình học theo chốt người dùng. Tuyến đang an toàn không bị phát GOTO lại; MCU đang chạy cần WAIT và RFID dừng mới trước tuyến thay thế. [ADR-079](decisions/ADR-079-stopped-edge-occupancy.md).

23/10/2026, REQ-FAILURE-REPLAN-01: ERROR/LWT/timeout/ACK-failure kích hoạt replan theo `currentNode + activePath + uncertainPath + mutex component`; xe đang chờ mutex của owner lỗi cũng được rút FIFO cũ và thử đường vòng. Không có đường thì BLOCKED/WAIT; mutex xe lỗi vẫn giữ tới khi có bằng chứng clearance. [ADR-078](decisions/ADR-078-failure-footprint-replanning.md).

17/10/2026, REQ-MUTEX-ENCODER-01: firmware hai xe có thể trả M1–M6 trước RFID kế tiếp sau khi có bằng chứng đuôi đã qua biên: mỗi bánh đủ30 xung tịnh tiến từ lần đầu đọc thẻ M, thẻ mất ổn định100ms và route/phiên/vị trí còn khỏe. Pivot/STOP/tìm line không cộng; offline/lỗi không nhả. Core vẫn cấp một owner/FIFO và kiểm tra đúng `grant_id`; thay đổi này giảm thời gian giữ khóa, không giải quyết deadlock nhiều khóa hoặc xung đột cạnh. [ADR-068](decisions/ADR-068-encoder-mutex-tail-clearance.md).

17/10/2026: mapv4 bỏC1–C4; cạnh bo có weight135cm (nhãn85+50) và hướng rời/tới tách riêng. Dijkstra dùng weight, NodeManeuver đi xuyên/rẽ dùng tiếp tuyến ở đúng đầu cạnh. Via không là node hoặc điểm pivot. Giữ6mutex, trả M2 tạiG6/M3 tạiG1 theo RFID đúng tuyến/token; [ADR-064](decisions/ADR-064-rounded-map-without-corner-tags.md).

16/10/2026, phục hồi bắt line sau LEFT/RIGHT là máy trạng thái cục bộ của firmware, không phải replan: giữ nguyên chặng, task và mutex; retry cùng hướng đúng một lần rồi mới phát `LINE_LOST`. Server không cấp lại `NODE_ORDER`; mất liên lạc vẫn dừng và quyền vùng không tự nhả. [ADR-061](decisions/ADR-061-single-turn-line-retry.md).

## Bài toán nền

Hai robot di chuyển trên graph của kho. Node biểu diễn điểm nhận biết hoặc dừng; cạnh có hướng và trọng số. Mã hiện có dùng RFID/node, Dijkstra, task priority, quản lý pin và mutex zone. Kích thước robot, tốc độ và loại nhận biết vị trí vẫn chờ thông số.

## Tìm đường

`MapGraph::findShortestPath` là Dijkstra với priority queue. Với trọng số không âm, dùng tổng weight để so sánh chi phí. Trước khi gọi cần xác nhận node tồn tại, hướng cạnh hợp lệ, edge nối node đã có, đơn vị weight và quy tắc node bị chặn.

Không so sánh số node với tổng chi phí đường. Hàm chọn trạm sạc hiện có nhánh dùng độ dài vector đường; khi cải tiến cần dùng tổng trọng số nếu yêu cầu là trạm gần nhất theo khoảng cách. Không kết luận “tối ưu toàn đội xe” từ Dijkstra từng xe.

A star chỉ phù hợp khi có heuristic admissible theo đúng đơn vị. Khoảng cách Euclid không mặc nhiên là lower bound nếu x/y là pixel và weight là cm hoặc chi phí tùy ý. Dijkstra là baseline để đối chiếu.

## Điều phối hai robot

MutexArbiter giữ owner theo zone và FIFO chờ. Firmware phải xin quyền trước khi vào, chờ grant, rồi trả quyền sau khi rời hoàn toàn. Điểm rời cần tính cả thân robot, không chỉ thẻ RFID phía trước.

Các dạng xung đột cần mô hình hóa:

- Hai xe vào cùng node hoặc vùng hẹp.
- Hai xe đổi chỗ trên một cạnh hai chiều.
- Xe quay/rẽ làm thân quét sang phần đường khác.
- Xe offline hoặc lỗi nhưng vẫn chiếm vùng.
- Hai xe giữ các zone khác nhau và chờ zone của nhau.

Khóa node đơn lẻ không giải quyết toàn bộ các trường hợp trên. Nếu thiết kế nhiều zone, cần thứ tự nhận quyền hoặc đặt trước đủ vùng, timeout chỉ kích hoạt kiểm tra phục hồi, không tự chứng minh đường trống.

## Phân công và pin

Đề xuất baseline: lọc xe IDLE/online/không lỗi, đủ pin; tính chi phí tới đích; ưu tiên task theo rule xác định; tie break ổn định. Ghi cách so priority và chi phí, không dùng thứ tự unordered_map làm chính sách công bằng.

Pin thấp cần kiểm tra tồn tại trạm sạc và đường tới trạm. Hysteresis tránh dao động trạng thái quanh ngưỡng. Sạc xong phải có bằng chứng firmware; mặc định 90% trong core là fallback, không phải phép đo thật.

## Thước đo và tình huống kiểm chứng

| Nhóm | Tình huống | Kết quả cần kiểm chứng |
| --- | --- | --- |
| Đồ thị | Hai đường có số node khác với tổng weight | Chọn tổng cost đúng |
| Hướng | Cạnh một chiều và đồ thị không liên thông | Không đi ngược; trả không có đường |
| Chặn | Node có obstacle, extraBlocked | Đường tránh hoặc giữ chờ rõ ràng |
| Giao nhau | Hai xe xin cùng zone | Mỗi lúc tối đa một owner hợp lệ |
| Tranh chấp | Đi ngược cạnh, giữ hai zone | Có quy tắc chống xung đột/deadlock |
| Sự cố | Mất ACK, offline trong zone, restart | Không giả định xe biến mất khỏi bản đồ |

Đo tổng chi phí, thời gian hoàn thành, thời gian chờ zone, số replan và khả năng phục hồi. Các ngưỡng đạt yêu cầu phải do nhóm chốt.

Tham khảo [PythonRobotics](https://github.com/AtsushiSakai/PythonRobotics) cho thuật toán học tập và [Open RMF](https://github.com/open-rmf/rmf_demos) cho ý tưởng điều phối. Đây là tham khảo; chưa tích hợp MAPF, CBS hoặc RMF vào core. Kỹ năng: [agv-algorithms](../.agents/skills/agv-algorithms/SKILL.md).

## Bản triển khai 29/09/2026

Audit 02/10/2026 bổ sung giữ node thường của route và thành phần mutex nối trực tiếp, ngăn dùng chung G1/G3 hoặc chờ chéo trong hành lang. Lệnh riêng xe gặp xung đột route giữ BLOCKED/WAIT và tick thử lại; mutex đơn lẻ vẫn FIFO. Core mất broker gián đoạn task/giữ ERROR và đường chưa xác minh ngay. [ADR-011](decisions/ADR-011-simulation-conflict-audit.md), [kết quả](testing-simulation-audit-2026-10-02.md). Có thể tăng chờ; chưa chứng minh tối ưu toàn đội hoặc an toàn hình học.

Đã dùng tổng trọng số cho chọn xe/trạm, tie break bằng ID. Fallback vẫn tránh node/đích đang chiếm và route chưa xác minh. Mất telemetry/ACK/lỗi làm gián đoạn task, không tự giao lại; giữ quyền zone. Các kết quả C++ và MQTT fixture nằm ở [hồ sơ kiểm thử](../tests/README.md); chưa chứng minh tránh va chạm hình học hoặc deadlock trên hai robot thật.
