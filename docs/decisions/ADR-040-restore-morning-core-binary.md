# ADR-040 — Khôi phục binary Core xe 01 đã dùng hồi sáng

Cập nhật kết nối lại 10/10/2026: người dùng gửi ảnh web3000/Core mất kết nối. Kiểm tra không còn tiến trình Core hoặc listener8080; gateway mới PID22124 vẫn chạy. Sau kiểm tra checkpoint không có tuyến/task chờ, khởi động chính binary đã ghim với `--mode robot01`, PID26112. Core/MQTT/storage ready, xe01 S1/IDLE, caCLOSED. Hash vẫn trùng binary hồi sáng, không build hoặc rewind counter. Bằng chứng `.qa/core-reconnect-20261010/verification.json` và backup dữ liệu trước start; không phát GOTO hoặc start_shift thủ công.

Ngày 10/10/2026. Trạng thái: đã khôi phục theo REQ-CORE-RESTORE-01.

## Nguồn và bằng chứng

`SOURCE`: sau khi hỏi Core test xe 01 hồi sáng đã bị thay đổi gì, người dùng yêu cầu khôi phục Core đó. Yêu cầu này thay phần rebuild Core vận hành trong xử lý khởi động của REQ-SIM-10; không hủy các sửa client web mô phỏng.

`OBSERVED`: trước khi ghi đè, `build/agv_core.exe` có LastWriteTime 10/10/2026 03:14:03.7866505 (Asia/Saigon), Core PID 22344 đang chạy từ file này. `.qa/grant-panic-build/agv_core.exe` có đúng thời điểm đó; hồ sơ `.qa/live-grant-panic-final/verification.json` lúc 03:17:22 ghi `core_binary_matches:true`, xe 01 tại S1/IDLE, Core/MQTT/storage khỏe. Bản `.qa/robot01-mid-reset-build` lúc 03:28 là bản khác và không được chọn để khôi phục.

Binary được chọn có SHA-256 `FAE44789D352BE8A6FAF7C3E2A98E120D6AE85F7BDC86E50A57CEB366ADA46F5`, kích thước 14.788.422 byte. Đây là binary ADR-035; không tự nhập thêm thay đổi điều khiển từ mã nguồn hiện tại.

## Quyết định và thao tác

- Sao lưu binary mới bị thay, cấu hình F5 và hai file nguồn đang sửa vào `.qa/restore-morning-core-20261010-080341`.
- Khôi phục `build/agv_core.exe` bằng bản đã chọn; giữ thêm bản bảo vệ tại `build/robot01-morning/agv_core.exe`. Cả hai được kiểm tra hash trùng với binary nguồn.
- Cấu hình F5 **AGV core (workspace root)** dùng `build/robot01-morning/agv_core.exe`, bỏ `preLaunchTask`. Menu chọn chế độ và working directory vẫn ở gốc. F5 không build lại mã nguồn mới để ghi đè phiên bản khôi phục. Build mặc định vẫn có thể build mã nguồn hiện tại; đầu ra đó không phải binary F5 đang được ghim.
- Hoàn tác riêng thay đổi `RuntimeStore::mapsCompatible` và lời gọi trong `AGVserver.cpp` vừa thêm sau ảnh checkpoint, chưa build. Không có snapshot đầy đủ mã nguồn ở thời điểm 03:14; khôi phục được binary, không tuyên bố toàn bộ source hiện tại là source hồi sáng.
- Phiên Core lỗi 2964 đã thoát nhưng GDB 12412 còn giữ handle; kiểm tra gateway mất Core và cổng 8080 không còn listener trước khi đóng riêng GDB này. Không tự khởi động lại Core, mở ca hoặc phát lệnh robot. Người dùng F5 rồi chọn 1 để thử xe 01.
- Không thay firmware hoặc hoàn tác dữ liệu vận hành về bản sao cũ. Map/checkpoint/shift/counter hiện tại được giữ nguyên; không rewind command/task ID.

## Kiểm chứng và giới hạn

`VERIFIED`: chạy binary hồi sáng với bản sao năm file `data` hiện tại, broker và cổng riêng tại `.qa/restore-morning-core-20261010-080341/fixture` PASS: chế độ robot01, 26 node, checkpoint phục hồi, MQTT/storage khỏe, không robot/queue/owner, counter tiếp nối. Bằng chứng `verification.json`/`verification.log` trong thư mục khôi phục.

`VERIFIED`: hai binary đích hash trùng nguồn; hash của năm file `data/map_layout.json`, `runtime_state.json`, `shift_state.json`, `command_sequence.json`, `task_sequence.json` trước/sau khôi phục không đổi. Manifest và binary mới bị thay được giữ trong thư mục khôi phục. F5 JSON dùng binary bảo vệ và không còn preLaunchTask.

`OPEN`: chưa thao tác F5 trong VS Code hoặc chạy lại xe thật sau khôi phục. Cần kết thúc trạng thái debug cũ bằng Shift+F5 nếu VS Code vẫn hiện phiên cũ, sau đó F5/chọn 1. Kiểm thử bằng bản sao dữ liệu không ghi DB hoặc kết nối broker robot thật.
