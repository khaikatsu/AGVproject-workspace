# ADR-055 — Hai HC-SR04 xe 01, dừng tạm và cảnh báo trên cạnh

Ngày 15/10/2026. Trạng thái: đã sửa mã, chờ kiểm thử bàn và xe thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng cung cấp sketch đo HC-SR04 trước GPIO12/13, sau GPIO0/15 và ảnh chân ESP32; yêu cầu Core test 01 cảnh báo phía có vật cản, xe dừng ở ngưỡng 15 cm, web đánh dấu vật cản theo tỉ lệ bản đồ và tô đỏ đoạn liên quan, rồi đi tiếp khi vật cản được dời.

`OBSERVED`: firmware đã có module sonar trước nhưng tắt chốt trong profile chạy thử; GPIO0 đang dùng cho còi, GPIO15 cho servo không bật. Cờ obstacle của telemetry pipe cũ khiến Core khóa node kế và lập lại tuyến, không phù hợp vật cản giữa cạnh. Core test 01 đã nhận encoder vị trí ước tính trên cạnh; map dùng tọa độ cm.

## Quyết định

- Riêng profile 01: sonar trước 12/13, sau 0/15; còi active LOW chuyển GPIO21. Hai servo nâng hạ vẫn chưa cấu hình điều khiển; tín hiệu phần cứng chung ở GPIO17 theo ảnh không được tự coi là hai kênh điều khiển độc lập.
- Kích từng sonar luân phiên cách ít nhất 50 ms; echo timeout 30 ms. Sonar phía di chuyển (trước khi tiến, sau khi lùi) quyết định chốt. Đọc hợp lệ ≤15 cm thì dừng motor, giữ route/command/grant và đóng băng timeout chặng. Khi đọc hợp lệ >20 cm thì tiếp tục tuyến cũ; khoảng 15–20 cm giữ trạng thái dừng. Timeout với ECHO LOW được coi là ngoài tầm đo, đúng cách hiểu `-1 = không có vật` của sketch người dùng; ECHO kẹt HIGH hoặc mẫu cũ dừng theo lỗi cảm biến. ECHO LOW do dây đứt không thể phân biệt bằng HC-SR04, nên phải thử bàn và giám sát ca đầu.
- Xe 01 giữ cờ telemetry pipe bằng 0 để không khóa node; diagnostic JSON thêm hướng và khoảng cách trước/sau. Core test 01 chỉ dùng thông tin này để hiển thị, không thay Dijkstra, task hay mutex.
- Chấm web được chiếu từ vị trí encoder hiện tại thêm khoảng cách sonar dọc cạnh có hướng, giới hạn trong cạnh; cạnh đỏ khi còn báo vật cản. Đây là **ước tính** vì encoder, tầm đo sonar, vị trí gắn cảm biến và quãng phanh chưa được hiệu chuẩn. RFID vẫn là mốc node thật.

## Chốt thử nghiệm

Kiểm thử phần mềm 15/10/2026: native profile01 27/27, profile02 26/26, module sonar host, Core unit và encoder core unit PASS; PlatformIO build ESP32 profile01 PASS; fixture MQTT/Core/gateway/Edge desktop/mobile PASS với cảnh báo trước/sau và map, counter, DB riêng. Đã gom ảnh/Core vào `build/robot01-dual-sonar`; [kiểm thử và cách chạy](../testing-sonar-robot01-2026-10-15.md). Chưa có kiểm chứng cảm biến hoặc quãng phanh thật.

Trước khi cho xe chạy: cấp HC-SR04 đúng nguồn, hạ mức ECHO 5 V xuống mức an toàn 3,3 V cho ESP32 ở cả hai kênh; kiểm tra GPIO0 lúc boot không giữ mức LOW; xác nhận dây còi đã chuyển GPIO21 và GPIO15 không còn nối servo. Thử trên giá kê với vật cản trước/sau, mất echo, dời vật cản, mất mạng và STOP; đo tốc độ/quãng phanh với tải thật. 15 cm là ngưỡng đọc, chưa chứng minh xe dừng cách vật cản 15 cm. Chưa nạp firmware hoặc kiểm chứng phần cứng trong thay đổi mã này.
