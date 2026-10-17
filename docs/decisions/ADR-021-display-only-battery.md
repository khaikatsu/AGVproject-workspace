# ADR-021 — Nguồn phần trăm pin NCKHAGV

Ngày 08/10/2026. Trạng thái: đã chốt theo REQ-BATTERY-02.

## Bổ sung 16/10/2026 — REQ-BATTERY-06

Người dùng xác nhận số phần trăm đã lọc đang hiển thị khá hợp lý và yêu cầu dùng nó làm chuẩn. Quyết định này thay phần “display-only” của REQ-BATTERY-02, nhưng không đổi cách firmware lấy mẫu hoặc payload telemetry v0.

- Core chỉ nhận làm pin điều phối khi message diagnostic sống có đúng robot ID, `batteryCalibrated=true` và `batteryPercentage` hữu hạn trong 0–100. Message retained bị loại như trước.
- Giá trị cập nhật đồng thời `battery`, `battery_measured`, `needs_recharge` và trường hiển thị. Chính sách hiện có tiếp tục dùng ngưỡng thấp20%, nhận việc lại40% và mục tiêu90%.
- Snapshot thêm `battery_control_source`: `diagnostic`, `telemetry` hoặc `unavailable`. Telemetry số0–100 của firmware cũ vẫn là nguồn hợp lệ và được ưu tiên khi nhận.
- NCKHAGV tiếp tục gửi `?` trong telemetry để giữ tương thích v0. Dấu `?` không xóa pin diagnostic còn mới trong3giây; nếu diagnostic ngừng/không calibrated thì Core trở lại `battery_measured=false`, pin−1 và khóa giao việc mode thường.
- Các mode thử vẫn giữ ngoại lệ không chặn theo ngưỡng đã có; mode thường dùng nguồn diagnostic này cho mở ca, nhận task và chính sách pin.

Thay đổi này không chứng minh bộ sạc thật. Mode thường ở pin thấp vẫn có thể đi vào luồng về trạm/`CHARGE_START` hiện có; cần nghiệm thu riêng trước khi vận hành không giám sát. Chưa restart Core vận hành hoặc phát lệnh xe thật trong thay đổi source này.

## Bối cảnh

Người dùng muốn thử phần trăm pin trên server trước khi áp dụng ngưỡng 20%/40%. Firmware NCKHAGV đã có `batteryPercentage` trong JSON `agv/{id}/diagnostic`; Core hiện chỉ đọc telemetry pipe cho trạng thái pin dùng trong điều phối.

## Lựa chọn

- Giữ phép tính pin trong `BatteryModule` với cấu hình cầu chia 30 kΩ/7.5 kΩ và dải 6.4–8.4 V.
- Firmware NCKHAGV tiếp tục gửi `?` trong trường pin telemetry điều khiển. Phần trăm chỉ được đọc từ diagnostic và đưa vào `battery_display_percent` cùng `battery_display_available`.
- Dashboard và lịch sử dùng trường hiển thị riêng. Không cập nhật `battery`, `battery_measured` hoặc `needs_recharge` từ diagnostic; do đó chẩn đoán pin không gọi các quyết định pin/thứ tự task.
- Firmware cũ gửi phần trăm trực tiếp trong telemetry tiếp tục dùng luồng điều khiển hiện có.
- Sau quan sát trên xe dao động 65–70%, firmware lấy nhiều mẫu ADC theo từng đợt không chặn vòng lặp, loại mẫu biên và làm mượt phần trăm trước khi gửi diagnostic. Số đo thô của từng đợt vẫn có trong `batteryAdcMv` để đối chiếu.

## Hệ quả

### Bổ sung 15/10/2026 — REQ-BATTERY-04

- Giữ 16 mẫu/đợt khoảng 500 ms, bỏ một mẫu cao nhất/thấp nhất rồi chia trung bình 14 mẫu; lấy mẫu rải theo vòng điều khiển, không dùng vòng chờ hoặc `delay`.
- Phần trăm giữ nguyên khi một trong hai duty motor khác 0. Sau lần motor hoạt động gần nhất, chờ 3 giây rồi chỉ dùng các đợt lấy mẫu trọn vẹn lúc nghỉ. Bộ lọc hiển thị được khởi tạo lại từ mẫu nghỉ để không kéo sụt áp lúc chạy vào phần trăm.
- Cả trung bình đợt hiện tại và giá trị lọc giữa các đợt phải cùng vượt biên 1,5 điểm theo một hướng trong ít nhất 3 đợt liên tiếp. Sau đó chỉ đổi 1 điểm, cách lần đổi trước ít nhất 5 giây. Mốc 0%/100% dùng biên 0,5 điểm để vẫn đạt được hai đầu dải.
- Điện áp `readVoltage()`/`batteryVoltage` vẫn cập nhật kể cả lúc motor chạy, theo bộ lọc điện áp cũ, để chốt điện áp bench tiếp tục hoạt động. `batteryPercentage` dùng bộ lọc nghỉ riêng, vì vậy có thể không khớp nội suy tức thời từ `batteryVoltage` khi xe chạy hoặc đang hồi điện áp.
- Firmware chưa có đầu vào xác nhận sạc thật; cho phép phần trăm tăng hoặc giảm chậm lúc nghỉ. Không suy sạc từ lệnh `CHARGE_START` hoặc tự phát `charging_done`.
- Lúc boot phần trăm chưa có giá trị cho tới đợt nghỉ hợp lệ đầu tiên (khoảng 3,5–4 giây nếu vòng điều khiển chạy đều). Xe chạy liên tục giữ số cũ đến lần nghỉ đủ lâu; đây là chủ ý hiển thị theo yêu cầu, không phải số đo dung lượng đã hiệu chuẩn. Payload, pin điều khiển `?` và ngưỡng điều phối giữ như trước.

Không cần đổi payload telemetry v0 hoặc schema SQLite. Phần trăm từ dải điện áp tuyến tính là ước tính và chưa được hiệu chuẩn trên xe. Khi muốn áp dụng ngưỡng pin cho NCKHAGV, cần một yêu cầu riêng sau khi người dùng xác nhận kết quả hiển thị.

## Chuyển đổi và kiểm thử

Core C++ đã build thành công vào thư mục tạm của Codex. Dashboard JavaScript đã qua kiểm tra cú pháp. Bản firmware hiển thị pin đầu tiên đã build và người dùng báo quan sát trên xe dao động 65–70%; chưa có số đo điện áp đối chiếu. Bản lọc mới `esp32dev_trial` đã build thành công ngày 08/10/2026; cần nạp và xác nhận dao động thực tế. Khi thử, xác nhận dashboard hiển thị phần trăm trong khi `battery_measured=false`, và hiện `Chưa đo` khi diagnostic chưa có giá trị hợp lệ.

## Nguồn

Kiểm chứng bổ sung 15/10/2026: host `tests/battery-host/battery_smoothing.cpp` link module thật PASS nhiễu/dao động ngắn, sụt áp motor/điện áp vẫn sống, thời gian nghỉ, xác nhận xu hướng, giới hạn 1%/5 giây, tăng trở lại, 0%/100% và tràn millis. PlatformIO build `esp32dev_two_robot_bench_01`/`esp32dev_two_robot_bench_02` PASS (RAM 17,5%/17,3%; flash 73,0%/72,5%). Ảnh tại `.pio/build/<env>/firmware.bin` trong project NCKHAGV; firmwareBuild lần lượt `20261015-robot01-battery-smoothing` và `20261015-battery-smoothing`. Chưa nạp hoặc xác minh độ mượt trên xe thật.

- REQ-BATTERY-01 và REQ-BATTERY-02 trong `docs/requirements/register.md`.
- `firmware/NCKHAGV/NCKHAGV/main (1).cpp`, cấu hình pin do người dùng bổ sung.
- Hợp đồng MQTT tại `docs/07-network.md`.
