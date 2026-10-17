# ADR-062 — Tìm lại line tại chỗ, giữ chặng đang chạy

## Cập nhật17/10/2026 — REQ-ROBOT02-LINE-GRACE-7S-01

SOURCE: người dùng muốn giữ lệnh khoảng7s khi mất line ngắn, để xe tự bám tiếp mà không bị server khóa ngay. Áp riêng xe02 theo phạm vi firmware đang chỉnh;01 giữ giới hạn cũ.

IMPLEMENTED: `AGV_LINE_LOSS_GRACE_MS=7000` là hạn xác nhận lỗi trong pha Following tiến/lùi, dù có hoặc không có hướng tìm.000 đầu tiên vẫn STOP, quét cục bộ chỉ được600ms tính từ lúc mất line như trước; sau600ms dừng và giữ route/index/mutex để chờ.010 liên tục100ms/≥3 mẫu vẫn cần để phục hồi; nếu đạt trước7s thì tiếp tục task cũ ngay, không đợi hết7s hoặc gửi lạiNODE_ORDER. Hết7s còn chưa xác nhậnline thì faultLINE_LOST. Cửa sổ được tính riêng mỗi lần mất; lượt quét vẫn chỉ một lần/chặng. Không reset timeout chặng15s, vì vậy MotionTimeout hoặc lỗi an toàn vẫn có thể kết thúc sớm hơn. Vật cản giữ pause đồng hồ như cơ chế cũ.

Không tăng timeout quét/xoay/TurnCapture lên7s. Không đổi Core để bỏ qua lỗi: diagnostic SAFE_STOP/ERROR hoặc error event vẫn được xử lý ngay khi firmware thật sự chốt lỗi. Trong grace firmware giữ taskActive/errorNONE; không dùng offline/ACK làm bằng chứng an toàn vùng. MQTT/schema/ACK không đổi. Giữ nguyên tuning PWM người dùng đang chỉnh, gồm correction lùi240 trong profile02 tại thời điểm sửa. BuildID02 `20261017-robot02-line-grace7s-v4`.

VERIFIED kiểm tra host cơ bản `tests/motor-host/line_grace.cpp` PASS: tiến/lùi, có/không hướng tìm, dừng sau600ms và giữ task ở5–6,5s, bắt lại ở6,6s và tiếp tục đúngroute/index sau xác nhận, lỗi ở7s lần mất tiếp, mạng/STOP ưu tiên. Không chạy toàn bộ native/integration theo yêu cầu tiết kiệm kiểm thử; chưa upload/thử xe thật. Các giới hạn600ms/5s bên dưới là lịch sử trước bổ sung này đối với02.

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
