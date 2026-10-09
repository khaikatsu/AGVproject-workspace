# ADR-003 — RFID tại ngã tư giữa và định danh bản đồ gọn

Ngày: 29/09/2026. Trạng thái: IMPLEMENTED theo yêu cầu trực tiếp sửa bản đồ của người dùng.

## Bối cảnh

Hai đường dọc cắt đường ngang giữa nhưng map mặc định và các map trong `src` chưa có node giao. File `data/map_layout.json` đã có CROSS_1/2 riêng, tạo sai khác giữa các cách chạy. Cửa kho phía trong ở y=50/90, chưa ở giữa các đoạn y=0…70 và y=70…140. ID dài dễ đè lên nhánh và trọng số trên giao diện.

## Quyết định

Thêm M5/M6 loại mutex tại hai giao điểm; tách cả đường ngang và dọc qua chúng. Giữ hướng cạnh đã có. Rút gọn ID thành M1…M6, K1…K6, G1…G6, S1/S2, GS1/GS2, C1…C4. Đưa cửa/kho phía trên về y=35, phía dưới về y=105; cập nhật trọng số theo độ dài tọa độ, giữ tổng chiều dài tuyến.

Đồng bộ ba JSON và map mặc định C++; frontend nhận graph từ core. Tên ngắn vẽ trong node; tooltip/chú giải nêu loại. Không thêm map riêng trong trình duyệt và không đổi định dạng MQTT.

## Hệ quả và kiểm chứng

ID là giá trị trong telemetry/path/target/zone, nên firmware/simulator dùng bảng tên mới. UID RFID và kích thước thật vẫn OPEN. Không chuyển đổi snapshot/history cũ hoặc counter tự động. Kiểm chứng C++, MQTT với hai robot giả lập và browser được ghi trong [bản cập nhật bản đồ](../15-map-layout-update.md); đây là bằng chứng phần mềm, chưa phải kiểm thử tránh va chạm vật lý.
