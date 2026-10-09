# Web mô phỏng hai AGV

Bản clone dùng HTML/CSS/JS, gateway và **binary Core C++ hiện có**. Hai xe `01`/`02` thay ESP32 bằng client MQTT 3.1.1. Không viết lại Dijkstra, phân công, pin, ca làm việc hoặc MutexArbiter bằng JavaScript.

## Chạy trên Windows

Từ gốc workspace:

```powershell
./scripts/run-simulation.ps1
```

Mở **http://127.0.0.1:3100/simulation**. Nếu cần build Core từ source mới:

```powershell
./scripts/run-simulation.ps1 -Build
```

Đổi cổng web khi 3100 đang được dùng: `./scripts/run-simulation.ps1 -Port 3101`. Script dùng Node và Mosquitto có sẵn; Mosquitto mặc định `C:/msys64/ucrt64/bin/mosquitto.exe`, hoặc thư mục trong `AGV_MOSQUITTO_BIN`. Dependency dùng `AGVwebserver/node_modules`; nếu chưa cài, chạy `npm ci` trong AGVwebserver. Không cần cài MQTT.js, Arduino hay ROS.

Địa chỉ 3100 chỉ chạy một điểm mở trang nhẹ. Khi mở `/simulation`, trình duyệt được chuyển tới cổng riêng của phiên mô phỏng. Core, broker, gateway và hai xe ảo chỉ khởi chạy lúc đó. Mỗi tab gửi tín hiệu còn xem mỗi 4 giây; sau khi tab cuối đóng hoặc trình duyệt bị tắt, phiên ghi báo cáo và dừng trong khoảng 20 giây. Mở lại địa chỉ 3100 tạo phiên sạch; URL cổng riêng trong thanh địa chỉ chỉ dùng cho phiên đang chạy. Nếu trình duyệt bị ngủ hoặc máy mất mạng cục bộ hơn khoảng chờ, phiên cũng dừng.

### Tự khởi động sau khi bật máy

Từ 06/10/2026, `AGV Simulation Auto Start` chỉ mở điểm truy cập nhẹ sau khi đăng nhập Windows, không tự mở trình duyệt hoặc Core. Chạy `./scripts/install-simulation-autostart.ps1` để cập nhật tác vụ và gỡ `AGV Simulation Health Check` cũ; tác vụ kiểm tra mỗi phút sẽ làm mô phỏng tự chạy lại sau khi đóng tab.

Nếu kết nối MQTT của xe ảo tự rớt khi xe **đứng yên tại trạm riêng**, không có tuyến/hàng/quyền vùng và pin còn, simulator kết nối lại rồi dùng luồng xác nhận RFID/pin và xóa lỗi Core trước khi cho chạy. Trạng thái mất kết nối được hiển thị riêng trên web. Xe rớt mạng giữa hành trình, đang chở hàng hoặc bị người dùng bấm **Ngắt MQTT** vẫn giữ dừng và cần bấm **Phục hồi xe / RFID** sau khi đối soát; mất telemetry không phải bằng chứng xe đã rời vùng chung.

URL cố định để mở: **http://127.0.0.1:3100/simulation**. Muốn tắt tự mở điểm truy cập sau đăng nhập, chạy `Disable-ScheduledTask -TaskName 'AGV Simulation Auto Start'`; bật lại bằng `Enable-ScheduledTask` cùng tên. `stop-simulation.ps1` dừng phiên hiện tại nhưng giữ điểm truy cập; mở URL cố định sẽ tạo phiên mới. Log điểm truy cập nằm tại `simulation/autostart-logs/`. Mỗi phiên mới có dữ liệu riêng trong `simulation/runs/` và không tự tiếp tục hành trình dở.

Đây là dịch vụ **cục bộ trên máy này**: lúc máy tắt nguồn, `127.0.0.1` không thể phục vụ web. Sau đăng nhập, chỉ điểm truy cập 3100 sẵn sàng; mô phỏng chờ người dùng mở trang.

Ctrl+C đóng điểm truy cập và phiên đang chạy, ghi `report.json`. Nếu chạy nền, dùng `./scripts/stop-simulation.ps1` để dừng phiên hiện tại. Runner phát hiện điểm truy cập đã chạy và in URL, không khởi động trùng. Mỗi lần mở lại sau khi dừng tạo **phiên sạch riêng**; lịch sử phiên trước vẫn còn. Không phục hồi ca cũ tự động. Mỗi phiên trong `simulation/runs/run-*`, có `run.json` ghi PID/cổng, bản đồ ban đầu, map làm việc, counter command/task, runtime/shift, SQLite, log Core/broker và `events.jsonl`. Không xóa/rewind counter của Core đang chạy.

Web bind loopback. Core/broker chọn cổng localhost riêng; cấm chọn web 1883/3000/8080. Mô phỏng không đọc/ghi DB hoặc counter vận hành, không publish vào broker 1883. Các thao tác sửa/lưu bản đồ của dashboard chỉ sửa bản sao của phiên mô phỏng.

## Dùng giao diện

1. Chờ hai xe báo node S1/S2; bấm **Bắt đầu ca**. Đây là RFID do simulator tạo, không phải phép đo từ RC522.
2. **Đi tới** trong từng bảng xe: nhập node rồi giao đúng xe. Bảng **Giao nhiệm vụ** gốc vẫn có lựa chọn tự phân công; click xe/node trên map vẫn hoạt động.
3. **Lấy → trả hàng**: chọn hai kho. Simulator gửi task đến kho lấy, đợi `task_id` đó được Core đánh dấu completed, chờ thao tác lấy giả lập, rồi gửi task đến kho trả. Nếu đã đứng tại kho thì chỉ chạy thời gian thao tác tại đó. Ô vuông vàng là hàng ảo.
4. **Pin thấp 15%**: Core quyết định về sạc theo thuật toán thật. Khi xe đang giữa cạnh, injection áp dụng tại RFID kế tiếp để không tự đổi hướng/teleport giữa đường. **Hết pin 0%** dừng ngay tại vị trí nội suy, báo lỗi; vùng đang giữ không tự nhả.
5. **Nạp pin 85% → Phục hồi xe / RFID**: kết nối lại nếu offline, đưa xe ảo về node đọc gần nhất, bỏ phần cạnh đang dở, đợi Core xác nhận node/pin và heartbeat mới rồi gửi clear-error. Xe chỉ hết giữ dừng khi Core chấp nhận. Có thể dùng **15% → Phục hồi xe / RFID** để kiểm tra về sạc sau lỗi hết pin; đổi pin đơn thuần không tự xóa lỗi hoặc tạm dừng. Đây là thao tác di chuyển lại xe trong mô hình, không phải khẳng định robot thật đã về node đó. Chi phí phần đường đã đi được giữ; thao tác đặt lại vị trí không tính là đường di chuyển.
6. **Tạm dừng / Tiếp tục** giữ vị trí và vẫn có telemetry. **Ngắt MQTT** tạo LWT offline thật trên broker riêng; **Kết nối lại** chưa tự xóa lỗi. **Mất 1 ACK** bỏ ACK kế tiếp; command retry cùng ID chỉ ACK lại, không chạy lại đường.
7. **Kết thúc ca** sử dụng chức năng Core gốc: hoàn thành task điều hướng đang chạy rồi về trạm riêng. Chuỗi lấy/trả gồm nhiều task là kịch bản simulator, bị đánh dấu gián đoạn khi ca kết thúc; không tự tạo phần tiếp theo ngoài ca. Hàng đang trên xe giữ lại, có nút **Gỡ hàng ảo** khi xe dừng và chuỗi đã kết thúc.
8. Xem kết quả bên trái, tab Nhiệm vụ/Báo cáo, tải **Báo cáo JSON** và **Nhật ký JSONL**. Bảng hai bên có thanh cuộn; bấm tiêu đề AGV để gập/mở bảng từng xe.

### Đơn hàng nhiều kiện cho hai xe ảo

Trong tab **Nhiệm vụ** hoặc **Tổng quan**, mở khung **Đơn hàng kho** sau khi bắt đầu ca. Chọn kho lấy, thêm các kho giao và số kiện ở từng dòng. Ví dụ lấy **5 kiện tại K1**: nhập `K2 × 2`, `K4 × 3`, rồi bấm **Tạo đơn và giao cho 2 xe**. Một đơn có tối đa 20 kiện. Có thể nhập nhiều dòng cùng kho đích; thứ tự nhập là thứ tự ưu tiên.

Server mô phỏng tạo một chuyến lấy/giao cho mỗi kiện, mỗi xe chở một kiện mỗi chuyến. Xe có ít chuyến hơn trong đơn được ưu tiên; khi ngang lượt, server ước tính tổng trọng số đường có hướng từ xe tới kho lấy và từ kho lấy tới đích để chọn xe. Những kiện cùng kho giao giữ một xe phục vụ theo thứ tự; nếu xe đó bận, server xét tiếp kiện ở kho giao khác để dùng xe rảnh. Core C++ vẫn tính tuyến Dijkstra thực tế, giữ đường và quyền vùng chung. Nếu Core từ chối đường đang bị chiếm, kiện chờ rồi thử lại; hai xe chỉ cùng chuyển động khi Core cấp được hai tuyến. Vì vậy thứ tự cấp và hoàn tất kiện giữa các kho có thể khác thứ tự nhập.

Khung đơn hàng hiển thị từng kiện, xe phụ trách và tiến độ `đã giao/tổng`. Một kiện chỉ được tính giao xong khi task_id Core hoàn thành và xe ảo tới đúng RFID kho giao. Nếu xe lỗi, mất mạng, hết pin hoặc task bị gián đoạn, đơn chuyển **Cần đối soát** và dừng cấp kiện mới; hàng có thể vẫn đang trên xe ảo, nên không tự tạo lại chuyến đó. Có thể xem chi tiết trong báo cáo JSON/nhật ký, xử lý xe rồi tạo đơn mới cho phần chưa giao sau khi xác minh. Đơn hàng và dữ liệu xe ảo của phiên được giữ qua tải lại trang, nhưng lần khởi động mô phỏng mới tạo phiên sạch.

API chỉ trong mô phỏng: `POST /sim-api/orders` với ví dụ:

```json
{"pickup":"K1","deliveries":[{"dropoff":"K2","quantity":2},{"dropoff":"K4","quantity":3}]}
```

`GET /sim-api/state` và báo cáo JSON có mảng `orders`. Server vận hành cổng 3000, schema MQTT và firmware không đổi. Mô phỏng S1 không có tải trọng/kích thước kiện hoặc bảo đảm tránh va chạm thân xe thật.

Nếu pin thấp khiến Core trả task về queue/phân công xe khác, chuỗi hàng của xe ban đầu ghi interrupted; task Core có thể vẫn hoàn thành bởi xe khác. Không coi điều đó là hàng trên xe cũ đã được trả. Nếu chỉnh hướng đường giữa cạnh làm tuyến mới không còn khớp cạnh đang đi, xe ảo báo lỗi và cần xác nhận RFID.

## Số đo và giả định

| Số liệu | Cách tính |
| --- | --- |
| Quãng đường | Tổng độ dài Euclid từ tọa độ node, kể cả phần cạnh đã đi; giả định đơn vị cm theo map v3 |
| Chi phí tuyến | Trọng số cạnh × tỷ lệ cạnh đã đi; không phải tiền, điện năng hoặc tổng chi phí vận hành |
| Thời gian di chuyển lý tưởng | Khoảng đường / vận tốc giả định, lưu trong sự kiện `route_plan` |
| Thời gian thực | Đồng hồ wall-clock, gồm MQTT/Core, chờ và thao tác hàng |
| Thời gian mô phỏng | Tích phân thời gian × hệ số hiện hành; lưu theo event/task/job |
| Chờ mutex | Thời gian simulator có tuyến nhưng chưa có token vào node mutex; có cả độ trễ cấp quyền |
| Hoàn thành task | Trạng thái và `task_id` Core; ACK không phải kết quả di chuyển |

Mặc định 25 cm/s, hao pin 0.002%/cm, sạc 2%/giây, lấy/trả 2 giây; tất cả là thông số giả định chỉnh được. Tick xe 50 ms, poll trạng thái 200 ms, giữa các cạnh có chu kỳ xử lý và chờ mạng. Thời gian task lấy từ lần simulator quan sát trạng thái, có sai số poll; đo chính xác độ trễ MQTT/phần cứng cần phép đo riêng. Hệ số × chỉ tăng xe/hàng; timeout/heartbeat/retry của Core vẫn chạy theo thời gian thực. Quyền đã cấp nhưng xe điểm chưa vào vùng được trả khi hủy tuyến, kể cả grant đến trễ; vùng đang chiếm/đang đi vào vẫn giữ khi STOP hoặc offline.

Đây là **S1 hợp đồng và logic**, có nội suy tuyến để dễ theo dõi. Xe là điểm, chưa có kích thước, motor, quay, phanh, cảm biến hoặc kiểm chứng va chạm thân xe. Mất mạng/0% pin dừng mô hình nhưng không chứng minh quãng dừng robot thật. Counter/cache command tồn tại trong phiên; không thay thế thử reboot/NVS trên MCU. Log có thời điểm, cấu hình, map và input/output để đối chiếu; không cam kết replay thời gian y hệt vì Core dùng clock và luồng mạng thật.

## Kiểm tra

Audit lỗi/xung đột bổ sung tại [ADR-011](../docs/decisions/ADR-011-simulation-conflict-audit.md), [báo cáo](../docs/testing-simulation-audit-2026-10-02.md). Core vẫn Dijkstra/FIFO nhưng đã bổ sung giữ node thường/hành lang mutex; lệnh riêng xe có thể BLOCKED chờ đường. Mất broker giữ ERROR/task interrupted. Phục hồi sau Core restart xin lại token vùng đang chiếm trước khi chạy; các vùng ngoài điểm RFID đã xác nhận được clear bằng token hiện tại trong mô hình S1.

Chạy audit tổng hợp (build trước; `--browser` cần cấu hình Playwright bên dưới):

```powershell
./scripts/build-core.ps1 -Tests -OutputDirectory .qa/audit-build
$env:AGV_CORE_BIN = Join-Path (Get-Location) '.qa/audit-build/agv_core.exe'
$env:AGV_CORE_TEST_BIN = Join-Path (Get-Location) '.qa/audit-build/core_tests.exe'
node tests/run-simulation-audit.js
```

Runner lưu kết quả/log trong `.qa/full-audit-*`. Có thể chạy riêng `node tests/simulation.faults.js` hoặc thêm ID như `F05 F06 F11`. Đây là bộ tình huống hữu hạn, không bao trùm mọi tổ hợp lỗi hay nghiệm thu robot thật.

```powershell
node tests/simulation.robot.test.js
node tests/simulation.low-battery.js
node tests/simulation.integration.js
node tests/simulation.mutex.js
node tests/simulation.browser.js
```

Unit robot kiểm tra ID/dedup/empty path, cạnh sai/sạc ngoài trạm và giữ quyền đang chiếm khi STOP. Browser test cần Playwright và Edge/Chromium đã cấu hình, xem [tests](../tests/README.md). Integration luôn dùng `.qa/` và cổng riêng; không mở database vận hành. Kiến trúc và kết quả: [ADR-010](../docs/decisions/ADR-010-isolated-simulation.md).

Sửa lỗi 15% ngày 02/10/2026: phản hồi nút pin phân biệt đã gửi Core, đợi RFID, xe lỗi/tạm dừng/offline; nút phục hồi thực hiện đúng trình tự ở trên. Mỗi lệnh sạc mới reset cờ `charging_done`, hỗ trợ nhiều chu kỳ sạc. Test low-battery kiểm tra K6→S1 với pin15, 0→15→phục hồi, tạm dừng/tiếp tục, offline→phục hồi và lặp sạc tại S1. Core/FleetManager/MutexArbiter không thay đổi.
