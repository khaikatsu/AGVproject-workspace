# ADR-060 — Tạm bỏ chốt pin trong Core, sạc thủ công

Ngày: 16/10/2026. Trạng thái: đã triển khai theo yêu cầu trực tiếp, tạm thời phục vụ test.

## Bối cảnh

SOURCE: nhóm sạc thủ công; muốn bỏ chốt pin bằng `//`, không xóa mã, để thử chức năng. Trước đó nhóm yêu cầu ngừng nhận task khi xuống ≤30%. OBSERVED: Core vẫn còn cấu hình 20/40/90 và có nhánh pin thấp tự chuyển tuyến đi sạc; chỉ bỏ chốt GOTO không đủ để tránh gián đoạn task.

## Quyết định

- Trong `src/core/AGVserver.cpp`, comment `setChargeThresholds(20,40,90)` và dòng `setBatteryTestBypass(false)`; dòng đang chạy là `setBatteryTestBypass(true)`. Các điều kiện pin gốc tại GOTO, phân công, tiếp tục BLOCKED và mở ca được giữ bằng `//` ngay cạnh nhánh có cờ bypass.
- Khi bypass: không đòi số đo hoặc mức % pin để mở ca, nhận task tự động hay GOTO. Pin telemetry/diagnostic vẫn cập nhật, không giả thành 100%.
- `setAutomaticChargingEnabled(false)` vì sạc thủ công: không tự lấy task/tuyến của xe để về sạc theo pin, không gửi CHARGE_START. Kết thúc ca vẫn hoàn tất task rồi về S1/S2 theo cơ chế hiện có, tới trạm đỗ IDLE thay vì tự nhận đang sạc.
- Cờ mặc định của FleetManager giữ hành vi cũ cho các test độc lập; Core main chủ động cấu hình chính sách tạm cho mọi scope. Snapshot thêm `fleet.battery_gate_enabled=false` và `fleet.automatic_charging_enabled=false`; dashboard hiện rõ bypass/sạc thủ công.
- Không bỏ chốt scope, lỗi, mất telemetry/mạng, RFID, ACK, sonar hoặc mutex. Không sửa firmware, PWM, hiệu chuẩn hay bảo vệ điện áp trên MCU.

## Bật lại sau test

Comment dòng `setBatteryTestBypass(true)` và bỏ `//` dòng `setBatteryTestBypass(false)` trong Core main; rebuild và khởi động lại Core sau khi dừng xe an toàn. Không tự bật lại sạc tự động vì nhóm sạc thủ công. Trước khi bật chốt pin phải triển khai/kiểm chứng chính sách ≤30% đã yêu cầu; cấu hình 20/40 cũ được bảo tồn để tham khảo, không phải ngưỡng nhóm đã chọn để vận hành tiếp.

## Kiểm chứng và giới hạn

Unit kiểm tra cả 01/02 ở pin chưa đo, 0/29/30/39%, GOTO, task tự động, không giành tuyến đi sạc, về trạm khi kết thúc ca và khả năng bật lại chốt. Fixture MQTT/Core riêng kiểm tra main thật ở ba scope, pin chưa đo/0%, mutex và không có CHARGE_START. Xem kết quả thực chạy trong [hồ sơ kiểm thử](../../tests/README.md). Không phải nghiệm thu phần cứng, không tự restart Core đang vận hành hoặc nạp xe.

Tạm ghi đè phần ngưỡng pin/sạc tự động trong ADR-021, ADR-058 và tài liệu ca; không thay nguồn số đo pin đã duyệt.
