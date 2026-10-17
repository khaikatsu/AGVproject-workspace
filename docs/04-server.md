# Phát triển server và lưu trữ

CURRENT 16/10/2026, ghi đè tạm để test: Core main comment ngưỡng pin gốc, bật bypass cho mở ca/task/GOTO và tắt tự sạc vì nhóm sạc thủ công. Kết thúc ca vẫn về trạm, đỗ IDLE; RFID/MQTT/ACK/mutex và scope giữ nguyên. Hai cờ trạng thái được công bố cho dashboard; không sửa firmware. [ADR-060](decisions/ADR-060-temporary-battery-bypass-manual-charge.md).

CURRENT 16/10/2026: một entrypoint/binary Core phục vụ scope xe 01, xe 02 hoặc cả hai. Scope là danh sách robot tham gia, không phải nhánh thuật toán trial; các chốt pin20/40, RFID, MQTT, ACK và mutex giữ như vận hành thật. State thêm `operation_profile` và `active_robot_ids`; robot thêm `participating`. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

Core C++ quản lý map/fleet/mutex. Gateway Node.js chuyển lệnh, phát state cho browser và ghi SQLite. Nền này là `OBSERVED`; các cải tiến bên dưới là `PROPOSED`.

## API đang có

| Nhóm | Endpoint tiêu biểu | Ý nghĩa và giới hạn |
| --- | --- | --- |
| Trạng thái | `GET /api/health`, `/api/state`, `/api/agvs` | Socket core và dữ liệu cache, không chứng minh sức khỏe từng cảm biến |
| Điều khiển | `POST /api/tasks`, `/api/agvs/:id/goto` | Chuyển add_task/direct_goto; 202 chưa là ACK core |
| Hủy/lỗi | `POST /api/tasks/cancel`, `/api/agvs/:id/clear-error` | Chuyển yêu cầu hủy/xóa lỗi; chưa có kết quả tương quan đầy đủ |
| Bản đồ | `GET /api/map`, `POST /api/map/node`, `/api/map/edge`, `/api/map/obstacle` | Sửa graph qua core |
| Snapshot | `GET/POST /api/map/snapshots`, `POST /api/map/snapshots/:id/apply` | Apply gửi load_map_data đang thiếu handler core |
| Lịch sử | `GET /api/tasks/history`, `/api/agvs/:id/history`, `/api/logs` | Đọc SQLite, có giới hạn số record trong mã |
| WebSocket | `/ws` trên gateway | Broadcast state tới browser |

Đây là tập endpoint tiêu biểu, không phải OpenAPI đầy đủ. `server.js` là nguồn xác minh endpoint trước khi sửa hoặc gọi.

## Chuẩn dữ liệu đề xuất

- Task có `task_id`, `request_id`, `robot_id`, trạng thái, lý do kết thúc và các mốc thời gian.
- Command có định danh phiên, ID duy nhất trong phiên, trạng thái nhận/thực hiện/thất bại.
- Log có `core_session_id` cùng `seq`; không deduplicate mọi phiên chỉ bằng seq.
- Bản đồ có revision; apply cần validate toàn graph và báo kết quả trước khi gateway cập nhật bản đang chạy.

Chưa có migration trong bộ nền. Khi triển khai cần adapter tương thích và plan chuyển dữ liệu; không sửa trực tiếp DB đang hoạt động để thử nghiệm.

## Xử lý và đồng bộ

Core kiểm tra mục tiêu, khả năng đi đến, pin và khả năng nhận việc. Gateway kiểm tra schema, kiểu dữ liệu, giới hạn và lỗi liên kết. Mỗi thao tác tác động cả DB và core cần định nghĩa thứ tự, thất bại giữa chừng và phục hồi; không coi việc socket.send thành công là transaction xuyên hệ thống.

Khi sửa callback đa luồng, vẽ thứ tự lock giữa MapGraph/FleetManager/MutexArbiter. Kiểm tra các callback notify/publish có gọi trở lại module đang giữ lock không. Comment về thread safety cần được đối chiếu với hành vi thực.

Vòng lặp core dùng steady clock cho timeout; thời gian log/báo cáo cần timezone rõ ràng. SQLite hiện dùng `datetime('now')`; chuẩn hóa UTC và chuyển giờ ở UI khi thiết kế lịch sử.

## Kiểm tra cần thiết theo thay đổi

- REST validation và phản hồi khi core offline hoặc command bị từ chối.
- Task trùng đích, hủy khi đang chạy, thay đổi target, lỗi, restart core.
- Snapshot lỗi/mất node/weight âm và apply bị từ chối.
- Reconnect không tạo nhiệm vụ mới hoặc ghi nhầm hoàn thành.
- Test DB dùng file riêng, test core dùng broker và robot giả lập riêng.

Tham khảo ranh giới fleet manager/adapter từ [rmf_demos](https://github.com/open-rmf/rmf_demos). Xem [kiến trúc](01-architecture.md), [mạng](07-network.md) và [kỹ năng server](../.agents/skills/agv-server/SKILL.md).

## Bản triển khai 29/09/2026

`server.js` giữ entrypoint; `gateway.js` chứa factory test được với DB riêng. `db.js` chỉ mở DB khi gọi createDatabase. Gateway chờ command_result/request_id, không đoán hoàn thành theo target, không tự apply snapshot khi reconnect. Core xử lý WebCommands.hpp với task_id/session và graph validation. Xem [bản cập nhật](14-web-server-update.md), [ADR-002](decisions/ADR-002-web-core-results.md) và [HTTP/WS v1](../contracts/web-server.md).
