# ADR-069 — Đơn hàng tự động hai xe dùng kết quả servo

Ngày18/10/2026. Trạng thái: IMPLEMENTED phần mềm gateway/web; phần cứng OPEN.

## Nguồn và hiện trạng

`SOURCE`: người dùng dùng giao diện đơn hàng tự động khi Core chạy scope xe01+02 và yêu cầu xe thực hiện nâng/hạ, không phải nút Lấy/Hạ thủ công của scope một xe. Kho dự kiến có bốn kệ và hai vị trí trống; khi chưa có cảm biến tải, chu kỳ servo tạm đại diện trạng thái đang mang kệ.

`OBSERVED`: Core thống nhất đã nhận `warehouse_lift` và quản lý `LIFT_UP`/`LIFT_DOWN`, command ID, diagnostic/cargo độc lập cho01/02. Firmware source hai profile cũng có capability lift. Tuy nhiên `orders.js` chỉ chờ timer5s ở `pickup_service`/`dropoff_service`; chỉ `warehouse-actions.js` của scope một xe gọi servo. Vì vậy đơn hai xe trước thay đổi có thể chạy tới đích mà servo không hoạt động.

## Quyết định

- Giữ API tạo đơn và thuật toán ghép/phân xe hiện tại. Scheduler chỉ chọn robot tham gia có diagnostic `lift.supported=true` còn sống và không đang được ghi nhận có cargo.
- Sau task đi tới kho hoàn thành, đúng RFID và xe IDLE, item chuyển qua bước chờ servo. Gateway chỉ gửi `warehouse_lift` khi `lift.ready/fresh/observed_node` đúng kho; lưu `lift_command_id` Core trả về.
- Chặng giao chỉ được phát sau `DONE_UP` đúng command ID, `lift.active=false`, xe trở lại IDLE và `virtual_cargo_known/virtual_cargo=true`. Chặng lùi khỏi kho giao chỉ được phát sau điều kiện tương ứng `DONE_DOWN` và cargo false.
- ACK, timer gateway hoặc kết quả của lệnh cũ không hoàn tất thao tác. Quá5s chưa có servo ready hoặc15s chưa có DONE đúng lệnh, servo interrupted, xe rời kho, mất mạng/Core/phiên hay gateway restart đều chuyển đơn sang `needs_attention`; không replay và không fallback mô phỏng.
- UI hiển thị riêng chờ servo, gửi lệnh, đang nâng và đang hạ. Health công bố yêu cầu capability và hai timeout để nhận biết backend đã nạp thay đổi.
- Không đổi MQTT/Core/firmware trong lần này vì handler và diagnostic tương quan đã tồn tại. Chu kỳ cơ khí tiếp tục5s đồng bộ giữa firmware/Core. Đề xuất3s chỉ áp dụng sau khi đo servo và sửa đồng bộ, không hạ riêng timer gateway.

## Giới hạn và kiểm chứng

`VERIFIED`: unit order/warehouse21/21, gateway14/14, syntax và diff check PASS trong fixture riêng. Kiểm tra gồm hai xe, thiếu capability, DONE sai ID, offline/restart và chỉ lùi gate sau hạ đúng kết quả.

`OPEN`: chưa restart dịch vụ vận hành, chưa upload/đo servo thật và chưa có cảm biến xác nhận kệ. `virtual_cargo` chỉ chứng minh firmware đã hoàn thành chu kỳ PWM theo phần mềm. Mô hình tồn kho/đặt chỗ cho bốn kệ chưa triển khai; cần xác nhận câu “K1 và K6 trống” có nghĩa bốn kệ ban đầu ở K2–K5, rồi bổ sung ID kệ và đối soát vị trí.
