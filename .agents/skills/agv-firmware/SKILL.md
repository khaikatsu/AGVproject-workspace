---
name: agv-firmware
description: "Phát triển firmware MCU cho robot AGV gồm cảm biến motor máy trạng thái và MQTT tới server. Dùng khi có phần việc vi điều khiển; không tự suy ra bo mạch pinout từ comment core."
---

# Firmware AGV

Đọc `AGENTS.md`, `docs/08-firmware.md`, `docs/07-network.md` và yêu cầu phần cứng liên quan. Các đường dẫn tính từ gốc workspace; code mới đặt trong `firmware/`.

- Kiểm tra MCU/framework, pinout, điện áp, sensor, driver và policy dừng. ESP32 trong comment là giả định làm việc. Nếu thiếu dữ liệu cần cho motor, tiếp tục phần contract/parser độc lập rồi làm rõ phần bị thiếu.
- Tách callback mạng, parser/queue, máy trạng thái, control motor và sensor. Không delay trong MQTT callback đến hết hành trình; ACK sau khi lệnh hợp lệ được nhận vào xử lý.
- Dùng đúng payload v0 nếu core chưa chuyển phiên bản. Path là node còn lại, không phải duty PWM hoặc tọa độ trực tiếp. Cần bảng node/RFID/hướng di chuyển do nhóm cung cấp.
- Command trùng: ACK lại, không lặp thao tác. Dedup v0 chỉ theo cmd_id không giải quyết restart; phối hợp session/persistence với server khi yêu cầu.
- Xin zone trước khi vào; release sau khi thân robot rời hết vùng. Không tiếp tục vì timeout chờ grant đơn thuần; thử mất grant/request lại theo hợp đồng.
- Local stop, watchdog và cảm biến lỗi phải có hành vi từ thiết kế phần cứng. Không coi offline timeout core là thời gian robot được chạy mất điều khiển.

Chọn ESP MQTT hoặc PubSubClient theo framework; đo kích thước GOTO và bộ nhớ. Kiểm chứng parser/dedup/queue bằng fixture, tiếp theo bench sensor/motor rồi tuyến thật khi được giao. Chỉ nêu đã build/flash/đo khi thực sự thực hiện và ghi cấu hình. Tham khảo có chọn lọc tại `docs/11-github-references.md`.
