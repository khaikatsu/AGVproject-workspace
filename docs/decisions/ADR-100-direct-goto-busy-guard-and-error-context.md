# ADR-100 — Chặn GOTO chồng tuyến và lưu ngữ cảnh INVALID_COMMAND

Ngày30/10/2026. SOURCE: người dùng báo đã khôi phục rồi ra lệnh đi tiếp nhưng bị Invalid command, yêu cầu tiếp tục xử lý.

## Bằng chứng và giới hạn

OBSERVED qua GET API: lịch sử có INVALID_COMMAND xe01/task-105510 tại G4 và xe02/task-105511 tại G2. Log xe02 có hai mốc `direct_goto` cùng11:35:38 và cùng11:38:30 trước lỗi. Khi kiểm tra, xe02 đã về S2/IDLE; xe01 OFFLINE ở G4, còn footprint chưa xác minh. Log cũ không lưu payload hoặc nhánh kiểm tra MCU đã từ chối, nên chưa xác định chắc chắn nguyên nhân của từng lần lỗi. Không phát lại lệnh để tái hiện trên xe thật.

OBSERVED từ mã: `FleetManager::directGoto` chỉ chặn WORKING/BLOCKED khi `nodeOrdersSupported=true`. Capability chưa tới hoặc mất có thể cho GOTO ghi đè tuyến; firmware từ chối GOTO nếu navigator còn active. Đây là lỗ hổng đã xác định từ mã, không phải bằng chứng mọi lỗi lịch sử đều do gửi chồng tuyến.

## Quyết định

- Core kiểm tra WORKING/BLOCKED hoặc task/target/path chưa kết thúc độc lập capability trước khi thay tuyến. Trả command_result409, không gửi GOTO mới, không đưa task đang chạy về queue, giữ tuyến/task cũ. Bộ cấp ID có thể tiến khi yêu cầu bị từ chối; không rewind counter.
- Web chỉ cho chọn xe IDLE, không task/target/path, không lỗi/lift active/reset/hướng chưa xong/chờ proof. Cùng điều kiện cho thẻ xe, marker, dropdown và bước gửi form. Hàng đợi tự chọn xe vẫn giữ scheduler hiện có. Core vẫn là chốt có thẩm quyền khi snapshot web cũ hoặc có hai client.
- Khi nhận INVALID_COMMAND, Core chụp `core_context` trước interrupt/STOP: node, task_id, target, route_cmd_id, route_start, path, heading/source và payload đang chờ ACK nếu có. Đây là ngữ cảnh cuối Core biết, không khẳng định đó là payload MCU đã từ chối. Giữ lỗi nguyên gốc, ERROR/STOP và footprint/mutex; không tự reset/resume.
- Không đổi payload MQTT/WS schema, firmware, hướng sau phục hồi, motor, pin hoặc tham số riêng01/02. Cảhai xe dùng chung chốt. Không nới nhánh kiểm tra GOTO ở MCU.

## Kiểm chứng và áp dụng

Core đã build riêng tại `.qa/invalid-command-recovery-20261030/agv_core.exe`, SHA256 `4DF9A147A8DD098BC884C0A9C8E54BCEB318DD2D7309B168CE33EDE11F33CE0D`. Không ghi đè hoặc restart Core vận hành.

Edge headless `tests/vehicle-cards.browser.integration.js` PASS desktop/mobile với Core giả/SQLite riêng, gồm01/02 bận/blocked/task chưa xóa/reset/lift active không gửi GOTO, phạm vi, mất dữ liệu, reconnect và phục hồi. Fixture phục hồi cũ đã cập nhật hợp đồng hiện tại: ngoài trạm dùng hướng và recover_and_continue, thay trường vehicleRecoveryMoved đã bỏ. Bằng chứng `.qa/vehicle-cards-y7zgNQ`, log `.qa/invalid-command-recovery-browser.log`.

Bộ `core_tests` tổng biên dịch được nhưng dừng ở fixture `after-mid-route-reset`: fixture không cung cấp hướng, trạng thái còn heading_confirmation_required, directGoto trả `Can xac nhan huong dau xe truoc khi chay`. Chốt này nằm trước và không đổi trong bản sửa; probe `.qa/invalid-command-recovery-20261030/reset-probe.cpp` xác nhận thông báo. Không nới chốt hoặc ghi PASS toàn bộ hồi quy. Kiểm thử tập trung `tests/direct-goto-guard.cpp` PASS cả01/02 với capability unknown/present/lost: command_result409, không GOTO/requeue chồng tuyến, giữ task/path/state cũ, lỗi vẫn STOP/ERROR và log có ngữ cảnh trước interrupt. Log `.qa/invalid-command-recovery-focused.log`.

Chưa áp dụng Core vận hành hoặc chạy xe thật. Firmware không sửa/build/test/nạp; build ID source giữ `20261030-robot01-cargo-monitor-v28` / `20261030-robot02-cargo-monitor-v28`. Muốn xác định nhánh INVALID_COMMAND lịch sử cần payload/log MCU lúc lỗi; ngữ cảnh mới hỗ trợ lần điều tra tiếp theo sau khi Core mới được áp dụng.
