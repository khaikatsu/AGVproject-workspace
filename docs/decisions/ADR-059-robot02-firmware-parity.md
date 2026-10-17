# ADR-059 — Xe02 dùng cấu hình chung xe01, giữ PWM riêng

Ngày: 16/10/2026. Trạng thái: IMPLEMENTED; nghiệm thu phần cứng OPEN.

Bổ sung mới nhất cùng ngày, REQ-ROBOT02-PWM-230-ASYM-01: giảm mức cơ sở tiến/lùi/chỉnh line/chỉnh lùi của xe02 từ245 xuống230 và đổi trim chạy thành trái−35/phải+15, cho duty thực tế195/245. Pivot giữ245/245 với trim xoay0/0. BuildID02 `20261016-robot02-pwm230-l195-r245`; xe01, capability và mọi chốt an toàn không đổi. Cấu hình PWM245 toàn bộ ngay bên dưới là lịch sử. Chưa upload hoặc nghiệm thu chuyển động thật.

Bổ sung trước đó cùng ngày, REQ-ROBOT02-PWM-245-01: theo yêu cầu tăng PWM, xe02 đổi các mức tiến/lùi/chỉnh/xoay230→245, giữ trim−40/0 và trim xoay0/0. Tiến/lùi thực đặt205/245, pivot245/245; xe01 không đổi. BuildID02 `20261016-robot02-pwm245`. Các mức230 và buildID hardware-parity bên dưới ghi lại quyết định trước lần tăng này; phần cứng/capability và chốt an toàn vẫn giữ nguyên. Chưa upload hoặc nghiệm thu chuyển động thật.

## SOURCE

Người dùng xác nhận xe02 đã hoàn thiện giống xe01, giữ PWM của xe02, các cấu hình xe và thông số khác giống xe01. Đây là yêu cầu mở rộng các tính năng encoder, hai sonar, còi, servo và lùi về trạm sang xe02. Không lấy binary mang ID01 để nạp xe02.

## Quyết định và triển khai

- `profiles/SharedRobotConfig.h` chứa capability và hiệu chuẩn chung; hai profile có ID và PWM riêng. Xe02 giữ tiến/lùi/chỉnh line230, trim trái−40/phải0, pivot230 và trim pivot0/0. Xe01 giữ nguyên tiến220/lùi190/pivot210.
- Cùng ESP32/L298N/pinout: encoder16/4 với161xung/100cm; sonar trước12/13, sau0/15; còi21 activeLOW; servo17 một kênh hạ0°/nâng45° danh định,50Hz/1000–2000µs và5000ms. Ngưỡng sonar15/20cm, echo timeout60ms, mất line dừng ngay/chờ xác nhận5s, timeout chặng15s/xoay2500ms như xe01.
- Pin ADC36, scale5/offset0 và6,4–8,4V dùng chung theo xác nhận người dùng; không xem đây là số đo hiệu chuẩn độc lập trên xe02. Các mạng riêng vẫn ưu tiên nếu có; workspace hiện cả hai dùng file LocalNetworkConfig.h chung.
- ID/topic vẫn02, trạm riêng vẫn S2/GS2. Khi về S2, đầu xe hướng ra GS2 (N), đuôi vào S2, xoay ở GS2 rồi lùi và dừng tại RFID S2; chờ diagnostic stationReady để hoàn thành. Cần line dẫn hướng tương ứng như cấu hình xe01 người dùng đã xác nhận giống nhau.
- Core/odometry bỏ khóa ID01: mỗi xe có boot/session/seq/route và servo/cargo độc lập. Loại mẫu có robot_id khác topic, dù boot/seq trùng. ACK không hoàn tất servo hoặc task, encoder không nhả mutex.
- Scope riêng02 có cùng nút Lấy/Hạ như01, đi kho→chu kỳ servo→lùi gate. Dashboard hiện tốc độ cả hai và cảnh báo/kiện đúng ID. Điều phối đơn hàng hai xe vẫn là luồng riêng hiện có; không tự đồng nhất timer mô phỏng đơn với kết quả servo vật lý.

## Kiểm chứng và áp dụng

Xem [hồ sơ kiểm thử](../../tests/README.md). Ảnh firmware dùng env `esp32dev_two_robot_bench_02`; build ID mới nhất là `20261016-robot02-pwm230-l195-r245`. Chưa upload hoặc thử motor/servo thực; Core mới build riêng trong `.qa/robot02-parity-build`, chưa thay Core vận hành. Khi nạp phải xác minh đúng cổng/bo xe02. Góc servo, quãng phanh, hệ số encoder và độ ổn định nguồn cần đối chiếu trên xe.

Thay phần giới hạn xe02 không encoder/sonar sau/servo của ADR-050, ADR-055, ADR-057 và bổ sung lùi S2 cho ADR-056. Không thay điều kiện mutex, chống replay và watchdog.
