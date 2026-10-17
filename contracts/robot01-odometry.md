# Hợp đồng odometry xe 01 và 02 — schema 1

16/10/2026: REQ-ROBOT02-PARITY-01 mở cùng schema cho cả hai profile. Topic `agv/{id}/odometry`, `robot_id` là `01` hoặc `02` và phải trùng topic. Core lưu phiên/seq/route độc lập; cùng boot ID hoặc seq giữa hai xe không ghép dữ liệu. Ví dụ01 bên dưới vẫn hợp lệ; [ví dụ02](examples/robot02-odometry.json). [ADR-059](../docs/decisions/ADR-059-robot02-firmware-parity.md) thay các giới hạn chỉ xe01/mode test01 ở phần lịch sử dưới đây.

Diagnostic01 bản `20261014-robot01-network-clock-fix` thêm `networkServiceAgeMs` (integer ms từ lần worker phục vụ), `coreHeartbeatAgeMs` (integer ms/null nếu chưa có heartbeat), `mqttClientState` (integer từ PubSubClient), `wifiRssi` (integer từ SDK). Đây là chẩn đoán, không đổi odometry/telemetry/command. Trong Core test01, session diagnostic encoder phải hợp lệ trước xử lý motion; SAFE_STOP/ERROR mới đúng phiên và node đưa Core ERROR qua luồng lỗi nếu Core còn khỏe, để không báo IDLE khi mất error event QoS0. [ADR-051](../docs/decisions/ADR-051-robot01-network-health-and-diagnostic-fault.md).

14/10/2026. Producer `esp32dev_two_robot_bench_01`, consumer hiển thị Core mode `robot01`. Topic `agv/01/odometry`, QoS0/không retained/mỗi200 ms; telemetry pipe, command/ACK/mutex giữ nguyên.

| Trường | Kiểu / giới hạn | Ý nghĩa |
| --- | --- | --- |
| schema_version/robot_id | số nguyên 1/string `01` hoặc `02` | Phiên bản/xe, ID phải trùng topic |
| core_session_id | chuỗi 1–95byte | Phiên heartbeat Core |
| boot_id/connection_session | số nguyên 1…2³²−1 | Boot ID ngẫu nhiên/generation MQTT |
| seq/uptime_ms | số nguyên 0…2³²−1 | Thứ tự mẫu/millis, có wrap |
| left_ticks/right_ticks | số nguyên 0…2³²−1 | Raw RISING ticks, không reset theo RFID |
| pulses_per_100 cm | finite number 0,1…1.000.000 | Hệ số thử 161 |
| route_cmd_id/path_index | số nguyên 0…INT_MAX/0…31 | GOTO ID/index 0-based trong path MCU nhận |
| from_node/to_node | chuỗi ≤7byte | RFID mốc/node kế |
| tracking/translating/reverse | boolean | Chặng hợp lệ/bám line chuyển động/lùi theo navigator |
| distance_cm | finite number 0…100.000 | Distance chặng, chỉ cộng khi translating |
| speed_cm_s | finite number 0…500 | Tốc độ cửa sổ ≥200 ms, 0 khi STOP/pivot |

Tracking false: to rỗng, route/distance/speed 0, translating/reverse false; from là RFID đã xác nhận hoặc rỗng. Tracking true cần route dương/from và to khác nhau. Translating false cần speed 0 nhưng có thể giữ distance lúc dừng chờ.

Diagnostic v1 thêm encoderSupported/encoderBootId/encoderCoreSessionId; Core dùng session/uptimeMs để gắn phiên trước khi nhận mẫu. Loại retained, sai Core/boot/connection session, seq trùng/cũ, uptime lùi>500 ms so diagnostic, distance giảm cùng chặng và route/node/index sai. Không cập nhật online/lastSeen hoặc mở chốt lỗi.

Snapshot robot thêm encoder_supported/speed_cm_s(number/null)/odometry(object/null)/estimated_position(object/null). Ngưỡng cũ1000 ms; Web cộng tuổi snapshot. Node vẫn là RFID gần nhất. Estimate có source encoder/from/to/x/y/progress≤0,98/distance_cm/edge_length_cm/route_cmd_id/path_index/reverse. Map test 01 dùng cm; hiệu chuẩn/trượt/RFID gây sai số. Không dùng để hoàn thành task/nhả mutex/điều khiển motor và không lưu checkpoint.

[Ví dụ](examples/robot01-odometry.json), [ADR-050](../docs/decisions/ADR-050-robot01-encoder-position-speed.md).

Diagnostic kiểm tra còi14/10/2026 của profile01: `buzzerOn` boolean là yêu cầu theo action/duty, `buzzerPinLevel` số0/1 là mức digitalRead GPIO0 tại lúc chụp snapshot. Còi activeLOW; lùi có duty âm: true/0, STOP/tiến/pivot: false/1. Không suy ra đã phát âm từ mức pin; firmware cũ có thể thiếu cả hai trường. Không tham gia điều phối/ACK/task/mutex. Ví dụ phần diagnostic khi lùi: `{"motor":"BACKWARD","leftDuty":-150,"rightDuty":-190,"buzzerOn":true,"buzzerPinLevel":0}`.
