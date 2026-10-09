# ESP32 + RC522 test giao tiếp với AGV server

**Cập nhật 01/10/2026:** build Core và nạp sketch mới cho cả hai xe cùng lúc: mutex_grant/mutex_ack/mutex_release dùng JSON `{zone, grant_id}`. Sketch xin lại khi chưa nhận quyền và ACK lại grant trùng; không nhận bare-text grant cũ. Có LWT/status và heartbeat Core; mất liên lạc quá 3 giây hoặc đổi phiên Core latch dừng logic, quét RFID không tự chạy lại. cmd_id cao nhất lưu NVS trước ACK. Đọc [cấu hình TLS/user/password/ACL và giới hạn failsafe](../../docs/18-hardening.md).

Biên dịch ESP32 thật ngày 01/10 với board package **3.3.12**; chưa upload hoặc đo phanh trên board. TLS mặc định tắt cho lab: khi chuyển broker TLS phải bật MQTT_USE_TLS, port8883, user bằng ROBOT_ID, mật khẩu riêng và CA_PEM. Đồng hồ NTP phải hợp lệ; không tắt xác minh chứng chỉ.

Một sketch: [AGV_Connection_Test.ino](AGV_Connection_Test.ino), ESP32 DevKit/Arduino IDE + PubSubClient. Cập nhật theo luồng ca: **01 → S1, 02 → S2**. Vị trí đọc RC522; pin và charging_done vẫn là dữ liệu test. [driver.cpp](driver.cpp) kết nối L298N, ba mắt line, servo quét và HC-SR04 với lệnh Core. `ENABLE_AUTONOMOUS_MOTION=false` trong [TestConfig.h](TestConfig.h) giữ motor tắt cho tới khi đã kiểm thử dây/chiều quay/cảm biến/rẽ/vùng trên xe thật. Relay sạc chưa có.

Hai chân encoder dự phòng GPIO4/GPIO16 đã được bỏ khỏi Config vì sketch này không đọc encoder.

## Nối RC522

Bộ chân người dùng đã xác nhận:

| RC522 | ESP32 DevKit |
| --- | --- |
| 3.3V | 3V3 |
| GND | GND |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| SDA / SS | GPIO5 |
| RST | GPIO22 |
| IRQ | Không dùng |

SDA ở module này là SPI SS, không phải I2C. Nguồn 3.3V. Nếu Serial báo version 0x00/0xFF, kiểm tra dây/nguồn; MQTT vẫn kết nối nhưng chưa có vị trí xác minh. UID 4/7/10 byte được chuyển thành HEX 8/14/20 ký tự, không dấu cách.

**Chân SS đã đổi từ GPIO21 sang GPIO5 theo bảng chân 03/10/2026.** Nếu board vẫn đấu dây cũ, chuyển dây trước khi nạp sketch này; GPIO21 dành cho servo 3. Driver dùng ENA14/IN1-27/IN2-26, ENB25/IN3-33/IN4-32, line 39/34/35, TRIG12/ECHO13 và servo quét GPIO15. ECHO phải qua chuyển mức/chia áp. [Bảng chân đầy đủ](../bang-phan-chan-agv.md).

## Chế độ motor theo lệnh Core

Motor tắt khi boot và tại mỗi RFID. `GOTO` được nhận/ACK theo MQTT v0 nhưng chưa tự khởi động motor. Sau khi xác nhận đường thật và hướng rẽ của **đoạn kế**, dùng Serial `motor arm`; `motor off` dừng ngay. `motor arm` chỉ có hiệu lực nếu đã bật `ENABLE_AUTONOMOUS_MOTION` sau kiểm thử bàn, đã quét RFID hợp lệ, đang có `GOTO`, MQTT/heartbeat còn sống và không ở trạng thái lỗi. Nếu node hiện tại hoặc node kế là M1…M6, sketch xin `mutex_request` và đợi đúng `mutex_grant` cho cả hai vùng cần thiết trước khi chạy. `mutex_release` vẫn làm bằng lệnh `release M5` sau khi xác nhận **toàn thân xe** đã ra khỏi vùng; RFID ở node mới chưa đủ chứng minh điều này.

Trong lúc chạy, driver bám line theo logic từ hai file người dùng cung cấp, siêu âm phía trước chặn khi ≤15 cm hoặc hết thời gian đo; line đen active HIGH, 010/101/111 đi thẳng và 000 dừng motor. STOP/WAIT, mất mạng/heartbeat, RFID sai thứ tự và đến mỗi node cũng dừng. Servo chỉ quét khi motor tắt; khi cần chạy, servo về 90° và chờ đo phía trước hợp lệ. Mã chưa giải quyết tự rẽ ở ngã tư, đo pin thật, nâng/hạ hàng, bộ sạc hoặc kiểm chứng phanh vật lý. Không bật motion chỉ để thử trọn tuyến tự động.

## Cấu hình và upload

1. Mở .ino, chọn **ESP32 Dev Module** cho ESP32 DevKit dùng ESP32 gốc và đúng cổng COM.
2. Cài **PubSubClient**, **ArduinoJson 7.x**, **MFRC522** bằng Library Manager. SPI và Preferences nằm trong board package ESP32. Bản đã compile: ESP32 3.3.11, PubSubClient 2.8, ArduinoJson 7.4.3, MFRC522 1.4.12.
3. Kiểm tra WIFI_SSID, WIFI_PASSWORD, MQTT_HOST đã có trong sketch. WiFi bạn điền được giữ nguyên. Với Mobile Hotspot của laptop hiện tại, broker là `192.168.137.1:1883` và sketch đã dùng IP này; `192.168.1.4` là IP cũ. Nếu đổi mạng/hotspot, kiểm tra lại bằng `ipconfig`, không dùng localhost trên ESP32. Serial nay in ESP IP và gateway trong dòng `[MQTT CONNECT]`/`[MQTT FAILED]`. MQTT_USER/MQTT_PASSWORD giữ tùy chọn auth hiện có.
4. Xe thứ nhất ROBOT_ID="01"; xe thứ hai đổi thành "02". Client ID thêm MAC, nhưng không cho hai board cùng gửi telemetry một robot ID. homeStation giữ 01→S1, 02→S2; nếu đổi trạm của core phải đồng bộ hàm này.
5. Upload; Serial Monitor **115200 baud**, **Newline/Both NL & CR**. Không còn START_NODE mặc định.

Khởi động lại core mới để hỗ trợ UNKNOWN/Chờ RFID: [hướng dẫn ca/server](../../docs/17-shifts-and-home-charging.md). Broker cần listener LAN và quyền kết nối phù hợp; sketch không thay service/firewall.

## Bảng UID trên cả hai xe

26 UID do người dùng cung cấp ngày 03/10/2026 đã nằm sẵn trong firmware. Nạp cùng phiên bản sketch cho cả hai board, chỉ đổi `ROBOT_ID` thành `01` hoặc `02`. Đặt thẻ đúng node vật lý rồi quét: xe gửi tên node tới Core, không cần `learn` trước. Dùng `tags` trên Serial Monitor để xem mapping đang có.

| Node | UID | Node | UID |
| --- | --- | --- | --- |
| C1 | B7BC2A95 | C2 | B771B795 |
| C3 | A79A0695 | C4 | 575A1995 |
| M1 | 579E8495 | M2 | 072F2495 |
| M3 | 27CE0C95 | M4 | D78A3395 |
| M5 | 17693195 | M6 | F7354695 |
| G1 | 37F61F95 | G2 | B7A5BD95 |
| G3 | A7520C95 | G4 | 27582F95 |
| G5 | E7CDB995 | G6 | 37869F95 |
| K1 | 57D42295 | K2 | C3008837 |
| K3 | 9746BB95 | K4 | 27EB4095 |
| K5 | A75E1595 | K6 | 47683395 |
| GS1 | 035F2539 | GS2 | 97770296 |
| S1 | 77B04295 | S2 | D7E21F95 |

Khi xe 01 quét `77B04295`, Serial sẽ ghi `[POSITION RFID] S1` và telemetry `S1|85|0`; xe 02 quét `D7E21F95` sẽ báo S2. Đây là vị trí đo từ thẻ, không phải vị trí mặc định khi bật nguồn. Mỗi lần bật lại vẫn cần quét mới. Chưa quét hoặc gặp UID ngoài bảng thì gửi UNKNOWN, web hiện **Chờ quét RFID**.

Nếu thay một thẻ, gõ `learn NODE` rồi quét thẻ mới. Mapping thay thế được lưu bằng Preferences trong NVS namespace `agv-rfid`, key `uid-NODE`; `forget NODE` xóa thay thế và trở về UID mặc định trong bảng. `learn cancel` hủy chờ học. Một UID không được gán hai node trên cùng board. Mapping NVS kiểu cũ (key chỉ là tên node) bị bỏ qua sau cập nhật này; `last-cmd` vẫn giữ nguyên để không phát lại command cũ. `learn` chỉ là thao tác hiệu chỉnh, không dùng để đổi vị trí xe đang làm task.

Mất WiFi/MQTT cần đọc lại thẻ để xác minh. Nếu thẻ còn đứng yên tại đầu đọc, sketch dùng WUPA để đọc lại thẻ đã HALT, không cần nhấc thẻ ra; thẻ chưa học đứng yên không làm flood telemetry. Cache command/quyền mutex giữ trong RAM, cmd_id cao nhất lưu NVS; không tự release hoặc tiếp tục route. Hai xe báo đúng S1/S2, pin test ≥40% thì mở **Bắt đầu ca**. Core gửi WAIT rồi giao GOTO. ACK không đổi vị trí; telemetry từ thẻ mới đổi map. **Kết thúc ca** do core quản lý: xong task cuối, GOTO về trạm riêng, CHARGE_START sau RFID tại trạm.

Nếu chỉ test một board xe01, mở core với AGV_HOME_STATIONS đặt thành JSON {"01":"S1"}; [cấu hình ca](../../docs/17-shifts-and-home-charging.md) có hướng dẫn. Mặc định core đòi đủ01 và02, nên thiếu một xe sẽ giữ nút Bắt đầu ca khóa.

## Test task và mutex

Ví dụ từ G1 tới K4, core gửi M5 M6 G4 K4. Quét thẻ đúng node theo route, xin quyền trước khi vào zone:

    request M5

Chờ MUTEX GRANT M5 rồi quét M5. Xin M6 trước khi vào M6. Khi đã rời hẳn M5 thì release M5; sau khi đo G4 và rời M6 thì release M6; quét K4 để báo đích. Release là thao tác test bằng tay, chưa có cảm biến chứng minh thân robot rời vùng. Không tự release khi mất mạng.

Quét thẻ chưa học trong GOTO khiến core gián đoạn task, giữ đường/vùng chưa xác minh và ERROR. Quét lại thẻ đúng, xác minh tình trạng rồi Xóa lỗi trên web; RFID đọc lại không tự xóa lỗi.

## Serial và giả lập

| Lệnh | Tác dụng |
| --- | --- |
| help, status | ID/home, mạng, node/mode, command cache/zone và nguồn dữ liệu test |
| learn NODE, learn cancel, tags, forget NODE | Quản lý UID, lưu NVS |
| simulate on / simulate off | Bật giả lập / trở lại RC522; mỗi lần đổi đều bỏ xác nhận vị trí |
| node S1 | Chỉ cho phép khi simulate on, gửi node giả lập |
| battery 85, obstacle 0 | Pin0…100 và vật cản0/1 giả lập |
| request M5, release M5 | Request cần vị trí; release cần grant đã nhận |
| charged 90 | Test sạc xong, chỉ sau CHARGE_START mới và đúng trạm riêng |
| ack off/on | Test retry; duplicate ACK lại, không nhận hành động lần hai |
| telemetry off/on | Test offline; MQTT vẫn phục vụ |
| error TEST_ERROR | Báo lỗi để core gián đoạn task/gửi STOP |
| motor arm, motor off | Cho phép thử một đoạn khi đã mở gate sau kiểm thử bàn; dừng ngay và bỏ arm |

Chưa có RC522: simulate on rồi node S1/node S2. Mặc định **simulate off**. Đổi chế độ trong task đang chạy có thể gây ERROR vì mất xác nhận vị trí. Pin và charged vẫn là test, không phải số đo điện hay xác nhận bộ sạc hoạt động.

## Hợp đồng và bằng chứng

Giữ [MQTT v0/v0.1](../../docs/07-network.md): telemetry pipe, JSON command, ACK text số, mutex request là text zone và grant/ACK/release là JSON có `grant_id`, QoS0, command không retained. UNKNOWN là giá trị chưa đo, không phải node map. WAIT/STOP nhận kể cả chưa định vị; GOTO cần vị trí hợp lệ; CHARGE_START cần RFID tại trạm riêng.

JSON≤2048byte, MQTT buffer2304, path≤64node, cmd_id1…2147483647; cache8 command giữ qua reconnect nhưng không qua reset MCU. 26 ID map cố định phải cập nhật nếu map đổi. Heartbeat1 giây; reconnect có timeout, không chờ vô hạn trong callback.

Bản trước tích hợp driver đã compile ESP32: **923708byte flash (70%), 55264byte globals (16%)**, output .qa/esp32-rfid-test-build. Bản 03/10/2026 chưa build ESP32, đã qua [host probe](../../tests/firmware-host/probe.cpp), [driver GPIO shim](../../tests/firmware-host/driver_bench.cpp), [MQTT nhận lệnh](../../tests/firmware.integration.js) và [hai sketch trong ca](../../tests/shift.integration.js) với Core/gateway/Mosquitto riêng. Cách chạy [tests/README.md](../../tests/README.md). Chưa upload/đo RC522, WiFi, motor hoặc bộ sạc trên board thật.

