# ADR-046 — Xe 01 chờ RFID tối đa 5 giây khi mắt mất line

Ngày 12/10/2026. Trạng thái: IMPLEMENTED và kiểm chứng phần mềm; chưa nạp hoặc thử xe thật.

## Nguồn và quan sát

`SOURCE`: người dùng quan sát lỗi mất line xuất hiện trước khi xe 01 đọc thẻ ở cua, khiến server không gửi lệnh xoay, và yêu cầu chờ khoảng 5 giây trước khi báo lỗi.

`OBSERVED`: bản `esp32dev_two_robot_bench_01` đặt `LINE_LOSS_CONFIRM_MS=0`. Trong pha bám line, `CoreNavigator` báo `LINE_LOST` ở mẫu `000` đầu tiên. `AGVController` chỉ xác nhận node từ RFID; Core cần node/diagnostic dừng đúng chặng mới cấp `NODE_ORDER`. Mẫu line đọc sau khi xe dừng có thể khác mẫu gây lỗi.

## Quyết định

Đặt `LINE_LOSS_CONFIRM_MS=5000` chỉ cho profile bench xe 01 và dùng bộ đếm này cả khi xe 01 không chạy chế độ solo. Mẫu `000` dừng motor ngay, giữ tuyến còn hoạt động và tiếp tục vòng đọc RFID. Nếu mắt thấy lại line hợp lệ trước hạn, xoá bộ đếm và bám tiếp. Nếu đọc được RFID node dự kiến trước hạn, dừng tại node và chờ `NODE_ORDER` như luồng hiện tại. Nếu vẫn không thấy line và không có RFID sau 5 giây, chốt `LINE_LOST` và giữ motor dừng. STOP, lỗi mạng, vật cản, giới hạn thời gian chặng và các chốt an toàn khác vẫn có hiệu lực. Xe 02 và payload MQTT/Core giữ nguyên.

Xe không tiến khi `000`. Nếu thẻ nằm ngoài tầm RC522 tại điểm dừng, 5 giây chờ không đưa đầu đọc tới thẻ; cần dịch thẻ, kéo dài vạch hoặc điều chỉnh điểm dừng dựa trên số đo thật. Không dùng khoảng chờ để chứng minh xe đã rời vùng mutex.

## Kiểm chứng cần có

`VERIFIED`: native profile 01 và 02, 42/42 test PASS. Test xe 01 chứng minh `000` làm motor STOP ngay, RFID node dự kiến trong 5 giây chuyển sang chờ lệnh mà không chốt lỗi, không có RFID thì chốt lỗi đúng hạn, line trở lại thì tiếp tục và mất MQTT vẫn dừng/lỗi ngay. Test xe 02 xác nhận vẫn chốt mất line ở mẫu `000` đầu. Build `esp32dev_two_robot_bench_01` PASS (RAM 17,1%, flash 72,3%). `firmwareBuild` của xe 01 là `20261012-line-loss-5s-stop` để nhận diện ảnh mới, xe 02 giữ chuỗi cũ.

`OPEN`: chưa nạp ảnh mới lên xe. Cần thử trên giá kê rồi một khúc cua thật với Serial `[LINE]`, `[RFID]`, `[NODE_ORDER]`, diagnostic `linePattern/state/error` và `firmwareBuild` để đo khoảng cách/tầm đọc thực tế.
