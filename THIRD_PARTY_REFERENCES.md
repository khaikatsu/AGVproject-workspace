# Ghi nhận tài liệu và mã tham khảo

Bộ nền được viết cho workspace ngày 28/09/2026 sau khi tra cứu các repo chính chủ. Chưa clone, vendor hay sao chép implementation của repo tham khảo. Các tài liệu diễn giải kiến trúc và gợi ý áp dụng, không gán bản quyền upstream cho mã tự viết của nhóm.

Danh mục và nguồn license: [docs/11-github-references.md](docs/11-github-references.md). Cần kiểm tra lại license đúng file/package/commit trước khi đưa code upstream vào dự án.

## Nhật ký sử dụng sau này

| Ngày | Repo và URL file | Tag hoặc commit | Phần dùng và file đích | License và notice |
| --- | --- | --- | --- | --- |
| Chưa có | Chưa nhập mã upstream trong bộ nền | Chưa pin | Tham khảo thiết kế qua danh mục | Chưa phát sinh notice cho code sao chép |
| 29/09/2026 | [PubSubClient](https://github.com/knolleary/pubsubclient/tree/v2.8/src), [API](https://pubsubclient.knolleary.net/api) | v2.8, thư viện có sẵn trong Arduino IDE | Dependency include/link khi compile sketch ESP32; không vendor mã thư viện vào repo | MIT, Copyright 2008–2020 Nicholas O'Leary; LICENSE.txt có trong bản cài |
| 29/09/2026 | [ArduinoJson](https://github.com/bblanchon/ArduinoJson/tree/v7.4.3/src) | v7.4.3, bản cài hiện có | Dependency parser cho sketch và host probe; không vendor mã thư viện | MIT, Copyright 2014–2026 Benoit Blanchon; LICENSE.txt trong bản cài |
| 29/09/2026 | [Arduino ESP32](https://github.com/espressif/arduino-esp32/tree/3.3.11/libraries), [WiFi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html) | Core 3.3.11 đã cài | Board package compile FQBN esp32:esp32:esp32 và API WiFi/NetworkClient cho sketch | WiFi.h/NetworkClient.h: LGPL-2.1-or-later theo header; package giữ notice các dependency/toolchain; không sao chép source vào repo |
| 29/09/2026 | [MFRC522 source](https://github.com/miguelbalboa/rfid/blob/master/src/MFRC522.h), [repo](https://github.com/miguelbalboa/rfid) | Package v1.4.12 đã cài, xác minh library.properties; không vendor source | Include MFRC522 trong sketch, API UID/PICC/PCD; handler learn/mapping do workspace viết | UNLICENSE/public domain theo file UNLICENSE của package; không sao chép ví dụ/upstream implementation |
| 29/09/2026 | [ESP32 Preferences](https://github.com/espressif/arduino-esp32/tree/3.3.11/libraries/Preferences/src), [API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html), [SPI](https://github.com/espressif/arduino-esp32/tree/3.3.11/libraries/SPI/src) | Board package 3.3.11 đã cài | Include SPI/Preferences để nối reader và lưu UID/NVS, không lưu vị trí | LGPL-2.1-or-later theo header; dependency qua board package, không vendor source |

`src/json.hpp` đã có trước bộ nền và là mã third party cần xác minh version/notice khi quản lý dependency. Các package Node.js đã có `package-lock.json`; không suy ra mọi dependency mang cùng license với Express hoặc dự án.
