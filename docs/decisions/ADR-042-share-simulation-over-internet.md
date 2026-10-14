# ADR-042 — Chia sẻ web mô phỏng qua Internet

Ngày 10/10/2026. IMPLEMENTED; đã kiểm tra link HTTPS.

`SOURCE`: người dùng yêu cầu người khác truy cập mô phỏng, xác nhận phạm vi Internet và yêu cầu tiếp tục.

`OBSERVED`: launcher3100 bind loopback, chỉ nhận Originlocalhost và trả URL127.0.0.1/cổng phiên. Tunnel launcher đơn thuần sẽ chuyển trình duyệt về máy người xem; gateway cũng từ chối Host/Origin công khai.

## Quyết định

- Thêm `--public-proxy` cho launcher; bind127.0.0.1:3110 và proxy HTTP/upgradeWS `/ws` tới duy nhất gateway phiên mô phỏng do launcher tạo.
- POST `/launch/start` và nút mở lại dùng `/simulation` tương đối. Assets/auth/REST/WS cùng origin. GET launch status thêm `public_proxy`.
- Kiểm tra Origin khớp Host trước proxy; Origin khác bị403 cả HTTP/WS. Sau kiểm tra mới đổi Host/Origin về targetlocalhost, bỏ Forwarded/X-Forwarded-*. Không thay chính sách gateway vận hành.
- Quick Tunnel kết nối outbound qua3110; không mở firewall/router inbound, không tunnel3000/8080/1883. Broker/Core/DB/counter mô phỏng vẫn riêng và loopback.
- `scripts/share-simulation.ps1` tải cloudflared chính chủ, kiểm tra SHA-256 metadata, chạy nền ẩn; PID/link/log lưu `.qa/public-simulation`. Chạy lại trả link tunnel đang sống; `-Stop` tắt đúng tunnel, giữ launcher cục bộ.
- Người có link điều khiển chung hai xeảo. Máy/tunnel cần chạy; mở tab tạo phiên, đóng tab cuối tự dừng/lưu báo cáo. Link tạm thời; hostname cố định chưa yêu cầu.

## Kiểm chứng và giới hạn

`VERIFIED`: fixture `tests/simulation.public.test.js` kiểm tra HTTPSOrigin/redirect/assets/auth/REST/WS/foreignOrigin/idle/reopen; `tests/simulation.on-view.js` kiểm tra launcher mặc định PASS.

`VERIFIED`: cloudflared2026.10.0 checksum `86aee4017b26625cee8484c113558f48effa4cd47f7aa05fcf425604e5d2b23c` trùng release. LinkHTTPS trả200. Edge qua chính linkInternet PASS: cùng origin, WS, hai xe, viewerheartbeat, reload/mobile390px, không pageerror; bằng chứng `.qa/public-simulation-browser-cRwqhA/verification.json` và `public-mobile.png`.

`OPEN`: QuickTunnel không bảo đảm uptime; link đổi khi tạo tunnel mới. Không thử robot vật lý trong việc chia sẻ mô phỏng.

Nguồn ngày10/10/2026: [Quick Tunnels](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/), [release2026.10.0](https://github.com/cloudflare/cloudflared/releases/tag/2026.10.0). Không sao chép implementation upstream.
