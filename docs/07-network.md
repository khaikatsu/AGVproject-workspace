# Giao thức mạng và hợp đồng MCU với server

15/10/2026, REQ-ROBOT01-SERVO-LIFT-01: thêm Core v0 `LIFT_UP`/`LIFT_DOWN` với `cmd_id` và `node=K1..K6`, không retained. Chỉ firmware01 một servo GPIO17 nhận; ACK vẫn là biên nhận và dedup cùng ID không phát PWM lại. Diagnostic thêm `liftSupported`, `liftCommandId`, `liftBusy`, `liftState` (IDLE/LIFTING/LOWERING/DONE_UP/DONE_DOWN/INTERRUPTED), `liftElapsedMs`, `virtualCargo` boolean/null. Core chỉ đọc các trường này từ diagnostic đúng phiên Core/boot/kết nối encoder đã bind của xe01. Trạng thái fleet thêm LIFTING/LOWERING, object `lift`, `virtual_cargo` và `virtual_cargo_known`. Chỉ DONE đúng cmd/node/phiên, MCU IDLE/taskActivefalse/STOP/duty0/0, kết quả cargo tương ứng và đủ5s ở MCU/Core mới đổi kiện ảo và mở khóa di chuyển. Mất kết quả12s/STOP/lỗi/mạng không tự hoàn tất; giữ mutex/counter. [ADR-057](decisions/ADR-057-robot01-servo-lift-test.md), [ví dụ](../contracts/examples/robot01-servo-lift.json).

15/10/2026, REQ-ROBOT01-REVERSE-HOME-01: diagnostic thêm boolean `reverseHomeSupported` (profile01 true, còn lại false). Core chỉ cho ID01/capability này nhận NODE_ORDER GS1→S1 với boolean `reverse_after_turn:true`: maneuver/crossings mô tả góc xoay trước, sau đó mới bám lùi vào S1. Các lệnh khác thiếu cờ được hiểu false; parser/dedup/ủy quyền MCU kiểm tra cờ và cấm áp ngoài GS1→S1. Firmware cũ tiếp tục luồng tiến vào/quay tại S1; cần chạy Core mới cùng firmware01 mới để dùng luồng lùi. Không đổi telemetry v0, ACK vẫn chỉ là nhận lệnh, hoàn thành vẫn chờ RFID/diagnostic stationReady theo ADR-053. [ADR-056](decisions/ADR-056-robot01-reverse-home-request.md), [ví dụ](../contracts/examples/robot01-reverse-home.json).

15/10/2026, REQ-ROBOT01-SONAR-05: hạn chờ hoàn tất ECHO profile01 hiện60ms theo xung thực tế47,5ms. Payload MQTT không đổi; xung đã kết thúc ngoài dải đo vẫn có khoảng cách null, không biến xung47,5ms thành số cm. HIGH quá60ms hoặc mẫu stale vẫn lỗi. Mục hạn45ms của REQ-03 bên dưới là lịch sử. [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, REQ-ROBOT01-SONAR-03: không đổi payload; profile01 nhận xung không thấy vật khoảng 38 ms là ngoài tầm thay vì báo lỗi HIGH ở 30 ms. Khoảng cách vẫn `null`/obstacle false khi ngoài tầm, không giả thành một giá trị cm; HIGH quá 45 ms hoặc mẫu cũ vẫn không hợp lệ. Cần phân biệt `null` ngoài tầm với `null` khi kênh tắt/đang thu mẫu qua ngữ cảnh; bản diagnostic hiện chưa có trường riêng cho nguyên nhân này. [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026, REQ-ROBOT01-SONAR-02: xe 01 chỉ kích kênh sonar theo hướng dự định. Khi lùi/chờ dời vật cản sau, `obstacleDirection=rear`, `frontUltrasonicCm=null`; khi tiến hoặc không có tuyến lùi, `obstacleDirection=front`, `rearUltrasonicCm=null`. Khi vừa đổi kênh, cả hai khoảng cách có thể `null` trong lúc motor dừng chờ mẫu mới (tối đa 200 ms). Không coi `null` là số đo thông đường; thời hạn thu mẫu hết mà cảm biến vẫn không hợp lệ thì firmware chốt lỗi. Không đổi topic, telemetry pipe hoặc schema diagnostic. [Ví dụ](../contracts/examples/robot01-sonar.json), [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

15/10/2026 — profile xe 01 có diagnostic v1 bổ sung `obstacleDirection` (`front`/`rear`), `frontUltrasonicCm`, `rearUltrasonicCm` (số cm hoặc `null`); `obstacle` và `ultrasonicCm` hiện có biểu diễn sonar theo hướng chuyển động. Telemetry pipe của xe 01 vẫn `node|?|0`, vì cờ obstacle pipe cũ sẽ khóa node và lập lại tuyến. Core menu 1 dùng diagnostic kết hợp odometry cùng phiên để xuất `fleet.agvs[].obstacle_detection` (`from,to,x,y,range_cm,direction,source`) trong state WS; `null` khi không còn cản, dữ liệu cũ hoặc không có ước tính encoder hợp lệ. Đây là cảnh báo hiển thị trên cạnh, không thay task/mutex/map obstacle điều phối. [Ví dụ](../contracts/examples/robot01-sonar.json), [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

14/10/2026: hoàn thành đỗ S1/S2 không còn yêu cầu `rfidAgeMs≤250`; thẻ đã đọc trong phiên, RFID đích theo route và diagnostic đỗ khỏe còn mới≤3s là căn cứ. `null` vẫn từ chối; reset lỗi/trả mutex giữ ngưỡng250ms. [ADR-053](decisions/ADR-053-station-completion-rfid-anchor.md), [ví dụ](../contracts/examples/station-ready.json).

14/10/2026, kiểm tra còi xe01: diagnostic profile01 thêm `buzzerOn` boolean theo action/duty và `buzzerPinLevel` số0/1 đọc tại GPIO0 lúc chụp snapshot. ActiveLOW: lùi phải yêu cầu true/chân0; STOP/tiến/pivot false/chân1. Hai trường không xác nhận âm thanh, điện áp nguồn hoặc chuyển động thực tế; không dùng để mở chốt motor. FirmwareBuild20261014-robot01-buzzer-diagnostic đã build, chưa nạp. [Báo cáo](testing-buzzer-robot01-2026-10-14.md).

14/10/2026, chẩn đoán sau nạp encoder: firmware01 sửa race đọc tuổi kết nối giữa hai task, diagnostic thêm networkServiceAgeMs/coreHeartbeatAgeMs/mqttClientState/wifiRssi. Core test01 xét đúng phiên trước motion diagnostic và chốt ERROR khi MCU còn SAFE_STOP/ERROR mà Core chưa biết do error event QoS0 mất. Không tự reset từ helper hoặc nới watchdog. [ADR-051](decisions/ADR-051-robot01-network-health-and-diagnostic-fault.md), [kiểm tra live](testing-network-robot01-2026-10-14.md). Chưa áp lên bo/Core sống; phiên đo45s chỉ ở STOP.

14/10/2026: profile 01 thêm encoder trái GPIO16/phải GPIO4 theo sketch người dùng, topic JSON `agv/01/odometry` mỗi 200 ms không retained. Core mode `robot01` dùng cho vị trí/tốc độ ước tính; node/task/mutex theo RFID và chốt hiện có. Diagnostic thêm capability/boot/Core session; telemetry pipe giữ v0. [Hợp đồng](../contracts/robot01-odometry.md), [ADR-050](decisions/ADR-050-robot01-encoder-position-speed.md) cập nhật việc bỏ encoder của firmware 01 trong đoạn lịch sử bên dưới; xe 02 chưa bật encoder.

Chia sẻ mô phỏng qua Internet ngày10/10/2026: `scripts/share-simulation.ps1` mở Quick Tunnel HTTPS tới proxy loopback3110, HTTP/WS cùng origin. Chỉ gateway phiên mô phỏng được proxy; MQTT/Core/DB/counter riêng giữ nguyên. Hợp đồng MCU không đổi. [ADR-042](decisions/ADR-042-share-simulation-over-internet.md).

## Hợp đồng hiện tại

Chẩn đoán reset 09/10/2026: JSON diagnostic NCKHAGV có thêm `resetReason`, `resetReasonCode` từ `esp_reset_reason()`, `firmwareMode` (`bench`, `solo`, `locked`) và `motionEnabled`. Các trường là thông tin boot/chế độ để phân biệt bo reset với mất MQTT và bản firmware khóa motor; không là lệnh điều khiển, không xác nhận vị trí/an toàn vùng. Core vẫn nhận firmware cũ thiếu các trường, telemetry pipe và command không đổi. Reset reason là dữ kiện SDK, không thay phép đo điện áp/Serial khi chẩn đoán nguồn.

Trạng thái `OBSERVED`, cập nhật 01/10/2026: telemetry/command giữ v0, mutex chuyển v0.1 có generation/ACK; cần cập nhật đồng bộ cả hai sketch/Core. [Gia cố và cấu hình](18-hardening.md). Core publish QoS0, command/grant/telemetry không retained; status/LWT retained. Prefix agv/ và robot ID ASCII một segment.

| Topic | Chiều | Payload |
| --- | --- | --- |
| `agv/{id}/telemetry` | Robot → core | `M1|85`, `M1|85|0` hoặc `M1|?|0` khi pin chưa đo |
| `agv/{id}/command` | Core → robot | JSON `cmd`, `cmd_id`; GOTO thêm `path`, `target` |
| `agv/{id}/ack` | Robot → core | `42` dạng text, xác nhận đã nhận command 42 |
| `agv/{id}/mutex_request` | Robot → core | `zoneId` |
| `agv/{id}/mutex_release` | Robot → core | JSON `{zone,grant_id}`; chỉ trả quyền đúng token |
| `agv/{id}/mutex_grant` | Core → robot | JSON `{zone,grant_id}` |
| `agv/{id}/mutex_ack` | Robot → core | JSON `{zone,grant_id}`; đã nhận quyền, chưa chứng minh chuyển động |
| `agv/{id}/status` | Robot → core | online / LWT offline, retained |
| `agv/{id}/diagnostic` | Robot → core | JSON chẩn đoán; NCKHAGV gửi `batteryPercentage`, `batteryCalibrated` để hiển thị thử |
| `agv/server/heartbeat` | Core → mọi xe | core_session_id, không retained |
| `agv/{id}/obstacle` | Robot → core | ID node có vật cản |
| `agv/{id}/error` | Robot → core | Nội dung lỗi dạng text |
| `agv/{id}/charging_done` | Robot → core | Rỗng hoặc phần trăm pin dạng text |

Ví dụ ID topic `01`: core lưu ID `01`, log có thể thêm tiền tố `AGV_`. Không tự dùng `AGV_01` ở mọi nơi nếu firmware dùng `01`.

```json
{"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"}
```

Core loại node hiện tại khỏi path khi assign; firmware không mặc định phần tử đầu là node đang đứng. Các command khác: `{"cmd":"STOP","cmd_id":43}`, `WAIT`, `CHARGE_START`, `RESET_FAULT`, `NODE_ORDER`.

Cập nhật NODE_ORDER 10/10/2026: NCKHAGV mới dừng tại từng RFID và gửi diagnostic `nodeOrdersSupported:true`, `awaitingNodeOrder:true`, `routeCommandId` (cmd_id GOTO), `pathIndex`, `nextNode`, `stationTurnRequired`. Core kiểm tra tuyến/node/chặng, diagnostic STOP mới và đã đọc RFID thật trong phiên hiện tại (`rfidAgeMs` không null) rồi gửi `{"cmd":"NODE_ORDER","cmd_id":44,"route_cmd_id":42,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":1}`. Không đòi thẻ còn dưới đầu đọc/tuổi≤250ms khi xe đã xác nhận node và dừng chờ; quét thẻ rồi chờ bấm lệnh vẫn hợp lệ. Chốt≤250ms để phục hồi mutex/lỗi vẫn giữ; hoàn tất đỗ trạm dùng RFID đã đọc trong phiên cùng diagnostic đỗ mới theo ADR-053. Maneuver STRAIGHT/LEFT/RIGHT/REVERSE/TURN_AROUND_RIGHT/STATION_TURN_RIGHT; LEFT/RIGHT quay tại chỗ rồi bắt line, không chạy vòng cung tới thẻ tiếp. MCU kiểm tra trùng/sai node/step/hướng trước khi thực hiện; lệnh không bỏ qua grant. Retry cùng cmd_id, ACK chỉ nhận. Firmware cũ không có capability giữ luồng cũ. Core và firmware mới phải cập nhật đồng bộ; [ADR-034](decisions/ADR-034-server-node-orders-for-pivot-turns.md).

Cập nhật 10/10/2026: diagnostic có `headingSupported:true`, `heading` (`N/E/S/W/UNKNOWN` theo map), `headingNode`, `headingSource` (`bay_default/route/unknown`). Kho K1–K6: đầu hướng vào kho khi xử lý hàng, tiến vào/lùi ra. Trạm S1/S2: đầu hướng ra GS1/GS2, tiến xuất phát; lúc về xe tiến vào S, dừng tại RFID rồi quay đầu ở trạm; chỉ báo sẵn sàng sau khi bắt lại line hướng ra GS. Firmware phát hướng sau RFID thật. Heading route lấy từ đoạn bám line thật, đảo vector chuyển động khi lùi để giữ hướng đầu xe. Core chỉ hiển thị hướng đúng node/vị trí hợp lệ, diagnostic mới; offline/dữ liệu cũ hiện UNKNOWN. Đã bỏ SET_HEADING và gán hướng operator; GOTO JSON và telemetry pipe giữ nguyên. IR chỉ đo lệch line, hướng mặc định là quy ước đặt xe. [ADR-033](decisions/ADR-033-stations-face-outward.md).

Diagnostic đỗ trạm thêm `stationDockingSupported:true` và `stationReady`. Firmware hỗ trợ giữ tuyến active trong lúc dừng/quay tại S. Core chỉ hoàn thành lượt về trạm khi đã nhận RFID đích theo route và diagnostic mới đúng node, đầu S1=S/S2=N, `AGV_IDLE/NONE/STOP`, duty0/0, taskActive=false, rfidAgeMs đã biết trong phiên. Tuổi lần đọc thẻ có thể vượt250ms vì đầu đọc rời thẻ khi pivot; tuổi diagnostic vẫn phải≤3s. Mẫu ready cũ bị vô hiệu khi giao GOTO; không hoàn thành từ ACK. Capability thiếu ở firmware cũ giữ hành vi cũ; sau khi Core đã yêu cầu xác nhận đỗ, thiếu capability không bỏ chốt. Bộ sạc thật chưa tích hợp.

Cập nhật 09/10/2026: thao tác `clear_error` của người vận hành gửi `RESET_FAULT` riêng cho ESP32 NCKHAGV sau khi Core xác nhận xe online và có vị trí RFID hợp lệ. `WAIT` chỉ dừng, không xóa lỗi đã chốt. ESP32 chỉ nhận reset khi không còn tuyến đang chạy, đúng phiên MQTT/vị trí, line và điều kiện chuyển động hợp lệ; motor vẫn dừng, lệnh reset không tự chạy lại task. Nếu reset bị từ chối, ESP32 phát lỗi về Core. ACK của `RESET_FAULT` vẫn chỉ xác nhận nhận lệnh; đọc diagnostic `state/error` để xác minh chốt đã được xóa. Bản ESP32 cũ chưa hiểu lệnh này cần nạp lại trước khi dùng nút xóa lỗi.

Quản lý ca giữ MQTT: robot gửi `S1|85|0` hoặc `S2|85|0` để báo tại trạm; node khác vẫn là vị trí nhận được. Core dùng GOTO về trạm, CHARGE_START khi telemetry tới trạm và WAIT khi mở ca. Sketch RC522 đã cập nhật khớp luồng này; bộ sạc thật chưa tích hợp. Xem [hướng dẫn ca](17-shifts-and-home-charging.md).

Giá trị bổ sung `UNKNOWN|85|0`: có heartbeat nhưng chưa xác nhận vị trí. Core đặt `UNLOCALIZED/position_valid=false`, không tự gán G1/S1/S2; RFID hợp lệ lần đầu đưa về IDLE và cho luồng ca tiếp tục. UNKNOWN trong kế hoạch đang chạy gián đoạn task/ERROR, giữ node đo trước đó để bảo vệ vùng và uncertain_path, không tự release mutex. RFID đọc lại một mình không tự xóa ERROR; ngoại lệ thử hai xe cần thêm diagnostic mới như ADR-026. Đây là mở rộng giá trị pipe v0 đã đồng bộ sketch/core/test, không phải JSON telemetry v1. Sketch không ACK GOTO mới khi chưa định vị hoặc CHARGE_START mới khi chưa ở trạm riêng.

ACK được gửi sau khi validate, lưu high-water cmd_id NVS và nhận lệnh hợp lệ. Command trùng trong cache ACK lại nhưng không lặp hành động; ID cũ sau MCU reboot bị từ chối, không chạy lại. ACK không báo đã đến đích. Grant retry cùng grant_id khoảng1s đến ACK, request chưa có grant được xin lại1s; release cũ dạng text bị từ chối. Offline/LWT không nhả vùng. Heartbeat/session watchdog3s dừng driver cục bộ; motor khóa ở cấu hình mặc định, chưa đo trên xe thật. Tích hợp motor 03/10/2026 không đổi topic/payload; xem [ADR-012](decisions/ADR-012-motor-firmware-integration.md).

Bổ sung 04/10/2026 cho NCKHAGV: dấu `?` ở trường pin chỉ có nghĩa **chưa đo**. Core nhận RFID và cập nhật node trên map, đặt `battery_measured=false`, chặn giao task và lệnh GOTO từ web cho tới khi pin được hiệu chuẩn; không diễn giải `?` thành 0%. Telemetry số 0–100 của sketch cũ giữ nguyên. Firmware NCKHAGV xin/ACK grant cho vùng M trước khi chạy đoạn liên quan, nhưng chưa tự release do chưa có bằng chứng toàn thân đã rời vùng. [ADR-015](decisions/ADR-015-basic-rfid-line-motion.md).

Bản thử một xe 04/10/2026 là ngoại lệ có điều kiện cho **lệnh thủ công xe 01 S1→GS1**: Core phải khởi động với `AGV_ONE_ROBOT_TRIAL=1` và ESP32 phải nạp env `esp32dev_trial`. Pin vẫn là `?`; giao nhiệm vụ tự động và các tuyến khác vẫn không được cấp quyền từ ngoại lệ này. [ADR-016](decisions/ADR-016-single-robot-server-motion-trial.md).

Cập nhật 05/10/2026: [ADR-017](decisions/ADR-017-solo-full-map-trial.md) mở rộng chính hai công tắc trial trên cho GOTO trực tiếp xe 01 tới mọi node có đường ngắn nhất có hướng trên map. Không đổi topic hoặc payload. Trial bỏ chờ mutex grant **chỉ khi có một xe**; Core từ chối GOTO nếu còn xe khác trong fleet/owner mutex cũ, và gửi WAIT nếu xe khác xuất hiện bằng telemetry lúc xe 01 chạy. Pin `?` vẫn chỉ được miễn chốt đối với GOTO trực tiếp của xe 01. Bản firmware thường và vận hành hai xe tiếp tục dùng grant/ACK/release; chưa có bằng chứng rời vùng để vận hành thật hai xe.

Cập nhật 09/10/2026: Core có cờ `AGV_TWO_ROBOT_TRIAL=1` riêng để thử giao việc cho 01/02 với pin điều khiển `?`. Chế độ này giữ mutex grant/ACK, giữ quyền khi offline, không dùng đường tắt `AGV_ONE_ROBOT_TRIAL`. Firmware đang nạp vẫn khóa motor. Firmware trong workspace đã thêm queue `mutex_release` JSON đúng grant ID, chỉ sau thẻ kế đo ≥50 cm hoặc thẻ thứ hai của chuỗi thẳng 35+35 cm sau M; chưa nạp/kiểm tra xe thật. Không gửi release khi đầu đọc chỉ hết thấy thẻ M. Dùng `confirm-clear` chỉ sau khi xác minh toàn bộ thân xe đã ra khỏi vùng. [ADR-025](decisions/ADR-025-two-robot-supervised-trial.md).

Cập nhật phục hồi 09/10/2026: NCKHAGV bổ sung `rfidAgeMs` vào diagnostic JSON (`null` nếu chưa đọc được thẻ mới trong phiên MQTT). Core thử hai xe chỉ tự đối soát lỗi/quyền vùng khi vừa nhận telemetry ở S1/S2 và diagnostic cùng node có `rfidAgeMs ≤250 ms`, motor dừng, hai duty PWM bằng 0, không task, Wi‑Fi/MQTT nối. Zone phải cách trạm ít nhất 50 cm theo cả đường ngắn nhất và khoảng cách thẳng trên map đo; lỗi/offline ở trong zone vẫn giữ owner. Core có thể tự gửi `RESET_FAULT` một lần cho lỗi mạng và chỉ xóa ERROR sau diagnostic `AGV_IDLE/NONE/STOP`. Firmware chưa có trường này tiếp tục dùng xác minh thủ công; payload telemetry pipe và command v0 không đổi. [ADR-026](decisions/ADR-026-safe-automatic-recovery-at-home.md).

Các [fixture](../contracts/examples/current-mqtt.json) và [schema command](../contracts/current-command.schema.json) ghi nhận hợp đồng để kiểm tra tài liệu. Schema chưa được gắn vào core/gateway; ràng buộc bổ sung trong schema thể hiện đầu vào mong muốn, không khẳng định parser hiện tại đã thực thi đầy đủ.

## Chọn giao thức theo lớp

13/10/2026: Core gửi snapshot/delta ưu tiên khi telemetry có RFID/position_valid mới, sau checkpoint thành công; gateway gửi vị trí mới ngay và preempt timer snapshot thường đang chờ. Nhịp heartbeat/broadcast thường vẫn200ms; burst RFID có thể vượt5Hz. MQTT giữ payload; WS state thêm core_snapshot_seq tăng theo core_session_id, gateway không apply snapshot cũ cùng phiên từ phản hồi lệnh đến muộn. Sequence delta từng socket giữ nguyên, không đoán node giữa hai RFID. Xem [ADR-049](decisions/ADR-049-priority-rfid-snapshots.md).

Cập nhật 13/10/2026: firmware mới báo `straightThroughSupported:true`. Core chỉ khi đã nhận capability mới gửi GOTO có `pass_through:[0,1,...]`, index 0-based trong `path` còn lại, không gồm đích cuối. Core tính từng bộ ba trước–hiện tại–kế tiếp, cạnh đúng chiều và đi thẳng; kho/trạm và gate ngay sau đoạn lùi ra kho không được đi xuyên. MCU kiểm tra lại hình học, hướng tiến và index, đọc RFID đúng chặng rồi mới tiếp tục bám line. Không dừng/chờ NODE_ORDER tại điểm đi xuyên đã được GOTO cấp phép khi cảm biến/mạng tốt và có đủ grant hiện tại/kế tiếp. MCU xin trước tối đa một zone của chặng tiếp theo khi đang tiến tới RFID đi xuyên; chưa có quyền thì dừng tại RFID trước vùng đó và chờ NODE_ORDER/grant theo luồng cũ. Tuyến C4→M4→GS2→M3→C3 có index [0,1,2]; các telemetry vẫn M4,GS2,M3,C3, chỉ C3 hoàn thành task. Không trả mutex tại thẻ M; clearance vẫn dùng thẻ rời vùng đã đo. Capability thiếu hoặc firmware/Core cũ giữ dừng từng node. Các ID, ACK, watchdog và telemetry pipe không đổi. Xem [ADR-048](decisions/ADR-048-route-straight-through.md).

Audit 02/10/2026: mất broker không đảm bảo Core nhận LWT; callback disconnect gián đoạn task và giữ ERROR/uncertain_path/mutex. Reconnect chưa tự xóa lỗi. Simulator phục hồi bằng telemetry/heartbeat mới, clear-error và token mới khi checkpoint yêu cầu recovery. Payload MQTT giữ nguyên; [ADR-011](decisions/ADR-011-simulation-conflict-audit.md).

| Kết nối | Lựa chọn nền | Điều cần chốt |
| --- | --- | --- |
| Browser ↔ gateway | HTTP REST và WebSocket `/ws` | Kết quả thao tác, reconnect, tuổi dữ liệu |
| Gateway ↔ core | WebSocket JSON | Request/result ID, lỗi và phiên bản map |
| Core ↔ robot | MQTT qua TCP/WiFi | Client ID, QoS, payload tối đa, auth, offline |
| MCU ↔ motor/cảm biến | GPIO PWM UART I2C SPI theo bo mạch | Điện áp, chân, timing, checksum nếu framing |
| MCU ↔ MCU phụ | UART/CAN/RS485 nếu thiết kế cần | Framing, địa chỉ, CRC, timeout và STOP |

QoS 0 có thể mất message; QoS 1 có thể có duplicate; QoS 2 mô tả bảo đảm truyền MQTT, không chứng minh một chuyển động vật lý thực hiện đúng một lần. ACK ứng dụng và deduplicate vẫn phải có theo hợp đồng. Nguồn: [chuẩn OASIS MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).

## Mở rộng cần thiết kế trước khi áp dụng

Đề xuất `v1`: `schema_version`, `robot_id`, `session_id`, `seq`, timestamp/uptime, node, pin, state, fault; command thêm phiên server và hạn dùng. Topic v1 hoặc capability negotiation phải tách rõ khỏi payload chuỗi v0.

Counter command/grant hiện được lưu và reserve block trước khi cấp ID; không reset khi restart. Task có counter riêng do Core cấp. Sketch lưu high-water cmd_id trong NVS và từ chối ID cũ sau reboot; heartbeat nhận diện phiên Core mới. Các trường session/TTL ngay trong command vẫn là đề xuất v1 chưa triển khai.

Không retained command di chuyển. Core hiện subscribe status/LWT, offline gián đoạn task và giữ vùng. Status retained chưa có timestamp/phiên: online không tự chứng minh robot sẵn sàng; Core cần telemetry/RFID mới. Khi reconnect, robot xác minh vị trí và trạng thái lỗi trước khi cho phép di chuyển.

## Mạng LAN và broker

Core hiện nối broker local. Robot dùng IP LAN của máy broker, client ID duy nhất và topic riêng. Tham khảo [Mosquitto](https://github.com/eclipse-mosquitto/mosquitto): chạy mặc định phù hợp kiểm tra local; kết nối máy khác cần listener và cấu hình xác thực được xác định rõ.

Broker TLS/ACL từng xe và Core/ESP32 user/password/CA đã có cấu hình cùng kiểm thử broker thật trong fixture. [Gia cố và provisioning](18-hardening.md) hướng dẫn chuyển sang TLS8883. Chưa áp cấu hình lên dịch vụ MQTT vận hành của người dùng; tài khoản/cert phải được nhóm tạo và đồng bộ hai xe trước khi chuyển listener.

[PubSubClient](https://github.com/knolleary/pubsubclient) publish QoS 0, subscribe QoS 0/1, buffer mặc định 256 byte tính cả header. GOTO dài có thể vượt buffer; đo kích thước thật và cấu hình hoặc chọn client khác. [ESP MQTT](https://github.com/espressif/esp-mqtt) hỗ trợ MQTT, TLS, will và các QoS; phù hợp cân nhắc khi dùng ESP-IDF. [MQTT.js](https://github.com/mqttjs/MQTT.js) phù hợp robot giả lập Node.js, chưa phải dependency của gateway hiện tại.

Kỹ năng: [agv-network](../.agents/skills/agv-network/SKILL.md). Firmware: [08-firmware.md](08-firmware.md).

## HTTP/WS cập nhật 29/09/2026

Map v3 đổi **giá trị ID node** thành M1…M6, K1…K6, G1…G6, S1/S2, GS1/GS2 và C1…C4. Telemetry/path/target/zone phải dùng ID mới; định dạng payload giữ nguyên. Xem [bảng tên và tọa độ](15-map-layout-update.md). Cập nhật 03/10/2026: [26 UID](../firmware/AGV_Connection_Test/README.md) đã được nạp sẵn vào firmware; telemetry vẫn gửi tên node, không gửi UID thô.

MQTT vẫn giữ bảng payload phía trên. Server cấp cmd_id qua CommandIds.hpp, lưu block vào command_sequence.json cạnh bản đồ; restart tiến trình không reset về 0 khi giữ đúng file. JSON telemetry/session MCU còn OPEN. Gateway–core WS v1 có request_id/command_result/core_session_id; browser có tuổi snapshot và task_history. Chi tiết tại [HTTP/WS v1](../contracts/web-server.md) và [bản cập nhật](14-web-server-update.md).

Gateway mặc định nối `ws://127.0.0.1:8080` để khớp listener IPv4 của core. Core dùng client ID `AGV_MasterServer`; chỉ chạy một core cho cùng đội xe/broker. Theo [MQTT 3.1.1 §3.1.4](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html), kết nối mới dùng ClientId đang hoạt động khiến broker đóng kết nối cũ. Core mới từ chối listener trùng trước khi kết nối MQTT; đổi port không biến hai core thành cơ chế dự phòng. Xem [chẩn đoán khởi động](16-startup-troubleshooting.md).

[Sketch test ESP32](../firmware/AGV_Connection_Test/README.md) dùng đúng v0, heartbeat 1 s, MQTT client ID theo robot/MAC và buffer/path giới hạn phù hợp map v3. Test đồng bộ parser với payload core thật tại [firmware.integration.js](../tests/firmware.integration.js). ACK chỉ xác nhận nhận lệnh; sketch không tự cập nhật node tới target.

## Firmware NCKHAGV bổ sung 04/10/2026

`firmware/NCKHAGV/NCKHAGV` đã chuyển từ schema `TASK/START` riêng sang command JSON `cmd/cmd_id` và telemetry pipe v0 ở lớp MQTT. Status retained và heartbeat Core 3 giây được dùng cho fail-safe; ACK chỉ là biên nhận. Bổ sung 04/10: firmware kiểm tra path theo cạnh có hướng, tính hướng rẽ từ ba RFID liên tiếp, xin quyền mutex và ACK grant trước đoạn chạm M1–M6. GOTO vẫn bị khóa motor vì chưa chứng minh toàn thân xe rời M1–M6 và chưa hiệu chuẩn chuyển động. Firmware không phát mutex release suy đoán. Cấu hình TLS/CA và tài khoản broker là tùy chọn theo listener thực tế, không tự đổi broker đang vận hành. Nếu pin chưa hiệu chuẩn, firmware gửi `node|?|obstacle` và `batteryCalibrated=false` trên topic chẩn đoán; Core hiển thị RFID trên map nhưng giữ `battery_measured=false` và chặn giao nhiệm vụ thông thường. Không dùng topic `error` cho trạng thái pin chưa đo vì Core coi đó là lỗi dừng. Chế độ `AGV_ONE_ROBOT_TRIAL=1` chỉ cho lệnh thủ công xe 01 S1→GS1; xem [ADR-016](decisions/ADR-016-single-robot-server-motion-trial.md). Chi tiết tại [ADR-014](decisions/ADR-014-nckhagv-core-sync.md) và [ADR-015](decisions/ADR-015-basic-rfid-line-motion.md).

Giới hạn S1→GS1 trong đoạn trên là lịch sử bản 04/10; bản 05/10 dùng phạm vi trial toàn map theo ADR-017. Không dùng bản firmware hoặc Core cũ cùng bản trial mới.

Project NCKHAGV đã bỏ encoder theo yêu cầu người dùng. Topic bổ sung `agv/{id}/event` của firmware này chỉ mang sự kiện/node/task/chiều lùi; không còn các trường `leftPulses`, `rightPulses`, `distanceCm`, `distanceMm`. Core v0 xác nhận đến node qua telemetry RFID, không qua topic sự kiện này. [Ví dụ sự kiện chẩn đoán](../contracts/examples/nckhagv-diagnostic-event.json) không thay đổi payload command/ACK/telemetry.

Cập nhật 08/10/2026: theo file pin người dùng bổ sung trong firmware, NCKHAGV cấu hình hệ số chia áp 30 kΩ/7.5 kΩ (5.0), pin 2S 18650 với dải nội suy tuyến tính 6.4–8.4 V. Trong giai đoạn thử hiển thị, firmware giữ telemetry điều khiển là `node|?|obstacle` và gửi phần trăm ở `agv/{id}/diagnostic`. Core lưu giá trị đó riêng thành `battery_display_percent`; trường `battery`/`battery_measured` điều khiển không đổi nên ngưỡng 20%/40% chưa áp dụng. Đây là ước tính theo điện áp, chưa được hiệu chuẩn/đối chiếu trên xe thật.

Cập nhật 07/10/2026: `agv/{id}/diagnostic` của NCKHAGV thêm `linePattern` số 0–7 (bit trái/giữa/phải) để chẩn đoán thử quay tại GS1. Đây là topic chẩn đoán, không thay đổi telemetry pipe v0 hoặc điều kiện Core xác nhận đến node. Bản `esp32dev_trial` chờ 1 giây sau mỗi RFID trước khi quay/đi tiếp; bản thường vẫn 200 ms.

Cập nhật crash/checkpoint10/10/2026: firmware01 buildId=20261010-grant-buffer-fix copy zone/token vào grant riêng trước publish mutex_ack, tránh buffer PubSubClient RX/TX ghi đè pointer JSON. Diagnostic firmwareBuild cho biết đúng ảnh nạp. Core lỗi checkpoint dừng outbound, thử save lại mỗi2s; chỉ sau lưu thành công phục hồi heartbeat. Gateway storage_healthy=false báo core_ready=false, command503 và chặn điều phối đơn. Payload mutex/command/telemetry pipe giữ nguyên. Xem ADR-035.
Core test01 10/10/2026 (ADR-036): clear_error/RESET_FAULT không yêu cầu ở S1; Core chờ diagnostic khỏe tại RFID hiện tại mới IDLE, ACK không đủ. Test01 mở/đóng ca tại node đã xác nhận, không ép về S1; at_home vẫn dùng trạm thực. Robot02/both giữ home gate. Không đổi command payload/firmware, không cần nạp lại. MCU vẫn kiểm tra line/mạng/vị trí/hướng; không trả mutex chỉ vì reset giữa tuyến.
## Vật cản cạnh trong phiên mô phỏng — 10/10/2026

Runtime dùng HTTP `/sim-api/edge-obstacles` và WS v1 của Core riêng, với `sim_edge_obstacle`/`sim_replan` chỉ bật bởi `AGV_SIMULATION=1`. `blocked_edges` trong map snapshot khóa đoạn vật lý cả hai hướng. MQTT command JSON, telemetry pipe, ACK và mutex giữ nguyên; không publish thao tác thử sang broker vận hành. Chi tiết và ví dụ tại [hợp đồng web](../contracts/web-server.md), [ADR-037](decisions/ADR-037-simulation-edge-obstacles.md).
