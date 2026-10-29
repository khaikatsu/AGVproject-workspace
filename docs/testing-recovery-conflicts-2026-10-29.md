# Kiểm tra xung đột khôi phục và điều phối — 29/10/2026

Yêu cầu: REQ-RECOVERY-CONFLICT-AUDIT-20261029. Phạm vi: luồng khôi phục/xác nhận hướng mới, cấp việc, tiếp tục đơn/thao tác kho, lỗi/mất mạng/kết ca và quyền mutex của01/02. Dùng Core giả, SQLite/cổng riêng và executable riêng; không điều khiển robot hoặc dữ liệu vận hành.

## Các vấn đề đã sửa

| Tình huống | Quan sát và xử lý |
| --- | --- |
| Diagnostic hoàn tất nhưng mất ACK | Core vẫn giữ retry nên sau đó có thể chuyển ERROR. Khi diagnostic chứng minh hoàn tất đúng lệnh xác nhận hướng/reset, xóa riêng retry của lệnh đó. ACK đơn thuần vẫn không mở IDLE/khóa hướng. |
| Reset/khôi phục/lift chen vào xác nhận hướng | Lệnh mới có thể thay slot chờ ACK của xác nhận hướng. Core từ chối các thao tác này khi hướng còn pending; nâng/hạ còn chặn khi hướng chưa được xác nhận. |
| Lỗi mới hoặc kết ca trong lúc chờ hướng | Pending cũ còn tồn tại và có thể được hoàn tất muộn. Vô hiệu pending khi ngắt việc/STOP/WAIT/reset/kết ca; giữ khóa hướng, không tự tiếp tục việc cũ. |
| Timeout rồi thử xác nhận lại | Retire đúng retry đã hết hạn. Diagnostic muộn của ID cũ không mở khóa cho ID mới; chỉ ID mới đúng node/boot/kết nối/uptime được nhận. |
| Gateway chọn xe đang khóa hướng | Scheduler trước đó vẫn coi xe IDLE là ứng viên; có thể đặt chỗ hoặc gửi lệnh bị Core từ chối. Lọc các cờ recovery/reset/heading ở phân công, chặng tiếp và nâng/hạ; hiển thị lý do chờ, xe khác đủ điều kiện vẫn được chọn. |
| Resume từ hai tab hoặc API cùng lúc | Bổ sung khóa theo action trong gateway; chỉ một lệnh resume được gửi. Resume đơn/thao tác kho bị chặn trước thay đổi sổ kho nếu xe đang khóa hướng. Lỗi revision trước khi resumeMove thành công không làm ghi ngược sổ kho. |
| Reconnect còn trong FIFO mutex | Rút xe khỏi hàng chờ cũ khi đổi boot/kết nối, giữ owner hiện có. Xe đang khóa hướng không được xin vùng mới. |

Tái hiện trước sửa: bộ Core ban đầu có12 ca thất bại/15 ca (6 tình huống trên mỗi xe) và3 ca mutex qua; ca gateway chọn xe khóa hướng cũng thất bại. Bộ cuối mở rộng thêm reset mất ACK, reconnect và timeout/thử lại.

## Kết quả

- `tests/recovery-conflicts.cpp`: **21/21 PASS**, gồm các trường hợp trên cho01/02, offline giữ owner, hai luồng tranh một mutex, token cũ không nhả quyền owner mới. Binary `.qa/recovery-conflicts-20261029/recovery-conflicts.exe`.
- `tests/shift_recovery.cpp`: **PASS** khôi phục01/02, K1–K6, resume/kết ca/checkpoint; `.qa/recovery-conflicts-20261029/shift-recovery.exe`.
- `tests/cargo-sensor-core.cpp`: **PASS01/02**, proof dừng/cảm biến hàng và chốt nâng/hạ; `.qa/recovery-conflicts-20261029/cargo-sensor-core.exe`. Đây là kiểm tra Core, không biên dịch hoặc chạy mã MCU.
- `node --test --test-isolation=none tests/gateway.test.js tests/orders.test.js tests/warehouse-actions.test.js tests/shift-recovery.test.js tests/cargo-sensor.test.js`: **65/65 PASS**. Sau rà catch/finally, chạy lại hai bộ kho/phục hồi: **13/13 PASS**.
- Browser `tests/shift-recovery.browser.integration.js`: **PASS** khôi phục→chờ reset→chờ hướng→resume→kết ca→hoàn tác→đối soát, desktop/mobile; `.qa/recovery-browser-1QoC0O/`.
- `scripts/build-core.ps1 -OutputDirectory .qa/recovery-conflicts-20261029/core`: **PASS**. SHA256 Core `70873BBCE7D19F78EA759A7C9E567A5BB5FB2F6EE2D4512AFDFB8A6194168EE2`.
- JavaScript syntax/diff whitespace PASS. Hash config01/config02, mạng và hiệu chuẩn pin khớp mốc trước thay đổi. Firmware v24 không thay đổi trong lượt kiểm tra này; không tạo/khôi phục/chạy test MCU.

## Giới hạn

Kết quả chứng minh các tình huống logic đã chạy với dữ liệu kiểm tra, không chứng minh toàn bộ hệ thống hết xung đột vật lý. Chưa thử hai xe thật, điện áp/cảm biến, quãng phanh hoặc hình học thân xe/kệ tại giao lộ. Core/gateway vận hành chưa được thay hoặc restart. Bản build phía trên là bản mới hơn bản kiểm tra khôi phục trước đó trong ADR-092.
