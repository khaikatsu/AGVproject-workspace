# ADR-030 — Xác nhận hướng đầu xe và hiển thị trước chấm RFID

Ngày 10/10/2026. Trạng thái: IMPLEMENTED; kiểm chứng phần mềm, chưa nạp lên ESP32.

Cập nhật cùng ngày: **SUPERSEDED** bởi [ADR-032](ADR-032-fixed-bay-heading.md) theo yêu cầu mới. Panel/API/lệnh gán hướng thủ công dưới đây đã bỏ; tài liệu này giữ lịch sử lựa chọn trước đó.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn mũi tên trước chấm xe trên bản đồ và chọn/xác nhận hướng đầu xe thực tế để tránh hiểu nhầm sau khi đặt xe bằng tay. Người dùng hỏi cảm biến hồng ngoại có tự xác định hướng không.

`OBSERVED`: ba mắt hồng ngoại đo vị trí tương đối với line; mẫu `010` không phân biệt hai hướng ngược nhau trên cùng line. RFID xác định node, không đo góc thân xe. Cặp GS1→S1 do di chuyển bằng tay từng bị lưu như một chặng chạy, làm tuyến S1→GS1 bị suy ra là lùi và khóa do chưa hiệu chuẩn lái lùi.

## Quyết định

Hướng theo tọa độ bản đồ: N lên, E phải, S xuống, W trái. Dashboard vẽ mũi tên đặc cho hướng ESP32 báo; mũi tên viền là lựa chọn chưa xác nhận. Hướng chưa biết hiện `?`; offline/dữ liệu cũ không vẽ hướng đã xác nhận. Chấm tiếp tục chỉ vị trí RFID gần nhất.

Người vận hành chọn xe, hướng và xác nhận xe đã dừng/đầu xe thực tế đúng lựa chọn. Gateway gửi `set_heading`; Core cấp cmd_id và gửi `SET_HEADING` tới ESP32. Core và firmware yêu cầu đúng node RFID vừa đọc, xe dừng, hết task/đường chưa xác minh; firmware cũ không quảng bá capability thì không được gửi lệnh. Không cấp task mới khi đang chờ xác nhận hướng. Có thể chọn và xác nhận lại nếu lệnh trước chưa được chấp nhận.

ACK/HTTP thành công chỉ là tiếp nhận. Core chờ diagnostic cùng node, hướng, cmd_id và `headingSource=operator`, RFID ≤250 ms, PWM 0/0 trước khi bỏ trạng thái pending. SET_HEADING không reset lỗi hoặc chạy motor. Reconnect/khởi động và RFID do đặt tay xóa hướng suy luận; chỉ đến đúng node dự kiến từ chặng navigator thực sự đang chạy mới cập nhật hướng theo cạnh và chiều tiến/lùi. Khi pivot, hướng tạm UNKNOWN. Hướng operator được dùng để lập lượt rẽ đầu tuyến; S1→GS1 trên map hiện tại có hướng S (xuống).

## Kiểm chứng và giới hạn

`VERIFIED`: Core unit, gateway/security 13/13, native firmware 90/90, native bench profile 01 14/14 PASS. MQTT/Core/gateway fixture riêng trong `.qa/heading-test-Dlticd/verification.json` kiểm tra node sai, xe đang chạy, RFID cũ, ACK và diagnostic sai ID không xác nhận hướng, diagnostic đúng xác nhận, offline ẩn hướng; không phát GOTO. Browser PASS trong `.qa/browser-test-1Jtzm7`, gồm chọn/checkbox/preview, xác nhận từ ESP32, firmware cũ, mất mạng và đổi node xóa draft. Build ESP32 env 01 PASS; chưa upload hoặc đo hướng thực tế. Xem [hợp đồng mạng](../07-network.md), [HTTP/WS](../../contracts/web-server.md).

`OPEN`: nạp firmware mới, chạy Core/gateway mới rồi thử xác nhận tại RFID thật. Hướng suy luận không phải phép đo góc; không thể phát hiện việc xoay xe bằng tay nhưng đầu đọc vẫn ở cùng thẻ. Khi làm việc đó phải xác nhận lại hướng trên web.
