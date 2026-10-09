# Hợp đồng và fixture

[current-command.schema.json](current-command.schema.json) ghi cấu trúc command v0 mong muốn; [examples/current-mqtt.json](examples/current-mqtt.json) là dữ liệu ví dụ. Chưa gắn schema vào parser runtime và chưa tạo giao thức v1.

Fixture không dùng để publish tự động. Chúng giúp core/firmware/simulator cùng hiểu telemetry text, command JSON và ACK text. Xem [tài liệu mạng](../docs/07-network.md).
