# Kiểm tra độ trễ xe → server → web ngày 30/10/2026

## Phạm vi

SOURCE: người dùng thấy xe đã chạy nhưng vị trí trên web cập nhật chậm. Đo thụ động hai xe01/02, không phát lệnh xe, không thay timeout, không restart dịch vụ hoặc sửa firmware/Core/gateway/web. Trong toàn bộ cửa sổ đo, hai xe IDLE tại S1/S2; chưa đo được chuyển động thật.

Probe chỉ MQTT SUBSCRIBE/PINGREQ, đọc WebSocket và HTTP GET health. Trình duyệt Edge headless riêng chặn mọi HTTP mutation. Không sử dụng client ID của robot, không khai báo LWT trên topic xe.

## Kết quả nền đã xác nhận

OBSERVED11:29:00–11:29:30+07, sau khi đã đóng kết nối chẩn đoán trực tiếp Core và để hàng đợi thoát. Chỉ theo dõi broker và WebSocket gateway hiện có; không mở thêm kết nối Core8080.

| Chỉ số | Xe01 | Xe02 |
| --- | ---: | ---: |
| Mẫu encoder nhận từ MQTT | 141 | 143 |
| Khoảng cách giữa hai mẫu, trung vị | 210,9 ms | 208,5 ms |
| Khoảng ngắt mẫu lớn nhất | 592,8 ms | 335,9 ms |
| Cùng mẫu MQTT → dữ liệu WebSocket web, trung vị | 76,7 ms | 79,4 ms |
| Cùng mẫu MQTT → dữ liệu WebSocket web, P95 | 146,7 ms | 154,3 ms |
| Cùng mẫu MQTT → dữ liệu WebSocket web, lớn nhất | 234,3 ms | 250,7 ms |
| Tuổi heartbeat Core tại xe, lớn nhất trong diagnostic | 378 ms | 344 ms |
| Tuổi xử lý worker mạng tại xe, lớn nhất trong diagnostic | 21 ms | 19 ms |
| RSSI trung vị | −54 dBm | −61 dBm |

Không có diagnostic MQTT/Wi-Fi false hoặc lỗi xe trong cửa sổ nền này; probe không có lỗi kết nối/delta. Core session giữ nguyên; gateway health connections=1, last_close=null, sequence_mismatches=0. Các trường max trong health là tích lũy từ lúc gateway chạy, không mặc nhiên là sự kiện mới trong phép đo.

Edge headless30s nhận253frame WebSocket, khoảng cách trung vị109,7ms, lớn nhất291ms. Hàm render mất trung vị2,1ms, P95=3,3ms, lớn nhất4,3ms; riêng bản đồ trung vị0,2ms, lớn nhất0,7ms. Có3long task74/102/225ms trong phiên trình duyệt, không có lỗi JS hoặc mutation. Đây là phiên Edge riêng, không đại diện chắc chắn cho tab người dùng đang mở hoặc tab nền.

## Hiện tượng nghẽn Core khi thêm kết nối chẩn đoán

OBSERVED: khi mở thêm một WebSocket đọc trực tiếp Core, cùng mẫu encoder đã có tại broker nhưng tới Core/web ngày càng muộn. Lượt xác nhận11:27:18–11:28:03+07 đối chiếu đúng `(robot_id, boot_id, connection_session, seq)`:

- Xe01: MQTT → Core trung vị2829,5ms; MQTT → web2825,8ms, lớn nhất5008,7ms.
- Xe02: MQTT → Core trung vị2788,3ms; MQTT → web2783,6ms, lớn nhất4996,4ms.
- Web vẫn được vẽ nhanh; độ trễ đã có trước khi dữ liệu tới trình duyệt.
- Sau đóng kết nối Core bổ sung, lượt đối chứng11:28:04–11:28:34 ghi hàng đợi giảm dần:10giây đầu còn trung vị khoảng4s,10giây cuối còn75,9ms/71,9ms. Lượt nền độc lập sau đó xác nhận mức76,7ms/79,4ms.

Phép đo trực tiếp Core đã làm tăng tải, vì vậy không dùng mức2–5s này làm độ trễ Wi-Fi nền hoặc khẳng định đây chính là lần trễ khi xe chạy mà người dùng thấy. Lượt đầu90s còn có một lỗi bắt tay400 do URL WebSocket web sai; không dùng lượt đó để tính MQTT → web. Lượt xác nhận và lượt nền dùng đúng `/ws?delta=1`, không lỗi.

OBSERVED từ mã: `src/core/AGVserver.cpp` xử lý callback MQTT dưới `g_stateMtx`; mẫu odometry được chấp nhận chạy `checkpointAndPublish()` rồi `broadcastState(true)`. Broadcast tạo snapshot/delta và gửi từng client khi vẫn giữ khóa trạng thái. `EncoderObservation::accept` đặt `seen=Clock::now()` lúc Core xử lý mẫu, nên `odometry.age_ms` có thể thấp dù mẫu đã chờ trước đó. Không nên dùng riêng tuổi này để kết luận độ trễ từ xe đã thấp.

PROPOSED: đo riêng thời gian chờ khóa, checkpoint, tạo JSON và gửi từng WebSocket trước khi chọn tối ưu Core. Chưa chứng minh thao tác ghi đĩa nào là nguyên nhân duy nhất; storage_metrics tích lũy từng ghi nhận max_commit_ms692, không có đối chiếu thời điểm đủ để kết luận quan hệ nhân quả. Không bỏ cơ chế lưu quyền/vùng hoặc xác minh STOP chỉ để tăng tốc hiển thị.

## Ý nghĩa đối với vị trí trên bản đồ

OBSERVED từ mã: firmware gửi odometry theo `ODOMETRY_INTERVAL_MS=200`; web nội suy marker trong180ms cho các mẫu cùng route tại `AGVwebserver/public/dashboard-map.js`. Có thể xuất hiện độ trễ nhìn thấy do chờ mẫu tiếp theo và làm mượt, ngay cả khi đường truyền không mất kết nối.

OPEN: cần ghi đúng một hành trình thật do người dùng vận hành, đối chiếu lúc bắt đầu chạy/RFID/chuyển cạnh với mẫu encoder và marker. Chưa xác nhận độ trễ khi đang chuyển động, chưa đo độ trễ một chiều tuyệt đối xe→broker vì đồng hồ ESP32/PC chưa đồng bộ. Thống kê biến thiên `arrival−uptime` chỉ cho phần trễ thay đổi tương đối so với mẫu nhanh nhất trong cùng boot/session. Bước nhảy encoder seq không được quy thành số gói mất vì seq không phải bộ đếm riêng mỗi lần publish.

Tăng MQTT_TIMEOUT không chữa độ trễ hiển thị hoặc hàng đợi Core. Chưa thay cấu hình hoặc mã ứng dụng trong lượt này.

## Bằng chứng

- `.qa/link-latency-20261030/baseline-no-core/summary.json` và `observations.jsonl`: kết quả nền sạch30s.
- `.qa/link-latency-20261030/confirmation/summary.json` và `observations.jsonl`: đối chiếu MQTT/Core/web45s khi thêm kết nối Core.
- `.qa/link-latency-20261030/gateway-only/summary.json` và `observations.jsonl`: hàng đợi thoát sau đóng kết nối Core.
- `.qa/link-latency-20261030/browser-summary.json`: số liệu Edge headless30s.
- `.qa/link-latency-20261030/probe.cjs`, `browser-probe.cjs`: mã chẩn đoán có thể rà soát.
