# Đồng bộ web và server — 29/09/2026

Cập nhật 18/10/2026: thêm sổ vị trí kệ và log/báo cáo kho SQLite theo [ADR-070](decisions/ADR-070-warehouse-racks-and-shift-reports.md). Vị trí ban đầu do người dùng nhập; các lượt chuyển kệ đặt chỗ độc quyền nguồn/đích/xe và cập nhật theo DONE servo đúng lệnh. Báo cáo bất biến được lưu khi Core CLOSED, có cảnh báo gián đoạn/cần đối soát và tải JSON. Không tự reset kho ở đầu ca mới.

Cập nhật18/10/2026: điều phối đơn hàng tự động scope hai xe đã thay timer bốc/dỡ bằng handshake servo hiện có. Gateway chỉ chọn xe báo lift sống, gửi `warehouse_lift`, chờ đúng `DONE_UP/DONE_DOWN` và cargo ảo trước khi phát chặng tiếp; UI hiện các pha chờ/nâng/hạ. Không thay Core/MQTT/firmware trong lần nối luồng này; xem [ADR-069](decisions/ADR-069-automatic-orders-use-servo-lift.md).

Mốc này lưu khảo sát ngày 29/09. Các phần auth/TLS/mutex/restart/delta đã được cập nhật ngày 01/10; đọc [gia cố hiện tại](18-hardening.md). Các giới hạn phần cứng còn OPEN.

Nguồn yêu cầu trực tiếp: người dùng đang chờ firmware và yêu cầu rà soát, sửa web/server, đặc biệt các phần hiển thị không khớp API. Các thay đổi dưới đây là `OBSERVED` trong mã mới; kết quả chạy ghi trong [hồ sơ kiểm thử](../tests/README.md). Phần firmware và nghiệm thu phần cứng vẫn `OPEN`.

## Các lỗi đã sửa

| Vấn đề | Hành vi hiện tại |
| --- | --- |
| Hàm API trải mảng thành object, làm danh sách snapshot/KPI trống | Giữ nguyên array/object của JSON; lỗi HTTP trả `null` và thông báo |
| Ô địa chỉ WebSocket không được sử dụng, tạo nhiều socket khi bấm kết nối | Dùng địa chỉ người dùng nhập, REST cùng host; đóng socket cũ; tự reconnect |
| Báo thành công khi chỉ gửi được socket | Gateway chờ `command_result` có `request_id`; HTTP 200 sau khi core chấp nhận; lỗi/timeout được hiển thị |
| Gateway tự ghi đè bản đồ bằng snapshot khi reconnect | Core là nguồn bản đồ đang chạy; snapshot chỉ được áp dụng qua thao tác người dùng |
| Core thiếu `load_map_data`; nạp lỗi có thể làm mất graph cũ | Có handler, validation và thay toàn graph sau khi candidate hợp lệ; graph cũ giữ nguyên khi lỗi |
| Task trùng đích bị ghép nhầm, target rỗng bị tính hoàn thành | Core/gateway dùng `task_id`; core trả trạng thái chính thức; hủy được từng ID |
| Robot lỗi/offline bị trả nhiệm vụ cho xe khác ngay | Task chuyển `interrupted`, đường chưa xác minh bị bảo vệ, quyền zone được giữ |
| Chọn xe/trạm theo số node | So tổng trọng số cạnh; tie break bằng ID để kết quả ổn định |
| Log cùng seq sau restart bị bỏ mất | Unique key là `core_session + core_seq`; UI cũng phân biệt phiên |
| Bộ đếm `cmd_id` reset theo tiến trình | Core lưu block ID vào `command_sequence.json`, giữ command/ACK số như trước |
| KPI đếm lại sự kiện ở ranh giới cùng giây | Dùng watermark ID; thêm số nhiệm vụ hoàn thành trong ca kể cả tạo ở ca trước |
| KPI gán thời gian trước lần đầu nhìn thấy robot cho robot đó | Chỉ tính từ quan sát đầu tiên; mất liên kết ghi `UNKNOWN`, không khẳng định robot OFFLINE |
| Pin mặc định/fallback bị hiểu là phép đo thật | UI phân biệt chưa đo/ước tính; chỉ phân công khi đã có telemetry hợp lệ và pin đo được |
| UI thiếu route/history/map editor | Thêm lộ trình, lịch sử task/robot, tuổi telemetry, lỗi, sửa node/cạnh qua API |
| Dữ liệu log, node hoặc tên bản đồ có thể được diễn giải thành HTML | Dùng text node hoặc escape; action được gắn listener thay vì chèn ID vào onclick |

## Tệp và quyền sở hữu dữ liệu

- [index.html](../AGVwebserver/index.html): bố cục và form, giữ HTML/JS.
- [public/dashboard.js](../AGVwebserver/public/dashboard.js): REST/WS client và hiển thị.
- [server.js](../AGVwebserver/server.js): entrypoint; `npm start` giữ nguyên.
- [gateway.js](../AGVwebserver/gateway.js): HTTP/WS, xác nhận lệnh, metadata và ghi lịch sử.
- [db.js](../AGVwebserver/db.js): `createDatabase(filename)`, SQLite migration và KPI. Import module không mở DB vận hành.
- [WebCommands.hpp](../src/WebCommands.hpp): xử lý và trả kết quả command web.
- [CommandIds.hpp](../src/CommandIds.hpp): cấp command ID đã lưu trên đĩa.
- [FLeetmanager.hpp](../src/FLeetmanager.hpp): task ID, trạng thái nhiệm vụ, đo pin/vị trí, dispatch và giữ đường chưa xác minh.
- [MapGraph.hpp](../src/MapGraph.hpp): validation, revision, tổng chi phí và lưu/nạp.

Core sở hữu graph/fleet/mutex và kết quả nhiệm vụ. SQLite sở hữu lịch sử/snapshot. Gateway không tự sửa graph cache khi gửi obstacle hoặc apply. Command result chứa snapshot mới để HTTP và WebSocket trình duyệt cùng nhìn thấy kết quả đã được core xác nhận.

Map revision tăng trong một phiên core; sau restart phải đối chiếu cùng `core_session_id`, không xem số revision đơn lẻ là phiên bản toàn cục.

Khóa điều phối `g_stateMtx` ở server bao quanh web command, MQTT mutation và tick/snapshot; callback state dùng cùng recursive mutex. Module chỉ gọi notify sau khi nhả khóa nội bộ. WebSocket đọc đủ byte khi TCP chia frame, giới hạn command 1 MiB và phản hồi ping. Bản này vẫn chỉ hỗ trợ text frame không phân mảnh, chưa TLS.

## Trạng thái và giới hạn thao tác

`agv_connected` nói socket gateway–core; `core_ready` nói có snapshot mới trong 5 giây. `protocol_supported` yêu cầu core WS phiên bản 1. `mqtt_connected` nói liên kết core–broker. UI khóa điều khiển khi một liên kết cần thiết mất hoặc dữ liệu cũ.

`last_seen_s` là số giây từ lúc core nhận telemetry, không phải timestamp cảm biến. Browser cộng tuổi snapshot để không đóng băng con số khi mất mạng. `node` là node đo gần nhất; đường nét đứt là kế hoạch. Không hiển thị vị trí liên tục giả định giữa các node.

Task gồm `submitting`, `unknown` (gateway), `pending`, `dispatched`, `completed`, `cancelled`, `interrupted`, `rejected`. `completed` chỉ đến từ logic core khi nhận telemetry hợp lệ ở đích và hết đường còn lại. ACK lệnh chỉ xác nhận nhận command. Hủy task gửi WAIT, không khẳng định robot đã dừng vật lý.

Sau lỗi/mất ACK/mất telemetry, robot giữ đường có thể còn chiếm; task không tự giao lại. Robot từng offline kết nối lại chuyển ERROR để người vận hành xác minh trước khi xóa lỗi. Clear-error cần telemetry mới và vị trí hợp lệ. Zone chỉ được trả qua luồng xác nhận rời vùng; mất telemetry không tự trả quyền.

Chỉnh cấu trúc graph/apply snapshot chỉ được chấp nhận khi core không có robot, hàng đợi hoặc chủ zone. Đây là baseline trong lúc chưa có firmware xác nhận dừng. Có thể xem/xóa snapshot và đọc lịch sử khi core offline; lưu snapshot mới cần dữ liệu core mới. Gỡ robot chỉ cho robot OFFLINE không giữ zone; UI yêu cầu xác minh đã đưa robot ra khỏi kho.

ID node không chứa `/`, `|` hoặc ký tự điều khiển. `priority` là integer 0..100, `weight` là integer 0..1000000 (0 được giữ đúng), tọa độ hữu hạn trong ±1000000. Node type: waypoint/mutex/charging/station. Cạnh hai chiều được xuất cả hai hướng, UI vẽ một đường với hai mũi tên; xóa cạnh hai chiều xóa cả cặp.

Command ID lưu block 1024 số, sau restart bỏ qua phần block chưa dùng. Chỉ chạy một core cho mỗi file counter; không xóa hoặc khôi phục file counter về phiên cũ. Giới hạn signed 32-bit; hết miền ID hoặc file lỗi làm dừng cấp lệnh. Bảo đảm đã kiểm tra là restart tiến trình với file được giữ, chưa phải kiểm chứng mất điện hoặc reboot MCU. Khi đưa firmware cũ vào hệ thống lần đầu, cần đối soát bộ nhớ dedup của robot.

## Chạy bản mới trên Windows

Đã kiểm tra bằng Node 24.21.0, GCC 15.2.0 (MSYS2 UCRT64), OpenSSL/libmosquitto hiện có. Không cài thêm dependency runtime của ứng dụng.

Từ gốc workspace:

```powershell
./scripts/build-core.ps1 -Tests
```

Chuẩn bị broker MQTT đang chạy theo cấu hình nhóm. Terminal core, working directory gốc workspace:

```powershell
./scripts/run-core.ps1
```

Terminal gateway:

```powershell
Set-Location AGVwebserver
npm start
```

Mở `http://localhost:3000`. Cần khởi động lại tiến trình gateway và chạy executable core vừa build để dùng hợp đồng mới. Core cũ vẫn xem được ở trạng thái chỉ đọc, các thao tác bị khóa.

Đổi cấu hình bằng biến môi trường trước khi chạy. Các mặc định:

| Biến | Mặc định | Ý nghĩa |
| --- | --- | --- |
| `PORT`, `HOST` | 3000, 0.0.0.0 | HTTP gateway |
| `AGV_WS_URL` | ws://127.0.0.1:8080 | Gateway → core; khớp listener IPv4 |
| `AGV_DB_PATH` | AGVwebserver/agv.db | DB; test luôn dùng file riêng |
| `AGV_COMMAND_TIMEOUT_MS` | 5000 | Chờ command result |
| `AGV_CORE_WS_PORT`, `AGV_CORE_WS_HOST` | 8080, 127.0.0.1 | Listener core |
| `AGV_MQTT_HOST`, `AGV_MQTT_PORT` | 127.0.0.1, 1883 | Core → broker |
| `AGV_MAP_PATH` | data/map_layout.json | Map đọc/lưu; UI lấy path từ result |
| `AGV_COMMAND_ID_PATH` | command_sequence.json cạnh map | Counter vận hành |

Gateway không tự đọc `.env`; [.env.example](../AGVwebserver/.env.example) là mẫu để đặt biến. File counter và SQLite là dữ liệu vận hành. Test tích hợp tự tạo map, broker/cổng localhost và DB riêng trong `.qa`; không sử dụng `agv.db` đang có.

Khởi động 29/09: core giữ độc quyền cổng trên Windows trước khi ghi map/counter hoặc kết nối MQTT. Khi chạy mặc định từ gốc, nếu chưa có counter cạnh map nhưng có `src/data/command_sequence.json`, core tiếp tục file cũ; biến môi trường vẫn được ưu tiên. Không xóa hoặc rewind counter. [Chẩn đoán và cách chạy](16-startup-troubleshooting.md) ghi lỗi chạy trùng và cấu hình F5.

## Kiểm thử và phần chờ firmware

Chi tiết cách chạy và phạm vi bằng chứng tại [tests/README.md](../tests/README.md). Có test REST với core giả lập, C++ logic, tích hợp C++/MQTT thật trên broker riêng, và trình duyệt headless ở desktop/mobile.

Chưa xác nhận robot thật, thời gian phanh, vùng quét thân xe, tải trọng hoặc pin thật. `cmd_id` được bảo toàn phía server nhưng dedup/boot session phía MCU vẫn cần firmware. MQTT telemetry giữ `node|battery[|obstacle]`, QoS 0; chưa thêm JSON telemetry, MCU session hoặc timestamp cảm biến. Cần thiết kế phục hồi vùng đang chiếm sau restart core cùng firmware trước khi dùng hai robot thật. TLS/ACL, ROS/Gazebo và tiêu chí nghiệm thu số đo vẫn theo yêu cầu mở.
