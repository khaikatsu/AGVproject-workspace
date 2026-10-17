# ADR-067 — Xe02 lùi chậm và căn line bằng hai bánh

Ngày17/10/2026. Trạng thái: IMPLEMENTED; chờ kiểm chứng bàn/sàn.

## SOURCE và OBSERVED

SOURCE: người dùng xác nhận cụm mắt ở đầu xe, đồng ý giảm tốc lùi và chỉnh chênh nhẹ hai bánh thay vì dừng hẳn một bánh. Chỉ áp xe02; yêu cầu chỉ build/kiểm tra cơ bản, không chạy toàn bộ test.

OBSERVED: profile02 người dùng đang chỉnh reverse210/correction240, trim lùi LEFT+5/RIGHT−10, capture230. Lùi thường duty215/200;011/001 dùng LEFT lùi245,RIGHT0;110/100 dùng LEFT0,RIGHT lùi230. Các comment về bánh vật lý trong profile không đồng nhất; không đổi ánh xạ motor hoặc suy ra dây từ comment. Các nhãn LEFT/RIGHT ở đây là kênh mã, cần đối chiếu bánh thật khi thử.

## Quyết định

- Riêng profile02: reverse200/correction200, bật `AGV_REVERSE_GENTLE_CORRECTION=1`, delta20. Giữ nguyên trim lùi đang có+5/−10; không hồi về tuning lịch sử−15.
- `010`: hai kênh lùi205/190.011/001: lùi205/170.110/100: lùi185/190. Delta là phần giảm bổ sung20 trên kênh cần chậm so với baseline có trim, không phải hai bánh luôn lệch đúng20. Không tăng bên mạnh để chỉnh.
- Giữ bảng hướng `reverseLineAction`; không đảo trái/phải chỉ vì mắt ở đầu xe. Pha settling sau bắt line dùng cùng chỉnh nhẹ và vẫn cap PWM như trước. Xe01 fallback giữ cách chỉnh cũ; tiến/xoay/capture/map/phản hồi line sớm/mutex/sonar/STOP không đổi. Không đổi MQTT/schema.
- `000` vẫn STOP/tìm line có giới hạn theo navigator, không tiếp tục lùi mù. Còi lùi theo duty/action giữ nguyên. BuildID02 `20261017-robot02-gentle-reverse-v4`.

## Giới hạn

VERIFIED: build env02 PASS RAM57948byte/flash962909byte, kiểm tra motor-host cơ bản01/02 PASS (duty/còi/STOP/settling và tiến/xoay giữ nguyên). Không chạy toàn bộ native/integration.

PWM200/delta20 là mức thử, chưa đo tốc độ thực hoặc ngưỡng khởi động motor. Kiểm tra GPIO chỉ chứng minh dấu/duty/còi/STOP, không chứng minh011/110 sẽ hội tụ010. Cần quan sát trên xe, bảo đảm cả hai bánh vẫn quay và không có vật cản. Nếu lệch tăng hoặc bánh yếu dừng thì STOP và đánh giá lại chiều chỉnh/ma sát/vị trí mắt; không bỏ chốt an toàn. Chưa upload/điều khiển xe thật trong lượt này.
