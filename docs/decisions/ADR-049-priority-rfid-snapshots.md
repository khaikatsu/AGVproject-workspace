# ADR-049 — Ưu tiên cập nhật vị trí RFID

Ngày 13/10/2026. IMPLEMENTED và VERIFIED phần mềm; đo chuyển động thật OPEN.

`SOURCE`: người dùng báo xe đã tới node nhưng server/web cập nhật trễ khoảng 2–3 giây sau thay đổi đi xuyên RFID. Yêu cầu giữ vị trí theo đo thật.

`OBSERVED`: firmware đang chạy 20261013-through-slow-reverse, đưa snapshot vào worker ngay khi RFID đổi và định kỳ 1 giây. Core chạy từ build/agv_core.exe có pass_through/capability mới. Quan sát thụ động 15 giây khi xe ở K6/LINE_LOST: median khoảng cách frame Core551ms/Web548ms, max904/1074ms. HTTP state chủ yếu15–43ms, tuổi snapshot128–740ms. Probe RuntimeStore với bản sao checkpoint61KB trong .qa mất8–13ms/write. Đây là phép đo khi đứng yên, không chứng minh nguyên nhân độ trễ 2–3 giây lúc chuyển động. File JSON thụ động tại .qa/straight-through-host/latency-live.json; không gửi lệnh hoặc sửa DB vận hành.

`IMPLEMENTED`: FleetManager updateTelemetry trả cờ khi có robot mới, RFID node đổi hoặc position_valid đổi. Core chỉ broadcast ưu tiên sau checkpointAndPublish thành công; không chờ tick/throttle200ms cho cờ này. Gói heartbeat cùng node và trạng thái không đổi vẫn đi nhịp nền. Giữ lock/order và sequence delta hiện có; commit hỏng không phát snapshot ưu tiên hoặc lệnh/grant chưa commit.

Gateway nhận state có id/node/position_valid thay đổi thì hủy timer snapshot thường đang chờ, lập lịch gửi0ms với state mới nhất; heartbeat vẫn throttle200ms mặc định. Thêm tùy chọn constructor broadcastIntervalMs cho fixture kiểm tra timer đang chờ, không thêm REST hoặc env cấu hình. History flush, task_id/result/session, reconnect và delta sequence giữ nguyên. Đây là ngoại lệ theo yêu cầu vị trí RFID của giới hạn5Hz REQ-HARD-04, không tăng broadcast định kỳ.

State có core_snapshot_seq tăng trong mỗi core_session_id, độc lập sequence stream trên socket; tất cả snapshot/command_result mang số được cấp dưới g_stateMtx. Gateway giữ state mới hơn và bỏ state có seq thấp hơn/bằng trong cùng phiên, vẫn xử lý kết quả request đến muộn. Tránh race phản hồi lệnh cũ ghi đè vị trí RFID đã gửi ưu tiên. Phiên mới cho phép seq reset; Core cũ thiếu trường giữ tương thích. Callback socket Core cũ bị bỏ qua sau reconnect. Counter này chỉ cho quan sát, không thay counter command/task bền.

Không đổi telemetry MQTT, firmware, tốc độ motor hoặc nội suy vị trí. Không gán node đích trước RFID. Nếu đầu đọc vật lý nhận thẻ trễ hoặc mạng/ghi đĩa bị nghẽn, tối ưu Core/gateway không loại bỏ phần trễ đó.

`VERIFIED`: gateway/security18/18 PASS, gồm timer thường bị preempt, sequence delta, history mới nhất, phản hồi lệnh chứa snapshot seq10 đến sau RFID seq11 không ghi đè vị trí, phiên Core mới cho phép reset seq1. Build/Core unit PASS. MQTT/Core/gateway/WebSocket fixture riêng tại .qa/straight-through-test-0B6SMn/verification.json đo13 lần đổi node, từ fixture publish MQTT tới browser WS nhận vị trí là10–29ms (trung bình17,15ms). Các node thẳng, contention/offline/token và legacy/rẽ vẫn PASS. NODE_ORDER/checkpoint failure regression PASS tại .qa/node-order-test-2gM5UH/verification.json. Đây là loopback/broker và robot giả lập, không đo thời gian RC522 đọc thẻ/WiFi trên xe thật.

Binary đã kiểm tra: build/rfid-latency/agv_core.exe, SHA256 EBA2B535EBB48D0EB4ACBFD8AC785DEEC71B5C849FB95D953025637D113C0530. Manifest và latency fixture lưu tại .qa/rfid-latency-release-20261013/verification.json. Không thay executable đang chạy hoặc restart dịch vụ trong lượt kiểm chứng.

`OPEN`: chưa thay Core/gateway vận hành. Cần đo lại khi xe thật đổi node để phân biệt thời điểm RC522 đọc thẻ với thời điểm quan sát thân xe tới node; không suy fixture loopback chứng minh đã hết trễ vật lý. Không cần nạp lại firmware cho thay đổi này.
