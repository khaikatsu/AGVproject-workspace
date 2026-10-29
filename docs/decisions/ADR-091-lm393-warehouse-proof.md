# ADR-091 — LM393 xác nhận lấy/hạ hàng

30/10/2026: [ADR-095](ADR-095-lm393-active-low.md) thay riêng cực tính theo ảnh người dùng: LOW/raw0=có hàng, HIGH/raw1=trống cho cả01/02 v27. Các quy ước HIGH=có hàng bên dưới là lịch sử. Giữ proof boolean và luồng hạ→lùi→xác nhận tại gate của ADR-093; chưa build/test/nạp v27.

29/10/2026: [ADR-093](ADR-093-dropoff-exit-cargo-proof.md) thay điều kiện sau hạ và bố trí cảm biến bên dưới: người dùng xác nhận xe còn nằm dưới kệ sau hạ nên LM393 vẫn HIGH; hạ đủ5s → LOWERED → lùi đúng gate → LOW ổn định mới DONE_DOWN/ghi giao hàng. Source v25 giữ HIGH/raw1=có hàng cho01/02; các ghi nhận LOW của lần đồng bộ v23 là lịch sử. Chưa build/test/nạp bản sửa.

Ngày 28/10/2026. SOURCE: người dùng duyệt triển khai xe tới đúng RFID kho, dừng, kiểm tra LM393; có hàng mới nâng, không hàng cập nhật kho trống. Người giám sát tiếp tục nhập Trống/Có kệ + mã kệ đầu ca. Người dùng xác nhận lắp cả hai xe và giữ polarity firmware: GPIO2, HIGH=có hàng, ổn định150ms.

## Quyết định

- Đồng bộ remote 28/10/2026 theo REQ-FIRMWARE-PULL-KEEP-CONFIG-20261028: source v23 ghép firmware từ `origin/main` commit `061b1d6ba995cbfd1d776462b081a1af38e05364`, nhận LM393 `INPUT_PULLUP`, LOW=có hàng cho cả01/02. Quy ước INPUT/HIGH trong v21/v22 bên dưới là lịch sử; mức điện thực và khả năng đọc đúng hàng vẫn chờ người dùng nạp/thử. Giữ diagnostic raw và chốt lift đang sửa tại máy, debounce150ms; config riêng01/02, mạng và hiệu chuẩn pin giữ nguyên hash. Remote cũng bổ sung bù PWM theo mức pin và cứu line quay180° trong logic chung; không sửa giá trị tuning hiện có trong hai profile. BuildID `20261028-robot01-remote-sync-v23` / `20261028-robot02-remote-sync-v23`; chưa build/test/nạp.

- Bổ sung REQ-LM393-RAW-DIAGNOSTIC-20261028: người dùng xác nhận xe02 nối GPIO2. Source v22 cho cả01/02 ghi mức điện digitalRead vào cargoRawLevel (0/1, trước lấy mẫu -1), diagnostic thêm cargoActiveLevel/cargoPin để phân biệt quy ước có hàng với mức thực. Serial [CARGO] phát theo nhịp telemetry khi motor STOP/PWM0/0; không in lúc đang chạy/quay. Không thay INPUT/polarity/debounce/chốt lift, không coi OUT sáng là HIGH đã đo. Core cũ bỏ qua trường phụ, cargoPresent vẫn là trạng thái dùng để điều khiển. Chỉ rà source/diff; chưa build/test/nạp v22 theo AGENTS.md.

- Giữ telemetry pipe, command/ACK/mutex. Diagnostic schema1 bổ sung `cargoSensorSupported`, `cargoSampleSeq`, `cargoSampleAgeMs`, `cargoStableMs`; `cargoStable=false` cho `cargoPresent=null`. Hai profile cùng chức năng, giữ cấu hình riêng/mạng/tuning. BuildID `20261028-robot01-lm393-v21` / `20261028-robot02-lm393-v21`.
- Core bind cùng Core session/boot/connection với diagnostic hiện có; kiểm kiểu/range/seq/uptime tăng, mẫu tuổi≤500ms, ổn định≥150ms; hết1500ms không có quan sát mới thì unknown. Snapshot `cargo_sensor` tách khỏi cargo ảo. Chỉ STOP/duty0/0, IDLE, hết task, RFID đã đọc trong phiên và đúng node mới là proof trước/sau nâng.
- Cả01/02 bắt buộc firmware sensor mới cho `warehouse_lift`; firmware cũ vẫn hiển thị/điều hướng nhưng lệnh nâng/hạ bị chặn. Gateway chỉ dùng nhánh tương thích khi Core cũ không công bố `cargo_sensor.required`; Core mới luôn bắt buộc. Cần cập nhật Core, gateway và nạp hai profile tương ứng.
- Trước nâng/hạ, Core yêu cầu có hàng ổn định. Firmware kiểm lại khi nhận lệnh để đóng khoảng trống mạng; không đạt thì không phát PWM. Servo giữ chu kỳ5000ms, thêm tối đa2000ms chờ kết quả cảm biến: nâng→có hàng, hạ→không hàng. Hết hạn chốt lỗi/INTERRUPTED, không replay. ACK không hoàn tất hành động.
- Sau DONE đúng cmd/node/phiên và cảm biến mới đúng trạng thái, Core mới xác nhận cargo; gateway mới cập nhật sổ kho rồi phát chặng tiếp. Mất hàng ổn định khi đang chở→local STOP/CARGO_LOST; nhiễu liên tục≥750ms→STOP/CARGO_SENSOR_UNSTABLE. Không tự nhả mutex, đổi node hoặc tự chạy tiếp khi cảm biến phục hồi.
- Nếu trước nâng xác nhận không hàng tại đúng RFID, transaction đặt ô nguồn trống, giữ mã kệ ở vị trí `unknown`/`needs_attention`, kết thúc reservation của lượt thiếu, lưu event `missing` và gián đoạn đơn/action. Không xóa mã kệ/lịch sử đầu ca hoặc tự cộng lại kệ khi đóng đơn/hoàn tác. Người vận hành đối soát khi ca đóng. Hai xe vẫn giữ độc quyền reservation như trước.
- OPEN “bỏ trạng thái tin cậy” trong ADR-090 vẫn giữ. Các chốt vị trí, lỗi, phiên, counter và mutex không bị bỏ. Luồng Core `LIFT_UP/DOWN` được nối cảm biến thật; không thay máy TASK/START lịch sử hoặc bỏ gate an toàn chưa xác định.

## Giới hạn vật lý và nghiệm thu

DO chỉ cho biết có vật trong vùng nhìn, không đo trọng lượng hoặc ID kệ; không phân biệt chắc chắn đứt dây/kẹt mức với hàng thật. Mã kệ do sổ đầu ca + reservation cung cấp. Cần bố trí để nhìn thấy kệ trước nâng và khi đang chở, đồng thời không còn thấy kệ sau hạ khi xe vẫn tại RFID. Nếu thực tế sau hạ vẫn nhìn thấy kệ, luồng giữ dừng/đối soát; không bỏ qua kiểm tra để cho lùi.

GPIO2 là chân strapping; điện áp DO, boot có/không hàng, chống nhiễu servo/motor và vị trí nhìn cần người dùng kiểm tra trên cảhai xe. Không lấy LM393 làm bằng chứng nhả vùng giao nhau.

Theo quy trình firmware trong AGENTS.md, không tạo/chạy test MCU, build hoặc nạp firmware; người dùng tự nạp/thử. Kiểm chứng server/web độc lập và trạng thái build ghi trong tests/README.md. Không chạy dịch vụ hoặc sửa DB/counter vận hành trong lượt triển khai.
