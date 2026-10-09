# Đánh giá mức đáp ứng đề tài và giải pháp hoàn thiện

> Bản phân tích này ghi hiện trạng trước lần sửa web/server 29/09/2026; số dòng bằng chứng thuộc phiên mã cũ. Các mục đã sửa và phạm vi đã kiểm chứng nằm ở [bản cập nhật](14-web-server-update.md). Firmware, nghiệm thu phần cứng và phục hồi MCU vẫn OPEN.


Ngày đánh giá: 29/09/2026. Căn cứ: phiếu ĐỀ TÀI NCKH SV NĂM 2026.docx và mã hiện có trong workspace. Đây là đề xuất sau khảo sát tĩnh, chưa phải yêu cầu nhóm đã chốt hoặc bản sửa code. Chưa build core, chạy robot, đo mạng hoặc kiểm tra phần cứng trong lượt đánh giá này.

## Kết luận và phạm vi

Kiến trúc hiện có đi đúng hướng quản lý robot bằng IoT: web, gateway Node.js, core C++, MQTT và dữ liệu SQLite. Phần cần hoàn thiện nhất là chứng minh hai robot thật đi được tuyến, dữ liệu web phản ánh đúng robot và các tình huống lỗi có cơ chế phục hồi rõ ràng.

Đề tài yêu cầu hai robot đi lộ trình lập trình trước, web quản lý bản đồ/lộ trình/pin/lỗi theo thời gian thực và báo cáo nghiên cứu. Phiếu chưa đặt yêu cầu SLAM, LiDAR, trí tuệ nhân tạo, cloud hoặc tích hợp ROS 2. Những phần đó chỉ mở khi mục tiêu nghiên cứu hay phần cứng thật cần đến.

## 1 Làm rõ phần cứng và cách robot biết vị trí

**Bằng chứng:** không thấy firmware thực thi hoặc hồ sơ BOM/pinout trong workspace; phiếu chưa chỉ định cảm biến. Đây không phải kết luận nhóm chưa làm robot ở nơi khác.

**Tác động:** core có thể tạo path nhưng chưa có bằng chứng firmware chuyển path thành điều khiển motor và báo vị trí đúng. Một dashboard đẹp chưa chứng minh yêu cầu chế tạo hai robot.

**Giải pháp:** bổ sung loại MCU/framework, motor/driver/encoder, cảm biến, nguồn/pin, kích thước và tải; kèm sơ đồ chân. Chốt cách bám tuyến và định vị. Nếu nhóm đã dùng line và RFID thì ghi bảng tag → node, điểm rẽ, điểm chờ và điểm đích; không tự mua hoặc đổi cảm biến từ gợi ý này.

Tách firmware thành điều khiển chuyển động, đọc cảm biến, máy trạng thái và tác vụ mạng. Mỗi node trong path phải có quy tắc đi thẳng/rẽ/dừng theo bản đồ thực. Đo khoảng dừng tại tốc độ/tải dùng thử, xử lý vật cản ngay trên robot, xác minh vị trí khởi động trước nhận task.

**Bằng chứng hoàn tất:** từng robot thật đi trọn tuyến và gửi node/pin/lỗi khớp quan sát thực. Xem [firmware](08-firmware.md).

## 2 Hoàn thiện quản lý bản đồ từ web đến core

**Bằng chứng:** gateway gửi `load_map_data` ở `server.js` dòng 59 và 400; nhánh command core không xử lý lệnh này. `MapGraph` có hàm nạp dữ liệu nhưng handler chưa nối vào luồng đó.

**Tác động:** web lưu snapshot hoặc nhận phản hồi apply không chứng minh core đang chạy đúng snapshot. Đồ thị trên web và lộ trình được core tính có thể khác nhau.

**Giải pháp:** thêm handler `load_map_data`, kiểm tra toàn bộ bản đồ rồi áp dụng như một thao tác thống nhất. Kiểm tra ID trùng, cạnh nối node không tồn tại, trọng số không hợp lệ, zone/trạm sạc tham chiếu sai và node robot đang chiếm. Nền đơn giản nên chỉ cho thay bản đồ khi robot đã dừng ở trạng thái được xác nhận.

Thêm `request_id`, `map_revision`, kết quả accepted/rejected và lý do. Gateway chỉ hiển thị bản đồ đang chạy sau khi core xác nhận; thất bại phải giữ bản đồ cũ. Node/tag thực, chiều cạnh và đơn vị cần được thống nhất.

**Bằng chứng hoàn tất:** save, apply, restart và load lại cho cùng revision; bản đồ sai bị từ chối không làm thay đổi map đang chạy.

## 3 Theo dõi nhiệm vụ bằng định danh xuyên hệ thống

**Bằng chứng:** gateway suy luận task hoàn thành khi `prevTarget` có giá trị và `newTarget` rỗng, tại `server.js` dòng 138. Ghép task theo đích cũng không đủ phân biệt nhiều nhiệm vụ cùng node.

**Tác động:** xóa target do lỗi/đổi trạng thái có thể bị lưu thành completed; lịch sử và KPI không phản ánh đúng kết quả thực. HTTP 202 hiện chỉ báo gateway đã chuyển yêu cầu qua socket.

**Giải pháp:** đưa `task_id` xuyên web, gateway, core và log; dùng sự kiện trạng thái rõ ràng: pending, assigned, running, completed, cancelled, failed, interrupted. Không kết luận task hoàn thành chỉ từ target rỗng. Core xác nhận kết quả theo nhiệm vụ và bằng chứng robot, gateway ghi DB theo sự kiện đó.

Nhiệm vụ vận chuyển có lấy/trả hàng cần điều kiện hoàn thành riêng; nếu phạm vi chỉ là đi tới node thì không thêm pickup/dropoff thành yêu cầu bắt buộc.

**Bằng chứng hoàn tất:** hai task cùng đích, hủy khi chạy, lỗi xe, đổi đích và restart không làm lẫn lịch sử hoặc báo nhầm completed.

## 4 Giữ vùng đang bị robot mất mạng chiếm

**Bằng chứng:** `releaseZonesOnOffline_ = true` và `checkTimeouts` gọi `releaseAgvZones`; hàm đó có thể cấp grant ngay cho xe chờ. Core có giữ node xe offline trong busy set, nhưng nhánh tìm đường fallback bỏ busy set; không thể dựa riêng vào busy set để chứng minh tránh xe đang đứng.

**Tác động:** mất liên lạc không chứng minh robot đã rời giao nhau. Grant tiếp theo có thể không phù hợp với vùng thực đang chiếm. Timeout ACK cũng không chứng minh command chưa chạy, nên không tự giao lại hành động vật lý cho xe khác khi kết quả còn chưa rõ.

**Giải pháp:** baseline đặt `setReleaseZonesOnOffline(false)` nhưng cần bổ sung trạng thái zone unknown/blocked và xử lý phục hồi. Giữ phần đường robot có thể đang chiếm, gồm cạnh đang đi và zone liên quan, không chỉ node RFID cuối. Tách vật cản/robot chiếm thật khỏi đặt trước đường tương lai: tìm đường fallback có thể xếp hàng qua phần đặt trước, không đi xuyên phần chiếm thật chưa xác minh.

Firmware phải có local watchdog và chính sách dừng đã đo. Sau reconnect, robot báo vị trí/trạng thái, xác nhận đứng yên và đồng bộ nhiệm vụ trước nhận đường mới. Chỉ giải phóng vùng khi có bằng chứng đã rời hoặc người vận hành xác minh hiện trường. Xóa xe khỏi danh sách cũng không được tự hiểu là xe vật lý đã biến mất.

**Bằng chứng hoàn tất:** hai xe xin cùng zone, mất grant, offline trong zone, offline giữa cạnh và reconnect đều không tạo quyền đi vào vùng chưa được xác nhận trống.

## 5 Chọn xe và trạm sạc theo chi phí đường đi

**Bằng chứng:** Dijkstra trong `MapGraph` xét trọng số, nhưng `dispatchPending` và `pickChargingStationNoLock` trong FleetManager, cùng `nearestCharging` trong MapGraph, so `path.size()` khi lựa chọn.

**Tác động:** nếu mục tiêu là gần nhất theo khoảng cách, số node không phản ánh khoảng cách. Đường hai cạnh tổng 300 cm có thể được ưu tiên trước đường ba cạnh tổng 150 cm.

**Giải pháp:** planner trả cả path và tổng cost; phân công/trạm sạc dùng cùng cost theo đơn vị đã chốt. Kiểm tra trọng số không âm và dữ liệu cạnh trước tìm đường; tie break ổn định theo ID hoặc chính sách công bằng. Có thể bổ sung thời gian chờ vùng vào cost khi yêu cầu cần, nhưng giữ baseline khoảng cách để so sánh.

Hai xe cần quy tắc đối với cạnh hai chiều hẹp, quay/rẽ và vùng dùng chung. Cho một xe chiếm toàn đoạn hẹp là baseline dễ kiểm chứng; không cần chuyển sang thuật toán nhiều robot phức tạp nếu hai xe và bản đồ nhỏ đã đủ mục tiêu.

**Bằng chứng hoàn tất:** đồ thị test có số node và tổng weight trái chiều vẫn chọn cost thấp; hai xe tranh đoạn hẹp có thứ tự chờ/phục hồi xác định.

## 6 Đồng bộ command khi retry và restart

**Bằng chứng:** core có `cmd_id`, ACK và gửi lại; `cmdSeq_` khởi đầu từ 0 mỗi lần tiến trình chạy. Command v0 chưa có phiên server hoặc thời hạn hiệu lực.

**Tác động:** firmware dedup chỉ bằng ID có thể bỏ nhầm lệnh mới sau core restart hoặc thực hiện lệnh cũ sau MCU reboot. Nhận ACK chỉ chứng minh nhận lệnh theo hợp đồng ứng dụng, không chứng minh chuyển động hoàn thành.

**Giải pháp:** thiết kế version kế tiếp với phiên server, command ID, task ID, phiên boot robot và kết quả xử lý. Firmware kiểm tra lệnh cũ/trùng/thứ tự, ACK lại duplicate hợp lệ nhưng không lặp hành động. Reconnect thực hiện bắt tay đồng bộ và giữ robot chờ tới khi trạng thái được xác nhận. Không retained command di chuyển. Đổi hợp đồng phải đồng thời ở core, firmware, simulator và tài liệu.

QoS 1 có thể giúp giao message ít nhất một lần nhưng vẫn có duplicate. QoS không thay thế dedup hoặc xác nhận kết quả hành động. PubSubClient chỉ publish QoS 0; nếu đang dùng nó thì cần chọn phương án dựa trên khả năng thư viện, không chỉ đổi tham số server. ESP MQTT là lựa chọn cần cân nhắc khi framework thật là ESP-IDF.

**Bằng chứng hoàn tất:** mất ACK, duplicate, lệnh cũ đến muộn, reboot MCU và reboot core không tạo hành động lặp hoặc bỏ nhầm lệnh mới.

## 7 Web hiển thị dữ liệu thực và tuổi dữ liệu

**Bằng chứng:** `agv_connected` do gateway thêm phản ánh socket gateway-core; core có `lastSeen` nội bộ nhưng chưa thấy timestamp đo của từng robot trong state hiện tại.

**Tác động:** gateway còn kết nối không chứng minh mỗi robot còn phát dữ liệu. RFID/node là vị trí rời rạc; chuyển động vẽ trên web nếu nội suy phải được phân biệt với số đo thật. Pin mặc định hoặc mô phỏng không đáp ứng phần đo pin robot thật.

**Giải pháp:** thêm metadata từng robot: thời điểm gateway/core nhận telemetry, sequence, phiên boot và timestamp đo hoặc uptime khi phù hợp. Hiển thị online/stale/offline, lần cập nhật cuối, node đo gần nhất và lộ trình được duyệt. Nếu muốn đo trễ cảm biến đến browser, đồng bộ clock hoặc công bố phương pháp ước lượng và sai số; không trừ timestamp của hai clock chưa đồng bộ.

Hiệu chuẩn số đo pin theo nguồn và mạch đo thật, có trạng thái không xác định; các ngưỡng 20/40/90 trong mã cần xem lại theo kết quả đo. Báo lỗi cần mã lỗi, thời điểm, trạng thái khắc phục; không tự xóa lỗi chỉ vì telemetry quay lại.

**Bằng chứng hoàn tất:** ngắt một robot làm đúng robot đó chuyển stale/offline, robot kia vẫn cập nhật; pin và lỗi trên web khớp đo/quan sát thực.

## 8 Build và kiểm thử có thể lặp lại

**Bằng chứng:** tên header lệch case so include, `AGVtest.cpp` chứa JavaScript, chưa có build manifest C++ và firmware trong file được khảo sát.

**Giải pháp:** chuẩn hóa filename/include, phân loại tệp thử, tạo target CMake riêng cho core và test; pin dependency và hướng dẫn working directory/cấu hình broker. Không compile tất cả cpp vào một executable. Đưa IP/port và cấu hình test khỏi chỗ phải sửa mã nếu triển khai bước này.

Tạo hai robot giả lập cùng hợp đồng MQTT để kiểm chứng web/core trước. Dùng broker/map/DB test riêng. Sau đó một robot thật, rồi hai robot thật. Simulator không thay thế kết quả motor, cảm biến và tránh xung đột vật lý.

**Bằng chứng hoàn tất:** build từ hướng dẫn trên máy nhóm, test có input và kết quả rõ, hai robot thật chạy kịch bản nghiệm thu. Không sửa DB hiện tại để thử sửa code.

## 9 Phần nghiên cứu và tiêu chí nghiệm thu

Phiếu mô tả chức năng nhưng chưa có bộ chỉ tiêu đo. Nhóm cần thống nhất với giảng viên một bảng yêu cầu → thử nghiệm → kết quả → giới hạn. Các số bên dưới là ví dụ đề xuất để thảo luận, chưa phải yêu cầu gốc hoặc kết quả đã đạt.

| Hạng mục | Ví dụ tiêu chí đề xuất | Cách chứng minh |
| --- | --- | --- |
| Hai robot thật đi đúng tuyến | Mỗi robot 20 lượt, mục tiêu tỷ lệ thành công từ 95 phần trăm trên tuyến/điều kiện được chốt | Log node đích, video và nguyên nhân mọi lượt lỗi |
| Web cập nhật theo thời gian thực | Mục tiêu p95 trễ không quá 1 giây trên LAN thử đã mô tả | Đo nhiều mẫu theo phương pháp clock/độ trễ rõ ràng |
| Bản đồ | Apply/restart giữ đúng revision, dữ liệu sai bị từ chối | So state core, web và snapshot |
| Vùng dùng chung | Không cùng chiếm vùng trong bộ ca thử, có thử offline giữa vùng | Timeline grant/release và quan sát robot thật |
| Pin và lỗi | Dữ liệu đo thật, sai số/chu kỳ được nhóm chốt | Đối chiếu thiết bị đo và fault injection có kiểm soát |
| Phục hồi mạng | Không chạy lặp command hoặc báo sai completed trong các ca đã định nghĩa | Log task/session/command và quan sát |

Không suy rộng “không va chạm trong ca thử” thành bảo đảm an toàn cho mọi môi trường. Ngưỡng realtime một giây cũng không áp dụng cho dừng vật cản tại motor; dừng cục bộ có tiêu chí riêng từ kích thước/tốc độ/quãng dừng.

Một hướng nghiên cứu phù hợp là đánh giá ảnh hưởng của độ trễ/mất message tới điều phối hai robot và so phân công theo số node với tổng trọng số. Chạy cùng map, tập task và điều kiện đầu vào, lặp nhiều lượt; đo thời gian hoàn thành, chờ zone, tỷ lệ lỗi và trễ giao tiếp. Nếu so mô hình có/không điều phối xung đột, thử phương án không điều phối trong mô phỏng.

Báo cáo cần nêu câu hỏi nghiên cứu, phương pháp, sơ đồ phần cứng/kiến trúc, thuật toán, lựa chọn giao thức, thiết lập thử nghiệm, số liệu thật, giới hạn và nguồn tham khảo. Cách này giúp phần nghiên cứu có bằng chứng thay vì chỉ liệt kê công nghệ.

## Thứ tự thực hiện đề xuất

1. Chốt phần cứng/bản đồ/tuyến và tiêu chí nghiệm thu; xác nhận các phần nhóm đã làm ở ngoài workspace.
2. Sửa nền build và luồng nạp bản đồ; định danh task/result, command session và chính sách vùng khi mất mạng.
3. Tạo simulator hai xe; sửa lựa chọn cost và kiểm tra các ca xung đột/retry/restart.
4. Tích hợp từng robot thật, hiệu chuẩn pin/định vị và đo dừng cục bộ.
5. Chạy hai robot cùng web; thu log/video/số đo và hoàn thiện báo cáo.

Có thể làm phần cứng và server song song theo nhóm sau khi contract đã rõ. Chưa cần tự động sạc, ROS 2/Gazebo, SLAM, cloud hoặc nhiều đội robot nếu những phần đó không phục vụ mục tiêu đã chốt. Theo dõi pin là yêu cầu trong phiếu; dock và sạc tự động chưa được ghi như đầu ra bắt buộc.

## Nguồn và hồ sơ liên quan

- [Hiện trạng mã](02-current-system.md), [sổ yêu cầu](requirements/register.md), [kiểm thử](09-testing.md).
- [Gateway](../AGVwebserver/server.js), [core](../src/AGVserver.cpp), [FleetManager](../src/FLeetmanager.hpp), [MapGraph](../src/MapGraph.hpp).
- [Chuẩn OASIS MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html) về ngữ nghĩa QoS.
- [PubSubClient](https://github.com/knolleary/pubsubclient) về QoS publish và kích thước buffer; [ESP MQTT](https://github.com/espressif/esp-mqtt) về các khả năng client ESP-IDF.

Các giải pháp và ngưỡng đề xuất ở tài liệu này là phân tích của workspace, không được gán là yêu cầu hoặc bảo đảm từ upstream.
