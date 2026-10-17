# Hiện trạng mã và khoảng trống tích hợp

CURRENT17/10/2026 sau lỗi checkpoint: [ADR-065](decisions/ADR-065-rounded-map-checkpoint-recovery.md) cho Core chuyển riêng mapv3→v4 có backup/giữ mutex/lịch sử và chờ xác minh xe. Bản sửa đã được cập nhật vào build/agv_core.exe và khởi động scope cảhai theo xác nhận người dùng; gateway nhận22node/35cạnh/4geometry, Core/MQTT/storage khỏe. Firmware vẫn cần nạp mapv4 trước chạy thật; chi tiết triển khai tại tests/README.md.

CURRENT17/10/2026: source mapv4 có22node, bỏC1–C4 và bốn cạnh bo135cm theo ảnh; scope01/02/cảhai dùng cùng graph. Fallback include/map/MapGraph.hpp và bảng route firmware đã đồng bộ. Tọa độ là sơ đồ; xem [ADR-064](decisions/ADR-064-rounded-map-without-corner-tags.md) và tests/README.md về số đo/kiểm chứng/áp dụng. Core vận hành cần build/restart và firmware01/02 cần nạp bản tương ứng.

CURRENT 16/10/2026: entrypoint hiện tại là `src/core/AGVserver.cpp`, header ở `include/`. Menu F5 dùng một Core thống nhất và chọn `active01`, `active02` hoặc `active-both`; scope lọc xe tham gia nhưng không đổi luật pin/RFID/MQTT/mutex. Các đường dẫn `src/AGVserver.cpp`/`src/FLeetmanager.hpp` trong khảo sát 28/09 bên dưới là lịch sử trước khi tổ chức lại source. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

Khảo sát tĩnh ngày 28/09/2026. Không chạy server, binary, migration hoặc thao tác với robot. Trong lúc khảo sát, các tệp C++ được chuyển từ gốc vào `src/`; tài liệu dùng vị trí cuối cùng đã quan sát.

## Các thành phần

| Tệp | Quan sát |
| --- | --- |
| `src/AGVserver.cpp` | MQTT libmosquitto, WebSocket tự viết, ghép map/fleet/mutex |
| `src/MapGraph.hpp` | Load/save JSON, đồ thị, Dijkstra, tìm trạm sạc |
| `src/FLeetmanager.hpp` | Trạng thái xe, phân công, pin, retry ACK, offline, replanning |
| `src/Mutexarbiter.hpp` | Chủ sở hữu zone, hàng đợi FIFO, request/release |
| `AGVwebserver/server.js` | Express REST, WS trình duyệt, bridge tới core |
| `AGVwebserver/db.js` | SQLite WAL, lịch sử xe/task/log, snapshot và KPI |
| `AGVwebserver/index.html` | Dashboard hiện có trong một tệp HTML |
| `src/AGVMQTT.cpp` | Ví dụ riêng nhận telemetry; không phải entrypoint core đầy đủ |
| `src/AGVmap.cpp` | Bản mã liên quan map; cần xác định vai trò khi chuẩn hóa build |
| `src/AGVtest.cpp` | Nội dung JavaScript backend, không phải test C++ |
| `src/json.hpp` | Header nlohmann/json đã có; chưa ghi pin version trong bộ nền |

## Giá trị đọc từ mã

- Core MQTT `127.0.0.1:1883`; WebSocket `8080`.
- Gateway HTTP `PORT` mặc định `3000`; `AGV_WS_URL` mặc định `ws://localhost:8080`.
- `data/map_layout.json` là bản đồ làm việc; `src/map_layout.json` là bản có sẵn khác vị trí. Chưa kết luận hai bản có cùng nội dung.
- Tick định kỳ khoảng 700 ms, có wake cho sự kiện obstacle/error.
- Offline 10 s; ACK timeout mặc định 3000 ms; gửi lại tối đa 3 lần.
- Ngưỡng pin mặc định ở FleetManager: thấp 20%, nhận task lại 40%, kết thúc sạc mặc định 90%. Core có thể cấu hình qua setter; cần đọc `main()` khi triển khai.
- MQTT publish/subscribe trong core dùng QoS 0; payload command không retained.

## Việc cần xác minh hoặc sửa

| ID | Bằng chứng | Tác động | Việc tiếp theo |
| --- | --- | --- | --- |
| GAP-01 | Gateway gửi `load_map_data`; handler core có `save_map`, còn load file thực hiện lúc khởi động, chưa có nhánh `load_map_data` | Apply/restore snapshot không đồng bộ core | Định nghĩa command, validation và phản hồi; test tích hợp |
| GAP-02 | Include `FleetManager.hpp`, `MutexArbiter.hpp`; filename khác case | Build trên filesystem phân biệt case có thể lỗi | Chuẩn hóa tên và mọi include khi làm build |
| GAP-03 | `AGVtest.cpp` có `require('express')` | Không có bằng chứng bộ test C++ từ tệp này | Phân loại mã và thêm test có ý nghĩa ở nhiệm vụ triển khai |
| GAP-04 | DB đoán hoàn thành từ target trước/sau | Task hủy, lỗi, đổi đường có thể bị suy luận sai | Dùng task ID và sự kiện kết quả từ core |
| GAP-05 | `releaseZonesOnOffline_ = true` | Robot mất mạng có thể còn đứng trong zone | Chốt chính sách giữ vùng, xác minh vị trí và phục hồi |
| GAP-06 | `cmdSeq_` và log seq ở RAM | Restart có thể trùng ID trong phiên cũ | Thiết kế session ID/persistence và test restart |
| GAP-07 | WS core tự viết, không wss; MQTT chưa có thiết lập auth/TLS ở client hiện tại | Cần rõ phạm vi mạng cho demo và vận hành | Kiểm tra parsing, listener, ACL, TLS theo yêu cầu mạng |
| GAP-08 | Không thấy firmware hoặc simulator trong file ban đầu | Chưa chứng minh end to end | Chốt bo mạch và tạo simulator hợp đồng trước |

Các GAP là hồ sơ nền để làm việc tiếp theo, không phải thay đổi code đã thực hiện. Comment ghi “đã sửa” không được dùng làm bằng chứng runtime. Xem [kế hoạch kiểm thử](09-testing.md).

## Cập nhật triển khai 29/09/2026

Các bảng trên giữ khảo sát nền trước thay đổi. GAP-01, GAP-02, GAP-04 đã sửa; GAP-05 đổi sang giữ vùng và gián đoạn nhiệm vụ; GAP-06 có phiên core cho log/task và counter command lưu đĩa. GAP-07 đã sửa đọc TCP từng phần, giới hạn payload và ping, còn TLS/ACL/fragmentation chờ yêu cầu. Đã có kiểm thử C++/gateway/browser và tích hợp với broker riêng; chưa có firmware hoặc robot thật. Xem [bản cập nhật](14-web-server-update.md) và [kết quả kiểm thử](../tests/README.md).
