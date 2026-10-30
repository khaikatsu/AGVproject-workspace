# Phát triển server và lưu trữ

30/10/2026 mới nhất: [ADR-101](decisions/ADR-101-shift-home-departure-readiness.md) chặn tuyến returnToHome mới và retry BLOCKED khi chưa có hướng/STOP/RFID mới đúng phiên hoặc lệnh cũ chưa ACK. Giữ CLOSING và lý do chờ, không phát GOTO sai hoặc tự gỡ lỗi; tuyến đã phát không lặp mỗi tick. STALE_COMMAND ghi core_context như INVALID_COMMAND. Core đã build riêng, chưa áp vận hành; không đổi firmware/timeout/counter.

30/10/2026: [ADR-100](decisions/ADR-100-direct-goto-busy-guard-and-error-context.md) chặn directGoto chồng tuyến độc lập capability, giữ task/target/path đang hoạt động và trả409. INVALID_COMMAND lưu core_context trước interrupt/STOP để truy vết. Build riêng thành công, chưa thay Core vận hành; không sửa firmware.

30/10/2026 mới nhất: [ADR-099](decisions/ADR-099-cargo-monitoring-and-direct-reconciliation.md) bỏ proof LM393 khỏi nâng/hạ/chạy tiếp; vẫn đúng ID/phiên/RFID/STOP/5000ms. Core nhận cargoMonitoringOnly từ MCUv28 và dùng diagnostic uptime tăng độc lập cảm biến. Sổ kho chờ DONE/task gate, không tự ghi missing từ cargo. Đối soát cargo bỏ gate/cảm biến/diagnostic/trạm, cho MQTT offline; giữ task/tuyến/lift active, không actuator/nhả lỗi/mutex. Chưa build/test/triển khai.

30/10/2026, mới nhất: `orders.resolve` thực hiện transaction sổ kho/đóng đơn trực tiếp; không gọi Core `reconcile_test_cargo` và không yêu cầu `physically_clear`. Event dùng `operator_requested/inventory_only/physical_completion_verified:false`; giữ cargo Core/cảm biến, revision, kiểm tra xung đột và báo cáo cũ. [ADR-098](decisions/ADR-098-one-click-inventory-rollback.md) thay luồng hoàn tác ADR-089.

30/10/2026: [ADR-097](decisions/ADR-097-healthy-home-without-heading-confirmation.md) bỏ chu kỳ xác nhận hướng cho xe khỏe đỗ đúng S1/S2. Diagnostic mới, đúng phiên, motorSTOP và không task/footprint/lift/reset nhận hướng bay_default hoặc docking route/ready đúng quy ước; lỗi chỉ reconnect được tự gỡ trong phạm vi này. Không tự xóa lỗi thật, cargo hoặc mutex.

30/10/2026: [ADR-096](decisions/ADR-096-durable-shift-closure-and-reconciliation.md) tách mốc ca CLOSED bền vững khỏi điều kiện mở ca mới. Đơn đã kết ca không bị ghi lại khi Core mất kết nối; mốc legacy được ghim trước save và bằng chứng đối soát được publish ngay sau commit. Cần Core/gateway mới, giữ toàn bộ chốt và lịch sử.

29/10/2026, REQ-DROPOFF-EXIT-PROOF-20261029: Core nhận capability `dropoffExitSupported`; sau LOWERED tại kho giữ cargo=true/dropoff_exit_pending, chỉ cấp chặng lùi một cạnh K→G. Đúng DONE_DOWN/lift ID/boot/kết nối/cảm biến trống mới tại gate mới xóa hold. Scheduler đơn/thao tác một xe chờ thêm task gate completed rồi mới confirmLift/ghi sổ kho, không ghi giao ngay khi servo hạ hết5s. Checkpoint giữ hold sau restart nhưng không khôi phục quyền chạy hoặc proof phiên. Chưa build/test/chạy bản sửa. [ADR-093](decisions/ADR-093-dropoff-exit-cargo-proof.md).

29/10/2026, REQ-UNIFIED-RECOVERY-HEADING-20261029: Core giữ khóa hướng khi phục hồi thiếu hướng chắc chắn/xe được di chuyển hoặc đổi boot/kết nối/restart. Khóa áp dụng directGoto/resume/dispatch/assignPath và chốt phát GOTO/NODE_ORDER/CHARGE_START. Xác nhận hướng dùng yêu cầu riêng khi xe đã khỏe, chứng minh dừng đúng node; chỉ diagnostic operator khớp cmd_id/node/hướng/boot/kết nối/uptime mới mở khóa. Không suy thành công từ ACK hoặc hướng bay_default. [ADR-092](decisions/ADR-092-unified-recovery-heading.md).

28/10/2026, REQ-RECONCILED-ORDER-HOLD-20261028: `warehouse-inventory.reconciliationFor` tra event đối soát toàn kho đã xác nhận sau mốc kết ca; đơn đã kết ca và không còn move mở được trình bày closed/warehouse_reconciled và không tiếp tục giữ xe. Bản ghi DB gốc, cargo và lịch sử task giữ nguyên. nearest ghi rõ xe/đơn đang chặn khi chưa có bằng chứng. [ADR-090](decisions/ADR-090-shift-recovery-and-binary-inventory.md).

28/10/2026, LM393: `cargo-proof.js` dùng snapshot Core đã kiểm phiên/seq/tuổi/STOP, không suy kho trống từ unstable/null. `confirmMissing` ghi ô trống + mã kệ unknown/needs_attention + event missing trong transaction; đơn/action gián đoạn, không nâng/đi tiếp. `confirmLift` kiểm thêm cảm biến mới tương ứng DONE, giữ idempotency hiện có. [ADR-091](decisions/ADR-091-lm393-warehouse-proof.md).

28/10/2026: `end_shift` chuyển CLOSING rồi hủy queue/việc treo và quyền tiếp tục; WAIT vẫn chờ proof, không xóa mutex hoặc tồn kho bằng suy đoán. Gateway dừng scheduler trước lệnh kết ca; đơn chưa giao đóng, lượt có kệ/đặt chỗ giữ needs_attention và `closed_by_shift`. Restart gateway không chạy lại cả đơn còn queued. `resume_task` giữ task ID, tính tuyến và cmd_id mới sau phục hồi RFID; gateway lưu pha chặng và đối soát sổ kho, không replay servo. Đối soát lỗi lưu bảng audit riêng, không xóa system_log/KPI. [ADR-090](decisions/ADR-090-shift-recovery-and-binary-inventory.md).

26/10/2026, REQ-WAREHOUSE-TEST-ROLLBACK-01: `orders.resolve` chờ batch Core `reconcile_test_cargo`, sau đó `warehouse-inventory.rollbackOrder` ghi kệ→kho lấy, kết thúc đặt chỗ, log và đóng đơn cùng transaction. Core chỉ cập nhật cargo ảo sau proof dừng/lift ID, giữ lỗi/mutex/vùng chưa xác minh. Lượt đã hạ không bị hoàn tác. [ADR-089](decisions/ADR-089-test-order-rollback.md), [hợp đồng](../contracts/web-server.md).

26/10/2026: gateway thêm ánh xạ tên kho/trạm độc lập graph theo REQ-WEB-LOCATION-NAMES-01. `location-names.js` quản lý song ánh/CAS revision, SQLite lưu cấu hình và ảnh tên cho task/đơn. HTTP có revision được dịch sang ID vật lý trước khi dùng planner/Core; các tích hợp API cũ vẫn dùng ID vật lý. Không thay UID, home, map/counter hoặc MQTT/firmware. [Hợp đồng](../contracts/web-server.md), [ADR-087](decisions/ADR-087-server-location-names.md).

CURRENT 26/10/2026, REQ-WEB-UNIFIED-MISSIONS-01: scheduler lưu `item.task_links` sau mỗi kết quả direct_goto, giữ đủ các chặng kể cả khi `item.task_id` bị xóa/thay. DB cung cấp khóa dòng chính xác cho link; gateway enrich lịch sử bằng order/item/phase theo ID, giữ liên kết qua adoption phiên. Không thay state machine nâng/hạ/giao hoặc điều kiện hoàn thành. [Hợp đồng](../contracts/web-server.md), [ADR-085](decisions/ADR-085-unified-mission-results-board.md).

CURRENT 26/10/2026, REQ-WEB-DELETE-RECORD-01: hai DELETE đơn/lịch sử thực hiện xóa logic bền vững ở gateway, không phát lệnh Core. Đơn giữ `deleted_at` trong JSON; `task_log` thêm cột nullable tương ứng, sync/adoption không xóa dấu. Danh sách lọc nhưng scheduler/chốt kho/báo cáo/task mở và nguồn KPI dùng đủ bản ghi. Xem [hợp đồng](../contracts/web-server.md) và [ADR-084](decisions/ADR-084-delete-individual-dashboard-records.md).

CURRENT 25/10/2026, REQ-WEB-SERVER-MAINT-01: planner, inventory, thao tác kho và browser dùng chung định nghĩa node kho `station` có ID K1–K6; station khác không được đưa vào đơn. `reserve` chỉ idempotent với cùng nội dung và trạng thái `reserved/carrying`; lượt `needs_attention`, đã kết thúc hoặc trùng key khác nội dung bị từ chối. Gateway restart nhận snapshot `CLOSED` cùng phiên Core chốt đúng một báo cáo có `observation_gap`.

CURRENT 25/10/2026: hủy task đã giao chốt lịch sử cancelled nhưng chưa cho xe IDLE/nhận việc ngay. Core giữ footprint, gửi WAIT và chờ diagnostic dừng/RFID mới; cảnh báo sau10s. Khởi động bắt lỗi file/config và exit1, giữ dữ liệu để đối soát; migration mở rộng stopped_nodes cũ vào uncertain_path. Các tối ưu serialize/dispatch/checkpoint và metric commit chậm theo [ADR-082](decisions/ADR-082-core-cancellation-startup-and-durability.md).

CURRENT 19/10/2026: Core xử lý `STALE_COMMAND` như lỗi lệch counter bền vững: xóa pending retry, không phát STOP ID thấp kế tiếp, không tự xóa ERROR từ diagnostic khỏe. Diagnostic `lastCommandId` đẩy `CommandIds` tiến và reserve bền vững phía trên; chỉ thao tác `clear_error` sau xác minh mới gửi `RESET_FAULT` ID mới. Không replay GOTO/task cũ. [ADR-072](decisions/ADR-072-stale-command-counter-recovery.md).

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
