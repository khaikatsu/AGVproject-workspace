# ADR-021 — Hiển thị pin tách khỏi ngưỡng điều phối

Ngày 08/10/2026. Trạng thái: đã chốt theo REQ-BATTERY-02.

## Bối cảnh

Người dùng muốn thử phần trăm pin trên server trước khi áp dụng ngưỡng 20%/40%. Firmware NCKHAGV đã có `batteryPercentage` trong JSON `agv/{id}/diagnostic`; Core hiện chỉ đọc telemetry pipe cho trạng thái pin dùng trong điều phối.

## Lựa chọn

- Giữ phép tính pin trong `BatteryModule` với cấu hình cầu chia 30 kΩ/7.5 kΩ và dải 6.4–8.4 V.
- Firmware NCKHAGV tiếp tục gửi `?` trong trường pin telemetry điều khiển. Phần trăm chỉ được đọc từ diagnostic và đưa vào `battery_display_percent` cùng `battery_display_available`.
- Dashboard và lịch sử dùng trường hiển thị riêng. Không cập nhật `battery`, `battery_measured` hoặc `needs_recharge` từ diagnostic; do đó chẩn đoán pin không gọi các quyết định pin/thứ tự task.
- Firmware cũ gửi phần trăm trực tiếp trong telemetry tiếp tục dùng luồng điều khiển hiện có.
- Sau quan sát trên xe dao động 65–70%, firmware lấy nhiều mẫu ADC theo từng đợt không chặn vòng lặp, loại mẫu biên và làm mượt phần trăm trước khi gửi diagnostic. Số đo thô của từng đợt vẫn có trong `batteryAdcMv` để đối chiếu.

## Hệ quả

Không cần đổi payload telemetry v0 hoặc schema SQLite. Phần trăm từ dải điện áp tuyến tính là ước tính và chưa được hiệu chuẩn trên xe. Khi muốn áp dụng ngưỡng pin cho NCKHAGV, cần một yêu cầu riêng sau khi người dùng xác nhận kết quả hiển thị.

## Chuyển đổi và kiểm thử

Core C++ đã build thành công vào thư mục tạm của Codex. Dashboard JavaScript đã qua kiểm tra cú pháp. Bản firmware hiển thị pin đầu tiên đã build và người dùng báo quan sát trên xe dao động 65–70%; chưa có số đo điện áp đối chiếu. Bản lọc mới `esp32dev_trial` đã build thành công ngày 08/10/2026; cần nạp và xác nhận dao động thực tế. Khi thử, xác nhận dashboard hiển thị phần trăm trong khi `battery_measured=false`, và hiện `Chưa đo` khi diagnostic chưa có giá trị hợp lệ.

## Nguồn

- REQ-BATTERY-01 và REQ-BATTERY-02 trong `docs/requirements/register.md`.
- `firmware/NCKHAGV/NCKHAGV/main (1).cpp`, cấu hình pin do người dùng bổ sung.
- Hợp đồng MQTT tại `docs/07-network.md`.
