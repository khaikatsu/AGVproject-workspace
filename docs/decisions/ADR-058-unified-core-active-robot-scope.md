# ADR-058 — Một Core thống nhất với phạm vi xe tham gia

Ngày: 16/10/2026  
Trạng thái: IMPLEMENTED trong mã nguồn, VERIFIED phần mềm; chưa thay Core vận hành hoặc chạy xe thật

## Nguồn và vấn đề

`SOURCE`: người dùng muốn thử lần lượt từng xe trong một Core duy nhất rồi mở dần thành Core thật chạy hai xe. Nhóm đã hoàn thiện server trước khi có xe nên các nhánh test/server và capability firmware phát sinh xung đột.

`OBSERVED`: workspace chỉ có một entrypoint Core nhưng mode `robot01`, `robot02`, `both` áp dụng luật trial khác vận hành thật: bỏ qua pin/sạc, một số diagnostic xe 01 chỉ được đọc ở `robot01`, còn giao diện kho phụ thuộc tên mode. Vì vậy cùng firmware có thể được xử lý khác khi chuyển sang hai xe.

## Quyết định

- Dùng một source và một binary `agv_core.exe`. Menu 1/2/3 chọn **phạm vi xe tham gia** 01, 02 hoặc 01+02; không chọn một biến thể thuật toán.
- Mode mới là `active01`, `active02`, `active-both`. Cả ba dùng luật vận hành thật: pin đo hợp lệ và ngưỡng 20/40, RFID/telemetry mới, ACK, mutex, lỗi và ca làm việc. Scope chỉ lọc xe có thể nhận task/GOTO/NODE_ORDER/CHARGE_START hoặc xin mutex.
- Xe ngoài scope vẫn được quan sát. Nếu đã xuất hiện, xe đó phải IDLE tại trạm riêng, hết task/tuyến/vùng trước khi mở ca; Core không phát chuyển động cho xe đó.
- Chọn scope lúc khởi động không tự phát GOTO hoặc CHARGE_START. Thay scope cần dừng/mở lại Core để đối soát trạng thái, không đổi nóng giữa lúc xe chạy.
- Capability xe 01 (encoder, sonar, servo) được đọc khi 01 nằm trong scope, gồm cả `active-both`; không còn phụ thuộc duy nhất vào tên mode `robot01`. Firmware vẫn tự khai báo capability qua diagnostic đúng phiên.
- `--mode robot01|robot02|both` cũ được giữ tạm để chạy hồi quy/fixture lịch sử. Menu F5 không dùng các ngoại lệ trial này nữa.
- Không đổi topic hoặc payload MQTT. Không cần nạp lại firmware chỉ vì thay scope Core.

## Kiểm chứng và giới hạn

`VERIFIED`: build và toàn bộ unit Core PASS; unit mới kiểm tra lọc direct GOTO, tự phân công, `participating`, encoder và ca scope. Fixture broker/Core/counter/map riêng PASS menu 01/02/01+02, pin thật, xe ngoài scope, mutex hai xe và restart giữ counter. Unit gateway/order/warehouse 19 ca PASS; cú pháp ba tệp JS PASS.

`OPEN`: chưa ghi đè `build/agv_core.exe`, chưa restart Core vận hành và chưa phát lệnh lên xe thật. Chưa kiểm chứng chuyển động, khoảng hở mutex, phanh, servo hoặc sạc vật lý. `CHARGE_START`/bộ sạc cần capability và kiểm thử bàn riêng trước khi dùng luồng kết thúc ca thật.
