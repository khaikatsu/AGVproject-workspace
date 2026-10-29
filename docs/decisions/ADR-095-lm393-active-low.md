# ADR-095 — LM393 Active-LOW cho cả hai xe

Ngày30/10/2026. SOURCE: ảnh người dùng chỉ rõ `CARGO_IR_ACTIVE_STATE=0`, LOW/0=có hàng[2LED], HIGH/1=trống[1LED]; người dùng yêu cầu sửa theo ảnh. Quyết định này thay cực tính Active-High trong ADR-091/093 và các mô tả lịch sử trước30/10, giữ nguyên luồng hạ/lùi ADR-093 và phục hồi ADR-094.

## Quyết định và tác động

- Cấu hình chung firmware01/02 dùng `CARGO_IR_ACTIVE_STATE=0`; GPIO2, INPUT_PULLUP và cửa sổ ổn định150ms giữ nguyên.
- CargoModule so sánh raw với active state: LOW→cargoPresent=true, HIGH→false. Khi chưa ổn định, diagnostic vẫn cargoPresent=null/cargoStable=false. cargoActiveLevel tự phát0 theo cấu hình.
- Core/gateway đọc cargoPresent/cargo_sensor.present boolean, không đảo lần nữa. Trước/sau nâng cần có hàng; sau hạ vẫn có thể LOW khi xe còn dưới kệ; chỉ ra đúng gate và HIGH/không hàng ổn định mới DONE_DOWN/ghi kho. Không thay góc servo, thời gian, task, mutex hay chốt phục hồi.
- BuildID source01 `20261030-robot01-lm393-active-low-v27`,02 `20261030-robot02-lm393-active-low-v27`. Không đổi cấu hình mạng, hiệu chuẩn pin hoặc profile riêng.

## Kiểm tra và giới hạn

OBSERVED qua đọc source: CargoModule begin/update cùng dùng `rawLevel == Config::CARGO_IR_ACTIVE_STATE`; diagnostic giữ boolean; Core/gateway không dùng raw để quyết định có hàng. Rà diff đồng bộ cho hai nhánh buildID và ví dụ hợp đồng. Không tạo/chạy test firmware hoặc build/nạp theo quy trình27/10. OPEN: người dùng nạp v27 và thử có/không kệ trên cảhai xe. Chưa thay dịch vụ/DB/counter vận hành; kết quả kiểm tra server của v26 không phải kiểm chứng mức điện LM393 trên xe.
