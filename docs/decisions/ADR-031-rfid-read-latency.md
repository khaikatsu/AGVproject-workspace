# ADR-031 — Giảm trễ quét và dừng tại RFID

Cập nhật sau yêu cầu khôi phục 10/10/2026: source đã trở về RFID20ms và binary xe01 đã nạp lại trùng hoàn toàn bản hồi sáng; xem [ADR-041](ADR-041-restore-morning-robot01-firmware.md). Phần tăng nhịp10ms bên dưới giữ lịch sử, đã được thay thế cho source hiện tại.

## Bổ sung theo yêu cầu tăng nhịp đọc — 10/10/2026

`SOURCE`: người dùng xác nhận xe chạy khá ổn và yêu cầu RC522 đọc nhanh hơn. `IMPLEMENTED`: giảm `RFID_POLL_INTERVAL_MS` trong NCKHAGV từ 20 xuống 10 ms; ảnh mới có `firmwareBuild=20261010-rfid-poll-10ms`. Đây là cấu hình chung, áp dụng cho profile 01/02 khi build và nạp ảnh tương ứng. Giữ xử lý dừng trước HALT, một WUPA mỗi poll, UID, đọc lặp không tiến tuyến, giới hạn log một lần/giây và các thông số motor/mạng/mutex hiện có. Sketch AGV_Connection_Test giữ cấu hình riêng.

10 ms là khoảng cách tối thiểu giữa các lần bắt đầu gọi đọc, tương ứng trần danh định 100 lần/giây. Không cam kết tốc độ UID thực tế gấp đôi: thời gian SPI, phản hồi/timeout RC522 và các tác vụ trong vòng điều khiển vẫn ảnh hưởng. Không tăng tầm đọc RF hoặc giải quyết thẻ phụ nằm gần GS1.

`VERIFIED`: test adapter dùng RFIDModule thật PASS: chưa đủ 10 ms không poll; đủ 10 ms đọc thẻ GS1 xuất hiện ngắn trước HALT; 50 lần đọc thẻ đứng yên không tiến node lặp; miss một WUPA, UID lạ và collision. Bảng route khớp map. Build ESP32 `esp32dev_two_robot_bench_01` PASS (RAM 55996, Flash 948029 byte). `OPEN`: nạp ảnh mới và đo thực tế quét/dừng trên xe; chưa tự nạp hoặc phát lệnh chuyển động.

Các mục dưới đây giữ hồ sơ quyết định 20 ms trước yêu cầu mới.

Ngày 10/10/2026. Trạng thái: IMPLEMENTED; kiểm chứng phần mềm, chưa nạp/đo trên xe.

## Nguồn và hiện trạng

`SOURCE`: người dùng báo xe chạy lố GS2 nên không quét được, hỏi khôi phục thông số quét như lần test xe đầu tiên.

`OBSERVED`: sketch `firmware/AGV_Connection_Test/AGV_Connection_Test.ino` poll 20 ms và xử lý node/dừng trước HALT. NCKHAGV poll 30 ms, kết thúc HALT và log Serial trước khi trả sự kiện cho controller. Khi không thấy thẻ, chuỗi REQA rồi WUPA có thể chờ hai timeout SPI. PWM xe 01 vừa tăng từ 230 lên 245; chưa có đo tốc độ hoặc cửa sổ đọc thực tế tại GS2.

## Quyết định

Đặt `RFID_POLL_INTERVAL_MS=20` như sketch test đầu. Dùng một WUPA cho cả thẻ mới và thẻ đã HALT, nhận OK/COLLISION rồi chọn UID; giữ khả năng đọc lại thẻ đứng yên phục vụ tuổi RFID. Trả UID/node cho controller trước khi HALT/log; controller dừng tại node đến/sai thẻ rồi gọi `finishRead()`. Giới hạn log thẻ đứng yên tối đa một lần/giây. Giữ PWM/profile, UID, map, clearance, mutex, mất line/mạng và timeout chuyển động.

20 ms là khoảng cách tối thiểu giữa lần bắt đầu poll, không cam kết một UID được đọc xong trong 20 ms: SPI và timeout RC522 vẫn chiếm thời gian. Đây là giảm trễ phần mềm, không tăng phạm vi RF và không thay điểm lắp anten hay quán tính cơ khí.

## Kiểm chứng và giới hạn

`VERIFIED`: host test biên dịch RFIDModule thật với stub MFRC522, kiểm tra GS2 xuất hiện ngắn được trả trước HALT, caller dừng trước cleanup, poll 20 ms, không thực hiện hai presence request khi miss, thẻ đứng yên được đọc lại, node kế tiếp/UID lạ và collision. Build ESP32 `esp32dev_two_robot_bench_01` PASS. Cách chạy tại [tests/README.md](../../tests/README.md).

`OPEN`: người dùng nạp bản mới rồi đo trên xe tại GS2; chưa chứng minh hết chạy lố hoặc đọc được mọi lần. Thay đổi nằm ở module chung, áp dụng cả profile 01/02 khi build/nạp tương ứng; không đổi riêng PWM xe 02.
