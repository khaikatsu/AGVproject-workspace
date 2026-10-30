# ADR-099 — Cargo chỉ theo dõi; nâng 5 giây và đối soát trực tiếp

Ngày30/10/2026. SOURCE: người dùng báo nút Đối soát kệ xe bị chặn bởi lỗi yêu cầu ra gate; yêu cầu bấm đối soát được ngay và xe đến kho/dừng thì nâng đủ5s, bỏ điều kiện cargo có hàng. Cargo chỉ theo dõi; áp dụng chung01/02.

OBSERVED: ADR-098 chỉ sửa Hoàn tác đơn/sổ kho; Đối soát kệ xe gọi endpoint khác. Core còn chặn gate/cảm biến/diagnostic; MCU còn chặn trước/sau nâng, khi mất hàng trên đường và cảm biến sau lùi. Sửa riêng web không thay được firmware đang nạp.

## Quyết định

- Bỏ LM393 khỏi điều kiện nâng/hạ, hoàn tất servo, di chuyển, tiếp tục tuyến và bảo dưỡng. Giữ đúng RFID kho, STOP/PWM0, không task/tuyến/lift chồng lấn, mạng, điện áp, cấu hình, line, sonar và ID/phiên lệnh. Không thay motor, góc servo, mạng hoặc hiệu chuẩn.
- MCU phát servo một lần theo ID; đủ5000ms và điều kiện vận hành hợp lệ thì DONE_UP dù cargo trống/unknown/unstable. Không chờ thêm cảm biến hoặc phát lại servo. Sau hạ vẫn LOWERED→lùi một cạnh K→G→dừng/RFID mới→DONE_DOWN; bỏ yêu cầu cảm biến trống và timeout cargo2000ms. Sổ kho vẫn chờ task gate completed và DONE_DOWN đúng ID/phiên, không từ ACK/timer gateway.
- Giữ đọc GPIO2/Active-LOW/debounce150ms/seq/tuổi/raw và hiển thị có/không/chưa xác định. Không phát lỗi mới CARGO_MISSING/CARGO_LOST/CARGO_SENSOR_UNSTABLE từ cảm biến; không tự đánh dấu kho trống/unknown hoặc ngắt đơn vì LM393. Cargo ảo theo chu kỳ servo, độc lập cảm biến; reservation/sổ kho vẫn kiểm nhất quán.
- Diagnostic schema1 thêm `cargoMonitoringOnly:true`; Core yêu cầu capability này trước lift mới để tránh dùng MCU cũ còn chặn cảm biến. Snapshot `cargo_sensor.required:false,monitoring_only:true`. Lift vẫn cần đúng command/node/boot/connection, elapsed≥5000ms, thời gian Core và diagnostic uptime tăng độc lập cargo sample sequence.
- Đối soát kệ xe gọi ngay, không hộp xác nhận hoặc yêu cầu cargo/gate/RFID/diagnostic/trạm/ca đóng. Đơn gián đoạn liên quan được hoàn tác lần lượt qua ADR-098 với revision mới, sau đó gửi đối soát cargo. Lỗi từng bước báo tại chỗ; không coi nhiều request là một transaction phân tán.
- HTTP `POST /api/agvs/:id/reconcile-cargo` nhận `{}`. WS `reconcile_test_cargo` giữ batch01/02/lift_command_id cũ; physically_clear/home_rollback không còn làm bằng chứng, lift ID cũ không chặn đối soát. Core chỉ xóa cargo ảo/hold ra gate; không sửa LM393/vị trí/hướng/lỗi/uncertain path/mutex và không phát MQTT. Kết quả `scope:virtual_cargo_only,physical_completion_verified:false`. Cho phép MQTT/xe offline; cần Core WS sẵn sàng để ghi trạng thái. Vẫn từ chối task/tuyến/lift đang active để không xóa trạng thái thao tác đang thực hiện.

Thay các chốt cargo của ADR-089/091/093/094. ADR-098 về Hoàn tác sổ kho tiếp tục hiệu lực. Đối soát không công nhận chuyến giao thành công hoặc tự cho phép xe chạy lại.

## Bàn giao và giới hạn

BuildID source: `20261030-robot01-cargo-monitor-v28` / `20261030-robot02-cargo-monitor-v28`. Giữ nguyên file profile01/02, mạng, hiệu chuẩn, GPIO/cực tính, thời gian/góc servo. Cần Core source mới, restart gateway/tải web và người dùng nạp cảhai profile v28.

Chỉ rà source/diff và cú pháp JavaScript/JSON. Không tạo/sửa/khôi phục hoặc chạy test; chưa build Core/MCU, nạp, mô phỏng, thử phần cứng, restart dịch vụ hoặc sửa DB vận hành theo quy trình firmware27/10. Các test cargo/đối soát cũ còn kỳ vọng chặn cảm biến/gate không còn mô tả yêu cầu mới, chưa sửa/chạy lại. Người dùng tự nghiệm thu cảhai xe.
