# ADR-082 — Xác minh dừng sau hủy, startup và độ bền Core

Ngày 25/10/2026. SOURCE: người dùng duyệt triển khai danh sách review Core sau đối chiếu mã. Bổ sung ADR-079 và ADR-065.

## Quyết định

1. Hủy task đã giao chốt lịch sử `cancelled`. Robot chuyển `BLOCKED`, `cancel_waiting_for_node=true`, hợp nhất đường cũ vào `uncertain_path`, gửi WAIT và rút FIFO cũ. Owner/token mutex giữ nguyên. Dispatcher, GOTO, về trạm và tự sạc phải chờ proof; ACK không giải tỏa footprint.
2. Diagnostic dừng phải đúng boot/connection đã bind, uptime mới hơn mốc WAIT, node khớp telemetry còn mới, RFID≤250ms, Wi-Fi/MQTT khỏe, motor STOP/duty0/task inactive/error NONE, trạng thái IDLE/WAITING. Nếu có `lastCommandId` thì phải≥lệnh WAIT. Đủ proof mới bỏ flag/uncertain và cho nhận việc. Thay tuyến đang chạy cũng giữ đường cũ tới proof cùng điều kiện.
3. Sau 10 giây chờ proof, Core log cảnh báo rồi tối đa mỗi 30 giây; snapshot có tuổi chờ. UI giải thích cần đưa xe về RFID và xác minh dừng. Dừng giữa hai thẻ tiếp tục chờ, không tự nhả mutex do timeout.
4. `main` bắt exception khởi động, báo ngữ cảnh file/cấu hình và exit1; listener dùng RAII. Parsing port/sinh phiên Core thực hiện trong phạm vi bắt lỗi. File hỏng không bị xóa/rewind. Counter hợp lệ có thể tiến thêm block khi khởi động thử, theo quy tắc không tái sử dụng ID.
5. Migration v3→v4 gộp `stopped_nodes` vào vùng chưa xác minh, đổi C sang cả hai đầu cạnh, rồi xóa proof dừng cũ. Restart vẫn OFFLINE và bảo thủ; giữ backup/owner/token/lịch sử theo ADR-065.
6. `durableSnapshot` dựng trực tiếp nhóm bền vững: robot ID/node/pin/recharge/state/path/target/task/uncertain/stopped, records/queue/sequence/reservation/accepting. Diagnostic/heading/độ mới/lift freshness không làm checkpoint đổi. Restore không tin lại proof/capability/heading từ đĩa.
7. Odometry/sonar lấy hai node và một cạnh qua `edgeGeometry`. Shift tái sử dụng fleet/mutex snapshot, chỉ làm mới sau nhóm lệnh có thể đổi trạng thái. Telemetry không đổi và tick quét dispatch tối đa mỗi 250ms; đổi node/task/sự kiện quan trọng vẫn quét ngay. Snapshot có `planning.dispatch_scans`. Không cache chỉ theo map revision vì xe/mutex/footprint cũng đổi đường.
8. Commit atomic>500ms log và tăng `storage_metrics` (`last_commit_ms/max_commit_ms/slow_commits`). Vẫn checkpoint thành công trước publish; retry Windows2s giữ chính sách cũ. Metric không loại bỏ I/O đồng bộ dưới khóa.
9. Diagnostic vượt 64 lần đổi boot có log một lần và cờ UI `encoder_session_limit_reached`. ID task cấp sau validation target/priority/định dạng ID xe. Save map bỏ obstacle TTL, giữ obstacle vĩnh viễn và không đổi map RAM; default map tăng revision.
10. Giữ firmware gửi lại mutex request mỗi 1s khi bị blocked. Comment arbiter ghi rõ false có thể là từ chối, không đảm bảo đã vào FIFO. Gom vai trò cố định tại `PhysicalLayout.hpp`; map vật lý tên `Kho AGV...` được kiểm S/K và cặp cạnh gate khi khởi động scope. Đổi tên RFID tổng quát vẫn cần cập nhật bảng route MCU cùng Core; đồ thị fixture không nhận profile vật lý.
11. Header JSON thay nguyên byte bằng release3.12.0/MIT; hash trong `check-vendor.ps1`, giữ byte qua Git và loại khỏi tool sửa include. Backup cũ tại `.qa/core-hardening-20261025/json-before.hpp`. Alias json/Clock khai báo tại `CoreTypes.hpp`; entrypoint include rõ ràng. Sạc thủ công nhả reservation logic khi đã đỗ, vị trí RFID tiếp tục bảo vệ trạm.

## Kiểm chứng và giới hạn

Runner `scripts/test-core-hardening.ps1` dùng `.qa` riêng; `-Integration` thêm broker MQTT riêng, `-Browser` thêm Core giả/DB riêng. Kết quả thực tế tại `tests/README.md`.

Chưa thay binary vận hành hoặc nạp/chạy robot. Proof dừng/RFID là mô hình logic, chưa đo quãng phanh. Không bỏ chốt bằng timeout. Map tổng quát/đổi tên thẻ cần hợp đồng phần cứng mới. Ghi đĩa vẫn đồng bộ trước publish; không cam kết mọi commit<500ms.
