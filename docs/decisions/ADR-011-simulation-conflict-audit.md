# ADR-011: Giữ đường thường/hành lang mutex và phục hồi sau mất Core/broker

- Ngày: 02/10/2026. Trạng thái: Accepted theo yêu cầu trực tiếp kiểm thử và sửa xung đột.
- SOURCE: người dùng yêu cầu mô phỏng các tình huống lỗi, sửa lỗi/xung đột và gửi kết quả.
- OBSERVED: audit ban đầu `.qa/fault-audit-pNjKso` tái hiện hai xe cùng tới G3/G1; token cũ sau Core restart giữ owner dù xe đã về trạm; broker restart nhanh không có LWT tới Core nên task còn dispatched khi xe đã dừng.

Giữ Dijkstra có trọng số, FIFO MutexArbiter, task ID và MQTT v0/v0.1. FleetManager bổ sung giữ node thường của route xe khác; fallback không được bỏ qua các node này. Mutex đơn lẻ vẫn dùng FIFO. Những mutex nối trực tiếp thành một thành phần được giữ như hành lang; xe khác chờ ngoài thành phần cho tới khi tuyến/occupancy trước đã rời. Không thu hồi quyền đang chiếm để giải quyết chờ chéo.

Lệnh riêng xe gặp xung đột đặt trước nhưng vẫn có đường hợp lệ về topology/occupancy được nhận dưới BLOCKED/WAIT, giữ task ID/đích và tick thử lại. Đường không có thật, vật cản hoặc đích đang bị chiếm vẫn bị từ chối. Hàng đợi tự phân công giữ pending khi chưa có tuyến an toàn. ACK không làm task completed.

Core mất broker gọi `reportNetworkLoss`: task đang chạy chuyển interrupted, xe có vị trí giữ ERROR, đường chưa xác minh và mutex được giữ. Kết nối MQTT trở lại không tự cấp quyền chạy. Queue/counter không reset. Hàm cấu hình `setNetworkAvailable` vẫn giữ nghĩa bật/tắt khả năng phân công.

Simulator tuần tự hóa phục hồi theo xe: mô hình điểm được xác nhận tại RFID gần nhất trong lúc giữ dừng, Core nhận vị trí/pin/heartbeat mới và ERROR rồi clear-error. Sau restart, token tại node đang chiếm phải cấp lại generation mới trước khi bỏ dừng. Những vùng ngoài điểm đã xác nhận được trả qua confirm-clear với owner/token hiện tại. Bằng chứng điểm ngoài vùng chỉ áp dụng cho simulator; robot thật vẫn cần xác minh vật lý, không suy ra từ mất telemetry.

Ràng buộc bảo thủ có thể tăng chờ hoặc chọn đường dài hơn. Không tuyên bố tối ưu throughput hay tránh va chạm thân xe. Nếu xe hỏng giữ vùng và không có lối tránh, cần người vận hành xử lý; không tự nhả owner để làm test hoàn thành.

Kiểm chứng dùng Core/gateway/MQTT và DB/map/counter riêng: [ma trận lỗi](../../tests/simulation.faults.js), [runner](../../tests/run-simulation-audit.js), [báo cáo](../testing-simulation-audit-2026-10-02.md).
