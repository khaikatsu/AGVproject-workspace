# ADR-057 — Nhúng servo GPIO17 cho test nâng/hạ xe01

## Mở rộng21/10/2026 — Người dùng xác nhận servo xe02 giống xe01

SOURCE: người dùng xác nhận xe02 cũng dùng hạ/mặc định180°, nâng100° và dải544–2400µs như xe01. IMPLEMENTED thêm bốn macro hiệu chỉnh trong Robot02Config.h, không chép tuning motor/line/mạng giữa hai profile. Cảhai giữ50Hz/16bit/GPIO17/LEDC5/5000ms;100° ra1575µs/duty5160,180° ra2400µs/duty7864. Boot vẫn duty0, không tự quay180°. BuildID02 `20261021-robot02-servo180-100-v8`; buildID01 giữv8. Không đổi giao thức hoặc trình tự Core/nâng/hạ/STOP.

Host cập nhật ca hiệu chỉnh để cảhai profile bắt buộc cùng dải/góc và cùng bảngPWM; profile không servo vẫn tắt. Kết quả kiểm thử/build trong tests/README.md. OPEN nạp/thử phần cứng; các mục xe02 giữ1000–2000µs/0–45° dưới đây là lịch sử đã bị thay thế.

VERIFIED cảhai: hostServoOnly PASS ba cấu hình servo và Core servo01/02; buildESP32 hai env PASS, chưa nạp/thử phần cứng.

## Chốt góc21/10/2026 — Mặc định/hạ180°, nâng100°

SOURCE: sau ảnh thông số, người dùng xác nhận rõ180° là mặc định và100° là nâng. IMPLEMENTED riêng profile01: đích LIFT_DOWN180°/LIFT_UP100°, giữ544–2400µs/50Hz/16bit/GPIO17/LEDC5/chu kỳ5000ms. Phép nội suy số nguyên hiện tại cho100°=1575µs/duty5160 và180°=2400µs/duty7864. Góc mặc định được hiểu là đích khi nhận lệnh hạ; boot vẫn duty0, không tự quay180° hoặc tự khôi phục vị trí sau reset. Không thay xe02, motor/line, mạng, watchdog hoặc vòng đời thao tác Core. BuildID01 `20261021-robot01-servo180-100-v8`.

Thông số0/45 tạm và mụcOPEN chờ góc bên dưới đã được thay bằng xác nhận này. Host kiểm thêm mốc100° độc lập và khẳng định góc thao tác từng profile; kết quả build/kiểm thử tại tests/README.md. OPEN nạp/thử cơ cấu thật; không coi kết quả PWM là đo vị trí hoặc tải vật lý.

VERIFIED180/100: host `scripts/test-robot-parity.ps1 -ServoOnly` PASS cảba cấu hình servo và Core servo01/02, buildESP32 hai env PASS. Chưa upload/thử phần cứng.

## Cập nhật21/10/2026 — Dải xung riêng xe01 theo ảnh người dùng

SOURCE: yêu cầu tích hợp thông số servo đã căn chỉnh, ảnh thấy50Hz/16bit và hai mốc544µs=0°,2400µs=180°. Đây là thông số người dùng cung cấp, không phải phép đo của agent; góc thao tác nâng/hạ mới không xuất hiện trong ảnh, đã hỏi bổ sung.

IMPLEMENTED: `Robot01Config.h` có min/max pulse và góc nâng/hạ riêng. Xe01 dùng544–2400µs, tạm giữ hạ0°/nâng45° cho đến khi người dùng xác nhận số mới; xe02 giữ1000–2000µs/0–45°. GiữGPIO17/LEDC5/50Hz/16bit, begin chỉ ghi duty0, chu kỳ5000ms, không thay luồng Core/lệnh/kiện ảo/STOP hoặc motor/line/watchdog. Config kiểm tra biên góc0…180 và0<min<max<chu kỳPWM khi compile. BuildID01 `20261021-robot01-servo544-2400-v7`; thay riêng phần hiệu chỉnh servo01 trên nền người dùng gửi theo ADR-075.

Với công thức hiện có, góc0/45/90/180° lần lượt ra544/1008/1472/2400µs, duty16bit1782/3302/4823/7864. Mức hạ0° nay phát544µs chứ không phải1000µs như trước: giữ cùng số độ không đồng nghĩa giữ vị trí cơ khí cũ. Không tự nạp hoặc chạy servo; xác nhận hai góc thao tác và giới hạn cơ khí trước thử có tải.

VERIFIED: `scripts/test-robot-parity.ps1 -ServoOnly` PASS servo01, servo02, profile không servo và fixture Core servo cho cảhai xe. Kiểm0/45/90/180°, từ chối góc ngoài biên/kênh tắt, không ghi kênh motor, không phát góc khi boot,5s/ngắt thao tác/rollover; ACK không được coi là hoàn thành. Kết quả build tại tests/README.md. OPEN góc nâng/hạ mới và thử phần cứng. Các mục0/45 với1000–2000µs dưới đây là lịch sử trước cập nhật này cho xe01.

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
