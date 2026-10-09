# Gia cố bảo mật, an toàn và độ bền — 01/10/2026

Danh sách sáu nhóm do người dùng đưa trực tiếp là SOURCE. Mã và kết quả dưới đây là OBSERVED sau triển khai; đo motor/phanh, pin/sạc thật và nghiệm thu công nghiệp vẫn OPEN. [ADR-009](decisions/ADR-009-durable-secure-control.md) ghi quyết định thiết kế; [kiểm thử](../tests/README.md) phân biệt fixture với phần cứng.

## Các thay đổi

| Nhóm | Đã triển khai phần mềm |
| --- | --- |
| Bảo mật | MQTT user/password và CA/TLS trên Core/ESP32; ACL từng username 01/02; HTTPS/WSS gateway; token API/WS; cookie HttpOnly/SameSite Strict; Origin/CORS giới hạn; giới hạn login, số lệnh chờ và frame; định danh ASCII; UI dùng DOM/textContent; POSIX ignore SIGPIPE |
| An toàn | Grant có grant_id/ACK/retry; release phải đúng token; checkpoint trước khi phát quyền/lệnh; offline/LWT giữ quyền; khôi phục quyền cần xác minh; ESP32 có heartbeat/session watchdog, latch dừng logic, NVS cmd_id và LWT |
| Độ bền | Core cấp task-ID từ counter riêng; queue/FIFO/task records/route chưa xác minh/mutex được checkpoint; task đang chạy thành interrupted sau reboot, pending giữ ID; DB nhận lại cùng task thay vì tạo bản sao; hysteresis giữ nhu cầu sạc từ <=20 đến >=40%; TTL cảm biến, khóa thủ công mặc định không hết hạn |
| Hiệu năng | Core và gateway gửi state tối đa 5 lần/giây; delta theo trường, map không gửi lại khi không đổi; DB batch trong transaction mỗi 100 ms và flush trước read/close; backpressure WS; retention |
| Giao diện | Giữ auto-reconnect, editor, routes, dropdown, giờ địa phương, command_result; thêm login/logout, chờ ACK quyền và xác minh vùng trống |
| Chất lượng | Unit C++ và Node; mô phỏng tám xe bằng MQTT; TLS/ACL/LWT với broker thật; sketch host và biên dịch ESP32; Docker/Compose và CI được cung cấp |

## Hai chế độ chạy

Mặc định chưa cấu hình token: gateway bind 127.0.0.1, API/WS chỉ nhận client loopback; Core WS cũng loopback. Đây là chế độ kiểm thử cục bộ. Xe thật cần broker LAN có tài khoản/TLS như cấu hình dưới đây. Không dùng chế độ localhost để tuyên bố đã bảo mật LAN.

Gateway nhận `AGV_AUTH_TOKEN` tối thiểu 32 ký tự, Core nhận `AGV_CORE_TOKEN` riêng tối thiểu 32 ký tự. Sinh token bằng bộ sinh ngẫu nhiên mật mã, không lấy token mẫu trong test. `AGV_ALLOWED_ORIGINS` là các origin HTTPS đầy đủ, phân cách bằng dấu phẩy. Same-origin được chấp nhận; CORS không mở `*`. API nhận `Authorization: Bearer <token>` hoặc cookie đăng nhập; WS trình duyệt dùng cookie, không đặt token vào URL/localStorage. Lệnh dùng cookie cần Origin; client CLI dùng Bearer. Token web là quyền điều khiển chung, chưa có tài khoản cá nhân/RBAC hoặc thu hồi từng phiên.

`AGV_HTTPS_CERT` và `AGV_HTTPS_KEY` bật HTTPS/WSS trực tiếp ở gateway. `AGV_SECURITY_REQUIRED=1` buộc gateway có HTTPS/token và Core MQTT có CA/tài khoản. Core WS trên Docker network riêng vẫn dùng WS + Bearer; không publish cổng 8080. Với Core khác host cần mạng riêng/VPN hoặc TLS proxy, vì socket WS Core chưa tự cung cấp WSS.

## Cấu hình Windows

Script [provision-security.ps1](../scripts/provision-security.ps1) tạo credential mới, không ghi đè và không tự đổi dịch vụ đang chạy:

```powershell
./scripts/provision-security.ps1 -LanAddress <IPv4-may-chay-broker> -OpenSsl C:/msys64/ucrt64/bin/openssl.exe -MosquittoPasswd C:/msys64/ucrt64/bin/mosquitto_passwd.exe
```

Thay placeholder bằng IPv4 LAN thực tế. Script tạo CA, cert có SAN cho localhost/broker/IP LAN, ba tài khoản core/01/02, ACL và `deploy/.env` chứa token ngẫu nhiên. Điền mật khẩu tài khoản core vào AGV_MQTT_PASSWORD trong .env. .env không được Node tự đọc: nạp biến môi trường trước khi chạy hoặc dùng Compose. Giữ CA private key và server private key trong thư mục riêng có quyền đọc phù hợp; chỉ phân phối **ca.crt** tới máy/ESP32. Chứng chỉ server hết hạn sau 365 ngày, cần gia hạn trước khi hết hạn. Đặt đồng hồ máy và NTP ESP32 đúng để xác minh cert.

Chạy broker bằng `mosquitto -c deploy/mosquitto.windows.conf`. Cấu hình mới nghe TLS 8883, anonymous=false; chỉ chuyển sang listener này sau khi đã đặt user/password/CA/port đồng bộ trên Core và cả hai sketch. Broker cũ trên 1883 không bị script dừng.

Biến môi trường Core:

```powershell
$env:AGV_MQTT_HOST='<IP-hoac-host-khop-SAN>'
$env:AGV_MQTT_PORT='8883'
$env:AGV_MQTT_USER='core'
$env:AGV_MQTT_PASSWORD='<mat-khau-core>'
$env:AGV_MQTT_CA='<duong-dan-tuyet-doi-ca.crt>'
$env:AGV_CORE_TOKEN='<token-core>'
$env:AGV_SECURITY_REQUIRED='1'
./scripts/run-core.ps1 -Build
```

Gateway nhận cùng AGV_CORE_TOKEN, AGV_AUTH_TOKEN riêng, AGV_ALLOWED_ORIGINS và AGV_HTTPS_CERT/KEY. Khi browser dùng cert CA nội bộ, cài CA vào trust store của thiết bị theo quy trình của nhóm; không vô hiệu hóa kiểm tra chứng chỉ để chạy chính thức.

Sketch đặt MQTT_USE_TLS=true, MQTT_PORT=8883, MQTT_USER bằng ROBOT_ID (01/02), MQTT_PASSWORD riêng và MQTT_CA_PEM là nội dung ca.crt. MQTT_HOST phải khớp SAN. PubSubClient vẫn publish QoS 0; ACK ứng dụng/retry/dedup xử lý mất/trùng message. Không retained command/grant/telemetry; chỉ status online/LWT offline retained. ACL không cho xe 01 gửi telemetry/đọc command của xe 02.

## Hợp đồng và phục hồi

Telemetry/command/ACK cũ được giữ. Phần mutex chuyển sang v0.1, **cần build Core và nạp cả hai sketch đồng bộ**:

```json
{"zone":"M5","grant_id":46}
```

Core gửi JSON trên mutex_grant, xe ACK cùng JSON trên mutex_ack và trả cùng token trên mutex_release sau khi thân xe rời hết vùng. Bare-text release cũ không được nhận. Mất ACK: gửi lại cùng ID khoảng mỗi giây, không cấp quyền cho xe khác. Xe xin lại mỗi giây nếu chưa nhận grant; duplicate được xử lý idempotent. ID grant dùng counter command hiện có. grant_acked chỉ nói đã nhận quyền, không chứng minh đã vào/ra vùng.

Core checkpoint `runtime_state.json` và cấp task-ID bằng `task_sequence.json` cạnh map; có biến AGV_RUNTIME_STATE_PATH và AGV_TASK_ID_PATH để test/triển khai riêng. Tiếp tục giữ command_sequence.json hiện có. Không xóa/rewind/share counter giữa hai Core. Checkpoint và counter thay atomically; POSIX fsync file/thư mục, Windows FlushFileBuffers và replacement write-through; retry tối đa 200 ms mỗi bước khi file bị khóa ngắn hạn, lỗi kéo dài sẽ dừng phát lệnh. File lỗi hoặc graph không khớp làm startup thất bại. Khi sửa cấu trúc graph, dùng save_map để lưu bản làm việc trước restart; không xóa checkpoint để bỏ qua lỗi đối soát.

Sau restart: robot được nhớ như **vị trí cũ chưa xác minh**, online/position_valid/battery_measured=false; marker không được coi là vị trí mới. RFID mới vẫn đưa robot từng biết sang ERROR để vận hành xác minh và clear-error. Task đang chạy thành interrupted; task pending và thứ tự priority/FIFO giữ nguyên. Ca trở về CLOSING, chưa dispatch cho tới khi tập kết/mở ca đúng quy trình. DB chuyển identity đã khôi phục sang core_session mới, giữ một dòng task và các mốc thời gian.

Mutex owner/queue giữ qua restart; recovery_required=true. Không phát lại grant khôi phục tự động. Owner đã xác minh có thể xin lại để nhận token mới; release/ACK token cũ không tác dụng. Thao tác “Xác minh vùng trống” yêu cầu xác nhận vật lý và owner/grant_id khớp khi xe lỗi/offline đã dừng và được đưa hoàn toàn ra ngoài. Trong ca thử hai xe, thao tác này cũng nhận xe `IDLE` đã xác minh RFID tại trạm riêng, không còn task, đích hay đường chưa xác minh; quyền mutex cũ có thể còn sau khi firmware reboot/mất mạng. Không dùng thao tác này để bỏ qua xe còn chuyển động.

ESP32 watchdog thiếu heartbeat/lệnh Core quá 3 giây hoặc đổi phiên Core/mất WiFi/MQTT sẽ xóa quyền chạy logic và latch failsafe. Quét RFID lại không tự chạy tiếp, duplicate GOTO không tái chạy; cần lệnh mới hợp lệ. Bản tích hợp 03/10/2026 đã nối gate này vào [driver L298N](../firmware/AGV_Connection_Test/driver.cpp), với motor khóa mặc định; chưa có đo phanh/điện trên board thật. Relay sạc chưa có. Cache ID cao nhất được lưu NVS trước ACK để từ chối lệnh cũ sau reboot.

## TTL và giới hạn

Sensor obstacle mặc định 30 giây; report lại sẽ gia hạn. POST /api/map/obstacle hỗ trợ ttl_s 0..86400; 0 là khóa thủ công không hết hạn. Sensor không ghi đè khóa thủ công thành tạm thời. TTL chỉ bỏ hạn chế planner, không chứng minh đường vật lý đã trống; cảm biến/local stop trên xe phải ngăn chuyển động nếu vật cản vẫn còn.

State_delta dùng base_state_seq/state_seq và changes cấp trường. Fleet/mutex gửi toàn trường khi đổi; không phải delta từng robot/cạnh. Core gửi map khi graph/obstacle đổi; reconnect luôn nhận full snapshot và không tự apply snapshot DB. Browser mới chọn /ws?delta=1; client không chọn vẫn nhận full snapshot từ gateway. Gateway/Core stream mất sequence sẽ reconnect, không áp delta lên baseline sai.

DB batch vẫn dùng better-sqlite3 trên cùng event loop, không phải worker thread. Flush trước truy vấn/KPI/close cho dữ liệu nhất quán. Khoảng chưa flush có thể mất khi gateway crash; live task/queue/owner được Core checkpoint. Retention mặc định 30 ngày/100000 dòng log/telemetry, giữ baseline mỗi robot, 50 map snapshots và 200 KPI. Task pending/dispatched không bị prune theo thời gian. Core giữ 1000 terminal task gần nhất và tối đa 1000 pending; lịch sử dài nằm DB. Báo cáo đã lưu giữ kết quả lúc tạo; không dùng retention để tính lại lịch sử đã xóa.

## Docker và CI

[Compose](../deploy/compose.yaml) dùng MQTT TLS8883, gateway HTTPS3000 và không expose Core8080. Tạo secrets/cert/.env trước, chạy `docker compose --env-file deploy/.env -f deploy/compose.yaml up --build`. Named volumes giữ map/counter/runtime/DB; map ban đầu chỉ copy nếu volume chưa có map. Không dùng `down -v` trong vận hành vì xóa trạng thái. Trên Linux đảm bảo key đọc được bởi broker UID1883 và gateway UID1000, ví dụ owner1883/group1000/mode0640; password file chỉ broker cần đọc. Windows bind mount cần quyền đọc của Docker Desktop. Compose chỉ mount ca.crt/server.crt/server.key, không mount CA private key. Sao lưu ca.key trong kho riêng có quyền truy cập phù hợp.

[CI](../.github/workflows/ci.yml) build C++ trên Ubuntu, unit Node/C++, system integration, mô phỏng hardening và tài liệu. CI chỉ chạy khi workspace được đưa lên GitHub. Docker/CI đã tạo cấu hình, **chưa chạy Docker hoặc pipeline GitHub trên máy này**. Test firmware-host, browser và TLS có lệnh riêng trong tests/README.md.

## Tài liệu chính chủ đã đối chiếu

Tra cứu 01/10/2026, dùng làm căn cứ API/cấu hình; không sao chép mã upstream:

- [Mosquitto API](https://mosquitto.org/api/) cho username/password, CA và TLS verification.
- [Mosquitto configuration](https://mosquitto.org/man/mosquitto-conf-5.html) cho listener, ACL/password/TLS và giới hạn packet.
- [PubSubClient API](https://pubsubclient.knolleary.net/api) cho connect user/password/LWT; QoS0 publish và buffer.
