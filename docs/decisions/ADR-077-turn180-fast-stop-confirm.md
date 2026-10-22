# ADR-077 — Quay180° bắt line sớm, STOP rồi xác nhận tại chỗ

Ngày22/10/2026. Trạng thái: IMPLEMENTED phần mềm, kiểm chứng tại tests/README.md; chưa nạp hoặc thử xe thật.

## Nguồn và bằng chứng

`SOURCE`: người dùng báo quay180° chậm nhưng mắt đi qua line trước khi xe dừng; yêu cầu sửa sau khi trao đổi về nhịp đọc và xác nhận line. Giữ yêu cầu trước đó không nhận001/100 để bắt lại line, tránh nhầm nhánh.

`OBSERVED`: `observeTurnLine` trướcRFID chỉ xử lý rẽLeft/Right. TurnAround vàStationTurning kiểm line sau giao tiếpRC522, dùng000→010/101/111 và có thể chuyểnFollowing ngay ở mẫu cuối. `RFIDModule::readCard` gọi giao tiếp đồng bộ có thể chờ; kiểm mã chỉ chứng minh có khoảng chờ, chưa đo được độ trễ vòng điều khiển trên xe. G1 vàGS1/GS2 có thao tác quay đầu phải đếm đúng số nhánh theoRoutePlanner/NODE_ORDER; không thể dừng ở nhánh đầu như quay90°.

## Quyết định và triển khai

- Bật `AGV_TURN_AROUND_CAPTURE_ENABLED` cho hai profile trongSharedRobotConfig. Profile khác mặc định giữ policy cũ. BuildID01/02 `20261022-robot01-turn180-v13` / `20261022-robot02-turn180-v13`.
- ĐọcGPIO trong cùng vòng điều khiển, không tạo task/ISR điều khiểnmotor thứhai. `TURN_LINE_SAMPLE_MS=2` là khoảng tối thiểu/mục tiêu giữa các quan sát mới, không phải bảo đảm thời gian thực2ms. Tạm hoãn `rfid_.readCard()` và logsonar định kỳ khiTurning180/StationTurning hoặc đangxácnhận180; MQTTworker, hàngđợiSTOP, grant, sonar và kiểmtraan toàn vẫn hoạtđộng. RFID tiếp tục ở vòng sau khi xácnhận xong; không coi quay tại chỗ là đã đến node mới.
- Quan sát sớm trướcRFID choTurnAround/StationTurning. Giữ yêu cầu thấy000 rời vạch cũ và khoảng100ms giữa các lần tínhnhánh. Chỉ010/011/110 được tínhnhánh;001/100/101/111 không xácnhận đíchquay. Mỗi mảng line liên tục chỉ tínhmộtlần, phải có000 mới tínhnhánh tiếp. Nhánh90° trunggian tiếp tụcquay; chỉ nhánh cuối vàoTurnAroundCapture và yêu cầu controllerSTOPngay.
- TurnAroundCapture giữmotorSTOP, cần010/011/110 liên tục≥100ms,≥3mẫu mới cách≥2ms/gap≤50ms. Mẫu khác hoặc gapcũ xóa bằngchứng; mấtgrant/vậtcản phải xácnhận lại. Không quaytìm thêm nếu quán tính làm mấtline sauSTOP; giữdừng tới khi đủbằngchứng hoặc timeout.
- Giữ deadlinequay2500ms gốc gồm cả thời gianxácnhận; vật cản vẫn đóngbăngclock theo policy hiệncó. Timeout giữlỗiTurnTimeout và không tựclear/replay. STOP/mấtmạng/lỗicấuhình/sonar luônthắng.
- Hoàn tất nhánh quay180 trên tuyến: trảSTOP một vòng, giữroute/cmd_id/path_index/grant và chờ handoffsonar trước/sau nhưcũ.200ms đầu Following capPWM180 bằngLINE_RECOVERY_PWM. Sau đó trả bámline tiến/lùi bìnhthường. Đỗtrạm kiểu front-in: chỉ sau xácnhận mới `stationOriented`, giữđúng luồngWaitingGrant/Idle; reverse-dock S1/S2 hiệncó vẫn xoayởGS và chỉ hoànthành sauRFID thật.
- LogSerial `[TURN180] confirmed ... sample_target_ms=2 max_gap_ms=...` ghi khoảngquan sát lớnnhất trong lượt để đo trênxe; không đổi payload/topic/QoS/ACK hoặc dữ liệuCore/NVS. Không đổiPWMquay, PD, servo hay cấuhìnhmạng.

## Kiểm chứng và giới hạn

Host `scripts/test-turn180.ps1` nối navigator→Movement→MotorModule cho01/02: STOPsớm trướcRFID, xácnhận100ms, một/hainhánh, khôngđếmtrùng,011/110/010, loại001/100/101/111, nhiễu/gap/timestamptrùng/rollover, capPWM vàstationReady. KiểmSTOP/mạng/sonar/cấuhình/vậtcản/grant/timeout; task/chặng vẫngiữ trongxácnhận. Native tuyến vềS1/S2 vàG1 được cập nhật thêm bước xácnhận thật, không nới chốt antoàn để qua test. Các probe line-confirm/fast-line trướcđó vẫnPASS. Artifact/hash/build tại tests/README.md.

`OPEN`: đo `max_gap_ms` và quãngtrôi sauSTOP trênESP32, thửkêbánh rồi tuyếnngắn khôngtải và cótải. Không khẳngđịnh đã chạyđều2ms hoặc hếtxoaylố; cảm biến/nhiễu/quán tính và nguồn vẫn có thể ảnhhưởng. Không thayDriverSTOP sangphanh chủđộng khi chưa có kiểmtra phầncứng.

Thamchiếu: REQ-DUAL-TURN180-FAST-CAPTURE-01, [ADR-076](ADR-076-dual-narrow-line-confirm.md), [ADR-033](ADR-033-stations-face-outward.md), [ADR-056](ADR-056-robot01-reverse-home-request.md).
