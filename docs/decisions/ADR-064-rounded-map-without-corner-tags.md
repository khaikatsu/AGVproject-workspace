# ADR-064 — Map bo góc, bỏ RFID C1–C4

Ngày: 17/10/2026. Trạng thái: IMPLEMENTED trong source; kiểm chứng tại tests/README.md.

## Nguồn và phạm vi

SOURCE: người dùng đã bỏ bốn node góc vì xe tới thẻ rồi pivot dễ lỗi, thay bằng line bo góc và cung cấp ảnh mới. Áp dụng cùng map cho Core scope 01, 02 và cả hai theo ADR-058. Quyết định này thay phần C1–C4 của ADR-003; giữ các yêu cầu lịch sử trong sổ.

## Graph và chiều dài

Map còn 22 node: M1–M6, G1–G6, K1–K6, GS1/GS2, S1/S2. Sáu mutex và hai home giữ nguyên.

| Cạnh một chiều mới | Hướng rời → hướng tới | Chiều dài theo nhãn ảnh |
| --- | --- | --- |
| G1 → M1 | N → E | 85 + 50 = 135 cm |
| M2 → G6 | E → S | 50 + 85 = 135 cm |
| G6 → M4 | S → W | 85 + 50 = 135 cm |
| M3 → G1 | W → N | 50 + 85 = 135 cm |

PROPOSED/giả định triển khai: cộng hai nhãn 50/85 trên mỗi đoạn ngoài thành weight135. Nhãn85 ngoài và35+35 trong không thể đồng thời là khoảng cách chiếu dọc giữa cùng hai hàng. Vì vậy tọa độ x/y giữ lưới sơ đồ nội bộ, không tuyên bố kích thước hình học tuyệt đối hoặc bán kính bo đo thật. Bán kính vẽ trên web chỉ phục vụ hiển thị. OPEN: đo chiều dài RFID→RFID dọc line bo, bán kính và tọa độ thực để hiệu chuẩn; có số đo mới thì cập nhật geometry/weight đồng bộ.

Các cạnh còn lại giữ nhãn ảnh: cột trong35cm; ngang trên/dưới50cm; ngang giữa50–100–50cm; nhánh K/G và S/GS hai chiều20cm. Dijkstra dùng tổng weight theo chiều cạnh, không dùng đường chéo giữa hai node.

## Hành vi

Mỗi cạnh bo có `geometry:{via:{x,y},segment_lengths_cm:[a,b]}`. Via chỉ là điểm dựng hình, không có ID/thẻ/telemetry/dừng hoặc NODE_ORDER tại góc. Core và bảng route MCU lấy hướng rời từ đoạn đầu, hướng tới từ đoạn cuối. RFID M/G vẫn xử lý rẽ/dừng hoặc đi xuyên theo tuyến và grant như trước. Xe bám line qua góc bằng bộ điều khiển line hiện có; không chỉnh PWM hoặc timeout.

Core validate geometry trước khi thay graph, giữ geometry qua save/load/snapshot và khi sửa thuộc tính cạnh cùng weight/hướng. Đổi weight/hướng cạnh bo qua editor đơn giản bị từ chối; phải nạp graph có geometry nhất quán. Vị trí encoder dùng weight135 và nội suy hai đoạn sơ đồ; chấm sonar cũng là ước tính theo cạnh, chưa là phép đo tọa độ vật cản ở đường cong.

Firmware bỏ bốn UID C trong bảng hoạt động, giữ mọi UID của22node còn lại. Mutex M2 được trả tại RFID G6 thay C2, M3 tại G1 thay C3, sau khi tới thẻ đúng tuyến/token; không trả theo mất thẻ/timeout/encoder. Khoảng cách thẳng tới thẻ mới vẫn vượt50cm trên lưới đang dùng. Cần kiểm chứng thân xe thực tế đã ra khỏi vùng.

## Áp dụng

Bổ sung17/10/2026: [ADR-065](ADR-065-rounded-map-checkpoint-recovery.md) triển khai chuyển checkpoint v3→v4 có backup/giữ quyền và lịch sử sau lỗi startup người dùng báo. Quy tắc không tự chuyển bên dưới là trạng thái trước bản sửa này.

Build Core mới từ gốc workspace; map vận hành là data/map_layout.json. Cả firmware01/02 phải build/nạp bản mapv4 tương ứng trước khi chạy với Core mới. Firmware cũ không có hai cạnh nối mới nên không đủ để vận hành map này. Không tự chuyển checkpoint, snapshotSQLite hoặc counter, không tự restart Core hoặc phát lệnh xe. Snapshot cũ nếu áp dụng thủ công vẫn có thể khôi phục map cũ; người vận hành phải chọn đúng phiên bản.

Kiểm chứng phần mềm gồm Coreunit, parity hướng Core/MCU, navigator bám liên tục, native hai profile, encoder/sonar, MQTT/Core scopes và browser. OPEN: chưa nạp firmware hoặc thử trên sàn; khả năng bám góc bo phụ thuộc line, bán kính, cơ khí và tốc độ thực.
