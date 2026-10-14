# ADR-038 — Mũi tên hướng đầu xe và hướng đi trong mô phỏng

Ngày 10/10/2026. Trạng thái: triển khai theo REQ-SIM-09.

`SOURCE`: người dùng muốn mũi tên thể hiện hướng đi và hướng xe khi ra/vào trạm, lấy hàng và dỡ hàng trên web mô phỏng.

`OBSERVED`: lớp xe nội suy chỉ vẽ hình vuông và hàng, chưa có hướng; mũi tên của dashboard gốc là vị trí RFID/diagnostic, không thể dùng như hướng của xe ảo đang giữa cạnh. Quy tắc đã chốt ở ADR-033: kho tiến vào/lùi ra, trạm tiến vào rồi quay đầu ra cửa, xuất trạm bằng đi tiến.

## Quyết định

- Mũi tên trắng trong thân là đầu xe; mũi tên bên ngoài cùng màu xe là hướng chuyển động của cạnh đang đi. Xe dừng vẫn giữ hướng đầu, không vẽ mũi tên chuyển động khi không còn cạnh.
- Tính góc theo tọa độ map và cửa duy nhất của bay, không theo tọa độ pixel. K1–K3 đầu hướng W, K4–K6 hướng E khi đứng xử lý hàng; lùi ra giữ hướng đầu và hướng đi ngược lại. S1 đầu hướng S, S2 hướng N khi đỗ/đi ra; lúc tiến vào trạm đầu hướng theo cạnh vào.
- Xe ảo giữ `headingDeg` trong `/sim-api/state`/report để giữ hướng ở node thường sau khi hết tuyến. Đây là dữ liệu mô hình, không phát diagnostic hướng giả hoặc sửa MQTT/firmware/Core. Trang tương thích phiên cũ thiếu trường này bằng hình học bay/cạnh và hướng hiển thị trước đó.
- Xoay biểu tượng trong 650 ms khi đổi hướng; lúc vừa đến S hiện **Quay đầu** rồi đầu hướng ra cửa. Đây là thời gian hoạt ảnh hiển thị, không thêm thời gian quay/phanh vào điều phối S1, không khẳng định đã mô phỏng motor hay line.
- Nhãn **Tiến vào kho**, **Lùi ra kho**, **Tiến vào trạm**, **Tiến ra trạm** ở cạnh nhánh. Khi xe đứng tại kho trong `pickup_service`/`dropoff_service`, nhãn **Bốc hàng**/**Dỡ hàng** theo đơn gateway hoặc job kiểm thử tương thích. Cạnh đang đi ưu tiên hơn snapshot thao tác hàng cũ. Giữ biểu tượng hàng vàng.
- Chỉ vẽ một biểu tượng xe nội suy trên map; vị trí RFID Core vẫn ở bảng đội xe. Chú giải rõ hướng/vị trí là mô phỏng.

## Kiểm chứng và giới hạn

`tests/simulation.direction.test.js` PASS: sáu kho tiến vào/lùi ra, hai trạm tiến vào/quay đầu/tiến ra, bốc/dỡ và lưu hướng, cạnh đang đi ưu tiên trạng thái service cũ. Robot contract unit PASS. `tests/simulation.direction.browser.js` dùng Core/broker/DB riêng kiểm tra đầu S1/S2, giao đơn K1→K2, mũi tên ngược nhau lúc lùi, nhãn bốc/dỡ/lùi ra kho, quay đầu khi về trạm và mobile; PASS cuối tại `.qa/simulation-direction/run-e0rDz7`. Tab đang xem đã tải lại, xác nhận một mũi tên đầu mỗi xe, 01=90° và 02=270° tại trạm.

`OPEN`: chưa mô phỏng quỹ đạo xoay, sai số cảm biến, kích thước hoặc quãng phanh của xe thật. Heading mô phỏng không dùng làm bằng chứng hướng robot thật hay hoàn thành chuyển động.
