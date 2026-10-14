# ADR-053 — Trả task sau khi đỗ xong tại S1/S2

Ngày14/10/2026. Thay riêng điều kiện tuổi RFID để hoàn thành đỗ trạm của ADR-033; giữ quay đầu và các chốt phục hồi lỗi/mutex.

`SOURCE`: người dùng báo xe01 đã về S1 nhưng web vẫn Đang chạy/giữ đích S1, yêu cầu trả task. Người dùng xác nhận xe đang ở S1.

`OBSERVED`: Core giữ task-52283 dispatched/WORKING, path rỗng, ca CLOSING. Diagnostic thật20:04 báo S1/headingS, AGV_IDLE/NONE/STOP, duty0/0, taskActive=false, stationReady=true; rfidAgeMs171249→175278. Firmware20261014-robot01-buzzer-diagnostic, boot61058346/session1. Core đòi rfidAgeMs≤250 nên không chốt hoàn thành dù robot đã đỗ xong. Trace `.qa/encoder-network-live/run-2026-10-14T13-04-21-212Z`.

## Quyết định

Tuổi lần đọc thẻ là tuổi mốc vị trí; đầu đọc phía trước có thể rời thẻ khi quay đầu. Core nhận RFID đích theo route và diagnostic mới xác nhận đỗ: đúng node/hướng ra GS, stationDockingSupported/stationReady, AGV_IDLE/NONE/STOP, duty0/0, taskActive=false, WiFi/MQTT healthy, RFID đã đọc trong phiên (`rfidAgeMs != UINT32_MAX`, null vẫn từ chối). Không bắt lần đọc thẻ còn ≤250ms để hoàn thành task.

Vẫn cần vị trí hợp lệ/telemetry mới, activePath rỗng, currentNode=finalTarget, state WORKING/CHARGING_EN_ROUTE và diagnostic còn mới≤3s. Khi giao GOTO, stationReady cũ bị vô hiệu; capability thiếu không bỏ chốt. Mode01 encoder vẫn kiểm tra boot/connection/Core session trước xử lý diagnostic. Không hoàn thành từ ACK hoặc chỉ từ target, không nới điều kiện250ms của reset lỗi/trả mutex và không đổi quay/PWM/firmware.

Core set record cùng task_id sang completed, xóa task_id/target, đưa xe IDLE (hoặc CHARGING theo lượt sạc hiện có). Web dùng trạng thái Core, ca có thể chuyển CLOSED sau khi đỗ xong. Payload không đổi.

## Kiểm chứng và áp dụng

Core build/unit PASS tại `.qa/station-completion-build`. Fixture `.qa/heading-test-fH7caJ` MQTT→Core→gateway/DB riêng PASS: RFID/ACK chưa xong vẫn dispatched, quay/hướng vào/null RFID/lỗi không hoàn thành; ready mới cùng RFID tuổi175278ms chốt completed/IDLE, task_id/target rỗng, CLOSING→CLOSED và lịch sử cùng phiên một record khi lặp diagnostic.

Snapshot trước áp đã ghi task-52283 completed/updated_sequence1050, xe IDLE/S1/không đích hoặc task, ca CLOSED và không mutex. Đây là kết quả Core cũ sau lần đọc RFID mới của hệ thống/người vận hành; agent không giả lập diagnostic, không cancel task, không sửa DB/history bằng tay. Chỉ sau đó dừng Core PID28972, sao lưu binary/state/counter và thay bản đã kiểm chứng. Core mới PID15300 mode robot01 chạy nền lúc20:12:56, SHA256373962120B649ADF444C411FAA4D5A6DA6A9254675B815D2DB790383A27D7F66. Artifact `.qa/station-completion-apply-20261014`.

Đối soát20:14: task-52283 vẫn completed, xe online/IDLE/S1/headingS, task_id/target/path trống, ca CLOSED, Core/MQTT/storage healthy. Trace sau áp10s boot giữ61058346/session2, MCU AGV_IDLE/NONE/STOP/stationReadytrue. Không phát thao tác di chuyển/reset/clear-error, không upload firmware, không rewind counter/xóa dữ liệu.

`OPEN`: lượt về S1 mới sau bản sửa chưa chạy trên sàn; fixture đã tái hiện tuổi RFID cũ. Không dùng encoder hay thời gian để xác nhận node/hoàn thành quay.
