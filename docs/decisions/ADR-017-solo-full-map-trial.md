# ADR-017 — Xe 01 chạy thử theo đường ngắn nhất trên toàn bản đồ

Ngày 05/10/2026. Trạng thái: đã nạp bản thử vào ESP32 xe 01 và kiểm chứng kết nối Wi-Fi/MQTT với server; chưa kiểm chứng chuyển động thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng yêu cầu mở khóa motor để thử đúng một xe đi tới mọi node bằng lệnh từ web/server. Thông số pin và khoảng rời vùng mutex sẽ cung cấp sau. Người dùng muốn dùng thuật toán tìm đường ngắn nhất hiện có, chưa cần điều phối hai xe, và trước đó yêu cầu giữ nguyên chức năng web.

`OBSERVED`: Core đã dùng Dijkstra theo trọng số cạnh và đường có hướng. Bản thử cũ chỉ cho xe 01 đi S1→GS1; pin chưa đo là `?`; bản firmware thường khóa PWM. Firmware nhận path và đi theo RFID nhưng đợi grant ở vùng M1–M6, không thể tự release vì chưa có phép đo đuôi xe rời vùng. Xe thật chưa xuất hiện trong fleet tại lần kiểm tra 05/10; chưa chứng minh kết nối ESP32 hoặc chuyển động vật lý.

## Quyết định

- Giữ Dijkstra, map, topic/payload MQTT v0 và giao diện web. Khi Core khởi động với `AGV_ONE_ROBOT_TRIAL=1`, chỉ nhận GOTO **trực tiếp xe 01** tới một node khác có đường trên map, tối đa 32 chặng; từ chối giao task tự động và lệnh cho xe khác. Pin `?` được chấp nhận riêng cho chế độ này, không biến thành số phần trăm giả. Xe 01 phải IDLE, có RFID/telemetry mới; không có xe khác trong fleet hoặc owner mutex cũ. Nếu xe khác phát telemetry trong lúc chạy, Core gửi WAIT và gián đoạn task.
- Firmware build `esp32dev_trial` mở PWM cho xe ID `01`, nhận toàn bộ path có hướng và đi từng RFID. Sau lần quét S1 với hướng đặt xe đã mô tả, firmware giữ thông tin node trước và chiều lùi để tính rẽ cho lệnh tiếp theo. Khi vừa lùi mà cạnh kế tiếp tiếp tục thẳng theo chiều dịch chuyển, xe tiếp tục lùi. Không có hướng đầu xe đáng tin sau reboot ở node khác S1 thì từ chối GOTO.
- Chỉ trong tuyến trial đã kiểm tra ID/path, firmware bỏ bước xin grant mutex; Core không cấp hoặc giải phóng quyền vùng cho tuyến này. Đây là chế độ một xe khi xe còn lại đã đưa khỏi đường; không dùng cho vận hành hai xe. Bản `esp32dev` thường tiếp tục khóa motor và giữ giao thức mutex.
- Giữ STOP/WAIT, watchdog heartbeat 3 giây, dừng khi mất line, lỗi/vật cản siêu âm, RFID sai, timeout quay 2,5 giây và timeout mỗi đoạn 15 giây. PWM thử 140/255 trước trim; các giá trị này là giới hạn phần mềm, chưa phải thông số đã đo.
- Yêu cầu bổ sung 05/10/2026: khi `AGV_ONE_ROBOT_TRIAL=1`, ca làm việc chỉ theo 01→S1. Cho phép bắt đầu khi xe 01 online, RFID/telemetry mới tại S1, IDLE và không còn task/đường/vùng đang giữ; không yêu cầu xe 02 hoặc phép đo pin khi pin vẫn là `?`. Chính sách pin trial được cập nhật ngày 08/10/2026 tại ADR-023. Không tự gửi `CHARGE_START` hoặc GOTO về trạm trong chế độ thử; người thử đưa xe về S1 và xác nhận RFID. Lỗi, mất kết nối, vị trí không xác minh và sự xuất hiện của xe thứ hai vẫn khóa ca. Vận hành hai xe giữ chính sách pin/sạc và hai trạm hiện hành.

## Kiểm chứng và giới hạn

`SOURCE` bổ sung 05/10/2026: người dùng yêu cầu tạm bỏ cảm biến siêu âm trên xe để thử chạy, sẽ lắp và bổ sung sau. `OBSERVED`: Core nhận GOTO K4 nhưng xe dừng tại S1 với `ULTRASONIC_INVALID`; tám bản chẩn đoán MQTT liên tiếp có `ultrasonicCm=null`, `motor=STOP`, Wi-Fi/MQTT vẫn kết nối. Chưa xác định lỗi do dây, nguồn hay module HC-SR04. `IMPLEMENTED`: chỉ env `esp32dev_trial` không khởi tạo/đo siêu âm và bỏ nó khỏi điều kiện cho phép chuyển động; env thường giữ liên động siêu âm. Chức năng dừng khi mất line, mất mạng, RFID sai và timeout đoạn vẫn giữ. Trước khi chạy thật phải kiểm tra bánh trên giá kê, đường trống và khả năng ngắt nguồn motor trực tiếp; phải kiểm chứng lại cảm biến trước khi bật liên động cho trial. Đây là thay đổi có chủ đích so với chốt siêu âm trong quyết định ban đầu.

`VERIFIED` bổ sung: build `esp32dev_trial` và `esp32dev` đều thành công; 53 native test PASS. Nạp lại bản trial cuối cùng qua COM6, esptool xác minh hash và reset board. Chẩn đoán MQTT sau reset cuối báo `AGV_IDLE`, `error=NONE`, `motor=STOP`, `ultrasonicCm=null`, Wi-Fi/MQTT kết nối. Sau reset cuối, RFID chưa quét lại nên firmware báo `UNKNOWN`; Core giữ `ERROR/position_valid=false` theo đúng chốt xác minh vị trí. Chưa gửi GOTO sau thay đổi và chưa quan sát bánh quay. Cần quét lại thẻ S1 và xác minh bánh trên giá kê trước khi xóa lỗi/thử lệnh.

`VERIFIED` tiếp theo: firmware đã quét lại RFID S1 sau reset. Core đã xóa lỗi reconnect bằng API, hiện xe 01 `IDLE`, online, `position_valid=true`, không có task, ca vẫn RUNNING. Chưa phát GOTO sau sửa; chờ xác nhận xe được kê bánh và có người sẵn sàng ngắt nguồn motor.

`OBSERVED` tiếp theo: hai lần GOTO S1→GS1 đều hết hạn đoạn 15 giây tại S1 (`SEGMENT_TIMEOUT`). Xe phát còi báo lùi nhưng bánh đứng yên; đèn nguồn L298N sáng. Thử 2 giây trên giá kê với người dùng sẵn sàng ngắt nguồn motor: MQTT chẩn đoán báo `BACKWARD`, duty trái `-120`, phải `-140`, sau khi hủy task cả hai về 0, Core IDLE. Như vậy server/firmware đã cấp lệnh điều khiển, chưa xác nhận PWM điện thực tế ở chân driver hoặc nguồn motor. `SOURCE` mới: người dùng yêu cầu tăng PWM trial lên 220/255 để thử. `PROPOSED/IMPLEMENTED`: đổi riêng `esp32dev_trial` sang 220 trước trim (trái 200, phải 220); bản thường giữ 210. Cần thử 2 giây trên giá kê và đo nguồn/driver nếu bánh vẫn không quay.

`VERIFIED`: kiểm tra 650 cặp điểm đi–đến theo đường Dijkstra có hướng sau khi tới điểm đầu từ S1; Core unit kiểm tra chốt một xe, pin `?`, GOTO nhiều chặng và dừng nếu xe 02 xuất hiện. Native firmware test kiểm tra đi qua M không đợi grant trong trial nhưng bản thường vẫn đợi. Build Core và ESP32 trial thành công; fixture broker/Core/gateway riêng xác nhận S1→GS1→M2, task hoàn thành theo RFID M2, không theo ACK. Không đổi tiến trình server hay DB vận hành.

`VERIFIED` bổ sung 05/10/2026: PlatformIO build `esp32dev_trial` thành công và nạp qua USB CP210x `COM6` vào chip ESP32-D0WD-V3; esptool xác nhận hash các vùng flash và reset board. Mobile Hotspot Windows ban đầu chọn 5 GHz, ESP32 quét 20 mạng nhưng không thấy SSID cấu hình (`NO_AP_FOUND`). Sau khi đặt hotspot 2,4 GHz, ESP32 vào Wi-Fi; MQTT ban đầu không mở được TCP (`state=-2`) do Windows Firewall Public chặn inbound. Với sự cho phép cụ thể của người dùng, thêm quy tắc TCP 1883 chỉ từ `192.168.137.0/24` tới `192.168.137.1`; sau khi nạp lại, Serial ghi `[WiFi] Got IP`, `[MQTT] Connected`, API server báo AGV `01` online. Chưa gửi GOTO hoặc quan sát motor/RFID thật.

`VERIFIED` bổ sung ca thử một xe 05/10/2026: build C++ và unit test PASS cho 01/S1 với pin `?`, pin đã đo thấp, ERROR và xe 02 xuất hiện; không phát `CHARGE_START`. Fixture broker/Core/gateway riêng mở ca qua HTTP với chỉ một telemetry RFID S1 và pin `?`, không phát lệnh di chuyển/sạc. Bộ tích hợp hai xe cũ PASS với Core mới. Chưa coi đây là thử nghiệm phần cứng hoặc xác nhận ca trên Core vận hành.

`VERIFIED` trên server vận hành 05/10/2026: phát hiện hai Core trùng, dừng cả hai sau khi kiểm tra không có task/vùng mutex; chạy một Core đã kiểm thử với `AGV_ONE_ROBOT_TRIAL=1`, giữ `src/data` cho map/counter/runtime. Sau RFID S1 mới do người dùng quét và xóa lỗi reconnect qua API, Core báo xe 01 online/IDLE, pin chưa đo, `shift.home_stations={"01":"S1"}`, `phase=CLOSED`, `can_start=true`. Chưa bấm Bắt đầu ca trên xe thật hoặc gửi GOTO; motor và sạc vẫn chưa được kiểm chứng vật lý.

`OPEN`: người dùng bấm Bắt đầu ca và xác nhận phiên thật, kiểm tra trạng thái SAFE_STOP của firmware sau lỗi sạc cũ trước khi cấp GOTO, chiều quay bánh và bám vạch, thời gian rẽ/đoạn, độ tin cậy RFID, siêu âm khi đi lùi, điện áp pin/B25, vùng quét vật lý, vị trí xe thứ hai và điều kiện toàn thân rời M. Không khẳng định xe đã đi hết bản đồ ngoài đời. Chế độ một xe bỏ điều phối mutex nhưng không chứng minh đường trống; phải thử bánh trên giá kê rồi tăng dần từng chặng có người giám sát. Khi chuyển sang hai xe, quay về giao thức cấp/trả mutex và đo vùng trước.

## Điều chỉnh hướng xuất phát 07/10/2026

`SOURCE`: người dùng ra lệnh S1→GS1 trên xe 01 và báo xe lùi ra khỏi map. Quan sát này thay thế giả định 04–05/10 rằng đầu xe ở S1 quay ra ngoài và phải lùi về GS1.

`OBSERVED`: Core đang ở chế độ một xe; sau lần thử, xe 01 ở `ERROR`, `position_valid=false`, không còn task đang chạy nhưng giữ `uncertain_path=["GS1"]`. Ca đã được đưa về `CLOSING` để chặn GOTO tiếp theo. `RoutePlanner::plan` đã ép chặng đầu S1→GS1 thành `Reverse`, dẫn tới `CoreNavigator` chọn `MotorAction::Backward`. Chưa có đo đạc độc lập về hướng bánh, đầu xe, cảm biến line hay quãng đường thực.

`IMPLEMENTED`: với xe đặt ở S1 và đầu hướng về GS1, chặng đầu S1→GS1 nay là `Straight`/`Forward`; chặng GS1→M2 đổi hướng rẽ suy ra từ hướng đi tiến. Chỉ đổi quy tắc khởi hành không có RFID trước trong phiên; các tuyến khác, interlock và chế độ `esp32dev` thường giữ nguyên. Không tự xóa `uncertain_path`, mở ca hoặc gửi GOTO sau thay đổi.

`OPEN`: xác minh hai bánh cùng quay **về GS1** trên giá kê, mắt line còn thấy vạch, rồi mới thử đoạn S1→GS1 rất ngắn có người trực tiếp ngắt nguồn motor. RFID S1 phải được quét lại sau lần nạp mới; Core chỉ xóa lỗi sau khi xác nhận vị trí vật lý. Chưa coi sửa thuật toán hay nạp firmware là bằng chứng xe đã đi đúng hướng ngoài đời.

`VERIFIED` phần mềm/nạp 07/10/2026: bản đồ sinh khớp map (`generate-firmware-route.js --check`), 53 native test PASS, PlatformIO build `esp32dev_trial` PASS, nạp lại qua CP210x `COM6` vào ESP32-D0WD-V3; esptool xác minh hash và reset. Sau nạp, Serial đọc UID S1; Gateway báo xe 01 online, MQTT sẵn sàng, ca `CLOSING`, xe `ERROR`, `uncertain_path=["GS1"]`. Chưa phát lệnh di chuyển với bản mới; kết quả quay bánh vẫn OPEN.

`VERIFIED` thử ngắn sau khi người dùng xác nhận xe trên giá kê, đầu hướng GS1 và có thể ngắt nguồn motor: Core xóa lỗi khi RFID S1 hợp lệ, mở ca một xe và phát GOTO S1→GS1. Chẩn đoán MQTT ghi `AGV_MOVING`, `motor=FORWARD`, duty trái/phải `200/220`, sau đó RFID báo GS1 và `motor=STOP` (0/0). Người dùng xác nhận bánh quay **về phía GS1** trên giá kê. Core ghi task `task-13317` completed theo RFID; lệnh hủy tiếp theo trả 404 vì task đã hoàn thành. Ca đã kết thúc về `CLOSING`, xe IDLE tại GS1. Chưa kiểm chứng xe bám line và đi đúng hướng trên mặt đất; RFID/ACK không thay thế phép thử đó.

## Sự cố thử GS1→M2 ngày 07/10/2026

`SOURCE`: người dùng báo xe đã tới GS1 nhưng khi giao M2 xe đi thẳng một đoạn ngắn rồi lệch khỏi line. Ảnh dashboard hiển thị xe ở GS1, đường dự kiến tới M2 và ca đang chạy.

`OBSERVED`: task M2 `task-13319` bị gián đoạn với `LINE_LOST`, task M2 `task-13320` được hủy. Lịch sử RFID/Core cho thấy nhiều lần đổi S1↔GS1 và ít nhất một lần xe offline/reboot; có thời điểm Core bắt đầu task M2 với node S1 và path còn `GS1,M2`. Chẩn đoán sau khi hủy báo `motor=STOP`, duty 0/0. Do xuất hiện thêm lệnh GOTO trong lúc điều tra, Core thử một xe được dừng để không nhận lệnh mới; firmware phải tự dừng khi thiếu heartbeat Core. Chưa có trace line pattern/pivot trong lần lệch line nên chưa thể kết luận lỗi nằm ở hướng rẽ, cực tính bánh, vị trí RFID hay bám line.

`OPEN`: quan sát chiều quay khi rời GS1 và xác nhận đầu đọc đang trên hay đã qua thẻ GS1. Không mở lại Core/ca hoặc nạp thay đổi chiều lái khi chưa xác định điều này bằng phép thử bàn có người ngắt nguồn motor.

`VERIFIED` phần mềm trong thử bàn tiếp theo: người dùng giữ bánh trên giá kê, quét S1 rồi đưa đầu đọc sang thẻ GS1 bằng tay; Core/firmware đều xác nhận `GS1`, `IDLE` trước lệnh. GOTO M2 `task-14337` tạo chẩn đoán `AGV_WAITING` rồi `AGV_MOVING`, `motor=LEFT`, duty trái `+200`, phải `-220` tại uptime 306586 ms; tới 306951 ms firmware báo `AGV_SAFE_STOP/LINE_LOST`, duty 0/0. Core ghi task interrupted, giữ `uncertain_path=["M2"]`; ca `CLOSING`. Vì task đã gián đoạn trước yêu cầu hủy, API hủy trả 404. Đã chứng minh firmware ra lệnh quay trái, nhưng chưa biết hai bánh thực quay chiều nào hoặc mẫu cảm biến line tại lúc thoát chế độ quay.

`SOURCE` bổ sung: người dùng muốn xe **dừng hẳn khi vừa quét RFID, sau đó mới quay**; ở lần thử trên giá kê, đầu xe hầu như không xoay. `IMPLEMENTED`: tăng riêng thời gian dừng sau RFID của `esp32dev_trial` từ 200 lên 1000 ms trước mọi chặng; bản thường giữ 200 ms. Thêm `linePattern` vào topic chẩn đoán để đối chiếu mắt line khi thử lại. Không thay cực tính motor, chiều rẽ hoặc bỏ chốt `LINE_LOST` khi chưa có quan sát bánh/cảm biến thực. `OPEN`: build/nạp và thử lại trên giá kê, đo liệu xe dừng rõ trước khi pivot và nguyên nhân thoát line.

`VERIFIED` phần mềm/nạp: 53 native test PASS; cấu hình `native_trial` mới có 6 test PASS, gồm chốt `RFID_STOP_DWELL_MS=1000` và lệnh motor sau hết thời gian chờ. ESP32 `esp32dev_trial` build PASS, nạp COM6 thành công với hash verify và reset. Core đã khởi động lại, người dùng xác nhận xe trên giá kê, quét lại S1 rồi GS1 và đặt mắt line trên vạch; API xác nhận xe online/IDLE ở GS1 trước lệnh.

`VERIFIED` thử bàn bản dừng 1 giây: GOTO M2 `task-15361` cho bản chẩn đoán `AGV_WAITING`, `motor=STOP`, `linePattern=3` tại uptime 273125 ms; tới 274140 ms phát `AGV_MOVING`, `motor=LEFT`, duty trái `+200`, phải `-220`. Khoảng cách giữa hai mẫu là 1015 ms, phù hợp thời gian chờ 1000 ms. Tại 274575 ms firmware báo `AGV_SAFE_STOP/LINE_LOST`, `linePattern=0`, duty 0/0; Core gián đoạn task, giữ `uncertain_path=["M2"]` và ca `CLOSING`. Log: `logs/gs1-m2-dwell-bench-2026-10-07.jsonl`. Dữ liệu chẩn đoán chỉ chứng minh lệnh ra motor; còn chờ người dùng xác nhận chiều quay thực của từng bánh và hướng xoay đầu xe. Không xóa lỗi, không thử trên mặt đất khi chưa khắc phục nguyên nhân mất line.

`SOURCE/OBSERVED` tiếp theo: người dùng thấy hai bánh quay ngược chiều trên giá kê nhưng thân xe hầu như không xoay; chưa xác định được bánh nào quay tiến. Người dùng dự định chuyển đầu đọc RFID về giữa xe; vị trí thực so với trục bánh và mắt line còn OPEN. `IMPLEMENTED` trong mã nguồn: `CoreNavigator` chỉ coi đã rời vạch cũ khi cả ba mắt đọc `000`, tránh chuyển sang bám line vì mẫu cạnh `011` rồi `111` ngay tại giao lộ. Kiểm thử `native_trial` 7/7, toàn bộ `native` 54/54 và build `esp32dev_trial` PASS. Cổng CP210x `COM6` tạm không xuất hiện sau build, nên thời điểm đó chưa nạp được. Không đổi chiều quay bánh khi chưa xác định chiều vật lý.

`SOURCE/VERIFIED` bổ sung 07/10/2026: người dùng xác nhận đã lắp đầu đọc RFID vào giữa xe. Chưa có số đo khoảng cách từ tâm anten đến trục hai bánh, nên không suy ra tâm anten trùng tâm quay. CP210x xuất hiện lại tại `COM6`; đã nạp bản `esp32dev_trial` có điều kiện `000` qua COM6, esptool xác minh hash và reset thành công. Chẩn đoán MQTT sau reset: xe 01 online, `AGV_IDLE`, `motor=STOP`, duty 0/0, `node=UNKNOWN`, mắt line `010`. Core giữ ca `CLOSING` và vị trí không xác minh; chưa quét RFID mới, chưa xóa lỗi hoặc thử rẽ sau khi đổi vị trí đầu đọc.

## Bù lực quay tại GS1 ngày 07/10/2026

`SOURCE`: người dùng quan sát tại GS1 hai bánh bên trái mạnh hơn, khiến xe xoay trái lệch khỏi line; yêu cầu xoay tại chỗ mượt hơn.

`IMPLEMENTED`: chỉ trong `esp32dev_trial`, hạ tốc pivot danh định từ 220 xuống 180 và đặt trim pivot riêng bên trái -30, bên phải 0. Với chiều trái hiện tại, lệnh danh định thành bánh trái tiến 150 và bánh phải lùi 180; chạy thẳng và env thường không đổi. `CoreNavigator` vẫn đợi mẫu `000` rời vạch cũ rồi nhận lại line; lỗi line/timeout vẫn dừng motor.

`OPEN`: hiệu chỉnh này là đáp ứng ban đầu theo quan sát, chưa đo RPM/duty thực ở bánh. Build và thử bàn chưa thực hiện cho hiệu chỉnh này; trước khi chạy tuyến cần quan sát từng bánh, hướng quay thân xe và line pattern khi pivot GS1.

## Tăng lực pivot theo phản hồi 07/10/2026

`SOURCE`: sau khi nạp mức pivot 180, người dùng báo xe không đủ lực xoay và yêu cầu tăng nhẹ hai bên.

`IMPLEMENTED`: `esp32dev_trial` tăng `TURN_SPEED` từ 180 lên 200, giữ trim trái -30/phải 0; duty pivot danh định là trái 170 và phải 200. Env thường và tốc độ chạy thẳng không đổi.

`OPEN`: chưa build/nạp hoặc xác minh bánh với mức mới; thử pivot trên giá kê và theo dõi `linePattern` trước thử mặt đất.

## Tăng duty pivot lên 190/220 ngày 07/10/2026

`SOURCE`: người dùng yêu cầu nâng duty hai bên lên trái 190, phải 220.

`IMPLEMENTED`: chỉ `esp32dev_trial` đặt `TURN_SPEED=220`, trim trái -30/phải 0 nên duty pivot danh định là 190/220. Bản thường và chạy thẳng giữ nguyên.

`OPEN`: chưa build/nạp/xác minh phần cứng ở mức PWM này.

## Đảo mapping pivot theo test W/S/A/D ngày 07/10/2026

`SOURCE`: test sketch độc lập cùng pinout báo W tiến, S lùi; D (trái tiến/phải lùi) quay vật lý sang trái, A (trái lùi/phải tiến) quay sang phải.

`OBSERVED`: `MotorModule` trước đó gán `motorLeft()` thành trái tiến/phải lùi và `motorRight()` thành trái lùi/phải tiến sau khi áp dụng đảo chiều từng motor. Điều này ngược với nhãn hướng quay được quan sát trên xe.

`IMPLEMENTED`: đảo riêng hai pattern pivot trong `MotorModule`: `motorLeft()` ra trái lùi/phải tiến, `motorRight()` ra trái tiến/phải lùi. Không đổi GPIO, forward/backward, duty trial 190/220 hoặc trim trái -30.

`OPEN`: cần build/nạp và kiểm tra rẽ tại chỗ trên giá kê. Sau đó mới kiểm tra bắt lại line tại GS1; test sketch không chứng minh được bám line hoặc chạy tuyến an toàn.

## Bù lệch phải khi chạy thẳng ngày 07/10/2026

`SOURCE`: người dùng báo xe có xu hướng lệch phải khi đi thẳng.

`IMPLEMENTED`: riêng `esp32dev_trial`, tăng `LEFT_SPEED_TRIM` từ -20 lên -10, đưa duty thẳng danh định từ trái/phải 200/220 lên 210/220. Duty pivot 190/220 và trim pivot riêng không đổi; env thường giữ nguyên.

`OPEN`: cần nạp và thử đoạn thẳng ngắn có người giám sát; chưa đo tốc độ bánh hoặc độ lệch vật lý sau thay đổi.

## Đồng bộ cực tính theo sketch W/S/A/D ngày 07/10/2026

`SOURCE`: người dùng cung cấp sketch test dùng cùng GPIO: W đặt cả hai cầu L298N ở INx1 HIGH/INx2 LOW và xe đi tiến; S đảo mức và xe lùi. Sau đó GOTO S1→GS1 trên firmware lại làm xe lùi.

`OBSERVED`: `motorForward()` truyền duty dương, nhưng hai cờ `LEFT_FORWARD_INVERTED`/`RIGHT_FORWARD_INVERTED` đều `true`; `writeSide()` đảo duty nên thực tế phát pattern LOW/HIGH, đúng pattern S trong sketch.

`IMPLEMENTED`: đặt hai cờ đảo cực tính thành `false`, vì sketch xác nhận HIGH/LOW là tiến ở cả hai bên. Đồng bộ pivot với sketch: trái `drive(+left,-right)` (D pattern), phải `drive(-left,+right)` (A pattern). Không đổi GPIO, route S1→GS1 hay duty 210/220 chạy thẳng và 190/220 pivot.

`OPEN`: build/nạp bản mới; kiểm tra W/tiến, S/lùi và hai pivot trên giá kê, sau đó mới thử S1→GS1 đoạn ngắn. Vì lần trước xe chạy ngược, Core phải xác nhận lại vị trí RFID thực trước khi giao lệnh tiếp.

## Pivot trái hai bên cùng chạy tại GS1 ngày 07/10/2026

`SOURCE`: người dùng quan sát tại GS1 hai bánh trái lùi nhưng hai bánh phải đứng; yêu cầu pivot trái với bên trái lùi và bên phải tiến cùng lúc.

`IMPLEMENTED`: `motorLeft()` dùng `drive(-leftSpeed_, rightSpeed_)`, nên với polarity hiện tại lệnh LEFT danh định phát trái -190/phải +220. `motorRight()` dùng hai dấu đối xứng. Giữ chân GPIO, duty và polarity tiến/lùi đã xác nhận bằng sketch W/S. Pattern pivot này cập nhật lựa chọn ghi ở mục trước theo yêu cầu mới nhất của người dùng.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026; đây là kiểm tra biên dịch, chưa có bằng chứng chuyển động vật lý.

`OPEN`: cần nạp và kiểm tra ở giá kê. Nếu chẩn đoán cho `rightDuty=220` mà bánh phải không quay, điều tra GPIO25/ENB, kênh B L298N và dây motor; phần mềm không chứng minh được bánh đã quay. Hướng quay thân xe và khả năng bắt lại line tại GS1 vẫn phải quan sát thực tế.

## Sửa chiều quay vật lý GS1→M2 ngày 07/10/2026

`SOURCE/OBSERVED`: người dùng chạy thử bản pivot trái -190/+220, xe tới GS1 rồi quay phải trong khi tuyến cần rẽ trái để tới M2. Route/trace GS1→M2 đã cho hành động `LEFT`; kết quả này bác bỏ mapping pivot vừa chọn ở mục trước.

`IMPLEMENTED`: đổi `motorLeft()` thành `drive(+leftSpeed_, -rightSpeed_)`, duty danh định +190/-220; `motorRight()` thành `drive(-leftSpeed_, +rightSpeed_)`. Hai bên vẫn được cấp PWM cùng lệnh. Không đổi route, chân điều khiển, tốc độ hay polarity W/S.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026; chưa nạp hoặc xác nhận chuyển động của bản này.

`OPEN`: nạp bản mới và xác nhận `motor=LEFT`, `leftDuty=190`, `rightDuty=-220` cùng chiều quay thân xe trên giá kê; sau đó thử tuyến ngắn GS1→M2 có giám sát và theo dõi line pattern.

## Xác định vì sao pivot chỉ chạy một bên ngày 07/10/2026

`SOURCE`: người dùng thấy bản +190/-220 chỉ làm bánh phải lùi, bánh trái đứng; yêu cầu pivot trái trái lùi/phải tiến và pivot phải trái tiến/phải lùi, hai bên cùng chạy.

`OBSERVED`: `MovementModule::turnLeft()` đã đặt hai tốc độ dương và `MotorModule::drive()` ghi cả hai kênh PWM. Hiện thiếu log cho lần chạy mới để biết hành động thực tế là `LEFT` hay lệnh chỉnh line một bên. Báo cáo quay phải ở lần trước và yêu cầu mới về dấu pivot cần được phân giải bằng quan sát thân xe khi hai bên thực sự chạy.

`IMPLEMENTED`: đổi lại dấu `motorLeft()` thành âm/dương và `motorRight()` thành dương/âm theo yêu cầu. Thêm Serial `[MOTOR]` khi hành động thay đổi tại GS1, ghi hành động, duty hai bên, `linePattern` và node tiếp theo; thêm ràng buộc build để hai duty pivot không bằng 0.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026. `OPEN`: nạp, kê bánh và thu dòng `[MOTOR]`. Nếu thấy `LEFT left=-190 right=220` nhưng bánh phải đứng, cần kiểm tra kênh B L298N/dây/nguồn; nếu motor là `REVERSE_CORRECT_*`, cần điều tra vì sao navigator không còn ở pha pivot. Chưa xác nhận xe quay trái/bám lại line.

## Khớp chiều map bằng phép thử WASD ngày 07/10/2026

`SOURCE/OBSERVED`: bản trái lùi/phải tiến vẫn làm thân xe quay phải tại GS1. Trước đó bản trái tiến/phải lùi ở duty 190/220 chỉ thấy bên phải lùi; sketch WASD dùng cùng GPIO và PWM 255/255 đã cho pattern D (trái tiến/phải lùi) quay trái thực tế. Route GS1→M2 đã cho `LEFT`.

`IMPLEMENTED`: riêng `esp32dev_trial`, `motorLeft()` dùng trái tiến/phải lùi, `motorRight()` dùng chiều đối xứng; PWM hai bên khi pivot là 255/255 như sketch đã chạy. Giữ polarity W/S, tốc độ thẳng 210/220 và log Serial `[MOTOR]` tại GS1. Mức 255/255 nhằm kiểm tra hiện tượng bánh đứng dưới tải; không coi là hiệu chuẩn cuối.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026. `OPEN`: nạp rồi thử pivot trên giá kê, xác nhận `[MOTOR] LEFT left=255 right=-255` và hai bên thực sự quay; nếu bên trái vẫn không chạy ở 255, điều tra GPIO14/ENA, kênh A và motor. Chỉ thử đoạn GS1→M2 ngắn có giám sát sau khi cả hai bánh và chiều thân xe đúng; cần đo lại mức PWM để không vượt line.

## Xe 01 mới dùng quy ước trái/phải chuẩn ngày 07/10/2026

`SOURCE`: người dùng thay xe 01 bằng xe mới, xác nhận giữ nguyên bảng chân ESP32/L298N và xe mới không bị lỗi đảo trái/phải. Lệnh trái phải làm thân xe quay trái, lệnh phải phải làm thân xe quay phải.

`OBSERVED`: mapping pivot ở mục trước được chọn theo phép thử xe cũ; nó không còn là cấu hình mục tiêu cho xe mới. Route GS1→M2 vẫn phân loại `LEFT`.

`IMPLEMENTED`: pivot trái phát duty trái âm/phải dương, pivot phải trái dương/phải âm, hai bên cùng chạy ở duty trial 255/255. Chỉnh line khi tiến dùng bánh phải để chỉnh trái và bánh trái để chỉnh phải. Không đổi GPIO, cực tính W/S hoặc dữ liệu map. Chỉnh line khi lùi giữ nguyên vì chiều dịch chuyển lúc lùi ngược với hướng đầu xe.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026; đây chỉ là xác nhận biên dịch.

`OPEN`: nạp lên xe mới và kiểm tra bánh trên giá kê trước khi chạy GS1→M2; quan sát `[MOTOR] LEFT left=-255 right=255`, chiều thân xe và hai bánh đều quay. Trim đi thẳng 210/220 kế thừa xe cũ, cần đo lại độ lệch khi chạy xe mới.

## Giảm tốc và sửa chiều GS1 trên xe mới ngày 07/10/2026

`SOURCE/OBSERVED`: sau khi dùng bản xe 01 mới ở mục trên, người dùng quan sát xe qua GS1 quay phải và quay quá nhanh nên lệch line. Đây là bằng chứng thực tế bác bỏ chiều pivot `LEFT` trái âm/phải dương ở mục trên. Route GS1→M2 từng được phân loại `LEFT`; chưa có log hành động của chính lượt chạy mới.

`IMPLEMENTED`: `motorLeft()` cấp trái dương/phải âm, `motorRight()` cấp trái âm/phải dương; hai kênh cùng chạy. Giảm duty pivot riêng `esp32dev_trial` từ 255/255 xuống 200/200 để giảm tốc và giữ lực hai bên cân nhau. Chỉnh line khi tiến trở lại bánh trái cho `CorrectLeft`, bánh phải cho `CorrectRight`, khớp chiều quay vừa quan sát. Giữ nguyên cực tính tiến/lùi, GPIO, duty đi thẳng 210/220 và route.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026; chưa thử phần cứng của bản mới.

`OPEN`: nạp bản mới và kiểm tra `[MOTOR] LEFT left=200 right=-200`, cả hai bánh quay trên giá kê, thân xe quay trái, rồi mới thử đoạn GS1→M2 ngắn có giám sát. Nếu một bên không khởi động ở PWM 200, cần xác nhận log và nguồn/kênh motor trước khi nâng duty; chưa chứng minh xe bắt lại line.

## Chỉnh trim bánh trái ngày 08/10/2026

`SOURCE`: người dùng yêu cầu đặt trim bánh trái thành `-30` ở chế độ trial một xe và `-40` ở chế độ thường.

`IMPLEMENTED`: `LEFT_SPEED_TRIM = ONE_ROBOT_TRIAL ? -30 : -40`; `RIGHT_SPEED_TRIM` giữ `0`. Giá trị này được cộng vào duty bánh trái trong các nhánh chạy thẳng/chỉnh line; duty PWM thực tế vì vậy thấp hơn duty danh định tương ứng.

`OPEN`: chưa build, nạp hoặc đo trên xe thật. Xác nhận lực khởi động trên giá kê và thử đoạn line ngắn có giám sát trước khi chạy tuyến.

## Kết nối xe 02 để giám sát ngày 09/10/2026

`SOURCE`: người dùng muốn đưa xe 02 lên cùng server với xe 01, trước mắt chỉ kết nối và giám sát.

`IMPLEMENTED`: env `esp32dev_robot2_monitor` đặt ID `02` và giữ motor khóa. Core nhận telemetry theo ID riêng để hiện cả hai xe. Chốt thử một xe được giữ: xe 02 xuất hiện sẽ ngắt tuyến đang chạy của xe 01 và chặn GOTO trial mới, vì Core không thể xác minh vị trí và khả năng chuyển động của xe 02 chỉ qua MQTT.

`VERIFIED` ngày 09/10/2026: người dùng xác nhận phần cứng xe 02 giống xe 01 và motor an toàn. PlatformIO build/nạp COM6 thành công, hash được xác minh; Serial báo motor khóa, WiFi nhận IP và MQTT kết nối ID `02`. Core/web ghi xe `02` online, vị trí S2 hợp lệ; người dùng xác nhận xe thật đứng đúng trên RFID S2, sau đó xóa lỗi reconnect và Core đưa xe về `IDLE`. `OPEN`: xe 01 đang OFFLINE nên chưa xác nhận cả hai xe online đồng thời; chưa có bằng chứng hai xe chuyển động an toàn trên cùng đường.

`OBSERVED` tiếp theo: Core live đang chạy chế độ thường, trong pha kết thúc ca tự gửi `CHARGE_START` cho xe 02 ở S2; firmware báo `CHARGER_NOT_CONFIGURED`. `IMPLEMENTED` trong bản Core chưa triển khai live: biến `AGV_MONITOR_ONLY_02=1` giữ xe 02 ngoài phân công, GOTO trực tiếp và logic tự về sạc; firmware tiếp tục khóa PWM. `VERIFIED`: build Core riêng và `core_tests` PASS. `OPEN`: chuyển Core sau khi đối soát trạng thái xe 01 và giữ checkpoint/counter hiện có, rồi kiểm tra hai xe online trên cùng server.

`OBSERVED` ngày 09/10/2026: xe 01 không tự lên server sau khi bật do đang nạp sketch thử motor WASD. Sau khi người dùng cắm xe an toàn tại COM6, đã nạp env `esp32dev` ID `01`; hash flash được xác minh. Serial xác nhận motor khóa, WiFi nhận IP và MQTT kết nối với client ID `agv-01-E465B8834D34`. `OPEN`: chưa đọc được dashboard sau lần nạp vì công cụ bị auto-review chặn khi hết giới hạn sử dụng; cần xác nhận RFID thật, trạng thái Core và xe 02 vẫn online trước khi thao tác ca hoặc nhiệm vụ. Bản `esp32dev` hiện khóa chuyển động.

## Xác nhận mẫu mất line trên xe 01 ngày 07/10/2026

`SOURCE`: người dùng thấy web báo `LINE_LOST` tại GS1, trong khi mắt line lúc quan sát đang đọc `010`.

`OBSERVED`: CoreNavigator trước đây chốt lỗi ngay khi một lần đọc `linePattern=0` trong pha bám line. Lỗi được giữ lại, còn marker trên web lấy RFID cuối cùng; `010` sau khi xe dừng không tiết lộ mẫu lúc chốt lỗi. Chưa có log mẫu gây lỗi của lần chạy này.

`IMPLEMENTED`: riêng trial, khi đọc `000` trong pha bám line, dừng motor ngay và đợi tối đa 120 ms để xác nhận; nếu mẫu hợp lệ trở lại thì tiếp tục, nếu vẫn `000` thì chốt `LINE_LOST`. Ghi Serial `[LINE] blank`, `recovered` và `fault` để phân biệt mẫu thoáng qua với mất line liên tục. Bản thường giữ ngưỡng dừng tức thì cũ. Không bỏ kiểm tra line trong lúc chạy.

`VERIFIED`: PlatformIO build `esp32dev_trial` thành công ngày 07/10/2026. `OPEN`: nạp xe và thu log lúc GS1; kiểm tra cảm biến 010/000 khi đúng vị trí, rồi xác nhận hành vi trên đoạn ngắn có giám sát. Chưa kết luận nguyên nhân vật lý từ ảnh web.
