# ADR-061 — Thử bắt lại line một lần sau khi quẹo

Ngày: 16/10/2026. Trạng thái: IMPLEMENTED; nghiệm thu chuyển động thật OPEN.

Bổ sung mới hơn cùng ngày: [ADR-063](ADR-063-robot02-left-turn-capture.md) thay retry dài của riêng LEFT xe02 bằng STOP/xác nhận line và chỉnh ngược ngắn. RIGHT xe02 và profile khác giữ quy tắc bên dưới.

## SOURCE và phạm vi

Người dùng yêu cầu khi xe quẹo trái/phải nhưng chưa bắt được line, hệ thống không khóa lỗi ngay. Xe được quay lại cùng hướng thêm đúng một lần; bắt được line thì tiếp tục task, lần thứ hai vẫn không thấy mới báo lỗi lệch line lên server.

Phạm vi áp dụng là hai profile bench xe01/xe02 trong `SharedRobotConfig.h`. Không tự thử lại quay đầu `TURN_AROUND`, xoay đỗ tại S, mất line khi đang chạy thẳng hoặc mẫu line mơ hồ; các trường hợp đó giữ hành vi cũ.

## Quyết định

- MCU thực hiện phục hồi cục bộ vì MCU sở hữu cảm biến line và motor. Server không phát lại `NODE_ORDER`, không tăng `cmd_id` và không suy đoán xe đã bắt line.
- Lần quẹo LEFT/RIGHT đầu dùng timeout hiện có2500ms. Nếu chưa bắt lại line, motor STOP một vòng điều khiển, giữ nguyên route/task và quyền mutex cục bộ, đặt lại bộ đếm rời/bắt line rồi pivot cùng hướng thêm một lần với cùng giới hạn2500ms.
- Lần thử lại chỉ thành công sau khi cảm biến đã thấy `000` rời line cũ rồi bắt mẫu giữa hợp lệ `010/101/111`; khi đó firmware tiếp tục pha Following của chính chặng đang chạy.
- Nếu lần thứ hai hết hạn, firmware dừng và phát `LINE_LOST`; server mới chuyển task/xe sang lỗi theo luồng hiện có. Không lặp lần ba và không tự phát lại task.
- Mất Wi-Fi/MQTT, sonar không hợp lệ, vật cản, STOP hoặc hủy task vẫn được xử lý trước phục hồi. Offline không nhả mutex; retry không phải bằng chứng xe đã rời vùng.

MQTT v0, ACK, telemetry pipe, `NODE_ORDER`, timeout server và schema không đổi. Trong lần thử lại, diagnostic tiếp tục thể hiện xe đang chạy task; Serial có `turn_retry=1` hoặc `turn_retry_failed` để chẩn đoán.

## Kiểm chứng và giới hạn

Native profile01 **30/30 PASS**, profile02 **31/31 PASS**, gồm ca quẹo trái: lần đầu timeout không phát lỗi, thử lại cùng hướng, bắt line thì tiếp tục; lần hai thất bại phát `LINE_LOST`; mất MQTT trong lúc thử lại dừng `NOT_CONNECTED`. Hồi quy native trial **29/29 PASS** và two-robot không profile **28/28 PASS**, nơi capability retry tắt và timeout cũ được giữ.

Build ESP32 `esp32dev_two_robot_bench_01` và `_02` PASS, build ID lần lượt `20261016-robot01-turn-line-retry` và `20261016-robot02-pwm230-turn-retry`. Chưa upload hoặc thử trên xe thật. Cần kê bánh rồi kiểm tra hướng pivot, khả năng STOP giữa hai lần, góc quét thân xe và việc bắt đúng nhánh trước khi chạy tuyến hai xe.
