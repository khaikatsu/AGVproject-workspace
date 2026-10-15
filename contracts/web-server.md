# Hợp đồng HTTP và WebSocket web/server phiên bản 1

14/10/2026, Core mode `robot01`: robot 01 thêm `encoder_supported`, `speed_cm_s` (số/null), `odometry` và `estimated_position` (object/null). Estimate phục vụ hiển thị trên cạnh, giới hạn 98% trước đích và reset khi RFID đổi; `node`, task/history/mutex giữ dữ kiện RFID. Mẫu ≤1000 ms, đúng boot/connection/Core session và route/index mới valid; stale/lỗi/offline trả speed/estimate null. Không lưu checkpoint các trường này. [Hợp đồng encoder](robot01-odometry.md), [ADR-050](../docs/decisions/ADR-050-robot01-encoder-position-speed.md). HTTP/WS v1 và delta giữ nguyên.

Test kho độc lập10/10/2026: POST `/api/agvs/:id/warehouse-action` body `{action:"pickup"|"dropoff",warehouse:"K1"}` trả201 `{ok:true,action}` sau Core chấp nhậntask (hoặc xe đã ở kho). Chỉ mode robot01/id01; lỗi400/409/503 giữ cơ chế chung. Snapshot thêm warehouse_actions; action chứa id/robot/action/warehouse/gate/status/task_id/service_until/core_session_id/route/exit_route/reason. Status dispatching→to_warehouse→waiting(5s)→dispatching_exit→to_gate→completed, hoặc interrupted. Task/RFID/IDLE xác nhận mỗi đích; không tạoorder/cargo và không replay. Mode01 không nhận/điều phối orders; lịch sử giữ nguyên. Health warehouse_test_actions=true. [ADR-045](../docs/decisions/ADR-045-standalone-warehouse-tests-without-orders.md).

Cập nhật luồng kho10/10/2026: thời gian bốc/dỡ mô phỏng là5s, item thêm ready_exit/dispatching_exit/to_exit, exit_gate và route_plan.exit. Delivered chỉ sau task exit completed/RFIDgate/IDLE. GET `/api/health` thêm warehouse_service_ms. Đơn queued chưa phát lệnh không giữ robot khi cần đối soát; đơn đã bắt đầu giữ tới xác nhận người vận hành. [ADR-044](../docs/decisions/ADR-044-five-second-warehouse-service-and-gate-exit.md).

Đơn hàng test10/10/2026: order bổ sung selection_policy (`fixed:01`/`fixed:02`/`nearest`); item bổ sung waiting_reason và `route_plan: {robot_start,pickup:{path,cost},delivery:{path,cost}}`. Path dự kiến gồm node đầu/đích, cost là tổng trọng số; không có đường trả[]/null. Mode01 mặc định01 từ queued nhưng chỉ dispatch khi đủ điều kiện. Preview không thay task_id/kết quả hoặc tuyến thực tế Core cấp với mutex. [ADR-043](../docs/decisions/ADR-043-robot01-warehouse-order-planning.md).

Chia sẻ mô phỏng10/10/2026: launcher `--public-proxy` trả URL tương đối `/simulation` trong POST `/launch/start`; GET `/launch/status` thêm boolean `public_proxy`. Proxy REST/assets và `/ws` cùng origin, kiểm tra Origin/Host rồi chuẩn hóa về gateway loopback của phiên riêng. Payload REST/WS v1 giữ nguyên. [ADR-042](../docs/decisions/ADR-042-share-simulation-over-internet.md).

Client mô phỏng 10/10/2026: GET `/sim-api/state` và POST heartbeat `/sim-api/viewer` timeout 2,5 giây; POST điều khiển timeout 8 giây. Timeout/mất phản hồi POST là **kết quả chưa xác định**, không retry tự động. Poll giữ snapshot cuối, khóa điều khiển đến khi HTTP và WS/Core có dữ liệu mới; không apply map/task cũ. Ví dụ HTTP 503 `{ "error":"fixture unavailable" }` hoặc snapshot thiếu trường chuyển client sang dữ liệu cũ và thử lại với backoff tối đa 4 giây. Không đổi schema HTTP/WS/MQTT. HTML do launcher mở có `data-sim-launcher="http://127.0.0.1:<cổng-launcher>/simulation"` để quay về điểm mở khi cổng phiên đã đóng. [ADR-039](../docs/decisions/ADR-039-simulation-browser-reconnection.md).

Đã triển khai 29/09/2026; xem [bản cập nhật](../docs/14-web-server-update.md). MQTT MCU giữ hợp đồng tại [07-network.md](../docs/07-network.md).

15/10/2026 — Trong Core test 01, mỗi robot có thêm `obstacle_alert: null | {direction,range_cm}` và `obstacle_detection: null | {from,to,x,y,range_cm,direction,source}` ở snapshot/delta và GET `/api/agvs`. Alert hiện khi diagnostic sonar mới ≤3 giây dù encoder chưa có; Core ghi log khi phát hiện/dời vật cản. `x/y` là cm trong hệ map, chiếu từ vị trí encoder cộng số cm sonar theo cạnh có hướng; web tô đỏ cạnh `from→to` và chấm tại `x/y`. Điểm chỉ hiện khi odometry hợp lệ ≤1 giây và cùng chặng; mất dữ liệu hoặc dời vật cản trả `null`. Các trường này chỉ để giám sát, không là API khóa cạnh, không xác nhận khoảng hở an toàn hay vị trí tuyệt đối. [ADR-055](../docs/decisions/ADR-055-robot01-dual-sonar-edge-warning.md).

## Response HTTP

13/10/2026: snapshot/delta có vị trí RFID mới được ưu tiên qua Core/gateway sau commit, không chờ timer broadcast thường. State thêm `core_snapshot_seq` tăng trong mỗi core_session_id, độc lập sequence delta trên từng socket. Gateway không apply snapshot có seq thấp hơn/bằng state mới nhất cùng phiên, kể cả snapshot trong command_result đến muộn; kết quả request vẫn được xử lý. Core mới reset seq ở phiên mới, Core cũ thiếu trường giữ tương thích. Thời gian nhận state vẫn là thời gian gateway nhận, không phải thời điểm phần cứng đọc RFID. Không nội suy node. [ADR-049](../docs/decisions/ADR-049-priority-rfid-snapshots.md).

13/10/2026: tối ưu node thẳng là hợp đồng Core/firmware; API GOTO/task vẫn giữ nguyên. Core cấp phép `pass_through` theo tuyến nếu MCU có `straightThroughSupported`; web vẫn nhận đủ node/RFID và tiến độ task. HTTP/ACK không chứng minh xe đi xuyên hay đã đến đích. Điểm chưa có quyền mutex hoặc cần rẽ vẫn dừng; không có nút web bỏ qua mutex.

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
| POST /api/agvs/:id/clear-error | `{}`; cần vị trí, telemetry mới và broker kết nối. Ca thử `robot01/robot02/both` yêu cầu đúng trạm riêng S1/S2, trả 409 nếu còn ở giữa tuyến. HTTP thành công là đã gửi `RESET_FAULT`; Core giữ ERROR và task chờ đến diagnostic mới IDLE/NONE/STOP, PWM 0/0, RFID mới tại trạm. ACK không mở khóa chuyển động. |
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

Hướng đầu xe 10/10/2026: snapshot robot thêm `heading`, `heading_supported`, `heading_source`. Hướng N/E/S/W theo map; nguồn `bay_default` là quy ước đầu xe hướng vào kho K hoặc ra cửa trạm S, `route` từ chặng bám line, `unknown` khi chưa biết. UNKNOWN khi vị trí chưa hợp lệ/offline/diagnostic cũ. UI vẽ mũi tên đầu xe trước chấm RFID; lúc lùi mũi tên giữ hướng đầu xe; trạm S1 ↓/S2 ↑ để xuất phát tiến. Không còn REST `/api/agvs/:id/heading` (404), WS `set_heading`, trạng thái heading_pending hoặc MQTT SET_HEADING; người vận hành không gán hướng từ web. [ADR-033](../docs/decisions/ADR-033-stations-face-outward.md).

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

Đơn hàng kho ở `robot01/robot02` cũng chỉ chọn xe theo `shift.trial_robot_id`/`fleet.trial_robot_id`, kể cả xe không chọn có chi phí đường ngắn hơn.

Menu Core 09/10/2026 bổ sung các trường trong `shift`: `test_mode` (`normal`, `robot01`, `robot02`, `both`, hoặc `legacy01` cho cấu hình solo cũ), `trial_robot_id` (`01`, `02` hoặc rỗng), `supervised_trial` boolean. Ví dụ chọn 2: `{"test_mode":"robot02","trial_robot_id":"02","supervised_trial":true,"single_robot_trial":false,"two_robot_trial":false,"home_stations":{"02":"S2"}}`. `single_robot_trial` tiếp tục biểu thị solo 01 cũ; `two_robot_trial` chỉ true khi thực sự chọn cả hai. `fleet.trial_robot_id` đồng bộ bộ lọc giao việc. Không đổi topic/payload MQTT.

Ở `robot01/robot02`, `/api/tasks` chỉ giao cho ID đã chọn; GOTO xe khác trả 409. `shift.inactive_robots_ready` và các dòng `robots` có `participating:false` cho biết xe không chọn đã xuất hiện cần dừng/rảnh, RFID hợp lệ tại trạm riêng, hết task/đường chưa xác minh/quyền vùng trước khi mở ca. Xe không xuất hiện không bắt buộc tham gia. Core vẫn nhận telemetry và bảo vệ vùng/đường của xe không chọn. Xác minh thủ công quyền tại trạm áp dụng cho chế độ `supervised_trial` với các chốt owner/grant/physically_clear hiện có. [ADR-028](../docs/decisions/ADR-028-core-startup-mode-menu.md).

Core → gateway: initial full snapshot có state_seq, sau đó `{"type":"state_delta","base_state_seq":1,"state_seq":2,"changes":{"fleet":{}}}` với changes chứa các trường đã đổi. Gateway khôi phục baseline rồi xử lý state đầy đủ; thiếu sequence sẽ reconnect. Browser mới chọn /ws?delta=1, cũng nhận initial full rồi delta; client không chọn vẫn full snapshot. Tối đa 5 state/giây, command_result không đợi throttle. Không tự apply map snapshot khi reconnect.

Quản lý ca thêm `shift`: `{supported,single_robot_trial,phase,home_stations,robots,all_home,can_start}`. `single_robot_trial` chỉ bật khi Core chạy với `AGV_ONE_ROBOT_TRIAL=1`; khi đó `home_stations` chỉ có `01:S1`. Trong cấu hình mặc định 01/S1 và 02/S2, một xe đã xuất hiện và đủ điều kiện có thể mở ca; xe vắng từ đầu không chặn, còn xe đã xuất hiện rồi offline/lỗi vẫn chặn. Pin chưa đo (`?`) được miễn chốt mở ca khi chỉ có một xe; pin đã đo thấp vẫn khóa. Hai xe cùng xuất hiện tiếp tục cần cả hai ở trạm và pin đo ≥40%. Phase CLOSING/CLOSED/RUNNING hoặc UNAVAILABLE với đồ thị không hỗ trợ. Robot row có `id,home_station,actual_node,participating,at_home,ready,reason`; thiếu telemetry thì `participating=false`, actual_node rỗng, không gán vị trí mặc định. Fleet thêm `accepting_tasks/home_stations`. WS command `start_shift/end_shift` có request_id, trả command_result như lệnh khác. Task/GOTO khi gate đóng trả 409; task đang chạy tiếp tục, pending giữ nguyên. Chi tiết [ca và tập kết](../docs/17-shifts-and-home-charging.md).

Snapshot giữ map/fleet/mutex/logs, bổ sung `agv_connected`, `core_ready`, `protocol_supported`, `state_received_at`, `stale_after_ms`, `task_history`. Robot có `last_seen_s`, `online`, `position_valid`, `battery_measured`, `battery_display_percent`, `battery_display_available`, `error`, `task_id`, `uncertain_path`. Với firmware NCKHAGV khi đang thử hiển thị, phần trăm đến từ topic diagnostic và không được dùng cho ngưỡng điều phối; `battery`/`battery_measured` vẫn phản ánh riêng telemetry điều khiển. Tuổi dữ liệu là thời gian nhận tại core/gateway, chưa có timestamp cảm biến.

Snapshot gateway còn có `orders` (tối đa 50 đơn mới nhất) để web theo dõi thời gian thực; GET `/api/orders` đọc toàn bộ lịch sử. Mỗi đơn tối đa 20 kiện, số kiện ở từng kho là integer 1–20, không trùng kho trong cùng danh sách và tổng lấy/giao bằng nhau. Gateway tự ghép từng kiện theo tổng trọng số đường có hướng nhỏ nhất, không ghép lấy/giao tại cùng node. Mỗi kiện có `pickup`, `dropoff`, `robot`, `task_id`, `status`; các trạng thái chính là `queued`, `to_pickup`, `pickup_service`, `to_dropoff`, `dropoff_service`, `delivered`, `interrupted`. Sau khi Core báo task `completed` **và** xe IDLE tại RFID tương ứng, gateway giữ bước mô phỏng bốc/dỡ trong 5 giây rồi mới cấp chặng tiếp; sau dỡ còn task lùi ra gate trước khi cộng kiện giao. Đây không phải xác nhận hàng vật lý. Mất Core/MQTT, dữ liệu cũ, đổi phiên, task gián đoạn, vị trí sai hoặc gateway restart đưa đơn về `needs_attention`; không tự phát lại task không rõ kết quả. Xe gắn với đơn cần đối soát không được cấp đơn mới tới khi người vận hành xác nhận kiểm tra xe/kiện và đóng đơn. Đơn chỉ được tạo khi ca RUNNING và Core/MQTT sẵn sàng. Điều phối dùng `direct_goto` hiện có của Core, không đổi gate chuyển động, MQTT hoặc firmware.

Firmware RC522 gửi UNKNOWN khi chưa xác định node. Core thêm state `UNLOCALIZED`, vẫn online nếu heartbeat mới nhưng position_valid=false; boot node rỗng, không vẽ marker. UNKNOWN trong route đang chạy chuyển ERROR/gián đoạn task và giữ node đo trước/uncertain_path/mutex. Giá trị telemetry ghi tại [MQTT](../docs/07-network.md).

KPI `total_tasks/completed_tasks` là nhóm task mới tạo trong ca và kết quả tại thời điểm chụp báo cáo. `completed_in_period` đếm chuyển trạng thái completed trong ca, kể cả task tạo trước. `pending_tasks/dispatched_tasks` là tồn đọng hiện tại. `avg_travel_seconds` là từ lần gán đầu đến hoàn thành, có thể gồm chờ/requeue, không phải tốc độ hay thời gian chạy motor thuần. `telemetry_events` là số thay đổi trạng thái/node/pin đã lưu, không phải tổng MQTT packet.

Đỗ trạm: firmware mới giữ task active khi quay đầu tại S; Core giữ WORKING/CHARGING_EN_ROUTE tới RFID đích và diagnostic stationReady khỏe còn mới≤3s, đầu hướng ra GS. Sau đó task completed, task_id/target rỗng và xe IDLE/CHARGING theo luồng hiện có. Tuổi mốc RFID có thể vượt250ms sau pivot, nhưng phải đã đọc trong phiên; null không đủ. Chốt reset lỗi/mutex không đổi. Xem docs/07-network.md và ADR-053.

Lệnh rẽ tại node (ADR-034): web vẫn gửi task/GOTO như cũ. Core mới trao đổi NODE_ORDER LEFT/RIGHT với NCKHAGV sau RFID để xoay tại chỗ rồi bám line. Khi firmware hỗ trợ và đang WORKING/BLOCKED, GOTO trực tiếp mới trả 409 để không ghi đè lượt quay/di chuyển; task tự động vẫn có thể xếp hàng.

Servo test01 (15/10/2026, ADR-057): giữ POST `/api/agvs/01/warehouse-action` `{action:"pickup"|"dropoff",warehouse:"K5"}`. Action chốt `servo` từ capability tại thời điểm tạo. Có capability thì sau task/RFID kho/IDLE gateway gửi WS `warehouse_lift` với agv_id/node/action/request_id; Core trả `data.command_id` tương quan lệnh MQTT. Action chuyển dispatching_lift→handling, không phát lùi theo timer gateway; chỉ kết quả `fleet.agvs[].lift` DONE đúng command_id/node còn fresh, xe IDLE và virtual_cargo_known/cargo đúng mới phát GOTO gate. Core khóa GOTO/dispatch trong lúc lift active; HTTP409 khi đang nâng/hạ. Không capability thì giữ chờ mô phỏng5s của firmware cũ, UI ghi rõ không điều khiển servo và không tạo kiện ảo. Lịch sử action vẫn RAM20, không tạo đơn, không replay sau restart. WS snapshot thêm state LIFTING/LOWERING và `lift`, `virtual_cargo`, `virtual_cargo_known`; kiện là kết quả chu kỳPWM không phải hàng vật lý đã đo. Thiếu/mất phiên/kết quả cũ/ACK không cập nhật cargo hoặc tự lùi. [Ví dụ](examples/robot01-servo-lift.json).

Cập nhật crash/checkpoint10/10/2026: firmware01 buildId=20261010-grant-buffer-fix copy zone/token vào grant riêng trước publish mutex_ack, tránh buffer PubSubClient RX/TX ghi đè pointer JSON. Diagnostic firmwareBuild cho biết đúng ảnh nạp. Core lỗi checkpoint dừng outbound, thử save lại mỗi2s; chỉ sau lưu thành công phục hồi heartbeat. Gateway storage_healthy=false báo core_ready=false, command503 và chặn điều phối đơn. Payload mutex/command/telemetry pipe giữ nguyên. Xem ADR-035.
Core test01 10/10/2026 (ADR-036): clear_error/RESET_FAULT không yêu cầu ở S1; Core chờ diagnostic khỏe tại RFID hiện tại mới IDLE, ACK không đủ. Test01 mở/đóng ca tại node đã xác nhận, không ép về S1; at_home vẫn dùng trạm thực. Robot02/both giữ home gate. Không đổi command payload/firmware, không cần nạp lại. MCU vẫn kiểm tra line/mạng/vị trí/hướng; không trả mutex chỉ vì reset giữa tuyến.
## Mô phỏng vật cản cạnh — 10/10/2026

Chỉ server mô phỏng cung cấp `POST /sim-api/edge-obstacles` với `{ "from":"M5", "to":"M6", "blocked":true }`. Hai đầu phải thuộc một cạnh hiện có; vật cản khóa cả hai hướng của đoạn vật lý. State map thêm `blocked_edges:[{"from":"M5","to":"M6"}]`; bản đồ cũ không có trường này được hiểu là rỗng.

Gateway của phiên gửi WS v1 nội bộ `sim_edge_obstacle` với `from`, `to`, `blocked`, `deferred_agvs`, hoặc `sim_replan` với `deferred_agvs`. Danh sách tối đa hai ID `01`/`02`, do runtime xác định xe đang trên cạnh; không lấy danh sách từ HTTP người dùng. Core từ chối 403 nếu không có `AGV_SIMULATION=1`. Request/result vẫn dùng `request_id` và `command_result`. Xe đang trên cạnh nhận tuyến thay thế khi đã đến RFID; cạnh đang có xe bị chặn giữ dừng tại chỗ. Không bổ sung command MQTT.

Giao diện mô phỏng dùng duy nhất `/api/orders` với nhiều `pickups`/`deliveries`. `/sim-api/orders` cũ là API kiểm thử tương thích, không phải form đơn hàng hiện tại. Xem [ADR-037](../docs/decisions/ADR-037-simulation-edge-obstacles.md).

`GET /sim-api/state`/report bổ sung `robots[].headingDeg` cho hướng đầu xe ảo. Góc độ theo map/SVG: 0=E, 90=S, 180=W, 270=N; null là chưa có hướng. Ví dụ xe lùi ra kho: `{ "id":"01", "node":"K1", "headingDeg":180, "edge":{"from":"K1","to":"G1","progress":0.4} }`, đầu hướng W còn di chuyển E. Đây là dữ liệu simulator; không chuyển thành diagnostic robot thật, không thay telemetry/command MQTT. UI lấy Bốc hàng/Dỡ hàng từ `warehouseOrders[].items[].status` (`pickup_service`/`dropoff_service`). [ADR-038](../docs/decisions/ADR-038-simulation-direction-arrows.md).
