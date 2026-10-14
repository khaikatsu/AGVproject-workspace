# ADR-032 — Hướng cố định, tiến vào và lùi ra kho/trạm

Ngày 10/10/2026. Trạng thái: IMPLEMENTED; thay phần đặt hướng thủ công của ADR-030. Kiểm chứng phần mềm; chưa nạp và đo chuyển động thật.

Quy tắc trạm S1/S2 đã bị thay bởi [ADR-033](ADR-033-stations-face-outward.md): đầu hướng ra GS, tiến xuất phát; kho vẫn tiến vào/lùi ra. Nội dung dưới giữ lịch sử quyết định trước khi người dùng đính chính.

## Nguồn và quy ước đã chốt

`SOURCE`: người dùng yêu cầu bỏ phần tự chọn/gán hướng, dùng hướng mặc định cho kho/trạm để xe tiến vào và lùi ra. Sau khi làm rõ câu “hướng ra ngoài”, người dùng xác nhận **“Quay vào trong: tiến vào, lùi ra kho/trạm”**. Quy ước này thay hướng khởi động S1/S2 quay ra GS1/GS2 đã dùng trước đây và thay thao tác xác nhận tay tại ADR-030.

`OBSERVED`: IR đo lệch line, không đo góc tuyệt đối. Hướng cố định là quy ước lắp/đặt xe đã được người dùng chọn, không phải hướng đo bằng la bàn. Map có tám nhánh cụt K1–K6/S1/S2 với cửa G1–G6/GS1/GS2 tương ứng.

## Quyết định

Khi đọc RFID tại kho/trạm và xe đang dừng, firmware tự đặt đầu xe theo cạnh **cửa → kho/trạm**. Không phục hồi vị trí từ bộ nhớ: vẫn phải đọc RFID thật trong phiên hiện tại. Trên map đang dùng: S1 ↑ N, S2 ↓ S, K1/K2/K3 ← W, K4/K5/K6 → E. Khi ra, xe lùi từ kho/trạm tới cửa, giữ hướng đầu xe; khi vào, xe rẽ/bắt line rồi đi tiến. Heading ở node đường thường lấy từ chặng thật; tạm UNKNOWN khi pivot, cập nhật khi bắt lại line đúng tuyến hoặc tới RFID dự kiến. Đặt xe tay ở node đường thường không tạo heading từ cặp RFID giả.

Chỉ mở thử lùi trên đoạn ra kho/trạm trong env trial có motor được cấu hình. Chốt `REVERSE_STEERING_CONFIRMED=false` vẫn giữ cho trường hợp lùi khác; không coi sửa mã là đã hiệu chuẩn lái lùi. Hướng mặc định được ưu tiên khi lập tuyến từ kho/trạm, không để dấu chặng lùi cũ đảo ngược quy ước docking.

Nếu sau khi lùi ra, đường đi kế tiếp nằm đối diện đầu xe, planner chọn `TurnAround`, quay phải tại cửa rồi đi tiến. Firmware đếm nhánh line trung gian theo topology vật lý của map (kể cả cạnh một chiều về giao thông), yêu cầu `000` giữa các lần bắt lại line và ít nhất `TURN_MIN_MS` giữa hai lần đếm; không đếm lặp cùng một dải centered. Chỉ một nhánh đối diện thì cần một lần bắt line; có nhánh 90° trung gian thì cần hai. Giữ timeout 2500 ms, dừng mất mạng/line, mutex và dwell RFID. Đây là kiểm chứng theo map, chưa đo quay 180° thật; số nhánh line thực tế phải khớp map.

Dashboard giữ chấm RFID và mũi tên **đầu xe**. Khi lùi, mũi tên đầu xe không đảo theo chiều chuyển động. Bỏ panel, nút Hướng, preview, checkbox và xác nhận thủ công; bỏ REST `/api/agvs/:id/heading`, WS `set_heading` và MQTT `SET_HEADING`. Diagnostic chỉ thêm `headingSupported`, `heading`, `headingNode`, `headingSource=bay_default/route/unknown`; không cần cmd_id để gán hướng. Core nhận hướng đúng node từ diagnostic mới; offline, vị trí chưa hợp lệ, dữ liệu cũ hoặc nguồn operator cũ hiện UNKNOWN. Telemetry pipe/GOTO JSON giữ nguyên.

## Kiểm chứng và giới hạn

`VERIFIED`: 93/93 native firmware PASS, bao gồm tám hướng mặc định, Backward ra/Forward vào, không lùi ngoài bay, rẽ sau lùi, quay đầu qua một/hai nhánh, không đếm lại cùng line, timeout dừng và mutex/mạng. Audit Dijkstra/firmware 650 cặp node PASS. RFID adapter PASS cho poll 20 ms và dừng trước HALT. Gateway/security 13/13, browser PASS `.qa/browser-test-OSqt55`, hình `robot-heading-default.png`; API thủ công trả 404 và không phát lệnh.

`VERIFIED`: Core build/unit và MQTT/Core/gateway fixture `.qa/heading-test-AIRU8X/verification.json` PASS. Build ESP32 `esp32dev_two_robot_bench_01` PASS (RAM 16,9%, Flash 72,2%), tạo firmware.bin 953152 byte. Chưa upload.

`VERIFIED` vận hành server: cập nhật `build/agv_core.exe` và gateway sau snapshot không task/path/uncertain_path hoặc owner/waiter mutex, giữ cùng map/runtime/counter và chế độ `robot01`. Core/MQTT ready, panel manual không còn và API cũ trả 404. Counter command tiếp tục reserved_through 54272, task 37888, không rewind. Snapshot sau cập nhật chỉ có 01 online nhưng ERROR/vị trí chưa xác minh/heading UNKNOWN do firmware cũ, ca CLOSING; không phát GOTO, mở ca hoặc reset xe. Artifact `.qa/live-bay-heading-update/before.json`, `after.json` và log.

`OPEN`: nạp bản ESP32 01 mới, kiểm tra hướng đặt xe tại S1/K và chiều lùi/correction trên giá kê trước tuyến thật. Chưa chứng minh hướng suy luận luôn đúng khi trượt bánh hoặc người vận hành xoay xe trái quy ước. Bản sửa quét [ADR-031](ADR-031-rfid-read-latency.md) vẫn được giữ; chưa kết luận hết lố GS2 trên xe.
