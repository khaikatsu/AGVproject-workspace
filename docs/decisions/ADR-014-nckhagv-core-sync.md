# ADR-014: Đồng bộ NCKHAGV với Core và giữ khóa motor

Ngày: 04/10/2026. Trạng thái: áp dụng cho mã nguồn; cần build ESP32/kiểm thử bàn trước khi nạp và vận hành.

## Bối cảnh

Người dùng bổ sung toàn bộ project `firmware/NCKHAGV/NCKHAGV` và yêu cầu đồng bộ với server. Khảo sát mã thấy project dùng JSON `TASK/START` riêng, các topic và WiFi để trống, UID RC522 placeholder, trong khi Core hiện dùng GOTO/STOP/WAIT/CHARGE_START, ACK số, telemetry pipe và heartbeat phiên. Người dùng xác nhận chưa có bảng hướng rẽ từng đoạn hoặc cách xác nhận toàn thân xe rời M1–M6 và yêu cầu giữ khóa motor.

## Quyết định

Giữ project PlatformIO hiện có, thêm codec Core v0 và đưa `MQTTModule` về đúng topic/payload. NVS lưu high-water `cmd_id` trước ACK, deduplicate lệnh retry theo ID và nội dung; ACK lỗi được gửi lại sau reconnect. Trạng thái WiFi/MQTT/heartbeat, session Core và tuổi worker là điều kiện kết nối; telemetry chỉ phát sau heartbeat mới, và vị trí RFID phải thuộc đúng phiên kết nối/Core hiện tại. Đưa 26 UID đã xác định vào bảng của project mới. Thêm cấu hình mạng riêng bị ignore, hỗ trợ TLS có CA xác minh và chờ đồng hồ NTP. Không tự thay broker/counter đang chạy.

`ENABLE_CORE_MOTION=false` và tầng driver chặn PWM. GOTO báo lỗi khóa motor, STOP/WAIT giữ dừng, CHARGE_START báo chưa có bộ sạc. Không phát mutex release khi chưa chứng minh thân xe rời vùng. Pin chưa được hiệu chuẩn thì không gửi `node|0|...` vì Core hiểu đó là đo thật; thay bằng lỗi `BATTERY_UNCALIBRATED` và chẩn đoán riêng. Đây là giới hạn tạm thời của telemetry v0.

## Giới hạn và kiểm chứng

Parser Core và formatter telemetry/ACK chạy kiểm thử host ở `tests/nckhagv_protocol.cpp`; chưa kiểm chứng UART, GPIO, WiFi, TLS hoặc MQTT với ESP32 thật. `platformio.ini` vẫn pin ESP32 Arduino/ArduinoJson phiên bản cũ; máy hiện không có PlatformIO CLI. Lần thử biên dịch bằng Arduino CLI cài sẵn không hoàn tất vì helper PyInstaller không giải nén được `VCRUNTIME140.dll` vào thư mục tạm (permission denied), nên chưa khẳng định build thiết bị đạt.

## Bổ sung 04/10/2026: hướng rẽ từ bản đồ

`SOURCE`: người dùng làm rõ chiều đi đã ký hiệu bằng mũi tên và RFID dẫn tới rẽ hoặc đi thẳng; xe ước lượng dài 18 cm, rộng 12 cm, đầu đọc RFID cách đuôi khoảng 12 cm. `OBSERVED`: Core gửi path theo các cạnh bản đồ, nhưng tại M1/M5/M6 có nhiều nhánh hợp lệ và nhánh kho/sạc có cạnh hai chiều; cần cả node trước, node hiện tại và node kế để xác định hướng tương đối. `IMPLEMENTED`: sinh bảng 26 node/39 cạnh từ `data/map_layout.json`; RoutePlanner kiểm tra chiều từng cạnh và phân loại rẽ/thẳng/quay đầu. GOTO kiểm tra route trước khi báo khóa motor, nhưng không phát PWM. `OPEN`: biên vật lý vùng M1–M6, hướng xe ban đầu sau boot, grant/ACK/release mutex trong firmware này, độ chính xác tọa độ so với kho thật, thông số rẽ và hiệu chuẩn cảm biến. Chỉ biết xe dài 18 cm và vị trí đầu đọc chưa đủ để xác nhận đuôi đã ra khỏi vùng. Do đó giữ `ENABLE_CORE_MOTION=false`.

`SOURCE` bổ sung: người dùng chọn 28 cm làm ngưỡng thử tạm khi test một xe trước, sẽ đo kỹ sau; chưa đo PPR encoder, đường kính bánh hoặc thời gian rẽ. `IMPLEMENTED`: lưu 28 cm vào `Config::MUTEX_EXIT_TRIAL_CM` và thêm dòng Serial `[BENCH]` để lấy số xung/cảm biến khi motor khóa; encoder sau đó bị bỏ theo yêu cầu mới, nên dòng hiện chỉ có line/siêu âm/ADC. `OPEN`: map cm thực, hành trình đầu đọc sau khi rẽ, sai số đọc RFID và đo quãng đường không dùng encoder, ngưỡng siêu âm, pin và grant/release. Không dùng ngưỡng thử để tự trả quyền hoặc mở motor trước khi có số đo và kiểm thử bàn; test một xe vẫn phải theo hợp đồng Core.

`SOURCE` cập nhật sau đó: người dùng yêu cầu bỏ encoder vì chưa dùng; xác nhận ESP32 DevKit V1 38 chân/L298N/dây theo Config, line đen active HIGH với `010/101/111` là bám thẳng, mắt cách trục bánh trước 5 cm và bánh sau 10 cm, bánh đường kính 6,5 cm, vạch rộng 4,5 cm. Bánh trái tiến và bánh phải lùi để quay trái theo mô tả. Siêu âm yêu cầu dừng ở 15 cm; ECHO đã hạ mức. Xe 01 lùi S1→GS1, mắt vẫn nhìn thấy vạch, sau đó đi theo đường một chiều. Pin hai cell Li-ion 18650 qua B25 nhưng chưa đo điện áp/tỉ lệ chia. `IMPLEMENTED`: xóa encoder khỏi Config/controller/telemetry/event/nguồn biên dịch; đổi pivot trái/phải, nhận lại line `010/101/111` sau khi rời vạch cũ, đặt ngưỡng siêu âm 15 cm, bổ sung xử lý lùi S1→GS1 trong RoutePlanner. `OPEN`: chưa kiểm thử chiều quay thật, bám line khi lùi, timeout rẽ, tỉ lệ B25, cách đo 28 cm không dùng encoder và Core→motor/mutex. PWM vẫn khóa.

Còn OPEN: xác nhận rời mutex, đặc tính driver/phanh, siêu âm/pin/servo, CA/tài khoản broker vận hành, thử tải và kết nối trên hai xe. Chỉ sau khi có bằng chứng mô phỏng/bàn và cập nhật contract/test mới thiết kế lệnh chuyển động thực.

`SOURCE` xác nhận tiếp: S1→GS1 là đoạn lùi, ba mắt ở phía trước vẫn đọc được vạch suốt đoạn; bộ pin có hai cell nhưng chưa đo điện áp. `IMPLEMENTED`: thêm phân loại bám vạch khi lùi, gồm `010/101/111` lùi thẳng, mắt trái/phải lệch thì giảm một bên bánh để chỉnh hướng thân xe; `000` dừng an toàn. Đây là logic phần mềm chưa được kiểm chứng trên xe. `OPEN`: hướng hiệu chỉnh lùi và mức PWM thực tế, cách mắc/điện áp bộ pin và hệ số B25; không suy ra phần trăm pin từ số cell.
