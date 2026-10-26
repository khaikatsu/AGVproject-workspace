# Giao thức mạng và hợp đồng MCU với server

26/10/2026, REQ-WAREHOUSE-TEST-ROLLBACK-01: WS v1 gateway→Core thêm `reconcile_test_cargo`, batch ID01/02+lift_command_id và xác nhận xe trống. Core cần diagnostic dừng đúng boot/kết nối còn mới rồi chỉ cập nhật cargo ảo phục vụ lượt thử. Không gửi MQTT hoặc thêm command MCU; firmware không cần nạp lại. [Hợp đồng](../contracts/web-server.md), [ví dụ](../contracts/examples/test-order-rollback.json), [ADR-089](decisions/ADR-089-test-order-rollback.md).

26/10/2026, REQ-WEB-LOCATION-NAMES-01: hoán đổi tên kho/trạm chỉ nằm ở HTTP/WS browser–gateway và SQLite. Trường `location_names_revision` opt-in dịch tên sang ID vật lý tại gateway; MQTT command/telemetry, UID, bảng tuyến và trạm riêng của hai xe giữ ID cũ. Không có gói cấu hình tên gửi xuống MCU. [Hợp đồng và ví dụ](../contracts/web-server.md), [ADR-087](decisions/ADR-087-server-location-names.md).

24/10/2026, REQ-DUAL-GS-WIDE-LINE-01 chỉ đổi xác nhận cảm biến cục bộ khi docking: GS1/GS2 quay tới111 ổn định mới chuyển sonar sau và lùi; topic/QoS/payload/ACK/counter, NODE_ORDER `reverse_after_turn`, grant và RFID hoàn tất không đổi. [ADR-081](decisions/ADR-081-dual-gs-wide-line-confirmation.md).

23/10/2026, REQ-FIRMWARE-SERVER-SYNC-01: firmware server-sync-v16 lại phát diagnostic `lastCommandId` (high-water đã persist) để Core tiến counter; bỏ ngoại lệ bench nhận lệnh cũ ngoài cache. Khôi phục producer proof dừng theo schema1 hiện có; không thay topic, command/ACK/telemetry pipe hoặc hợp đồng proof. Giữ cấu hình mạng và tuning của RAR. [ADR-072](decisions/ADR-072-stale-command-counter-recovery.md), [kiểm chứng](../tests/README.md).

23/10/2026: odometry schema1 mở rộng tùy chọn `stopped_position` cho hai xe. Core xác thực đứng yên/phiên/seq/chặng và diagnostic mới trước khi thu hẹp vùng chặn về một node hoặc hai đầu đoạn, tự trả mutex ngoài vùng/replan. Thiếu/cũ/offline giữ bảo thủ; telemetry/command/ACK/mutex giữ payload cũ. [Hợp đồng](../contracts/robot01-odometry.md), [ADR-079](decisions/ADR-079-stopped-edge-occupancy.md).

22/10/2026, REQ-DUAL-TURN180-FAST-CAPTURE-01: turn180-v13 thêmxácnhậnSTOP100ms khiquay180. Trongxácnhận vẫnAGV_MOVING/taskactive, chưastationReady/hoànthànhtask; sauđó mớihandoffsonar vàbámline/đỗnhưcũ. RFIDđọc lại sauphaquay; khôngtựphátRFIDarrival hayreplaylệnh. MQTTworker/STOP/grant/sonar giữnguyên, khôngđổi topic/payload/QoS/ACK/counter hoặc timeoutoffline. Nhịpđọc mục tiêu2ms/logmaxgap chỉcụcbộ firmware. [ADR-077](decisions/ADR-077-turn180-fast-stop-confirm.md).

22/10/2026, REQ-DUAL-EDGE-LINE-ROLLBACK-01: narrow-v12 bỏ001/100 khỏi xác nhận phục hồi/bắtline sau rẽ của cảhai xe; chỉ010/011/110 xác nhận. Điều chỉnh cục bộ cảm biến/navigator, không đổi topic/QoS/payload/ACK/timeoutoffline/counter hoặc cấu hìnhmạng. Core vẫn nhận lỗi thực từMCU và không tựclear/replay sau lỗi/reset. [ADR-076](decisions/ADR-076-dual-narrow-line-confirm.md).

22/10/2026, REQ-DUAL-EDGE-LINE-CONFIRM-01: firmware line-edge-v10 nhận thêm001/100 ổn định trước khi phátLINE_LOST; đây là policy cảm biến/motor cục bộ. Route/cmd_id/path_index/grant giữ nguyên trong cửa sổ; Core không tự clear/replay sau lỗi/reset. Không đổi topic/QoS/payload/ACK/counter, cấu hình mạng riêng hay timeout offline. [ADR-076](decisions/ADR-076-dual-narrow-line-confirm.md).

21/10/2026, REQ-DUAL-NARROW-LINE-CONFIRM-01: firmware01/02 line-confirm-v9 sửa xác nhận line cục bộ; giữroute_cmd_id/path_index/grant trong cửa sổ phục hồi. TurnCapture tổng chờ1500ms nhưng quétmotor≤600ms, phần còn lạiSTOP quan sát;011/110 ổn định có thể xác nhận mà không cần mẫu cuối010. Following tiến thêm line hẹp010/011/110 ổn định100ms/≥3 mẫu/gap≤50ms; hết hạn mới phát LINE_LOST như cũ. Core không được tự xóa lỗi đãchốt hay replay sau reset. Không đổi topic/QoS/payload/ACK/counter hoặc mạng riêng. [ADR-076](decisions/ADR-076-dual-narrow-line-confirm.md), [kiểm thử](../tests/README.md).

21/10/2026, REQ-ROBOT02-LOCAL-NETWORK-RESTORE-01: người dùng duyệt khôi phục riêng mạng02 giống mạng01 đang cấu hình cho máy server192.168.137.1:1883. Đã backup mạng02 và binary trước đổi (ngoàiGit), kiểm hash rồi chép file mạng01 sang02; env02 giữID02 bằng profile/static_assert, không dùng ID dự phòng trong file mạng. Lần chẩn đoán xác nhận Core/MQTT khỏe, chỉ status02offline, binaryv8 vẫn chứa SSIDsource khác mạng cũ. Không thay payload/topic/counter/NVS/motor/line/servo/watchdog. Chưa upload/xác nhận reconnect xe02; không công bố SSID/mật khẩu. [ADR-075](decisions/ADR-075-sync-validated-two-robot-firmware.md), [kiểm thử](../tests/README.md). Mô tả “mạng02 giữ nguyên source” phía dưới chỉ đúng ở thời điểm khôi phục01 trước đó.

21/10/2026, ngoại lệ sau ghi đè được người dùng duyệt: riêng `LocalNetworkConfig01.h` đã khôi phục Wi-Fi và auth MQTT từ cấu hình cục bộ trước đó, broker192.168.137.1:1883. Mạng02 và mã điều khiển vẫn nguyên source. Kiểm chứng Core scope01+02/broker khỏe, xe02 có dữ liệu trực tiếp,01 không có telemetry mới; firmware xe01 build PASS nhưng chưa upload trong lượt khôi phục mạng, chưa xác nhận reconnect thực tế. Không ghi SSID/mật khẩu vào tài liệu/Git. [ADR-075](decisions/ADR-075-sync-validated-two-robot-firmware.md).

21/10/2026, ghi đè nguyên bản theo yêu cầu mới nhất: producer diagnostic NCKHAGV hiện trở về bản người dùng gửi, chưa có `lastCommandId`; Core vẫn có parser/allocator phục hồi theo ADR-072 nhưng thiếu trường đầu vào này thì không tự đồng bộ được counter MCU. Các bổ sung producer stale-sync-v5 bên dưới là lịch sử đã được thay thế; không xóa/rewind counter hoặc NVS để bù. Cấu hình mạng riêng01/02 của source đã chép cục bộ và ngoài Git. [ADR-075](decisions/ADR-075-sync-validated-two-robot-firmware.md).

21/10/2026: khôi phục producer `lastCommandId` trong diagnostic NCKHAGV01/02 sau khi phát hiện source hiện tại thiếu trường đã mô tả19/10. Giá trị là high-water NVS do network worker sở hữu, không phải route ID/ACK; Core dùng contract cũ để nâng counter trước RESET_FAULT. BuildID `20261021-robot01-stale-sync-v5` / `20261021-robot02-stale-sync-v5`. Không đổi topic, command hoặc bảng PWM. [ADR-072 bổ sung](decisions/ADR-072-stale-command-counter-recovery.md).

19/10/2026, REQ-STALE-COMMAND-RECOVERY-01: diagnostic firmware v1 thêm `lastCommandId` (0…INT_MAX), là high-water `cmd_id` đã lưu NVS. Core dùng trường này chỉ để nâng và persist counter lên phía trước. Khi nhận `STALE_COMMAND`, Core bỏ pending retry, giữ ERROR, không phát chuỗi STOP ID thấp và không tự xóa bởi diagnostic khỏe; sau khi người vận hành xác minh rồi `clear_error`, `RESET_FAULT` mới phải có ID lớn hơn high-water. Không ACK giả, không xóa NVS và không replay GOTO cũ. Firmware/Core cũ vẫn đọc được phần còn lại nhưng không tự phục hồi lệch counter. [ADR-072](decisions/ADR-072-stale-command-counter-recovery.md).

19/10/2026, REQ-ROBOT01-ROUNDED-LINE-01 không đổi topic, QoS, payload hay vòng đời lệnh. Kết quả rà soát cho thấy firmware01 phát `LINE_LOST` trước; Core/server chỉ nhận lỗi rồi gửi STOP theo cơ chế hiện có, không phải nguồn phát STOP giữa cung bo. Bản sửa nằm trong ánh xạ cảm biến→motor của profile01; không thêm resend, `NODE_ORDER` hoặc nhánh xử lý server riêng.

18/10/2026, REQ-ORDER-AUTO-LIFT-01 không thêm topic/payload: gateway đơn hàng tự động scope01+02 tái sử dụng WS `warehouse_lift`; Core phát `LIFT_UP`/`LIFT_DOWN` đúng robot/node/cmd_id như thao tác kho độc lập. ACK chỉ là nhận lệnh. Scheduler chờ diagnostic `DONE_UP`/`DONE_DOWN`, cargo ảo và trạng thái IDLE tương quan trước khi phát GOTO kế; sai/mất kết quả không tự chạy tiếp. Chu kỳ còn5s; đề xuất3s chưa áp dụng. [ADR-069](decisions/ADR-069-automatic-orders-use-servo-lift.md).

17/10/2026, REQ-MUTEX-ENCODER-01 không đổi topic/payload: firmware01/02 vẫn publish `agv/{id}/mutex_release` JSON `{zone,grant_id}`. Bằng chứng local mới là mỗi bánh≥30 xung tịnh tiến từ lần đầu đọc thẻ M, không đọc lại thẻ≥100ms và trạng thái route/phiên/vị trí khỏe. Serial `proof=encoder` chỉ để chẩn đoán, không thêm vào payload. Core vẫn xác thực owner/token rồi mới cấp xe chờ; ACK command/telemetry/odometry không tự nhả vùng. [ADR-068](decisions/ADR-068-encoder-mutex-tail-clearance.md).

17/10/2026, REQ-MAP-03: MQTT giữ payload/topic hiện tại, nhưng path/target/node dùng mapv4/22node, không còn C1–C4. Bốn cạnh bo nối G1→M1/M2→G6/G6→M4/M3→G1 không có RFID hoặc NODE_ORDER giữa cạnh; hướng đầu khi tới là tiếp tuyến cuối. Core và bảng route firmware01/02 phải đồng bộ. Map trong HTTP/WS/snapshot thêm geometry tùy chọn cho cạnh; [ADR-064](decisions/ADR-064-rounded-map-without-corner-tags.md).

16/10/2026, REQ-ROBOT02-LEFT-CAPTURE-01: LEFT xe02 thêm pha STOP/xác nhận line và chỉnh ngược ngắn tại MCU; vẫn giữ route_cmd_id/path_index/task và translating=false trong pha này. Không phát lại NODE_ORDER, không đổi payload hoặc ACK; thất bại mới phátLINE_LOST. Quy tắc retry trái dài xe02 trong mục REQ-TURN-LINE-RETRY-01 bên dưới đã được thay thế. BuildID02 `20261016-robot02-left-capture`. [ADR-063](decisions/ADR-063-robot02-left-turn-capture.md).

16/10/2026, REQ-LINE-SEARCH-01: tìm lại line tiến/lùi cục bộ giữ route_cmd_id/path_index và grant, không phát error trước khi hết giới hạn. Motor tìm dùng action LEFT/RIGHT và PWM180; odometry tracking giữ chặng nhưng translating=false trong lúc tìm/xác nhận line nên không cộng pivot. Mắt giữa ổn định mới tiếp tục, thất bại phát `LINE_LOST` theo contract hiện có. Không đổi topic/schema/ACK hoặc gửi lại GOTO/NODE_ORDER. [ADR-062](decisions/ADR-062-local-line-search.md).

16/10/2026, REQ-TURN-LINE-RETRY-01 không đổi MQTT/payload: firmware tự thử lại LEFT/RIGHT cùng hướng một lần trước khi phát lỗi. Timeout lần đầu chỉ STOP một vòng và giữ route/task/mutex; không publish error nên Core không khóa hoặc gửi lại `NODE_ORDER`. Nếu lần hai vẫn không bắt line, MCU phát `LINE_LOST` như cũ và Core mới gián đoạn task/giữ vùng chưa xác minh. ACK không chứng minh phục hồi chuyển động. [ADR-061](decisions/ADR-061-single-turn-line-retry.md).

16/10/2026, REQ-ROBOT02-PARITY-01: firmware02 phát cùng capability/schema diagnostic, lift và odometry như01 qua topic `agv/02/...`; encoder `robot_id` phải bằng ID trong topic (01/02), boot/connection/seq/route riêng từng xe. Core nhận sonar/lift02 đúng phiên, NODE_ORDER hỗ trợ `reverse_after_turn` trên cặp GS2→S2 như GS1→S1; cặp chéo bị từ chối. Telemetry pipe/ACK/mutex không đổi. [ADR-059](decisions/ADR-059-robot02-firmware-parity.md).

16/10/2026, REQ-CORE-UNIFIED-01: chọn thử xe 01/02/cả hai là cấu hình scope của cùng Core, không tạo giao thức firmware khác. Topic/payload giữ nguyên; Core chỉ phát lệnh chuyển động/sạc/NODE_ORDER và cấp mutex cho ID trong `active_robot_ids`. Diagnostic capability xe01 vẫn được đọc khi chạy scope hai xe. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

16/10/2026, REQ-BATTERY-06: người dùng chấp nhận phần trăm pin diagnostic đã lọc làm chuẩn. Core dùng `batteryPercentage` khi `batteryCalibrated=true` để cập nhật `battery`, `battery_measured`, `needs_recharge` và ngưỡng20/40; snapshot ghi `battery_control_source=diagnostic`. NCKHAGV vẫn gửi telemetry `node|?|obstacle` để tương thích; dấu `?` không xóa diagnostic còn mới trong3s. Diagnostic mất/không calibrated làm mode thường trở lại chưa đo và khóa task. Các mô tả display-only bên dưới là lịch sử trước quyết định này. [ADR-021](decisions/ADR-021-display-only-battery.md).

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
| `agv/{id}/diagnostic` | Robot → core | JSON chẩn đoán; NCKHAGV gửi `batteryPercentage`, `batteryCalibrated`; Core dùng giá trị calibrated làm pin điều phối theo REQ-BATTERY-06 |
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
