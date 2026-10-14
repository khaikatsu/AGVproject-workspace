# Giao tiếp vi điều khiển và điều khiển robot

14/10/2026: giảm nhẹ PWM xoay xe01 từ220 xuống200, trái/phải duty ngược chiều±200 để thử bắt line khi quay. Giữ tiến220/lùi190 và chốt line/timeout; cần nạp env01 và kiểm chứng trên sàn. [ADR-054](decisions/ADR-054-robot01-slower-pivot.md).

14/10/2026: riêng `esp32dev_two_robot_bench_01` đọc encoder bánh trước trái GPIO16/phải GPIO4, RISING/INPUT_PULLUP, lọc >300 µs, hệ số thử 161 xung/100 cm. ISR đếm raw ticks, quãng đường chặng chỉ cộng khi navigator bám line tiến/lùi; gửi odometry 200 ms. Chưa PID/nhả mutex theo encoder. [ADR-050](decisions/ADR-050-robot01-encoder-position-speed.md), [contract](../contracts/robot01-odometry.md) cập nhật việc loại encoder trước đây; profile 02/legacy chưa bật.

Cập nhật 01/10/2026: mutex đã chuyển sang JSON có grant_id/ACK/retry, sketch có LWT, TLS tùy cấu hình, watchdog và cmd_id trong NVS. [Thiết kế hiện tại và cấu hình](18-hardening.md) thay thế mô tả giao thức/phục hồi cũ bên dưới. Motor/phanh thật vẫn OPEN.

Người dùng đã xác nhận **ESP32 DevKit + Arduino IDE + PubSubClient + RC522**. [Sketch](../firmware/AGV_Connection_Test/README.md) có 26 UID node nạp sẵn, `learn` để thay thẻ, boot UNKNOWN đến khi quét; core/web đồng bộ Chờ quét RFID. [ADR-013](decisions/ADR-013-preloaded-rfid-uid-map.md). Bảng chân mới 03/10/2026 thay RC522 SS21 bằng SS5 để dành GPIO21 cho servo; L298N/line/HC-SR04 đã có module driver và liên động với MQTT/route nhưng motor khóa mặc định. [ADR-012](decisions/ADR-012-motor-firmware-integration.md). Chưa upload/đo board thật; đo pin, sạc và điều khiển rẽ vẫn OPEN.

## Thông số phải bổ sung

- MCU và framework: ESP-IDF, Arduino/PlatformIO, STM32 hoặc loại khác.
- Motor/driver, encoder, nguồn, pin, bộ đo pin và mạch sạc.
- Định vị: RFID, line sensor, odometry, LiDAR hoặc tổ hợp; bảng node ↔ tag.
- Cảm biến vật cản, nút dừng, hành vi an toàn khi mất mạng.
- Kích thước, tải trọng, vận tốc, khoảng dừng, sơ đồ chân và mức điện áp.

Không tự điền pinout, PWM hay điện áp từ một repo tham khảo; dùng datasheet và sơ đồ nhóm cung cấp.

## Tách các tác vụ

1. Điều khiển motor và đọc encoder/sensor theo nhịp phù hợp phần cứng.
2. Máy trạng thái local: khởi động, chờ, di chuyển, chờ vùng, sạc, lỗi, dừng.
3. Network worker: WiFi/MQTT reconnect, receive queue, telemetry, ACK.
4. Command parser: giới hạn độ dài, kiểu dữ liệu, path hợp lệ, cmd_id/session.

Callback MQTT không chạy vòng motor lâu hoặc delay chờ hết hành trình. Chuyển command hợp lệ vào queue/state rồi ACK; công việc điều khiển đọc trạng thái riêng. STOP có đường xử lý ưu tiên theo chính sách đã xác định.

## Vòng trao đổi hiện tại

Robot subscribe `agv/{id}/command` và `mutex_grant`; publish telemetry dạng `node|battery|obstacle`, ACK text và các event. Giữ đúng [hợp đồng v0](07-network.md) khi tích hợp core hiện có.

Với GOTO, path gồm node còn lại. Firmware dùng bảng chuyển node/hướng phù hợp đường thực; node ID không trực tiếp là lệnh motor. Khi đến RFID/node, publish số đo đã xác nhận. Vị trí khởi động phải được xác minh; không mặc định robot đang ở trạm sạc chỉ vì server hiển thị như vậy.

Trước vùng chung, dừng ở điểm chờ và xin mutex. Chỉ đi khi được grant cho đúng zone; release khi thân robot rời hết vùng. Nếu mất grant, request lại có thể được owner hiện tại xác nhận trong core; cần kiểm thử tránh chiếm hai lần.

## Retry và phục hồi

- Cùng command trong phiên: không lặp hành động, vẫn ACK lại.
- Payload sai hoặc quá dài: báo lỗi theo hợp đồng; không ACK như đã nhận lệnh hợp lệ.
- Mất MQTT: local watchdog chuyển sang hành vi đã chốt; không tự nhận path cũ sau reconnect.
- Server/robot reboot: thiết kế session ID hoặc lưu vết; đây là khoảng trống v0 cần xử lý.
- Sạc chỉ ở node/điều kiện xác nhận; `charging_done` gửi pin đo thật nếu có.

Thời gian watchdog, debounce sensor và tần suất telemetry chưa được chốt. Dùng timeout core 10 s làm dữ kiện, không tự suy ra robot được phép chạy không liên lạc 10 s.

## Thử nghiệm bàn và tích hợp

Đầu tiên log command/ACK/sensor khi motor chưa chạy; kiểm tra buffer cho đường dài và deduplicate. Sau đó thử motor với điều kiện bàn phù hợp, đo local stop, mất WiFi và cảm biến lỗi. Tích hợp hai robot chỉ sau khi đã xác minh vị trí, vùng dùng chung và chính sách mất mạng.

Tham khảo [ESP MQTT](https://github.com/espressif/esp-mqtt) hoặc [PubSubClient](https://github.com/knolleary/pubsubclient) theo framework. Điểm đặt code sau khi chốt: [firmware](../firmware/README.md). Kỹ năng: [agv-firmware](../.agents/skills/agv-firmware/SKILL.md).
## Project NCKHAGV bổ sung 04/10/2026

Mã ESP32 PlatformIO tại [NCKHAGV](../firmware/NCKHAGV/NCKHAGV/README.md) đã nhận hợp đồng MQTT v0 của Core, bảng 26 UID và fail-safe kết nối. Theo bổ sung 04/10, hướng rẽ được tính từ path và đồ thị có hướng; motor vẫn khóa vì điều kiện rời M1–M6, quyền mutex và hiệu chuẩn chuyển động chưa hoàn chỉnh. Pin ADC chưa hiệu chuẩn nên không gửi giá trị 0% giả cho Core. Xem [ADR-014](decisions/ADR-014-nckhagv-core-sync.md).

Theo thông số phần cứng bổ sung 04/10, project NCKHAGV đã bỏ module encoder và trường xung/quãng đường chẩn đoán; bánh 6,5 cm chỉ là kích thước cơ khí. Line đen active HIGH, mẫu đi thẳng `010/101/111`; siêu âm thử dừng ≤15 cm. Sketch độc lập cùng pinout xác nhận HIGH/LOW ở cả hai cầu là tiến, LOW/HIGH là lùi; REQ-FW-NCKH-19 sửa các cờ đảo cực tính vốn khiến GOTO S1→GS1 phát pattern lùi. Bám line trên mặt đất vẫn OPEN. Xe 01 trước đó được thử lùi S1→GS1; logic bám line khi lùi và dừng khi mất vạch có trong mã. Hai cell 18650/B25 chưa có điện áp đo nên Core chưa nhận pin thật. [ADR-014](decisions/ADR-014-nckhagv-core-sync.md) giữ rõ giới hạn mở motor và ngưỡng thử 28 cm.

Bản điều khiển cơ bản sau đó nối GOTO vào `CoreNavigator` theo từng RFID và xin/ACK quyền vùng M; chưa tự trả quyền vì chưa xác minh thân xe rời vùng. `node|?|obstacle` giúp Core hiện vị trí thật dù pin chưa đo; trường pin không được coi là số đo và Core tiếp tục chặn task. PWM vẫn khóa mặc định; host/Core test đã chạy, PlatformIO và xe thật chưa được kiểm chứng. [ADR-015](decisions/ADR-015-basic-rfid-line-motion.md).

Người dùng yêu cầu thử xe chạy bằng server: env `esp32dev_trial` mở PWM cho **duy nhất xe 01 lùi S1→GS1**, tối đa 8 giây, với Core opt-in `AGV_ONE_ROBOT_TRIAL=1`. Env `esp32dev` mặc định vẫn khóa motor. Build ESP32 và fixture Core/broker/gateway đã PASS; chưa chứng minh chiều lái, siêu âm khi lùi hoặc thử bánh trên giá kê. [ADR-016](decisions/ADR-016-single-robot-server-motion-trial.md).

Cập nhật 05/10/2026: theo yêu cầu mới, chính env/công tắc trial trên đã mở GOTO xe 01 theo đường Dijkstra tới mọi node có đường trên map; firmware đi từng RFID và bỏ chờ grant riêng trial một xe. Bản thường vẫn khóa motor. Mỗi đoạn dừng sau tối đa 15 giây, mỗi lần quay sau 2,5 giây; chưa hiệu chuẩn ngoài đời. Core chặn task tự động, xe khác và owner mutex cũ; telemetry xe khác trong lúc chạy làm gián đoạn trial. Kiểm chứng 650 cặp node và fixture phần mềm, chưa nạp hoặc chạy xe thật. [ADR-017](decisions/ADR-017-solo-full-map-trial.md).

Cập nhật 07/10/2026: quan sát xe 01 lùi ra khỏi map khi nhận S1→GS1 đã bác bỏ giả định hướng xuất phát cũ. Quy tắc trial mới yêu cầu đầu xe đặt tại S1 hướng về GS1 và chọn tiến cho đoạn này. Các đoạn mô tả lùi S1→GS1 ở trên là lịch sử thiết kế, không còn là hướng dẫn vận hành. Ca trial được kết thúc và lỗi vị trí được giữ để người vận hành kiểm tra trước lần thử tiếp theo. [ADR-017](decisions/ADR-017-solo-full-map-trial.md).

Cập nhật 07/10/2026: sketch W/S/A/D đã xác nhận polarity cũ của firmware đảo hai bên, khiến S1→GS1 chạy lùi. REQ-FW-NCKH-19 đặt hai cờ đảo cực tính về `false`. Sau khi pattern trái lùi/phải tiến vẫn làm xe quay phải tại GS1, REQ-FW-NCKH-23 đặt pivot trái trial về trái tiến/phải lùi ở 255/255, khớp phép thử sketch D quay trái. Serial Monitor ghi `[MOTOR]` cùng duty và line pattern để phân biệt pivot với chỉnh line một bên. Bản `esp32dev_trial` build thành công; chưa nạp/kiểm chứng chuyển động vật lý của bản mới.

Cập nhật xe 01 mới 07/10/2026: theo REQ-FW-NCKH-24, xe thay thế giữ nguyên chân nhưng dùng quy ước quay chuẩn: `LEFT` là trái lùi/phải tiến, `RIGHT` là trái tiến/phải lùi; cả hai bên PWM 255 trong trial. Chỉnh line khi tiến cũng đổi bên motor để `CorrectLeft` thực sự hướng trái và `CorrectRight` hướng phải. Mapping theo sketch xe cũ ở đoạn trên là lịch sử, không dùng cho xe mới. Duty danh định chạy thẳng trial 220/220, trim trái trial `-30` và chế độ thường `-40`; trim mới cùng khả năng bám line còn chờ đo trên xe mới.

Cập nhật sau thử GS1 07/10/2026: xe mới quay phải khi nhận mapping `LEFT` trên và pivot 255/255 quá nhanh. REQ-FW-NCKH-25 đổi `LEFT` thành trái tiến/phải lùi, `RIGHT` đối xứng, đồng thời giảm PWM pivot trial còn 200/200. Chỉnh line khi tiến khớp lại bên motor theo chiều quay thực tế. Mục REQ-FW-NCKH-24 ở trên là lịch sử; bản mới chưa nạp và chưa xác nhận bắt lại line.

Cập nhật pin 08/10/2026: firmware dùng GPIO36 và mạch chia 30 kΩ/7.5 kΩ để ước tính pin 2S trong dải 6.4–8.4 V. Để người dùng thử hiển thị trước, phần trăm gửi qua JSON diagnostic; telemetry điều khiển vẫn gửi `?`, nên Core chưa áp dụng ngưỡng 20%/40%. Điện áp và phần trăm cần được đối chiếu bằng đồng hồ trên xe trước khi cho phép ngưỡng điều phối sử dụng số liệu này.

Sau quan sát pin 65–70% trên xe, `BatteryModule` mới lấy 16 mẫu ADC không chặn trong mỗi đợt, bỏ mẫu cao nhất/thấp nhất, lọc giữa các đợt và thêm biên 1,5 điểm cho phần trăm. Bản `esp32dev_trial` đã build thành công ngày 08/10/2026; chưa nạp hoặc đo xe thật với bộ lọc này.

Cập nhật REQ-FW-NCKH-26: `010` là mẫu bám line hợp lệ; web báo `LINE_LOST` do lỗi firmware đã chốt từ một mẫu `000` trước đó, còn marker GS1 là RFID cuối. Để tránh chốt lỗi vì một mẫu thoáng qua, bản trial dừng motor ngay ở `000`, xác nhận mất line liên tục 120 ms rồi mới báo lỗi; Serial ghi mẫu `[LINE]` tại lúc xử lý. Cần nạp và quan sát xe thật để biết nguyên nhân gốc.
## Xe 01: sonar trước/sau (15/10/2026)

Profile `esp32dev_two_robot_bench_01` nhận HC-SR04 trước TRIG/ECHO GPIO12/13, sau GPIO0/15; còi active LOW dùng GPIO21 theo ảnh người dùng. Firmware kích xen kẽ cách 50 ms, timeout echo 30 ms; sonar phía di chuyển ≤15 cm giữ STOP và tuyến, chỉ nhả khi >20 cm hoặc timeout với ECHO LOW (ngoài tầm theo sketch thử). ECHO kẹt HIGH/dữ liệu cũ dừng lỗi cảm biến; dây ECHO đứt LOW không thể phân biệt với ngoài tầm đo. FirmwareBuild `20261015-robot01-dual-sonar`; xe 02 giữ pin và behavior cũ. Xem [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md), [hợp đồng](07-network.md). Chưa nạp/đo quãng phanh thực tế; ECHO 5 V phải qua chia áp/chuyển mức 3,3 V và cần kiểm tra GPIO0 lúc boot trước thử motor.
