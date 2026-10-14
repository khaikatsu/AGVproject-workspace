# ADR-041 — Khôi phục firmware xe 01 đã nạp hồi sáng

Ngày 10/10/2026. Trạng thái: đã khôi phục và nạp xe 01.

`SOURCE`: người dùng yêu cầu khôi phục firmware hồi sáng chạy được, sau khi Core đã khôi phục theo ADR-040.

`OBSERVED`: bo đang dùng `20261010-rfid-poll-10ms`; lần nạp cuối hồi sáng ghi `20261010-grant-buffer-fix`, SHA-256 `23A181746E65498AC9447E7D0243778600A49EC10678EBFE22DA0C1571D30CBF`. Workspace không còn bản sao binary đó. Lịch sử chỉnh sửa xác nhận thay đổi sau lần nạp là comment, chu kỳ RFID và chuỗi firmwareBuild trong Config.h.

`IMPLEMENTED`: sao lưu hai file cấu hình và binary/ELF/bootloader/partitions hiện tại vào `.qa/restore-morning-firmware-20261010`. Khôi phục comment, RFID20ms và firmwareBuild `20261010-grant-buffer-fix`; profile01 giữ PWM220, trim trái-40. Build `esp32dev_two_robot_bench_01` tạo SHA-256 trùng hoàn toàn ảnh đã nạp hồi sáng; giữ ảnh này tại `restored-firmware.bin` trong thư mục sao lưu. COM6/ESP32-D0WD-V3/MAC `ec:c9:ff:ff:09:dc` khớp xe01.

Đính chính PWM245 trong ADR-035: cấu hình245 tạo hash khác; binary thực tế đã nạp và ghi hash hồi sáng dùng PWM220, chạy thẳng có trim180/220. So hash toàn bộ ảnh là bằng chứng khôi phục mạnh hơn so các thông số trong tài liệu.

`VERIFIED`: build ESP32 PASS RAM55996/Flash948029, hash trùng trên; RFID adapter PASS poll20ms, dừng trước HALT, đọc lặp và UID/collision.

`VERIFIED`: PlatformIO upload COM6 SUCCESS, hash dữ liệu trên flash được esptool xác minh. Ba diagnostic sau boot báo đúng firmwareBuild `20261010-grant-buffer-fix`, POWERON, WiFi/MQTT=true, motorSTOP/duty0. Hash Core vẫn trùng ADR-040. Bằng chứng tại `.qa/restore-morning-firmware-20261010/diagnostic-after.jsonl` và `verification.json`.

`OPEN`: ngay sau khôi phục, diagnostic vẫn chốt `NETWORK_UNAVAILABLE`, nodeUNKNOWN/rfidAgeMs=null/line000. Bản firmware hồi sáng cũng ghi nhận lỗi mạng trong điều kiện hiện tại; việc hoàn tác RFID10ms chưa loại bỏ vấn đề. Chưa chứng minh nguyên nhân heartbeat chậm hoặc phần cứng. Core giữ binary ghim ADR-040; lỗi và quyền M2 được giữ để đối soát, không tự clear vùng/phát GOTO hoặc rewind counter/checkpoint. Upload không erase toàn flash, giữ NVS high-water command ID. Người vận hành cần quét lại S1, đầu hướng GS1 để xác nhận vị trí/hướng trước phục hồi theo quy tắc Core hồi sáng.

Config.h dùng chung hai profile: lần build sau của xe02 cũng dùng RFID20ms; lần khôi phục này chỉ nạp xe01.
