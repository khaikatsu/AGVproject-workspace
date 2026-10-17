# ADR-062 — Tìm lại line tại chỗ, giữ chặng đang chạy

Ngày: 16/10/2026. Trạng thái: IMPLEMENTED; kiểm chứng phần mềm trong hồ sơ test, nghiệm thu xe thật OPEN.

## SOURCE

Người dùng yêu cầu: mất line thì dừng ngay, giữ task/vị trí chặng, nhớ mắt thấy line cuối và hướng motor vừa chỉnh; xoay nhẹ về phía line vừa mất trong giới hạn; mắt giữa bắt line ổn định thì tiếp tục task, thất bại thì dừng và báo `LINE_LOST`.

## OBSERVED và phạm vi

Firmware hai profile bench trước đây dừng khi `000`, chờ tối đa5s rồi báo lỗi; LEFT/RIGHT có retry riêng theo ADR-061. `CoreNavigator` sở hữu route/index/cmd_id; server gián đoạn task khi nhận lỗi. Encoder một OUT/bánh hiện chưa đo được góc quay đã hiệu chuẩn. Xe02 có tuning mới trong `Robot02Config.h` tại thời điểm triển khai; giữ nguyên cấu hình đó.

## Quyết định triển khai

- Bật `AGV_LINE_RECOVERY_ENABLED` cho hai profile01/02. Áp dụng trong pha bám line tiến/lùi; không thay bộ đếm bắt nhánh của pha rẽ, quay đầu hoặc đỗ trạm.
- Lưu mẫu line hợp lệ cuối cùng và action motor tương ứng. Mẫu trái `100/110` ghi hướng chỉnh trái, phải `001/011` ghi hướng chỉnh phải; khi lùi dùng action chỉnh lùi tương ứng. Hướng ghi nhớ chỉ dùng trong750ms sau lần chỉnh, được xóa khi sang chặng mới, STOP, clear fault hoặc fault.
- `000` đầu tiên trả STOP ngay. Nếu còn hướng chỉnh đủ mới và chưa tìm trong chặng này, giữ dừng100ms rồi phát xung pivot cùng phía: PWM180 hai bánh ngược chiều, xoay40ms/nghỉ60ms, tối đa600ms tính từ lúc mất line. Chuyển xung/nghỉ dựa trên lần xuất motor thực ở vòng điều khiển; vòng lặp chậm vẫn phải xuất STOP rồi chờ đủ khoảng nghỉ trước xung tiếp, không bỏ qua nghỉ theo modulo thời gian. Các giá trị là thông số thử phần mềm, chưa hiệu chuẩn thành góc quét thực; độ trễ vòng điều khiển vẫn ảnh hưởng thời điểm dừng vật lý.
- Trong phục hồi chỉ nhận mẫu giữa hẹp `010`: motor dừng trong lúc xác nhận liên tục ít nhất100ms và ít nhất3 mẫu có thời điểm khác nhau. `101/111` không đủ xác nhận vì có thể là giao điểm; mắt bên không tự cho chạy tiếp. Mất mẫu giữa làm bộ đếm ổn định bắt đầu lại.
- Thành công tiếp tục Forward/Backward của cùng route_cmd_id/path_index/next_node. Không tạo task hoặc NODE_ORDER mới; không xóa/restart đồng hồ giới hạn chặng. Mỗi chặng chỉ được một lượt quét; lần mất tiếp theo có thể dừng chờ line nhưng không xoay lần nữa.
- Không có hướng đủ mới hoặc đã quét một lần: giữ dừng tối đa5s như grace cũ, cũng cần `010` ổn định mới tiếp tục. Hết giới hạn quét600ms hoặc grace5s thì fault `LINE_LOST`, Core xử lý gián đoạn task theo luồng hiện có.
- Trong lúc tìm, `following()` trả false: encoder không cộng pivot vào quãng đường chặng, không đi xuyên RFID hoặc xin trước zone chặng sau. Quyền vùng hiện tại vẫn giữ; mất grant dừng quét và không tự cấp lại quyền. RFID đích thực vẫn xử lý theo luồng đến node hiện có, dừng tìm và xóa bộ nhớ hướng của chặng cũ; RFID sai vẫn lỗi.
- Wi-Fi/MQTT, cấu hình motion/line, sonar, vật cản, STOP/hủy task giữ ưu tiên. Vật cản dừng motor và tạm ngưng đồng hồ quét như cơ chế pause hiện có; bộ đếm line ổn định bị xóa và phải xác nhận lại khi thông. Giới hạn chặng15s vẫn có hiệu lực và có thể kết thúc trước timeout tìm line.

MQTT/schema/ACK giữ nguyên; diagnostic vẫn báo task đang hoạt động khi đang tìm và chưa có lỗi. Serial có `[LINE] search=LEFT/RIGHT/STOP lastPattern=... lastMotor=...` và `search_recovered`. FirmwareBuild01 `20261016-robot01-line-search`,02 `20261016-robot02-line-search`.

## Kiểm chứng và OPEN

Xem [hồ sơ kiểm thử](../../tests/README.md). Ca native mô phỏng chuỗi cảm biến/thời gian kiểm hướng trái/phải cả tiến/lùi, xung/nghỉ/timeout, nhiễu mắt giữa, không đoán hướng khi bộ nhớ cũ, một lượt/chặng, giữ route/index, STOP/mạng/sonar/grant/vật cản, RFID, tràn millis và timeout chặng. Host motor kiểm duty thật tại adapter GPIO và quay lại mức PWM thường sau tìm line.

Chưa upload hoặc thử chuyển động thật. OPEN: đối chiếu chiều xoay theo mắt trên hai xe, lực khởi động PWM180, góc quét thực với xung40ms và quán tính sau STOP; thử đoạn line ngắn trước khi thử cùng xe khác. Ba mắt line không thể tự chứng minh vạch vừa bắt là đúng nhánh; giới hạn quét và kiểm tra RFID vẫn cần thiết.
