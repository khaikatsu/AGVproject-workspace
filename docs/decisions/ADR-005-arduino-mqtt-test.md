# ADR-005 — Sketch Arduino test MQTT v0 trên ESP32

Ngày 29/09/2026. Trạng thái: IMPLEMENTED cho REQ-FW-TEST-01. Người dùng xác nhận ESP32 DevKit + PubSubClient.

Một file `.ino` trong firmware, dùng ArduinoJson 7 để validate JSON server đang phát. Không đổi topic hoặc payload phía server: ACK text, telemetry pipe, command JSON và zone text. Sketch dùng MQTT client ID riêng theo robot/MAC, danh sách node map v3, buffer/path có giới hạn, heartbeat và reconnect. Parser lưu command trước khi ACK, cache duplicate trong RAM qua reconnect; nhận lại command cùng ID không lặp trạng thái xử lý.

Chọn receiver test trên Serial với telemetry giả lập, không suy ra pinout và không điều khiển motor. ACK không thay node; chỉ thao tác Serial mới phát node đích giả lập. Mutex và charging_done là event test chủ động. Dedup qua MCU reboot/session vẫn OPEN và cần thiết kế firmware vận hành riêng.

[Sketch và cách sử dụng](../../firmware/AGV_Connection_Test/README.md), [host test](../../tests/firmware-host/probe.cpp), [core/MQTT integration](../../tests/firmware.integration.js). Compile ESP32 thật khác với upload/đo trên board; ghi bằng chứng riêng.
