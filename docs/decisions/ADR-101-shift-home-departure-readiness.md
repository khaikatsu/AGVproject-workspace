# ADR-101 — Xác minh điều kiện rời gate khi kết ca về trạm

Ngày30/10/2026. SOURCE: người dùng báo task vừa hoàn thành, bấm kết ca thì xe01 INVALID_COMMAND và xe02 STALE_COMMAND; yêu cầu kiểm tra và sửa. Mở rộng chốt GOTO của [ADR-100](ADR-100-direct-goto-busy-guard-and-error-context.md) sang đường về trạm; giữ vòng đời ca [ADR-096](ADR-096-durable-shift-closure-and-reconciliation.md).

## Bằng chứng

OBSERVED12:30+07: Core vận hành PID13700, `build/agv_core.exe`, phiên `1793337029107567300-479159796`, pha CLOSING. Log kết ca12:29:05; xe01 nhận tuyến G2→M5→G3→M3→G1→M1→GS1→S1, route_cmd_id156609. Khi báo INVALID_COMMAND cùng giây, core_context đã có heading rỗng/source unknown. Xe02 STALE_COMMAND12:29:07, không có core_context ở bản trước vì chỉ ghi ngữ cảnh INVALID_COMMAND.

Diagnostic đọc thụ động sau lỗi: xe01 firmware cargo-monitor-v28, SAFE_STOP/INVALID_COMMAND/G2/heading UNKNOWN, lastCommandId156610; xe02 v28, IDLE/NONE/G5/heading E, lastCommandId156612. Không suy xe02 đã khôi phục lỗi Core từ diagnostic khỏe hoặc suy tất cả STALE_COMMAND là lỗi counter. Counter chỉ được đọc, không xóa hoặc rewind. Chưa có payload/nhánh từ chối MCU của xe02 để kết luận chính xác lần lỗi đó.

OBSERVED từ mã: `returnToHome` gọi assignPath khi có RFID/vị trí/cargo hợp lệ nhưng không bắt buộc hướng thực tế còn hợp lệ hoặc diagnostic STOP khỏe sau chặng trước. Core có thể coi task đã xong theo RFID trong khi MCU còn đang xử lý tuyến; đường về trạm không đi qua `directGoto` nên chốt ADR-100 không áp dụng. `assignPath` chỉ chặn cờ recovery heading, không chứng minh heading khác UNKNOWN. Khả năng lệnh sau bị inhibit khi STOP/WAIT ưu tiên hoặc quá tuổi vẫn cần payload/log MCU để xác minh; không coi đó là nguyên nhân đã chứng minh của xe02.

## Quyết định

- Trước **tuyến về trạm mới**, `returnToHome` kiểm tra trạng thái phục hồi/hướng/reset và lift/dropoff-exit. Với MCU có encoder session hoặc capability motion, bắt buộc diagnostic hướng mới≤1500ms, hướng N/E/S/W đúng RFID hiện tại; proof STOP khỏe đúng boot/kết nối, RFID≤250ms và telemetry mới; không còn lệnh đang chờ ACK. Thiếu proof trả lý do chờ, không gán hướng, phát GOTO hoặc chuyển ERROR bằng assignPath thất bại.
- ACK chỉ là biên nhận. Kết ca khi còn task vẫn phát WAIT và giữ cancelNeedsNodeProof; chỉ diagnostic khỏe mới hơn, đúng phiên/node và high-water phù hợp mới cho rời gate. Không dùng ACK thay proof dừng, không replay task/lift cũ.
- Tuyến về trạm đã phát tiếp tục theo cơ chế hiện hành; tick kết ca không cấp GOTO lặp. Nhánh BLOCKED tiếp tục tuyến về trạm dùng cùng chốt trước khi assignPath, tránh đi vòng kiểm tra ở lần retry.
- Snapshot ca phân biệt xe IDLE đang chờ hướng hoặc proof dừng với xe đang về trạm. Log STALE_COMMAND cũng chụp core_context trước interrupt như INVALID_COMMAND, thêm high-water diagnostic và stop_command_id; giữ nguyên lỗi, footprint/mutex và chính sách không phát STOP thấp khi STALE_COMMAND.
- Không đổi firmware01/02, payload MQTT/WS, ngưỡng timeout, cấu hình mạng/pin/PWM, kho hoặc counter. Không tự gỡ lỗi vận hành, xác nhận hướng vật lý hoặc ép CLOSED.

## Kiểm chứng và áp dụng

Build riêng `.qa/shift-return-build-20261030/agv_core.exe` thành công; SHA256 `2D45C5954A93EBA026157E2DCA903019E965B4A6D8F451C9D59DBF5E0E3B4EED`. Chưa ghi đè/restart Core vận hành hoặc chạy xe thật. Firmware không sửa/build/test/nạp, source build ID giữ `20261030-robot01-cargo-monitor-v28` / `20261030-robot02-cargo-monitor-v28`.

`tests/shift-return-readiness.cpp` PASS cho01/02 x hướng UNKNOWN, MCU còn task, motor còn chạy, RFID cũ, thiếu ACK và sai boot: không GOTO khi chưa đủ proof, không phát lỗi giả, STOP khỏe sau đó chỉ cấp một tuyến về trạm, tick không lặp. Kết ca giữa task: ACK WAIT hoặc diagnostic có high-water cũ chưa được về; proof mới đúng WAIT cho về và không khởi lại task cũ. `tests/shift-lifecycle.cpp`, `tests/home-heading.cpp`, `tests/direct-goto-guard.cpp` PASS giữ CLOSED/restart/quyền mở ca/mutex, hướng đúng tại home và chống GOTO chồng. Log tại `.qa/shift-return-fix-20261030/`. Không chạy lại bộ core_tests tổng vốn đang dừng ở fixture thiếu xác nhận hướng đã ghi ADR-100.

Để áp dụng cần dừng đúng Core đang chạy và build/chạy binary từ source mới. Chỉ restart binary cũ không cập nhật sửa lỗi; không cần nạp firmware cho bản sửa này. Các lỗi thật/heading UNKNOWN đang chốt phải được người vận hành phục hồi theo vị trí và hướng thực tế, không tự xóa bằng việc cập nhật source.
