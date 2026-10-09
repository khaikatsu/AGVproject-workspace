# ADR-022 — Đơn hàng nhiều kho trên web server thực

Ngày 08/10/2026. Trạng thái: triển khai phần mềm; chưa nghiệm thu xe/hàng thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng muốn đưa chức năng đơn hàng của mô phỏng sang web vận hành, nhập số kiện tại từng kho lấy và từng kho giao, server tự ghép và phân xe. Tính năng không được thay điều kiện cho xe chạy. Chưa có nâng hạ; xe đứng 10 giây tại mỗi kho để mô phỏng bốc/dỡ và web hiển thị trạng thái đó.

`OBSERVED`: mô phỏng ADR-019 chỉ nhận một kho lấy, nhiều kho giao và giả lập hàng ảo. Core vận hành cấp `task_id` cho `direct_goto`, báo task `completed` theo telemetry RFID; không có bằng chứng hàng vật lý đã được bốc/dỡ. Gateway có SQLite và WS snapshot, chưa có đơn hàng.

## Quyết định

- Gateway nhận `pickups:[{pickup,quantity}]` và `deliveries:[{dropoff,quantity}]`, tổng bằng nhau, tối đa 20 kiện/đơn. Kho phải là node `station`, không lặp trong cùng danh sách. Ghép số kiện qua bài toán luồng chi phí nhỏ nhất trên trọng số cạnh **có hướng**; không ghép cùng node hoặc cặp không có đường.
- Mỗi kiện một chuyến. Gateway chọn xe `IDLE`, online, đã định vị và chưa có task; ước lượng xe→kho lấy→kho giao để xếp ưu tiên. Một kho giao giữ cùng xe trong một đơn; không cấp xe vào kho lấy/giao đang có xe khác đứng. Hai xe có thể được cấp các kiện khác nhau cùng lúc. Đây là ước lượng điều phối; Core vẫn quyết định đường, vùng mutex, pin, ca và chấp nhận/từ chối `direct_goto`. Không sửa Core hoặc firmware.
- Với mỗi chặng, chỉ khi Core báo đúng `task_id` `completed` và xe `IDLE` tại RFID đích, gateway bắt đầu khoảng mô phỏng dịch vụ 10 giây. Trong khoảng này không cấp chặng kế cho cùng xe; UI đếm ngược bốc/dỡ. Hết 10 giây tại kho lấy mới xin task đến kho giao; hết 10 giây tại kho giao mới tăng số kiện `delivered`. Không gọi đây là xác nhận hàng vật lý.
- Gateway không giữ khóa chuyển động ở Core. Một lệnh khác hoặc task Core đã xếp hàng có thể khiến xe rời kho trước đủ 10 giây; khi đó đơn chuyển `needs_attention`, không tính bốc/dỡ xong. Bảo đảm dừng vật lý bất chấp lệnh ngoài cần cơ chế reservation tại Core/firmware riêng, nằm ngoài phạm vi thay đổi hiện tại.
- Đơn và từng trạng thái kiện được lưu trong SQLite. Nếu task gián đoạn, vị trí sai, mất Core/MQTT, dữ liệu cũ, đổi phiên hoặc gateway restart, đơn dừng ở `needs_attention`. Không tự phát lại chặng có kết quả không chắc chắn hay tự nhận đã giao. Xe đã gắn với đơn cần đối soát không được điều phối cho đơn khác đến khi người vận hành kiểm tra xe/kiện và dùng `POST /api/orders/:id/resolve` để đóng đơn. Các kiện chưa giao không tự chạy lại. Chưa có API tiếp tục phần đơn còn lại.
- HTTP và WS chỉ bổ sung dữ liệu đơn hàng; không đổi MQTT, ngưỡng pin hay điều kiện mở ca. Giao diện nhiệm vụ có form nhiều kho và tiến độ từng kiện.

## Giới hạn và kiểm chứng

`VERIFIED`: unit planner/coordinator và API Core giả xác minh ghép có hướng, tổng số kiện, task_id/RFID, hai khoảng mô phỏng và chặn tiến độ khi vị trí sai. Test gateway cũ vẫn qua. Không nạp firmware hay điều khiển xe thật để kiểm chứng thay đổi này.

`OPEN`: tải trọng một kiện, quy trình bốc/dỡ vật lý, khả năng hai xe hoạt động song song an toàn và thao tác đối soát/tiếp tục đơn sau gián đoạn. Mốc 10 giây là mô phỏng theo yêu cầu, không phải cảm biến hàng hay cơ cấu nâng hạ.
