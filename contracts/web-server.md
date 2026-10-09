# Hợp đồng HTTP và WebSocket web/server phiên bản 1

Đã triển khai 29/09/2026; xem [bản cập nhật](../docs/14-web-server-update.md). MQTT MCU giữ hợp đồng tại [07-network.md](../docs/07-network.md).

## Response HTTP

GET danh sách trả **array JSON trực tiếp**, không bọc `{ok,data}`. POST/DELETE thành công trả `{ok:true,...}`. Điều khiển trả 200 sau core xác nhận xử lý, không chứng minh hoàn thành chuyển động. Lưu snapshot vào SQLite trả 201.

Lỗi trả `{ok:false,error,unknown}`. 400 sai đầu vào; 404 không tồn tại; 409 xung đột trạng thái/core cũ; 413 command quá 1 MiB; 503 thiếu liên kết/dữ liệu mới; 504 hết thời gian chờ. `unknown:true` nghĩa có thể core đã xử lý nhưng gateway chưa nhận được kết quả; phải đối soát, không tự gửi lại.

| Endpoint | Body / response chính |
| --- | --- |
| GET /api/health, /api/state | Liên kết và state metadata |
| GET /api/shift | Object ca hiện nhận từ core |
| POST /api/shift/start, /api/shift/end | `{}` → `{ok,shift}`; xác nhận mở/nhận kết thúc ca, không chứng minh đã về trạm |
| GET /api/agvs, /api/tasks | Array robot / hàng đợi task có task_id |
| POST /api/tasks | `{target,priority?}` → `{ok,task_id}` |
| POST /api/agvs/:id/goto | `{target,priority?}` → `{ok,task_id}` |
| POST /api/tasks/cancel | `{task_id}` hủy một nhiệm vụ; `{target}` tương thích hủy mọi task tới đích |
| POST /api/agvs/:id/clear-error | `{}`; cần vị trí và telemetry mới; Core gửi `RESET_FAULT` cho ESP32 NCKHAGV, phải xác minh diagnostic đã rời `AGV_SAFE_STOP` |
| DELETE /api/agvs/:id | Robot OFFLINE không giữ zone |
| GET /api/tasks/history | Array DB, core_task_id/core_session/status/reason |
| GET /api/orders | Array đơn hàng kho lưu trong SQLite, mới nhất trước |
| POST /api/orders | `{pickups:[{pickup,quantity}],deliveries:[{dropoff,quantity}]}` → HTTP 201 `{ok:true,order}` |
| POST /api/orders/:id/resolve | `{physically_clear:true}` sau khi đối soát xe/kiện; đóng đơn, không chạy lại kiện chưa giao |
| GET /api/agvs/:id/history | Array sự kiện node/pin/state |
| GET /api/logs | Array log, core_session + core_seq |
| GET /api/map, /api/mutex | Map / object mutex |
| POST /api/map/node | `{id,x,y,type}`; thêm hoặc cập nhật |
| DELETE /api/map/node/:id | Xóa node và cạnh liên quan |
| POST /api/map/edge | `{from,to,weight,type?,path_class?,two_way?}` |
| DELETE /api/map/edge | `{from,to}`; xóa cả cặp nếu two_way |
| POST /api/map/obstacle | `{node,blocked:boolean}` |
| GET /api/map/obstacles/history | Array log vật cản |
| POST /api/map/save | `{name?}` → `{ok,id,name,path}`; path do core cấu hình |
| GET /api/map/snapshots | Array `{id,name,saved_at}` |
| POST /api/map/snapshots | `{name?}` → `{ok,id,name}`; lưu state mới từ core |
| GET /api/map/snapshots/latest | Map JSON mới nhất trong SQLite |
| GET /api/map/snapshots/:id | `{id,name,saved_at,map}` |
| POST /api/map/snapshots/:id/apply | `{}` → `{ok,map_revision}` |
| DELETE /api/map/snapshots/:id | Xóa snapshot, không đổi graph đang chạy |
| GET /api/kpi/latest, /api/kpi/history | Report / array report |
| POST /api/kpi/generate | `{}` → `{ok,report}` |

Chế độ thử một xe 05/10/2026 (`AGV_ONE_ROBOT_TRIAL=1` ở Core): người vận hành dùng `POST /api/agvs/01/goto` hoặc chọn xe 01 và node đích trên web. Core dùng Dijkstra trên bản đồ có hướng, chỉ nhận xe 01 đang IDLE/có RFID mới, không có xe khác trong fleet hoặc owner mutex cũ. `POST /api/tasks` bị từ chối trong chế độ này; dấu pin `?` chỉ được phép cho GOTO trực tiếp xe 01. Việc HTTP trả `task_id` vẫn chưa chứng minh xe đã chuyển động; task chỉ completed khi telemetry RFID đích đến. [ADR-017](../docs/decisions/ADR-017-solo-full-map-trial.md).

Chế độ thử hai xe 09/10/2026 (`AGV_TWO_ROBOT_TRIAL=1` ở Core): `shift.two_robot_trial=true` trong state; yêu cầu cả 01/02 online tại S1/S2, không lỗi/vùng chiếm trước khi mở ca. `POST /api/tasks`, `POST /api/agvs/:id/goto` và đơn hàng kho có thể giao cho 01/02 với pin điều khiển `?`; mutex vẫn cấp một chủ mỗi vùng và không nhả theo việc đầu đọc RFID mất tín hiệu. Firmware khóa motor hiện tại không thực hiện chuyển động từ lệnh Core. [ADR-025](../docs/decisions/ADR-025-two-robot-supervised-trial.md).

Từ 09/10/2026, bấm node trên dashboard khi chưa chọn robot gửi `POST /api/tasks` với `{target,priority}`. Core chọn xe `IDLE`, online, có RFID hợp lệ và đường khả dụng với tổng trọng số thấp nhất; bằng chi phí thì ID nhỏ hơn thắng. Trong chế độ thử hai xe, phần trăm pin chưa đo không tham gia lọc/chọn xe. Nếu chưa có xe khả dụng, task ở hàng đợi. Chọn robot trước rồi bấm node dùng `POST /api/agvs/:id/goto` và bỏ qua chọn tự động.

History có query `limit` integer dương, tối đa 1000; KPI tối đa 200. X/Y và trọng số/priority phải là number JSON, không nhận chuỗi số. Giới hạn kiểu và cấu trúc tại [bản cập nhật](../docs/14-web-server-update.md).

## Gateway → core

```json
{"cmd":"add_task","request_id":"request-uuid","target":"K3","priority":0}
```

```json
{"type":"command_result","request_id":"request-uuid","core_session_id":"core-session","ok":true,"data":{"task_id":"task-uuid"},"state":{"protocol_version":1,"core_session_id":"core-session","mqtt_connected":true,"map":{"nodes":[],"edges":[],"obstacles":[],"revision":1},"fleet":{"agvs":[],"pending_tasks":[],"tasks":[]},"mutex":{},"logs":[]}}
```

`state` trong result là snapshot sau thao tác. Result bị từ chối có `ok:false,status,error`, vẫn kèm state để đối soát. Không trộn command_result vào state browser.

01/10/2026: Core sinh task_id từ task_sequence.json; gateway chỉ dùng ID Core trả về/state để theo dõi. Request bị từ chối chưa là nhiệm vụ Core; timeout có unknown=true cần đối soát state, không tạo task giả. Khôi phục cùng ID qua core_session mới giữ một dòng lịch sử DB. [Gia cố](../docs/18-hardening.md).

API/WS yêu cầu Bearer token/cookie khi có AGV_AUTH_TOKEN; login/logout tại POST /auth/login `{token}` và /auth/logout; GET /auth/status. Origin được giới hạn. HTTPS/WSS khi cấu hình cert/key. Chế độ không token chỉ loopback.

POST /api/map/obstacle thêm ttl_s 0..86400 (0 giữ khóa thủ công). POST /api/mutex/:zone/confirm-clear nhận `{owner,grant_id,physically_clear:true}`; Core chỉ xử lý owner ERROR/OFFLINE, không còn robot, hoặc trong ca thử hai xe là robot `IDLE` online, RFID hợp lệ tại trạm riêng, không còn task/đích/đường chưa xác minh. Người vận hành phải xác nhận toàn bộ xe đã ra khỏi vùng; token owner/grant_id phải khớp. Fleet/mutex snapshot bổ sung storage_healthy, needs_recharge và grant_id/grant_acked/recovery_required.

Trong `AGV_TWO_ROBOT_TRIAL=1`, Core còn có phục hồi tự động tại trạm riêng theo [ADR-026](../docs/decisions/ADR-026-safe-automatic-recovery-at-home.md): diagnostic NCKHAGV cần `node,state,error,motor,leftDuty,rightDuty,taskActive,wifi,mqtt,rfidAgeMs` và trường pin display hiện có. `rfidAgeMs` là số mili giây từ lần RC522 đọc được thẻ cùng node trong phiên MQTT hiện tại, hoặc `null`; chỉ giá trị 0–250 được dùng để tự phục hồi. Telemetry phải mới tại S1/S2, xe dừng với hai duty PWM bằng 0, hết task và mỗi zone đang giữ cách trạm ≥50 cm theo đường và khoảng cách thẳng trên map đo. Core chỉ thử `RESET_FAULT` tự động với lỗi mạng, tối đa một lần; chỉ diagnostic khỏe `AGV_IDLE/NONE/STOP` mới xóa ERROR. Lỗi/offline không tự nhả owner và firmware cũ thiếu trường vẫn dùng API xác minh thủ công.

## Gateway → browser /ws

Core → gateway: initial full snapshot có state_seq, sau đó `{"type":"state_delta","base_state_seq":1,"state_seq":2,"changes":{"fleet":{}}}` với changes chứa các trường đã đổi. Gateway khôi phục baseline rồi xử lý state đầy đủ; thiếu sequence sẽ reconnect. Browser mới chọn /ws?delta=1, cũng nhận initial full rồi delta; client không chọn vẫn full snapshot. Tối đa 5 state/giây, command_result không đợi throttle. Không tự apply map snapshot khi reconnect.

Quản lý ca thêm `shift`: `{supported,single_robot_trial,phase,home_stations,robots,all_home,can_start}`. `single_robot_trial` chỉ bật khi Core chạy với `AGV_ONE_ROBOT_TRIAL=1`; khi đó `home_stations` chỉ có `01:S1`. Trong cấu hình mặc định 01/S1 và 02/S2, một xe đã xuất hiện và đủ điều kiện có thể mở ca; xe vắng từ đầu không chặn, còn xe đã xuất hiện rồi offline/lỗi vẫn chặn. Pin chưa đo (`?`) được miễn chốt mở ca khi chỉ có một xe; pin đã đo thấp vẫn khóa. Hai xe cùng xuất hiện tiếp tục cần cả hai ở trạm và pin đo ≥40%. Phase CLOSING/CLOSED/RUNNING hoặc UNAVAILABLE với đồ thị không hỗ trợ. Robot row có `id,home_station,actual_node,participating,at_home,ready,reason`; thiếu telemetry thì `participating=false`, actual_node rỗng, không gán vị trí mặc định. Fleet thêm `accepting_tasks/home_stations`. WS command `start_shift/end_shift` có request_id, trả command_result như lệnh khác. Task/GOTO khi gate đóng trả 409; task đang chạy tiếp tục, pending giữ nguyên. Chi tiết [ca và tập kết](../docs/17-shifts-and-home-charging.md).

Snapshot giữ map/fleet/mutex/logs, bổ sung `agv_connected`, `core_ready`, `protocol_supported`, `state_received_at`, `stale_after_ms`, `task_history`. Robot có `last_seen_s`, `online`, `position_valid`, `battery_measured`, `battery_display_percent`, `battery_display_available`, `error`, `task_id`, `uncertain_path`. Với firmware NCKHAGV khi đang thử hiển thị, phần trăm đến từ topic diagnostic và không được dùng cho ngưỡng điều phối; `battery`/`battery_measured` vẫn phản ánh riêng telemetry điều khiển. Tuổi dữ liệu là thời gian nhận tại core/gateway, chưa có timestamp cảm biến.

Snapshot gateway còn có `orders` (tối đa 50 đơn mới nhất) để web theo dõi thời gian thực; GET `/api/orders` đọc toàn bộ lịch sử. Mỗi đơn tối đa 20 kiện, số kiện ở từng kho là integer 1–20, không trùng kho trong cùng danh sách và tổng lấy/giao bằng nhau. Gateway tự ghép từng kiện theo tổng trọng số đường có hướng nhỏ nhất, không ghép lấy/giao tại cùng node. Mỗi kiện có `pickup`, `dropoff`, `robot`, `task_id`, `status`; các trạng thái chính là `queued`, `to_pickup`, `pickup_service`, `to_dropoff`, `dropoff_service`, `delivered`, `interrupted`. Sau khi Core báo task `completed` **và** xe IDLE tại RFID tương ứng, gateway giữ bước mô phỏng bốc/dỡ trong 10 giây rồi mới cấp chặng tiếp hoặc cộng kiện giao. Đây không phải xác nhận hàng vật lý. Mất Core/MQTT, dữ liệu cũ, đổi phiên, task gián đoạn, vị trí sai hoặc gateway restart đưa đơn về `needs_attention`; không tự phát lại task không rõ kết quả. Xe gắn với đơn cần đối soát không được cấp đơn mới tới khi người vận hành xác nhận kiểm tra xe/kiện và đóng đơn. Đơn chỉ được tạo khi ca RUNNING và Core/MQTT sẵn sàng. Điều phối dùng `direct_goto` hiện có của Core, không đổi gate chuyển động, MQTT hoặc firmware.

Firmware RC522 gửi UNKNOWN khi chưa xác định node. Core thêm state `UNLOCALIZED`, vẫn online nếu heartbeat mới nhưng position_valid=false; boot node rỗng, không vẽ marker. UNKNOWN trong route đang chạy chuyển ERROR/gián đoạn task và giữ node đo trước/uncertain_path/mutex. Giá trị telemetry ghi tại [MQTT](../docs/07-network.md).

KPI `total_tasks/completed_tasks` là nhóm task mới tạo trong ca và kết quả tại thời điểm chụp báo cáo. `completed_in_period` đếm chuyển trạng thái completed trong ca, kể cả task tạo trước. `pending_tasks/dispatched_tasks` là tồn đọng hiện tại. `avg_travel_seconds` là từ lần gán đầu đến hoàn thành, có thể gồm chờ/requeue, không phải tốc độ hay thời gian chạy motor thuần. `telemetry_events` là số thay đổi trạng thái/node/pin đã lưu, không phải tổng MQTT packet.
