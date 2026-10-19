# Hợp đồng và fixture

[current-command.schema.json](current-command.schema.json) ghi cấu trúc command v0 mong muốn; [examples/current-mqtt.json](examples/current-mqtt.json) là dữ liệu ví dụ. Diagnostic v1 có `lastCommandId` để Core chỉ nâng counter lên phía trước khi phục hồi `STALE_COMMAND`; trường này không phải ACK và không cho phép replay lệnh cũ. Chưa gắn schema vào parser runtime và chưa tạo giao thức command v1.

Fixture không dùng để publish tự động. Chúng giúp core/firmware/simulator cùng hiểu telemetry text, command JSON và ACK text. Xem [tài liệu mạng](../docs/07-network.md).
