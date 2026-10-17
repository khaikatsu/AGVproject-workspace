# ADR-068 — Nhả mutex theo đuôi xe và encoder hai bánh

## Sửa nhịp dừng sau khi trả khóa — 17/10/2026

`SOURCE`, REQ-MUTEX-EXIT-CONTINUE-01: người dùng báo xe vượt M3 một đoạn rồi dừng/đi tiếp và yêu cầu sửa. `OBSERVED`: sau đủ30 xung, controller queue release rồi gọi `revokeGrant`; navigator vẫn lưu RFID gần nhất là M3 nên `missingZone()` yêu cầu lại M3 và dừng. Probe host tái hiện `missing=M3 requested=M3 stopped=1`; token mới làm xe đi tiếp.

`IMPLEMENTED`: tách `completeZoneExit(zone,grantId)` khỏi thu hồi grant thông thường. Chỉ sau bằng chứng clearance và queue release thành công, đúng token đang giữ mới xóa token và ghi xe đã ra khỏi vùng hiện tại. Trạng thái này chỉ cho phép hoàn tất chặng tiến đang chạy từ M tới RFID kế tiếp; không thay quyền vào mutex kế tiếp hoặc tạo token sở hữu. Cả `missingZone()` và kiểm tra đi xuyên dùng trạng thái này để không dừng/xin lại vùng đã trả. Grant tới muộn cho vùng đã ra bị bỏ qua. Đọc node mới, STOP/hủy, fault, tuyến mới hoặc reset phiên đều xóa dấu đã ra; lần đi qua sau phải nhận grant mới. `revokeGrant` thông thường vẫn dừng nếu quyền vùng cần thiết bị thu hồi.

`VERIFIED`: native hai profile kiểm tra sáu mutex, ngưỡng30 xung tới navigator, token sai/lặp/tới muộn, fast-line không dừng, M5→M6 vẫn đợi đúng quyền M6, đi xuyên không bị ảnh hưởng và reset theo node/tuyến/lỗi/phiên. Kết quả build/test trong tests/README.md. BuildID01 `20261017-robot01-mutex30-exitfix-v4`,02 `20261017-robot02-mutex30-exitfix-v4`; grace7s của02 và tuning từng xe được giữ theo source hiện tại. Chưa upload hoặc đo xe thật trong lượt sửa này.

Ngày 17/10/2026. Trạng thái: triển khai và kiểm thử phần mềm cho profile 01/02; chưa nạp hoặc nghiệm thu trên sàn.

`SOURCE`: hai xe có cùng kích thước. Từ đầu xe tới đầu đọc RFID là 7,5 cm, từ đầu đọc tới đuôi là 9,5 cm. Đầu đọc bắt đầu thấy thẻ khoảng 3 cm trước tâm và hết thấy khoảng 3 cm sau tâm. Biên ra của vùng mutex cách tâm thẻ 3 cm. Encoder thực tế được người dùng cung cấp là 161 xung/100 cm cho cả hai xe.

`OBSERVED`: chỉ mất một lần đọc RC522 không chứng minh đuôi xe đã ra khỏi vùng. Xung bánh khi pivot tại node hoặc tìm lại line cũng không phải quãng tịnh tiến. Cách cũ chỉ trả mutex tại RFID rời vùng đã đo nên an toàn hơn nhưng giữ khóa lâu, làm xe sau phải chờ tới thẻ kế tiếp.

## Quyết định

- Mốc đếm là lần đầu đọc đúng RFID M1–M6 trong route đang chạy và đang giữ đúng `grant_id`. Từ mốc đọc sớm nhất, quãng cần thiết là `3 + 3 + 9,5 + 3 = 18,5 cm`: 3 cm tới tâm thẻ, 3 cm tới biên ra, 9,5 cm tới đuôi và 3 cm dự phòng.
- Với 161 xung/100 cm, `18,5 × 161/100 = 29,785`; luôn làm tròn lên thành 30 xung. Cả bánh trái và bánh phải đều phải đạt ít nhất 30 xung tịnh tiến. Không dùng trung bình hai bánh để một bánh quay đơn lẻ có thể mở khóa.
- Bộ đếm tích lũy chỉ tăng khi navigator đang bám line và motor đang tịnh tiến tiến/lùi. STOP, pivot, căn line và quét tìm line không tăng bằng chứng clearance. Đổi chặng không xóa tổng tích lũy dùng cho mốc mutex; phép trừ `uint32_t` chịu được wrap.
- RC522 phải không đọc lại thẻ mutex liên tục ít nhất 100 ms. Mỗi lần đọc lại cùng thẻ đặt lại cửa sổ này. Điều kiện 100 ms chỉ chống đọc hụt; không thay điều kiện 30 xung.
- Chỉ phát `mutex_release` khi xe còn đúng phiên MQTT, vị trí đã xác minh, route còn active, đang Following và không có lỗi control/navigator. STOP, mất mạng, mất định vị, chờ quyền, quay hoặc lỗi giữ khóa. Core vẫn xác thực owner và `grant_id`; QoS0/replay token giữ như trước.
- Cơ chế RFID rời vùng đã đo trong `MutexClearance` được giữ làm fallback bảo thủ nếu không có bằng chứng encoder. Serial ghi `[MUTEX_RELEASE] ... proof=encoder|exit_rfid` để thử bàn; payload MQTT không đổi.

## Kiểm chứng và giới hạn

Native profile 01/02 kiểm tra 30/29 xung, hai bánh, cửa sổ mất thẻ, trạng thái không an toàn, token và wrap counter. Probe encoder profile 01/02 kiểm tra STOP/pivot không tăng tổng xung tịnh tiến. Hai ảnh ESP32 được build sau thay đổi; kết quả và hash nằm trong `tests/README.md`.

`OPEN`: 161 xung/100 cm chưa có bảng nhiều lượt/sai số riêng từng bánh; chưa đo trượt bánh, quãng thật từ lần đọc đầu tới khi đuôi qua biên, hoặc phần thân quét khi rẽ. Trước khi cho hai xe chạy đồng thời, thử một xe có đánh dấu biên vùng và người giám sát; nếu đuôi chưa qua đủ, tăng ngưỡng chứ không giảm. Build thành công không phải nghiệm thu chuyển động thật.
