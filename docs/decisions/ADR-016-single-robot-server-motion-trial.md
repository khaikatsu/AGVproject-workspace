# ADR-016: Thử xe 01 chạy một đoạn bằng lệnh server

Ngày 04/10/2026. Trạng thái: triển khai phần mềm; chưa xác nhận chạy vật lý.

## Bối cảnh

`SOURCE`: người dùng yêu cầu xe phải chạy bằng lệnh server, thay cho bản firmware chỉ đọc RFID với motor khóa. Xe 01 đặt tại S1 và lùi theo line đến GS1. Pin hai cell 18650 qua B25 chưa đo hệ số chia; chiều lái lùi và tầm phủ siêu âm phía sau chưa được kiểm thử trên xe. Điều kiện rời vùng M1–M6 chưa xác minh.

`OBSERVED`: Core chặn `GOTO` khi `battery_measured=false`. Firmware thường khóa PWM. Trước sửa, firmware còn phát `BATTERY_UNCALIBRATED` lên topic `error`, khiến Core đưa xe vào ERROR dù dấu `?` trên telemetry đã biểu thị pin chưa đo.

## Quyết định

Thêm PlatformIO env `esp32dev_trial`, biên dịch `AGV_ONE_ROBOT_TRIAL=1`. Env `esp32dev` thường tiếp tục khóa motor. Bản trial chỉ chấp nhận `GOTO` của robot `01` tại S1 với đúng một node kế `GS1`, phân loại `Reverse`; mọi tuyến khác bị từ chối tại firmware. PWM thử là 140/255 trước trim, đoạn tối đa 8 giây, dừng tại RFID GS1, mất line, mất MQTT/heartbeat, siêu âm lỗi hoặc vật cản. Không có rẽ và không đi vào vùng M.

Core chỉ cho phép pin `?` vượt qua chốt pin với **lệnh thủ công S1→GS1 của xe 01** khi biến môi trường `AGV_ONE_ROBOT_TRIAL=1`, xe đang IDLE, tuyến trực tiếp còn trên map và không có xe khác online. Giao nhiệm vụ tự động vẫn cần pin đo thật. Telemetry tiếp tục giữ dấu `?`; không tạo phần trăm pin giả. Tình trạng pin chưa đo nằm trong telemetry/chẩn đoán, không phát lên topic lỗi dừng xe. Core chỉ đánh dấu đến nơi khi telemetry có RFID GS1; ACK vẫn chỉ là biên nhận.

Firmware không latch lỗi WiFi/MQTT trong lúc đang ghép nối lần đầu. Sau heartbeat Core đầu tiên, watchdog cũ hoạt động bình thường và lỗi mất kết nối buộc dừng. Lỗi khi đang chạy không tự xóa sau reconnect.

## Giới hạn và kiểm chứng

Thử vật lý phải có người giám sát, đường lùi trống, nút ngắt nguồn trong tầm tay; kiểm tra bánh xe trên giá kê trước khi đặt xuống line. Siêu âm phía trước không bảo vệ được đường lùi nếu lắp hướng trước. PWM 140/255 và 8 giây là giới hạn thử phần mềm, chưa phải tốc độ/quãng đường đo được. Không dùng chế độ trial cho hành trình qua M1–M6 hoặc hai xe. B25, chiều motor, lái lùi, tầm cảm biến và cơ chế rời vùng vẫn `OPEN`.

Host test kiểm tra route thử và stop, Core unit kiểm tra chốt pin/chặng, MQTT integration fixture kiểm tra lệnh Core→telemetry GS1→map; 52 PlatformIO native test và build `esp32dev_trial` đã PASS. Chưa upload hoặc chạy xe thật tại thời điểm ghi ADR này.
