# Báo cáo kiểm thử lỗi và xung đột AGV — 02/10/2026

SOURCE: người dùng yêu cầu mô phỏng tình huống lỗi, sửa lỗi/xung đột và gửi kết quả. REQ-SIM-02, [ADR-011](decisions/ADR-011-simulation-conflict-audit.md).

## Kết quả cuối

VERIFIED: **16/16 bộ kiểm thử PASS**, mất tổng 290,26 giây chạy các bộ trong audit tổng hợp. Trong đó ma trận mới có **15/15 nhóm tình huống PASS**; stress hai xe thực hiện 30 lượt chọn đích đồng thời với seed `20261002`. Phiên stress cuối có 59 task completed, 0 interrupted; một yêu cầu tuyến bị Core từ chối 409, không bị gán thành công. Không ghi nhận hai xe cùng đứng một node hay đi ngược trên cùng cạnh trong stress này.

Bằng chứng tổng hợp: [results.json](../.qa/full-audit-oFeZMD/results.json), [log ma trận lỗi](../.qa/full-audit-oFeZMD/simulation.faults.log), [kết quả từng tình huống](../.qa/fault-audit-HtSJih/results.json). Mỗi ca có map, DB, counter, checkpoint, log Core/broker, events.jsonl và report.json riêng. Hash bản đồ/counter/runtime/shift vận hành trước/sau không đổi. DB vận hành có gateway riêng đang ghi nên không yêu cầu nó đứng yên; DB thử nghiệm nằm trong `.qa/`.

Môi trường: Windows, Node 24, C++17/GCC, libmosquitto, Mosquitto loopback riêng, SQLite fixture, Edge headless/Playwright. Core audit được build từ source vào `.qa/audit-build`, tránh ghi đè executable đang chạy. Sau audit đã build lại `build/agv_core.exe` và unit test PASS. Không upload hoặc chạy robot thật.

## Các lỗi tái hiện và biện pháp bổ sung

| Lỗi OBSERVED | Bằng chứng trước sửa | Bản sửa |
| --- | --- | --- |
| Hai tuyến fallback cùng qua G3/G1, xe điểm cùng đứng một node | [F01 thất bại](../.qa/fault-audit-pNjKso/run-1uT1OS/failure.json) | Giữ các node thường của tuyến; fallback không bỏ qua đặt trước. Lệnh riêng có topology hợp lệ giữ BLOCKED/WAIT nếu chưa có đường trống, rồi tick thử lại |
| Nguy cơ hai mutex nối trực tiếp tạo giữ-chờ chéo ở tuyến ngược | Phân tích hold-and-wait; fixture F11 với M5↔M6 chỉ chạy sau sửa, chưa có trace lỗi trước sửa | Giữ thành phần mutex liền kề như một hành lang. Xe thứ hai chờ ngoài; FIFO vẫn dùng cho mutex đơn lẻ |
| Sau Core restart, xe rời vùng bằng token cũ bị từ chối; owner không được giải phóng và ca không đóng | [F05 thất bại](../.qa/fault-audit-pNjKso/run-4FMRAd/failure.json) | Simulator xác minh lại, xin token generation mới tại vùng đang chiếm trước khi bỏ dừng; vùng ngoài vị trí điểm đã xác nhận được clear với token hiện tại |
| Broker restart nhanh không có LWT tới Core: task vẫn dispatched trong khi xe giữ dừng | [F06 thất bại](../.qa/fault-audit-pNjKso/run-zQvr7n/failure.json) | Callback mất broker chuyển task interrupted, giữ ERROR/uncertain_path/mutex. Reconnect không tự cho chạy |
| Nguy cơ nhiều thao tác phục hồi cùng xe chồng lên nhau | Regression F15 sau bổ sung khóa, chưa ghi nhận sự cố trên phiên người dùng | Khóa thao tác đang xử lý theo xe; thao tác chồng trả 409 và chờ kết quả |

Dijkstra có trọng số, FIFO MutexArbiter, trạm riêng, vòng đời task/ca và payload MQTT giữ nguyên. Thay đổi ràng buộc điều phối có thể tăng chờ/chọn đường dài hơn; không tuyên bố tối ưu tổng đội xe. Luồng lấy/trả vẫn ở simulator, task Core vẫn là điều hướng.

## Ma trận 15 nhóm

| ID | Tình huống và kết quả cần đạt | Kết quả |
| --- | --- | --- |
| F01 | 30 lượt đích kho hai xe; không cùng node hoặc cạnh ngược; tuyến bị từ chối giữ đúng kết quả | PASS |
| F02 | Hủy giữa cạnh: dừng, task cancelled; giao mới cùng hướng không chạy lại task cũ | PASS |
| F03 | Node bị chặn TTL: BLOCKED, hết TTL tiếp tục đúng task ID | PASS |
| F04 | Hai xe cùng pin15 sau khi tới kho: về S1/S2 và hoàn tất sạc | PASS |
| F05 | Core restart khi01 giữ M5: giữ owner/recovery, ID tăng, phục hồi token và về trạm, đóng ca | PASS |
| F06 | Broker restart giữa chuyến: xe giữ vị trí, task interrupted, phục hồi rõ ràng | PASS |
| F07 | Release sai chủ/token, ACK sai, telemetry sai node/pin không làm đổi quyền/vị trí hợp lệ | PASS |
| F08 | Hủy chặng trả hàng: job interrupted, hàng còn trên xe; không báo trả thành công | PASS |
| F09 | Đóng ca giữa task và có queue: xong task cuối, về trạm, giữ queue sang ca mới; lệnh muộn bị từ chối | PASS |
| F10 | API/body/pin/node sai, sửa cấu trúc map khi có xe: trả lỗi đúng và giữ fleet | PASS |
| F11 | M5↔M6: hai tuyến ngược, xe02 chờ ngoài hành lang; cả hai hoàn thành | PASS |
| F12 | Telemetry đứt nhưng MQTT sống: model tới đích không làm task completed; Core timeout/giữ đường, phục hồi | PASS |
| F13 | Mất grant đầu: không vào vùng; retry cùng generation mới tiếp tục | PASS |
| F14 | Mất toàn ACK: retry rồi interrupted/ERROR, không tự chạy lại; ACK trễ không xóa lỗi | PASS |
| F15 | Hai phục hồi đồng thời cùng xe: một thao tác bị từ chối, xe phục hồi đúng | PASS |

## Regression toàn hệ thống

Các bộ đều PASS: core; gateway/security (11 subtest); virtual robot; ma trận lỗi; low-battery; simulation integration; simulation mutex; system integration; map integration; hardening integration; shift integration; firmware integration; TLS integration; startup integration; browser integration; simulation browser.

Ngoài ma trận trên còn kiểm chứng: weighted route/tie-break, node/cạnh không hợp lệ, task ID qua restart, queue/mutex checkpoint, tám MQTT client FIFO/grant ACK/retry, Origin/token/ACL/TLS/LWT, DB history không đoán completion, delta/throttle, firmware parser/dedup, snapshot/map validation, offline giữ vùng, nhiều chu kỳ sạc, 0→15→phục hồi, browser desktop/laptop/mobile và tải report.

Ảnh browser: [xe về sạc](../.qa/run-e57SQS/simulation-returned-to-charge.png), [chỉ dẫn phục hồi](../.qa/run-e57SQS/simulation-battery-recovery.png). Ảnh dùng fixture kiểm thử; không phải thử trên phần cứng.

## Chạy lại

```powershell
./scripts/build-core.ps1 -Tests -OutputDirectory .qa/audit-build
$env:AGV_CORE_BIN = Join-Path (Get-Location) '.qa/audit-build/agv_core.exe'
$env:AGV_CORE_TEST_BIN = Join-Path (Get-Location) '.qa/audit-build/core_tests.exe'
$env:AGV_PLAYWRIGHT_MODULE = 'C:/Users/DELL/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright'
$env:AGV_BROWSER_CHANNEL = 'msedge'
node tests/run-simulation-audit.js --browser
```

Máy khác cấu hình đường dẫn Playwright/browser tương ứng. Không có Playwright thì bỏ `--browser`; kết quả khi đó chỉ gồm các bộ không có trình duyệt. Chạy riêng nhóm mới: `node tests/simulation.faults.js`, hoặc thêm ID `F05 F06 F11`.

## Giới hạn còn mở

Đây là S1 logic/hợp đồng, xe là điểm. Chưa xác nhận thân xe, cạnh cắt nhau về hình học, phanh, quay, motor, nhiễu RFID, pin/bộ sạc thật, Wi-Fi thực hoặc firmware MCU reboot/NVS bằng board. Không dùng các kết quả này để khẳng định mọi tổ hợp lỗi đều đã thử hoặc tránh va chạm vật lý.

Seed chọn đích cố định; Core/clock/MQTT vẫn thật nên timing và số yêu cầu bị từ chối có thể khác giữa các lần chạy. Các đồng hồ task/job gồm thời gian chờ; waitSeconds của model không phải toàn bộ thời gian Core giữ BLOCKED. Telemetry pipe v0 chưa có timestamp/sequence/session của producer: không đảm bảo phân biệt mọi dữ liệu vị trí trễ nhưng vẫn đúng cú pháp. Nếu xe hỏng chiếm vùng và topology không có đường tránh, hệ thống giữ chờ cần xác minh; đó là kết quả an toàn dự kiến, không tự nhả vùng vì timeout.
