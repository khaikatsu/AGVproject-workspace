# ADR-052 — Giữ nhịp MQTT khi chạy Core bằng F5 trên Windows

Ngày: 14/10/2026.

`SOURCE`: người dùng báo telemetry treo đến13giây, server báo lỗi, xe phục hồi trạng thái rồi lặp lại.

`OBSERVED`: Core PID24628 chạy dưới GDB PID12436, gần một lõi CPU. Trace18:36 có heartbeat ngừng5575ms, vượt watchdog MCU3000ms; boot1015295379 giữ nguyên nhưng connection session6→11. Firmware đã là `20261014-robot01-network-clock-fix`. ACK tới broker trong0,1–0,4giây nhưng Core vẫn retry cùng ID. Có bằng chứng Core xử lý trễ; không suy ra mọi lần lỗi trước đó đều cùng nguyên nhân.

## Quyết định và căn cứ

- Thêm `_NO_DEBUG_HEAP=1` vào environment cấu hình F5 `.vscode/launch.json`. Giữ `-g`, breakpoint và watchdog. Microsoft mô tả biến này trong [General Environment Variables](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/general-environment-variables) và [giải thích heap của debugger](https://devblogs.microsoft.com/oldnewthing/20130103-00/?p=5653). Chỉ tham khảo tài liệu ngày14/10/2026, không sao chép mã upstream.
- Đối chứng cùng executable `-g`, bản sao chỉ đọc của400task: snapshot+checkpoint JSON trung bình16,50ms chạy trực tiếp;314,29ms dưới GDB mặc định;16,09ms với biến môi trường;14,23ms khi chỉ đặt biến cho inferior qua `set environment` như F5. Không ghi runtime vận hành. Debug heap tăng chi phí cấp phát JSON, giữ lock trạng thái lâu và làm trễ MQTT/heartbeat.
- Giữ heartbeat phụ thuộc tiến triển điều phối/checkpoint: watchdog vẫn phải dừng xe nếu Core thật sự treo. Không tăng timeout hoặc xóa lịch sử để che độ trễ.
- Thêm `tests/core-heartbeat.integration.js`: broker/cổng/counter/runtime riêng,1000task terminal, telemetry/diagnostic/encoder5Hz, WS và ACK robot giả; hỗ trợ GDB. Không thử chuyển động xe thật.

## Áp dụng và kiểm chứng

Fixture `.qa/heartbeat-test-b4iGck`:20giây,99heartbeat, gap tối đa278ms;99mẫu encoder lên WS, trễ tối đa197ms. Giữ đủ1000task; không GOTO/NODE_ORDER.

18:48:23 xác nhận ca robot01 CLOSED, không pending/dispatched task, không path/uncertain_path/target và không owner/waiting mutex; diagnostic thật STOP/PWM0/0/taskActive=false. Lưu Core/state/counter trước trong `.qa/core-network-apply-20261014`, dừng đúng PID24628, đặt Core đã qua unit/fixture vào `build/agv_core.exe`, chạy nền `--mode robot01` ngoài debugger PID28972. SHA256957ABB39F4B1A1CB9276658CAD31F8C23A7B60A67F43935CFC50C1E61EC82A44. Không gửi thao tác GOTO/RESET_FAULT/xóa lỗi hoặc rewind counter; checkpoint được Core ghi bình thường. Gateway/broker hiện có giữ nguyên.

Trace thật18:48:37–18:49:22: telemetry44/diagnostic44/odometry218/heartbeat221, gap tối đa1047/1078/307/269ms. Boot356294169/connection55 ổn định, không offline/error event/command. Firmware clock-fix được xác nhận qua diagnostic, agent không upload lượt này. Artifact `.qa/encoder-network-live/run-2026-10-14T11-48-37-232Z`.

`OPEN`: xe vẫn SAFE_STOP/NETWORK_UNAVAILABLE, RFID UNKNOWN. Cần người vận hành đưa tới RFID hợp lệ và phục hồi lỗi trước thử chạy. Trace xe dừng chưa nghiệm thu nguồn5V L298N/mạng khi motor chạy. Dừng tại breakpoint vẫn có thể kích hoạt watchdog đúng thiết kế. Muốn F5 lại phải dừng Core nền hiện có trước vì cổng8080/executable đang dùng.
