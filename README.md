# Workspace nghiên cứu hệ thống AGV trong kho

Đề tài: **Xây dựng hệ thống quản lý robot tự hành trong kho dựa trên nền tảng IoT**. Workspace phục vụ hai robot đi theo lộ trình định trước và web giám sát bản đồ, lộ trình, pin, lỗi theo thời gian thực.

Đây là bộ nền tài liệu, kỹ năng và quy trình phát triển trên mã đang có. Các lựa chọn phần cứng, tiêu chí nghiệm thu và thiết kế mở rộng vẫn chờ yêu cầu chi tiết. Ngày lập bộ nền: 28/09/2026.

## Bắt đầu

1. Đọc [tổng quan đề tài](docs/00-project-brief.md) và [hiện trạng mã](docs/02-current-system.md).
2. Xem [kiến trúc](docs/01-architecture.md), rồi chọn tài liệu chuyên môn bên dưới.
3. Đặt mô tả mới vào `docs/requirements/inbox/`; dùng [mẫu yêu cầu](docs/requirements/template.md). Cập nhật [sổ yêu cầu](docs/requirements/register.md) trước khi triển khai.
4. Khi phát triển bằng Codex, dùng kỹ năng tương ứng trong [.agents/skills](.agents/skills). Ví dụ: `$agv-firmware` hoặc `$agv-algorithms`. Có thể cần mở lại workspace để ứng dụng khám phá kỹ năng mới.
5. Mở [AGVproject.code-workspace](AGVproject.code-workspace) nếu dùng VS Code. Kiểm tra tài liệu bằng `python scripts/check_workspace.py`.

## Bản web/server mới

Đã chạy [audit lỗi/xung đột 02/10/2026](docs/testing-simulation-audit-2026-10-02.md): 16 bộ PASS, 15 nhóm lỗi và stress 30 lượt hai xe. Đã sửa đặt trước đường thường/hành lang mutex, mất broker và phục hồi sau restart; giữ Dijkstra/FIFO, payload và dữ liệu vận hành. Runner: `node tests/run-simulation-audit.js --browser` sau khi build/cấu hình theo báo cáo.

Đã có [web clone mô phỏng hai xe ảo](simulation/README.md), dùng Core C++ và thuật toán hiện có: `./scripts/run-simulation.ps1` rồi mở `http://127.0.0.1:3100/simulation`. Pin/lấy trả hàng/mất mạng/ACK và số liệu thời gian/quãng đường dùng broker, map, counter, DB riêng. Đây là S1 logic, chưa phải kiểm chứng vật lý.

Ngày 05/10/2026 từng thêm tự khởi động toàn bộ mô phỏng sau đăng nhập Windows; yêu cầu ngày 06/10/2026 thay cơ chế đó.

Ngày 06/10/2026 đổi theo yêu cầu mới: cổng 3100 chỉ là điểm mở trang; mô phỏng chạy khi có tab xem và tự dừng sau khi tab cuối đóng. Xem [cách dùng](simulation/README.md) và [ADR-020](docs/decisions/ADR-020-simulation-on-view.md).

Web mô phỏng có [đơn hàng nhiều kiện](simulation/README.md): nhập một kho lấy và danh sách kho giao/số kiện; server phân từng chuyến cho hai xe ảo, Core lập đường và giữ quyền vùng chung. Kịch bản 5 kiện, chạy song song khi đường không xung đột và xử lý lỗi đã được kiểm tra trong môi trường riêng.

Ngày 09/10/2026 có [Core thử điều phối hai xe](docs/decisions/ADR-025-two-robot-supervised-trial.md) với `AGV_TWO_ROBOT_TRIAL=1` và [script chạy riêng](scripts/run-two-robot-trial.ps1). Ca thử yêu cầu cả 01 ở S1 và 02 ở S2, giữ mutex và cho phép giao việc khi pin điều khiển còn `?`. Bản firmware bench chỉ dùng trên giá kê; đầu đọc rời thẻ M chưa phải bằng chứng toàn bộ xe đã rời vùng.

[Phục hồi lỗi/mutex tại trạm riêng](docs/decisions/ADR-026-safe-automatic-recovery-at-home.md) đã có trong mã Core và firmware: xe lỗi rời hàng đợi mutex; quyền đang giữ chỉ trả khi telemetry và diagnostic mới xác nhận xe dừng tại S1/S2, đủ xa vùng theo map đo. Core tự xóa lỗi sau diagnostic MCU khỏe, và chỉ thử reset tự động một lần cho lỗi mạng. Chưa thay Core đang chạy hoặc nạp firmware mới; cần hoàn tất fixture MQTT, build firmware và thử bàn trước khi dùng trên xe.

Sau khi người dùng đo tầm đọc RC522 8 cm và xác nhận khoảng cách map, firmware trong workspace đã có nhả mutex theo thẻ RFID kế đủ xa hoặc chuỗi thẳng 35+35 cm, kèm grant ID. Bản thường/giám sát vẫn khóa motor; ảnh bench xe 01 hiện dùng PWM cơ sở 150, xe 02 dùng 120 trên giá kê, cùng có chốt điện áp ADC 6,4–8,6 V. Native test ba env 81/81 cho bản bench trước khi chỉnh PWM; ảnh xe 01 PWM 150 đã build và nạp/verify vào bo mới. Bấm node trên bản đồ khi chưa chọn robot tạo task để Core chọn xe khả dụng gần nhất theo tổng trọng số đường; pin điều khiển `?` được bỏ qua chỉ trong chế độ thử hai xe. Kiểm thử Core và browser bằng fixture riêng đã PASS; chưa xác nhận hai xe hoàn thành lấy/trả hàng thật. Khi ca đang chạy, mất MQTT hoặc RFID `UNKNOWN` đưa xe về trạng thái cần xác minh; kiểm tra trạng thái Core mới nhất trước mỗi lần xóa lỗi/mở ca.

Đã bổ sung [gia cố 01/10/2026](docs/18-hardening.md): auth/TLS/ACL, grant ACK, checkpoint queue/mutex, Core cấp task-ID, failsafe/LWT, delta/throttle và retention. Core/sketch cần cập nhật đồng bộ mutex v0.1. Mặc định gateway chỉ localhost; chạy LAN cần cấu hình token/HTTPS và MQTT TLS. [Docker/Compose](deploy/compose.yaml) và [CI](.github/workflows/ci.yml) có cấu hình, chưa xác nhận runtime Docker/GitHub.

Đã đồng bộ giao diện/API và core WS v1. Xem [chức năng và cách chạy](docs/14-web-server-update.md), [hợp đồng HTTP/WS](contracts/web-server.md) và [kiểm thử](tests/README.md). Chạy core bằng `./scripts/run-core.ps1 -Build`, rồi `npm start` trong AGVwebserver. Chỉ mở một core và một gateway; [hướng dẫn lỗi kết nối/chạy trùng](docs/16-startup-troubleshooting.md).

Dashboard 01/10/2026 có bốn khu vực Tổng quan/Nhiệm vụ/Bản đồ/Báo cáo, thanh trạng thái Web–Core–MQTT và bản đồ phóng to trên mobile. Các thao tác server cũ vẫn ở đúng khu vực; xem [hướng dẫn giao diện](docs/03-web.md).

Bản đồ v3 có hai RFID giữa `M5/M6`, tên node ngắn và kho phía trong đặt ở trung điểm. Xem [bảng tên, tọa độ và kiểm chứng](docs/15-map-layout-update.md).

Đã thêm [ca làm việc và tự về trạm sạc](docs/17-shifts-and-home-charging.md): 01 → S1, 02 → S2; mở core chờ RFID/tập kết, nút bắt đầu/kết thúc ca và hoàn thành task đang chạy trước khi về trạm. Sketch RC522 đã cập nhật khớp luồng ca.

Đã có [sketch Arduino ESP32/RC522](firmware/AGV_Connection_Test/AGV_Connection_Test.ino) và [hướng dẫn cấu hình/nối dây/learn UID](firmware/AGV_Connection_Test/README.md), MQTT pipe/JSON/ACK v0 khớp server, boot chờ RFID thay cho G1 giả lập. Ngày 03/10/2026 đã tích hợp driver L298N/line/HC-SR04 theo bảng chân mới; motor khóa mặc định chờ kiểm thử bàn, bộ sạc thật chưa tích hợp. [ADR-012](docs/decisions/ADR-012-motor-firmware-integration.md).

Ngày 05/10/2026 đã mở rộng [chế độ thử riêng xe 01](docs/decisions/ADR-017-solo-full-map-trial.md): Core dùng Dijkstra cho GOTO trực tiếp tới mọi node có đường trên map, firmware `esp32dev_trial` đi từng RFID và mở PWM; bản `esp32dev` thường vẫn khóa motor. Đã build và kiểm thử phần mềm; chưa nạp hoặc xác nhận xe thật chạy. [Cách chạy và giới hạn](firmware/NCKHAGV/NCKHAGV/README.md).

## Tài liệu theo mảng

| Mảng | Tài liệu | Kỹ năng |
| --- | --- | --- |
| Web và giao diện giám sát | [Web](docs/03-web.md) | [agv-web](.agents/skills/agv-web/SKILL.md) |
| Server C++ và gateway Node.js | [Server](docs/04-server.md) | [agv-server](.agents/skills/agv-server/SKILL.md) |
| Tìm đường và điều phối hai robot | [Thuật toán](docs/05-algorithms.md) | [agv-algorithms](.agents/skills/agv-algorithms/SKILL.md) |
| Mô phỏng logic và vật lý robot | [Mô phỏng](docs/06-simulation.md) | [agv-simulation](.agents/skills/agv-simulation/SKILL.md) |
| MQTT HTTP WebSocket và kết nối nội bộ | [Mạng](docs/07-network.md) | [agv-network](.agents/skills/agv-network/SKILL.md) |
| Vi điều khiển kết nối server | [Firmware](docs/08-firmware.md) | [agv-firmware](.agents/skills/agv-firmware/SKILL.md) |

Xem thêm [đánh giá và giải pháp hoàn thiện](docs/13-gap-analysis-and-solutions.md), [kiểm thử và nghiệm thu](docs/09-testing.md), [lộ trình](docs/10-roadmap.md), [repo GitHub tham khảo](docs/11-github-references.md), [hướng dẫn chạy](docs/12-development.md) và [ghi nhận nguồn](THIRD_PARTY_REFERENCES.md).

[Lưu đồ xử lý server và giải thích lựa chọn thuật toán](docs/19-server-algorithm-flowchart.md) đối chiếu các luồng web, MQTT, Core, phân công hai xe và phục hồi với mã hiện tại.

## Cấu trúc

```text
AGVproject/
  AGVwebserver/             Web HTML, Express, WebSocket, SQLite hiện có
  src/                     Core C++ và các bản thử hiện có
  data/                    Bản đồ làm việc hiện có
  docs/                    Tài liệu kiến trúc và kỹ thuật
    requirements/          Sổ yêu cầu, mẫu, mô tả mới
    decisions/             Quyết định thiết kế và mẫu ADR
    exports/               Hai tài liệu Word
  .agents/skills/           Sáu kỹ năng Codex theo mảng
  contracts/               Hợp đồng MQTT hiện tại và dữ liệu ví dụ
  firmware/                Điểm đặt firmware sau khi chốt bo mạch
  simulation/              Điểm đặt mô phỏng sau khi chốt mô hình
  tests/                   Kế hoạch và kết quả kiểm thử
  scripts/                 Kiểm tra workspace và xuất Word
```

## Bản Word

- [Tổng quan workspace AGV](docs/exports/AGV_Workspace_Overview.docx), nguồn [00-project-brief.md](docs/00-project-brief.md).
- [Mẫu mô tả yêu cầu](docs/exports/AGV_Requirements_Template.docx), nguồn [template.md](docs/requirements/template.md).
- [Báo cáo thuật toán và kiến trúc server AGV](docs/exports/Bao_cao_thuat_toan_server_AGV.docx), tạo từ [script báo cáo](scripts/build_server_algorithm_report.py) và sáu hình trong `docs/figures/server-report/`.

Định dạng Word dùng `.docx`. Hai tài liệu nền được xuất từ Markdown bằng `scripts/export_documents.py`; báo cáo server dùng `scripts/build_server_algorithm_report.py`. Kiểm tra bố cục Word trước khi chia sẻ. Hướng dẫn cụ thể tại [phát triển](docs/12-development.md).

## Trạng thái

Đã build core và kiểm tra web/gateway/core bằng môi trường độc lập trên Windows. Chưa có nghiệm thu firmware, hai robot thật hoặc mô phỏng vật lý. Hai bản Word là bộ nền 0.1; thay đổi ứng dụng hiện tại xem tài liệu cập nhật ở trên. Không dùng bộ tài liệu này để suy ra rằng các tiêu chí nghiệm thu đã đạt. Thư mục chưa được khởi tạo thành Git repository tại thời điểm khảo sát.
