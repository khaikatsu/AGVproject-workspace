# ADR-057 — Nhúng servo GPIO17 cho test nâng/hạ xe01

Ngày15/10/2026. Trạng thái: đã triển khai chế độ test kết nối; chưa nạp hoặc chạy servo thật.

## SOURCE

Người dùng đã gắn tín hiệu servo nâng/hạ vào GPIO17 của xe01. Giữ luồng lấy/hạ hàng hiện tại và thời gian5s; bổ sung nhúng servo, server/web hiện Đang nâng hàng/Đang hạ hàng khi servo hoạt động. Sau nâng hiển thị một kiện ảo trên xe, sau hạ bỏ kiện ảo. Chỉ phục vụ test, không tự mở rộng quản lý đơn hàng hay xe02. Yêu cầu này bổ sung thao tác servo/kiện ảo cho ADR-045, không thay tuyến tới kho/lùi gate.

Bổ sung: người dùng xác nhận mới gắn một servo, muốn quay45° chủ yếu thử kết nối server–servo. Agent đã nêu quy ước test hạ0°/nâng45° trước triển khai. Đây không phải xác nhận góc cơ khí an toàn hoặc số đo servo thực. Dải1000–2000µs là cấu hình test hẹp; cần hiệu chuẩn theo mẫu servo/cơ cấu trước lắp tải.

## OBSERVED trước sửa

- Config.h: GPIO17 ứng với SERVO_LIFT nhưng tất cả SERVO_ENABLED=false, góc nâng/hạ=-1, min/max pulse=0, move time=0. ServoModule hiện cần hai vai trò lift/other có kênh độc lập và góc hợp lệ; không thể bật riêng GPIO17 chỉ bằng một cờ.
- CoreProtocol/AGVController chưa có lệnh lift trong hợp đồng Core v0 đang chạy. Luồng nâng/hạ của ControlLogic cũ không tự được gọi bởi nút warehouse-action.
- Gateway warehouse-actions.js hiện chỉ chờ5000ms sau task completed/RFID kho/IDLE, sau đó phát GOTO gate. Thao tác không tạo cargo/đơn hàng.
- Servo positional qua PWM không có đường phản hồi vị trí/cảm biến tải trong yêu cầu mới; hết5s không chứng minh cơ cấu đạt góc hay có hàng thật.

## IMPLEMENTED — chỉ chế độ test01

Giữ RFID/task_id để tới kho và lùi gate. Khi đã dừng tại kho, Core gửi lệnh nâng/hạ có ID tương quan; MCU chỉ chạy khi đúng phiên/node và không có tuyến đang chạy, motor dừng. Server không đổi kiện ảo chỉ bởi ACK hoặc timer gateway. MCU báo đang nâng/hạ, rồi kết quả sau chu kỳ5000ms; web mới cập nhật kiện ảo tương ứng. Kiện là biểu diễn trạng thái thao tác, không cảm biến xác nhận tải vật lý. Duplicate/retry không chạy servo lại; lỗi, STOP, mất mạng hoặc kết quả cũ không được tự suy thành hoàn tất. Không tự đưa servo về một góc khác khi reboot/STOP; chính sách giữ tải và cơ cấu cần kiểm tra trước thử thật.

Profile01 mở đúng một kênh GPIO17/LEDC5,50Hz/16bit. Begin duty0 không phát góc; nâng45 danh định tương ứng1250µs, hạ0 tương ứng1000µs, giữ cửa sổ5000ms không delay. ServoModule vẫn giữ nhánh cặp hai kênh cũ cho cấu hình khác; profile02 không mở servo. Bản xe01 `20261015-robot01-servo45-test` giữ sonar/encoder/PWMxoay210/lùiS1.

Core v0 thêm LIFT_UP/LIFT_DOWN + cmd_id/nodeK; parser/dedup bao gồm kiểu/node. Diagnostic thêm liftSupported/liftBusy/liftCommandId/liftState/liftElapsedMs/virtualCargo; Core chỉ nhận qua boot/kết nối và phiên encoder đã bind. WS warehouse_lift chỉ cho test01, ca cho phép, xeIDLE đúng kho, diagnostic MCU dừng mới≤3s đúng node, không tuyến/task/pending task/lỗi. Core chuyển LIFTING/LOWERING, lift.state=PENDING khi chờ MCU. Fresh diagnostic đang chạy đúng lệnh đổi lift.state; DONE cùng cmd/node/phiên/boot, MCU IDLE/taskActivefalse/STOP/duty0, cargo đúng và đủ5s cả hai phía mới chốt kiện ảo/mở IDLE. GOTO/dispatch bị khóa khi lift active; không nhận kết quả sau12s thì ERROR/WAIT, không tự lùi. Unsolicited busy sau restart cần kiểm tra, không đoán hoàn thành. Cargo known không tự khôi phục từ checkpoint; đổi boot/kết nối hoặc ngắt thao tác làm mất độ tin cậy, giữ dấu kiện trước đó mờ để cảnh báo nếu có.

Gateway chốt action.servo theo capability lúc tạo. Sau taskcompleted/RFID kho, chờ lift.ready/fresh/observed_node mới tại kho rồi gửi lệnh; tránh race telemetry đích tới trước diagnostic dừng. Handling không dùng service_until để hoàn tất. Chỉ đúng DONE/cargo/fresh/IDLE mới cấp tuyến lùi; ACK không đủ. Firmware cũ không capability giữ mô phỏng5s và UI ghi rõ chưa điều khiển servo, không gán kiện. Không tạo order hoặc sửa lịch sử/DB vận hành. Action vẫn RAM20, không replay sau restart. Trên web hiện Đang nâng/Đang hạ và một ô kiện trên marker; hạ xong mới bỏ, mất xác minh thể hiện mờ, không coi là đo tải thật.

STOP/lỗi/mất mạng hủy việc công nhận kết quả; không tự đổi góc hay replay. PWM giữ target cuối, không khẳng định trục đã dừng hoặc cơ cấu đã khóa: servo positional không có feedback, tự ngắt xung khi treo hàng có thể làm rơi tải. Không sử dụng chế độ test như cơ cấu nâng đã được nghiệm thu.

## OPEN

Góc thực, khoảng xung đúng mẫu và nguồn servo vẫn chưa được đo/xác nhận. Cần nguồn hạ áp phù hợp và chungGND, không cấp pin8,4V trực tiếp. Trước test phải tháo tải/kiểm tra giới hạn cơ khí để quy ước0/45 không va chạm, quan sát một servo riêng trước khi cho xe chạy. Chưa nạp/chạy servo hay restart hệ thống vận hành. Kết quả host/build/integration không thay xác nhận chuyển động thực hoặc hàng thật. Kết quả kiểm thử ghi tại tests/README.md.

Tham chiếu: REQ-ROBOT01-SERVO-LIFT-01; ADR-045; ADR-055 (GPIO15/21 đã dùng sonar/còi); kỹ năng agv-firmware.
