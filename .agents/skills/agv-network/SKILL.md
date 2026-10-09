---
name: agv-network
description: "Thiết kế và kiểm tra MQTT HTTP WebSocket, hợp đồng robot server, QoS reconnect và mạng LAN của AGV. Dùng cho giao thức mạng; điều khiển sensor motor thuộc agv-firmware."
---

# Giao thức AGV

Đọc `AGENTS.md`, `docs/07-network.md`, contract và yêu cầu mạng liên quan. Đường dẫn tính từ gốc workspace.

- Hiện tại `agv/{id}/{action}`, telemetry `node|battery|obstacle`, command JSON với `cmd_id`, ACK text số; QoS 0. Xác minh parser trước thay đổi.
- ID robot trong topic không mặc định có prefix AGV_. Topic chỉ ba segment; không đưa slash vào robot ID.
- MQTT ACK khác ACK ứng dụng và khác hoàn thành hành động. Retry cần dedup; counter reset đòi hỏi session/persistence cả server lẫn MCU.
- Không retained command di chuyển. LWT/status, JSON telemetry v1 và timestamp là đề xuất chưa được core hỗ trợ; nếu triển khai phải đồng bộ producer/parser/simulator và có lộ trình tương thích.
- Core kết nối broker local, gateway HTTP 3000/WS core 8080/MQTT 1883. Robot dùng IP LAN thật của broker; 127.0.0.1 trên robot là chính robot.
- Chọn client theo payload/QoS/framework. PubSubClient publish QoS 0 và buffer mặc định nhỏ; kiểm tra path dài. MQTT.js chưa là dependency hiện tại. Auth/ACL/TLS áp dụng theo phạm vi mạng và cấu hình được yêu cầu.

Khi đổi protocol, cập nhật `contracts/`, tài liệu và ca kiểm tra missing/duplicate/out of order/reconnect/reboot phù hợp. Kiểm tra giới hạn frame/payload và schema, không khởi broker trên mạng robot thật để thử. Nguồn chính chủ/chuẩn nằm `docs/11-github-references.md` và tài liệu mạng; kiểm tra version trước dựa vào API mới.
