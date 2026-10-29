# ADR-094 — Trạm bảo dưỡng và phục hồi lượt hàng hai xe

Ngày 30/10/2026. SOURCE: người dùng duyệt phạm vi và yêu cầu bắt đầu sửa. Thay phần phục hồi/thao tác thủ công trong ADR-090/092; giữ proof giao hàng của ADR-093.

## Quyết định

- S1 dành cho01, S2 dành cho02, kiêm bảo dưỡng. Firmware mới công bố `maintenanceRecoverySupported`, `maintenanceReady` (nguồn/line/sonar/mạng/RFID/motor/cargo đã ổn định cục bộ). Khi tự về, hoàn tất docking và đóng chặng về trạm trước khi bật khóa bảo dưỡng. Sau đó cần chuỗi diagnostic/RFID/STOP đúng phiên ổn định ít nhất2s. Tự thử RESET_FAULT một lần trong mỗi cửa sổ điều kiện ổn định cho lỗi đang chốt hoặc journal lift cũ, chỉ mở sau diagnostic khỏe; lỗi còn tồn tại giữ dừng. Khi điều kiện mất rồi đủ trở lại thì được thử lại. Reset tại trạm bỏ journal chưa xong nhưng không điều khiển servo. Cargo chưa rõ/còn hàng/chờ giao chưa xong không tự mở khóa nhận việc. Đầu xe tại trạm theo hướng quy ước; người đặt xe phải đặt đúng hướng đó vì RFID không đo hướng. Core tự thực hiện CONFIRM_HEADING và chờ kết quả, không tự quay xe.
- Ngoài trạm: một xác nhận vật lý + hướng gửi `recover_and_continue`. Core reset, chờ diagnostic đúng boot/kết nối/uptime/cmd rồi xác nhận hướng đã chọn. Gateway tự resume đúng task/lượt/action trong ca và phiên cũ; khi được xác nhận vị trí mới thì cập nhật resume_node. Cargo/sổ kho và đường vẫn phải hợp lệ. Không phát lại việc của ca trước hoặc replay sau restart gateway/Core.
- Lỗi một lượt không gián đoạn lượt khỏe cùng đơn. Gateway tiếp tục theo dõi, nâng/hạ và chặng ra gate của lượt khỏe. Giữ chốt nhận lượt mới tới khi hết lượt gián đoạn và mọi xe tham gia IDLE, không lỗi/khóa hướng/reset/lift hoặc cargo đang giữ. Snapshot công bố `fleet_recovery.state`: NORMAL, DEGRADED, WAITING_FLEET.
- Giữ stopped_position/stopped_nodes: proof dừng trên cạnh chỉ chặn hai đầu cạnh; chưa có proof giữ footprint rộng. RFID/mạng mất không tự chứng minh rời vùng. Khôi phục phải tái kiểm tuyến/mutex.
- Reset chờ tối đa15s, hướng5s, tick xử lý hết hạn ngay cả khi mất diagnostic. Thử lại do người vận hành hoặc phiên bảo dưỡng hợp lệ; không vòng lặp reset liên tục. Lỗi mới/kết ca/reconnect vô hiệu lựa chọn hướng và quyền tự resume.

## Nâng/hạ

Firmware cảhai có journal NVS `agv-lift-01/operation` hoặc `agv-lift-02/operation` theo profile: version2, cmd_id dùng làm operation_id, UP/DOWN, node, pha và fingerprint64bit của Core session sở hữu. Chỉ tiếp tục khi fingerprint heartbeat hiện tại khớp; không nối quyền qua Core restart. Đọc fingerprint qua sequence và hai atomic32 để tránh phụ thuộc atomic64 trên MCU. Ghi intent trước PWM, chỉ ghi khi đổi pha. Không rewind counter và không tạo cmd_id mới để phát lại thao tác cũ.

- Mất Wi-Fi/MQTT khi đang thực hiện chu kỳ cục bộ không tự hủy lift; motor di chuyển vẫn dừng theo watchdog. STOP chủ động hoặc lỗi cơ cấu vẫn gián đoạn. Chỉ WAIT reconnect có `preserve_lift:true` giữ giao dịch để nối lại; WAIT thường/kết ca hủy quyền tự nối. `RESET_FAULT` từ xác nhận vật lý có `verified_recovery:true` chỉ gỡ latch cũ khi các kiểm tra tại MCU đạt; vẫn phải chờ xác nhận hướng sau reset.
- Reboot khi còn intent: giữ PWM tắt lúc boot, chờ RFID kho đúng phiên, xe dừng, mạng/nguồn/line/sonar/cảm biến ổn định2s, không task. Tiếp tục tới cùng góc đích rồi chạy cửa sổ xác nhận mới; không suy tiến độ cơ khí từ số giây trước reset.
- Diagnostic thêm `liftRecoverySupported`. Core chỉ nối giao dịch đang giữ có cùng ID/node và high-water hợp lệ, dùng boot/kết nối mới cho bằng chứng tiếp theo. Chờ phục hồi giữ hold; không cho task mới/servo mới chen vào.
- DONE_UP cần proof có hàng. LOWERED vẫn là hết chu kỳ điều khiển5s, không phải đo góc. Chỉ lùi K→G, tại gate phải có proof không hàng và DONE_DOWN mới ghi kho một lần. LM393 không đo góc/tải và không nhận dạng kệ; chưa có encoder/công tắc hành trình servo.
- Journal mất/hỏng, ID/node khác, cảm biến mâu thuẫn, lỗi nguồn/cơ khí hoặc STOP chủ động giữ kiểm tra thực tế. Core/gateway khởi động lại giữ chính sách không replay; việc khôi phục sau reboot MCU chỉ nối giao dịch mà server hiện tại vẫn sở hữu.

## Bàn giao và giới hạn

Build ID source: `20261030-robot01-maintenance-recovery-v26` / `20261030-robot02-maintenance-recovery-v26`. Không đổi profile PWM, mạng, pin hay map. Không sửa DB/counter vận hành, không tự restart/nạp/chạy robot.

Kiểm chứng server ghi ở tests/README.md. Firmware chỉ rà source/diff theo quy trình27/10; chưa build/test/nạp. OPEN: người dùng nạp và kiểm tra hai xe, mất mạng/reboot khi nâng/hạ, hướng đặt trạm, cảm biến/kẹt cơ khí và clearance thực tế.

VERIFIED: server/gateway71/71, browser Edge01/02, Core bảo dưỡng/one-submit/lift reconnect cho01/02 và21/21 ca xung đột PASS; hồi quy shift_recovery/cargo-sensor-core PASS. Core build riêng PASS tại `.qa/maintenance-recovery-20261030/core/agv_core.exe`, SHA256 `C3D00730E99A7414CA3DA9E60FCFB911CB26EF644E0E77E696B5CEF00E99DDD9`. Chưa triển khai vận hành hoặc chạy phần cứng.
