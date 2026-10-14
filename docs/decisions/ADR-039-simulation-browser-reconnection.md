# ADR-039 — Phục hồi kết nối web mô phỏng và báo lỗi khởi động

Ngày 10/10/2026. Trạng thái: triển khai theo REQ-SIM-10.

## Nguồn và hiện trạng

`SOURCE`: người dùng yêu cầu xử lý khi web mô phỏng mất kết nối, sau đó gửi ảnh build Core thất bại và Node báo lỗi listen/EADDRINUSE.

`OBSERVED`: client xóa snapshot khi poll thất bại, chưa có timeout fetch và còn nút mô phỏng có thể bấm. Launcher chuyển sang cổng phiên riêng; khi lease hết hạn, poll cổng đã đóng không tạo lại phiên. Lúc kiểm tra, Core đang chạy từ `build/agv_core.exe` (PID 22344), gateway 3000 khỏe, launcher 3100 có phiên hoạt động. Ảnh thiếu lỗi GCC/linker phía trước nên chưa xác nhận duy nhất nguyên nhân build của lần trong ảnh; executable đang chạy là điều kiện ngăn linker ghi đè trên Windows.

## Quyết định

- Giữ snapshot cuối khi mất HTTP, browser offline hoặc phản hồi thiếu trường; đánh dấu dữ liệu cũ và thời điểm nhận. Ngừng nội suy vị trí, bỏ mũi tên chuyển động khi thiếu dữ liệu mới. Không suy mất web thành xe đã dừng hoặc mutex trống.
- Điều khiển chung và riêng mô phỏng cần snapshot HTTP mới dưới 3 giây, health runtime sẵn sàng, WebSocket/Core mới và chốt MQTT/protocol/storage hiện có. Mất riêng WS vẫn khóa lệnh dù HTTP hoạt động.
- GET state và heartbeat timeout 2,5 giây; mỗi luồng chỉ một request đang chờ. Poll bình thường 250 ms, lỗi lùi nhịp 500 ms → 1 s → 2 s → tối đa 4 s. Offline/pagehide hủy request và bỏ phản hồi thuộc lượt kết nối cũ. Online/pageshow tiếp tục kiểm tra, không tạo thêm interval heartbeat.
- POST mô phỏng timeout 8 giây. Mất phản hồi báo kết quả chưa xác định, không tự phát lại; đối soát bằng state mới. Nút thử lại chỉ nối WS và đọc state.
- Launcher truyền URL vào HTML qua `data-sim-launcher`, hỗ trợ cổng tùy chỉnh. Nút mở lại dẫn về launcher: phiên còn thì dùng lại, phiên đã đóng thì tạo mới. Không tự điều hướng/tạo phiên hoặc phục hồi task/map từ snapshot cũ.
- Script build phát hiện executable đích đang chạy, báo PID/đường dẫn, không tự dừng Core. Gateway bắt lỗi bind được ws chuyển tiếp, CLI báo cổng bận; lần chạy trùng không tác động gateway cũ.
- Không đổi MQTT, WS v1, lease/idle timeout, firmware, thuật toán hoặc dữ liệu vận hành.

## Kiểm chứng và giới hạn

Đối soát ảnh tiếp theo ngày 10/10/2026: UTF-8 không BOM của script bị Windows PowerShell 5 đọc sai tiếng Việt; đã đổi sang UTF-8 có BOM. Core PID 22344 được kiểm tra lại đúng executable đích, fleet trống, không pending/dispatched task và không mutex owner, rồi dừng riêng để giải phóng build/F5. Gateway/launcher/broker hiện hữu giữ nguyên; không tự chạy Core mới hoặc phát lệnh robot. Đây là xử lý phiên đang rảnh theo phản hồi lỗi của người dùng; script build vẫn chỉ báo lỗi, không tự kill tiến trình.

Browser regression cuối PASS tại `.qa/run-WmQFjV` sau thay đổi freshness của thanh Web/Core/MQTT; connection test cuối PASS. Các link trong tài liệu thay đổi đã kiểm tra riêng và PASS. Runner `tests/run-simulation-audit.js --browser` đã thêm ca connection; chưa chạy lại toàn bộ audit trong lượt này.

`VERIFIED`: Core build riêng tại `.qa/web-reconnect-build`. Build mặc định được chặn trước linker khi executable còn chạy. Gateway/security: 16/16 PASS, gồm cổng trùng không làm hỏng gateway cũ.

`VERIFIED`: [browser connection test](../../tests/simulation.connection.browser.js) với Core/broker/DB/cổng riêng PASS: browser offline/online, giữ hai xe/dữ liệu cũ, khóa lệnh, HTTP 503/backoff, payload thiếu, request treo, health chờ Core/MQTT, mất riêng WS, phản hồi POST bị mất không retry, đóng phiên/mở lại đúng launcher và mobile. Hình tại `.qa/simulation-connection/offline.png` và `offline-mobile.png`.

`OPEN`: chưa thử ngủ trình duyệt/BFCache bằng thao tác tay hoặc mạng kéo dài trên máy khác. Thay đổi module Node có hiệu lực ở lần khởi động tiếp theo; không tự dừng tiến trình vận hành. Giữ dữ liệu cũ chỉ để quan sát, không chứng minh robot thật đã dừng.

Kiểm thử bổ sung: `simulation.on-view.js`, `simulation.direction.test.js`, `simulation.robot.test.js` PASS. Browser regression có đồng bộ chờ frame vẽ xe và cache runtime cập nhật sau mở ca; phiên trước đó lỗi do assertion chạy trước frame hoặc API job đọc cache trước WS, không thay chốt runtime để ép test qua. `scripts/check_workspace.py` còn báo link cũ tới `src/` và artifact đã thiếu, cùng link trong `.pio/libdeps`; không có sửa các nội dung đó trong lượt này. Cú pháp các module JS đã kiểm tra bằng `node --check`.
