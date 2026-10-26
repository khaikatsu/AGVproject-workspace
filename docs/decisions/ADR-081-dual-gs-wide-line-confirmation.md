# ADR-081 — Xác nhận line rộng 111 khi quay docking GS1/GS2

Ngày 24/10/2026. Trạng thái: IMPLEMENTED trong source và kiểm thử phần mềm; chưa nạp/thử xe thật.

## Nguồn yêu cầu

`SOURCE`: người dùng đo line rộng 4,5 cm, khoảng cách giữa hai mắt kề nhau 1,5 cm, cụm mắt line cách trục bánh 4,3 cm và đoạn GS1→S1/GS2→S2 dài 25 cm. Người dùng sẽ làm dày phần line nối dài tại GS1/GS2 và yêu cầu khi quay về trạm, xe tiếp tục quay đến khi cả ba mắt đọc `111` rồi mới dừng/xác nhận để lùi.

Yêu cầu này thay quy tắc docking `010` của [ADR-080](ADR-080-dual-gs-center-confirmation.md). PWM/trim vẫn là tuning riêng của từng xe và không bị ép về một giá trị chung.

## Quyết định

- Chỉ áp quy tắc mới cho chặng có `reverse_after_turn` tại GS1→S1 hoặc GS2→S2. Rẽ thường, phục hồi line và bám line trên các chặng khác giữ quy tắc hiện có.
- Khi quay trái/phải docking, firmware tiếp tục pivot qua `011/010/110`; các mẫu này không chuyển sang lùi. Chỉ `111` mới chốt STOP và mở cửa sổ xác nhận.
- `111` phải liên tục ít nhất 100 ms, có ít nhất ba mẫu mới, khoảng mẫu tối thiểu 5 ms và gap tối đa 50 ms. Mẫu khác làm lại bằng chứng. Motor STOP trong cửa sổ xác nhận.
- Khi quay 180° docking, các nhánh trung gian vẫn được đếm bằng line hẹp như trước; nhánh cuối chỉ chốt tại `111`. Cửa sổ xác nhận tại chỗ giữ riêng xe01=500 ms, xe02=100 ms và vẫn nằm trong deadline quay 2500 ms.
- Sau xác nhận, firmware trả STOP một vòng, chuyển từ sonar trước sang sonar sau, xóa mẫu cũ và chỉ cấp PWM lùi sau khi có mẫu sonar sau mới hợp lệ. RFID S1/S2 thật mới hoàn tất đỗ.
- Giữ giới hạn quét/capture 600/1500 ms, grant, STOP, watchdog, vật cản, lỗi cảm biến, ID/mạng/NVS/counter và giao thức MQTT hiện có.
- Ghi nhận hình học vào cấu hình: `LINE_WIDTH_CM=4.5`, `LINE_SENSOR_SPACING_CM=1.5`, `FRONT_LINE_SENSOR_TO_AXLE_CM=4.3`. Các hằng này không tự chứng minh mức GPIO; nghiệm thu thật vẫn phải đối chiếu diagnostic `linePattern`.
- BuildID mới: `20261024-robot01-gs-wide-v18` và `20261024-robot02-gs-wide-v18`.

## Kiểm chứng và giới hạn

`scripts/test-docking.ps1` PASS cả hai profile, hai trạm và các hướng quay trái/phải/180°: `010/011/110` không chốt docking, `111` thoáng qua/nhiễu/gap/timestamp trùng không cấp lùi, `111` ổn định mới chuyển sonar sau và lùi; STOP/mạng/sonar/vật cản/timeout/RFID giữ chốt. `scripts/test-core-protocol.ps1` PASS 51/51 mỗi profile; `scripts/test-line-confirm.ps1` và `scripts/test-turn180.ps1` PASS cả hai profile, chứng minh rẽ/phục hồi thường không đổi.

Build ESP32 hai env PASS: RAM58204 byte mỗi ảnh; flash01=968461 byte, flash02=968441 byte. Binary01 dài975040 byte/SHA256 `3326841BE5E22563A647A839E021358E1CA37E8DA7C094225CB145A4BC12F64E`; binary02 dài975024 byte/SHA256 `21E6455E5D03B7088AA9529012B66CE776F4E375EF1A8A94318EA9A4D36CCCD6`; đã tìm thấy đúng buildID v18 trong từng ảnh.

`OPEN`: chưa nạp và chưa quan sát `111` trên line được làm dày. Trước chạy sàn phải kiểm ba GPIO line, chiều pivot, STOP tại `111`, quán tính sau STOP và sonar sau trên từng xe; kết quả host/build không chứng minh hình học hoặc độ bám thật.

## Tham chiếu

- REQ-DUAL-GS-WIDE-LINE-01.
- [ADR-056](ADR-056-robot01-reverse-home-request.md), [ADR-059](ADR-059-robot02-firmware-parity.md), [ADR-077](ADR-077-turn180-fast-stop-confirm.md), [ADR-080](ADR-080-dual-gs-center-confirmation.md).
