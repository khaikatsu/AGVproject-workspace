# Kiểm tra đường truyền server–xe 02 khi dừng tại GS2

Ngày 10/10/2026, giờ Asia/Saigon. SOURCE: xe nhận tuyến đến GS2 nhưng không quẹo, web báo mất kết nối; người dùng xác nhận không tắt/bật nguồn hoặc EN/RESET sau lần dừng vừa rồi.

## Quan sát trực tiếp

- Hotspot đã hoạt động lại: máy có `192.168.137.1` và Wi-Fi LAN `192.168.1.10`. Bất khớp IP thấy lúc 23:58 ngày trước không còn ở lần đọc này.
- Một Core, một broker Mosquitto. Broker đang chạy dịch vụ, listener `0.0.0.0:1883`; có socket từ `192.168.137.70` tới broker hotspot. Firewall rule hotspot TCP 1883 bật và giới hạn đúng subnet.
- Ping IP xe 4/4 thành công, RTT 29/8/19/25 ms. ICMP và MQTT được đo lúc motor dừng, không chứng minh chất lượng kết nối trong lúc pivot.
- Trace MQTT chỉ subscribe trong 30 giây: 30 telemetry, 29 diagnostic và 76 heartbeat Core. Khoảng cách lớn nhất quan sát: telemetry 1046 ms, diagnostic 1523 ms, heartbeat 1555 ms. Không thấy LWT hoặc uptime giảm trong cửa sổ trace.
- Diagnostic hiện tại: ID 02, bench/motion enabled, Wi-Fi/MQTT true, session 1, IDLE/NONE/STOP, PWM 0/0; RFID UNKNOWN, rfidAgeMs null, không task hoặc tuyến. Đây là MCU đã nối lại nhưng chưa xác nhận vị trí, không phải đã hoàn thành M3.
- Uptime lúc 00:07:27 khoảng 33706 ms, suy boot gần 00:06:54; log Core LWT mới nhất 00:06:52. Reset reason mới nhất POWERON/code 1. Những lần trước có PANIC nhưng không dùng reason của boot cũ cho lần hiện tại.
- Core phiên hiện tại còn MQTT khỏe, xe online nhưng position_valid=false; node GS2 chỉ là RFID cũ, ERROR giữ uncertain M3. M3 grant 49178/ACK/owner02 vẫn giữ do chưa có chứng cứ xe rời vùng.

## Kết luận và giới hạn

`OBSERVED`: đường truyền hiện hoạt động lúc motor dừng; đã có một lần bo khởi động lại gần thời điểm LWT, dù người dùng không thao tác reset. Chưa có bằng chứng server chặn riêng node GS2 hoặc mất heartbeat vượt 3 giây trong lần đo này.

`INFERENCE`: reboot làm mất tuyến/hướng/RFID của firmware, nên xe không tiếp tục quẹo và Core giữ lỗi. POWERON là sự kiện cấp nguồn theo ESP-IDF, nhưng tự nó không đo được điện áp hoặc xác định linh kiện gây reset. ESP32 lấy 5V từ L298N theo thông tin người dùng trước đó; ưu tiên kiểm chứng nguồn/dây/điều kiện đổi chiều motor. Không kết luận brownout hoặc sửa PWM/timeout theo suy đoán.

`OPEN`: cần trace Serial/nguồn khi motor đổi chiều để xác định nguyên nhân reset. Phép thử nguồn tách biệt phải làm khi xe đã dừng và tắt nguồn: tháo dây 5V L298N cấp ESP32, giữ GND chung, cấp ESP32 bằng một nguồn USB riêng; driver/motor giữ nguồn riêng hiện có. Người vận hành thử trên giá kê trước. Không cấp song song USB và 5V driver, không tự đổi jumper của module chưa có sơ đồ. Mở Serial trước lần thử, đối chiếu BOOT/ROUTE/MOTOR/Wi-Fi/MQTT và panic/backtrace nếu xuất hiện. Chưa thực hiện phép thử nguồn hoặc điều khiển xe trong lượt rà soát.

## Bằng chứng

Trace: `.qa/robot02-network-yrFF41/events.json` và `report.json`. Thu bằng `.qa/observe-robot02.js`, không publish command, không mở USB hoặc reset bo. Không thay broker/firewall, executable Core, firmware, counter hoặc dữ liệu vận hành trong lượt này.

Nguồn chính thức đã đối chiếu 10/10/2026: [ESP-IDF 4.4.8 — reset reasons](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/system.html#_CPPv416esp_reset_reason_t). Bảng ADC điện áp pin trong diagnostic không thay thế phép đo rail 5V/3,3V của ESP32.
