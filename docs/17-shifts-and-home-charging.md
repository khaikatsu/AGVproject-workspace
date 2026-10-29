# Ca làm việc và tự về trạm sạc

28/10/2026 — REQ-SHIFT-RECOVERY-20261028 thay hành vi “xong việc rồi về trạm” bên dưới: **Kết thúc ca** hủy task chờ/đang chạy và vô hiệu quyền tiếp tục. Xe nhận WAIT; chờ xác minh dừng/RFID rồi mới đưa về trạm. Không tự xóa mutex khi chưa có proof. Đơn chưa thực hiện đóng; kệ/đặt chỗ còn treo phải hoàn tác hoặc đối soát thực tế. Sau khi ca CLOSED, **Đối soát lỗi ca** chốt các lỗi đã xem, giữ lịch sử. Có thể yêu cầu kết ca khi MQTT mất kết nối nếu gateway còn Core mới/khỏe; xe offline vẫn chặn tập kết.

Trong cùng ca, chọn **Phục hồi tại K…** khi xe đã dừng đúng RFID kho K1–K6; server cần proof STOP mới và kết quả RESET_FAULT đúng lệnh, không bắt buộc heading hoặc nhiệm vụ cũ chỉ để xóa lỗi tại kho. Phục hồi ở node giữa tuyến vẫn cần RFID của task treo và heading hợp lệ. Sau khi khỏe, mở chi tiết đơn chọn **Tiếp tục lượt N** nếu chặng cũ còn được phép tiếp tục; Core kiểm hướng trước khi cấp tuyến. Chỉ chặng vận chuyển được tiếp tục; nâng/hạ dang dở cần đối soát, không phát servo lại. Nếu đã đưa về S1/S2, RFID mới cập nhật marker kể cả ERROR; proof dừng tại trạm trả vùng cũ mà không cần đi qua node trung gian. Nút xác nhận tại trạm vẫn cần RFID/diagnostic mới; thao tác nhấc xe chưa được cảm biến nhận diện không tự thành vị trí MCU. [ADR-090](decisions/ADR-090-shift-recovery-and-binary-inventory.md).

21/10/2026: nút **Mở ca** nay mở bước kiểm tra/nhập vị trí kệ, chưa phát lệnh ngay. Người vận hành sửa dữ liệu điền sẵn nếu cần, xác nhận kho/xe trống rồi bấm **Xác nhận và bắt đầu ca**; web lưu kho lúc CLOSED, sau đó mới gửi mở ca. Không thay điều kiện mở ca của Core. Nếu đã lưu nhưng mở ca thất bại/không rõ kết quả, dữ liệu kho vẫn được giữ và phải đối chiếu trạng thái trước khi thử lại. [ADR-074](decisions/ADR-074-warehouse-review-before-shift-start.md).

16/10/2026 — **Ghi đè tạm thời để test:** nhóm sạc thủ công; Core main bỏ chốt pin (giữ mã cũ bằng `//`). Pin thấp/chưa đo không chặn mở ca hoặc task/GOTO; không tự chuyển tuyến đi sạc, không phát CHARGE_START. Kết thúc ca vẫn về trạm riêng và đỗ IDLE. RFID, lỗi, MQTT và mutex vẫn giữ nguyên. Những mô tả ngưỡng20/40 và CHARGING bên dưới là hành vi gốc, hiện không áp dụng; yêu cầu ≤30% sẽ cần triển khai/kiểm chứng trước khi bật chốt lại. [ADR-060](decisions/ADR-060-temporary-battery-bypass-manual-charge.md).

16/10/2026: menu scope `active01`, `active02`, `active-both` dùng cùng quản lý ca và ngưỡng pin như Core thật. Chỉ các xe trong `active_robot_ids` tham gia ca; xe ngoài scope nếu đã xuất hiện phải được xác minh IDLE tại trạm và không giữ task/tuyến/vùng. Chọn scope lúc khởi động không tự gửi GOTO/CHARGE_START; sau khi ca đã RUNNING, kết thúc ca vẫn theo luồng về trạm hiện có. Bộ sạc/CHARGE_START vật lý còn OPEN. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

16/10/2026, REQ-BATTERY-06: mode thường dùng `batteryPercentage` calibrated từ diagnostic làm pin điều phối cho ngưỡng20/40; telemetry `?` không xóa số diagnostic còn mới. Các mode trial vẫn giữ ngoại lệ không chặn theo ngưỡng. Luồng `CHARGE_START`/bộ sạc thật chưa được nghiệm thu và không được suy là đang sạc chỉ từ trạng thái Core. [ADR-021](decisions/ADR-021-display-only-battery.md).

14/10/2026: [ADR-053](decisions/ADR-053-station-completion-rfid-anchor.md) sửa task kẹt WORKING tại S1 dù MCU đã đỗ xong. RFID đích cùng diagnostic mới stationReady/IDLE/NONE/STOP/hướng ra GS chốt task; không đòi đầu đọc còn nằm trên thẻ sau pivot. Task/đích rỗng cho phép ca CLOSING→CLOSED; phục hồi lỗi/mutex giữ250ms.

Cập nhật 01/10: sau Core restart, xe đã biết được giữ dưới dạng OFFLINE/vị trí chưa xác minh, task đang chạy gián đoạn, queue và mutex được khôi phục. RFID mới, xác minh lỗi/vùng và quy trình mở ca vẫn bắt buộc. [Chi tiết phục hồi](18-hardening.md).

Yêu cầu trực tiếp 29/09/2026: xe **01 → S1**, xe **02 → S2**; nút kết thúc ca trên web; hoàn thành task đang chạy rồi về trạm riêng. Người dùng đã duyệt **C3 → G1 → C1**. Sau lần sửa server/web, người dùng cho phép cập nhật firmware: đã có RC522, learn UID và boot chờ quét mới; [hướng dẫn sketch](../firmware/AGV_Connection_Test/README.md).

## Hành vi

| Thời điểm | Core và giao diện |
| --- | --- |
| Mở core | CLOSING: khóa nhận task, chờ telemetry mới. Không tự tạo robot hoặc gán tọa độ S1/S2 |
| Xe kết nối, RFID đúng trạm riêng | Core gửi CHARGE_START; vị trí vẫn là RFID nhận được |
| Xe rảnh kết nối ở node khác | Gửi GOTO về trạm riêng; map giữ node đo gần nhất cho tới telemetry tiếp theo |
| Cả hai xe đã về trạm | CLOSED: chờ Bắt đầu ca; xe phải online, vị trí hợp lệ, hết route/đích, không lỗi và không giữ mutex |
| Bắt đầu ca | Cần pin đo ≥40% mỗi xe và MQTT hoạt động. Core gửi WAIT, đưa xe về IDLE và mở phân công |
| Kết thúc ca | CLOSING: giữ task đang chạy để hoàn thành, dừng phân công hàng đợi, từ chối task mới bằng HTTP 409 |
| Task cuối của xe hoàn thành | Tự nhận đường về S1/S2, telemetry tới đích thì chuyển CHARGING |
| Sạc xong | Giữ xe ở trạm; không tự mở ca hoặc nhận task khi ca đang đóng |

Trạm mặc định là nơi xe phải tập kết, không phải vị trí do server đoán. Xe 01 báo `G1|85|0` thì map vẫn ở G1 và nhận GOTO về S1; chỉ `S1|85|0` xác nhận ở S1. ACK của GOTO không đóng ca và không thay vị trí. Thiếu telemetry thì mục trạm ghi **Chờ telemetry / RFID**.

Trạm riêng cũng áp dụng cho pin thấp: 01 chọn S1, 02 chọn S2. Pin ≤20%, lỗi, mất ACK hoặc mất telemetry vẫn có thể gián đoạn task theo chính sách hiện có. Đường bị chặn thì chờ đường và thử lại, không đặt xe lên trạm bằng phần mềm. Xe giữ mutex phải xác nhận trả quyền; core không tự nhả zone để đóng ca.

## Chạy bản mới

Đóng core/gateway cũ trước khi chạy. Từ gốc workspace:

```powershell
./scripts/run-core.ps1 -Build
```

Terminal khác:

```powershell
Set-Location AGVwebserver
npm start
```

Mở lại `http://localhost:3000`. Theo dõi **Ca làm việc & trạm sạc**. Bấm **Bắt đầu ca** khi đủ hai xe ở đúng trạm và đủ pin; trước tan ca bấm **Kết thúc ca**, theo dõi tới trạng thái đã tập kết.

Sketch mặc định đọc RC522, không đặt G1 lúc boot. Từ 03/10/2026 UID S1/S2 và 24 node còn lại đã nằm sẵn trong firmware, chỉ cần quét thẻ đúng vị trí; `learn` dành cho thay thẻ. Khi chưa quét, telemetry UNKNOWN làm core/web hiện Chờ quét RFID. Nếu chưa có reader, bật `simulate on` rồi nhập `node S1/S2` để test bằng dữ liệu giả lập. Driver motor đã liên động với Core nhưng khóa mặc định chờ kiểm thử bàn; bộ sạc chưa điều khiển.

## Bản đồ và dữ liệu

Ba JSON và map mặc định đã khớp C3 → G1 → C1. Mọi trạm đều đi tới/về mọi K1…K6. Không áp dụng snapshot cũ tự động khi reconnect; snapshot cũ có thể chứa hướng đường cũ và làm thiếu tuyến về trạm.

Core lưu chế độ ca vào `shift_state.json` cạnh map, có thể đặt `AGV_SHIFT_STATE_PATH`. File không lưu vị trí, đường đi hoặc task. Mỗi lần mở core đều tập kết và cần telemetry mới, kể cả file ghi RUNNING. File hỏng làm dừng khởi động, không âm thầm mở ca. Giữ nguyên counter command và SQLite vận hành.

Trong ca thử hai xe, [ADR-026](decisions/ADR-026-safe-automatic-recovery-at-home.md) cho phép tự đối soát lỗi/mutex khi xe đã được quét lại ngay tại S1/S2, firmware báo motor dừng/hết task và quyền vùng cách trạm đủ xa theo map đo. Mất telemetry hoặc node RFID đã lưu vẫn giữ khóa; lỗi cảm biến không tự reset. Core/firmware mới phải cùng được triển khai và kiểm thử bàn trước khi dùng luồng này trên xe thật.

`AGV_HOME_STATIONS` là JSON theo ID topic thật; mặc định `{"01":"S1","02":"S2"}`. Test độc lập một xe có thể dùng `{"01":"S1"}`. Các test hồi quy cũ dùng `{}` để tách khỏi luồng ca mới. Bản đồ legacy không có cặp trạm mặc định không có quản lý ca hoạt động.

Khi thử **duy nhất xe 01** với `AGV_ONE_ROBOT_TRIAL=1`, Core tự dùng trạm `01→S1` cho ca, không chờ xe 02. Xe 01 phải online, có RFID/telemetry mới tại S1, IDLE, không có task/đường chưa xác minh hoặc quyền vùng; không được có xe khác trong fleet. Pin chưa đo (`?`) hoặc pin báo thấp không khóa ca hay GOTO trực tiếp trong chế độ thử. Phần trăm từ diagnostic vẫn hiển thị và chưa dùng để điều phối; cần người vận hành giám sát mức pin thực. Core không tự gửi `CHARGE_START` hay GOTO về trạm; người thử đưa xe về S1 và quét RFID. Chế độ hai xe và ngưỡng pin/sạc của nó giữ nguyên. `shift.single_robot_trial` trong API cho web hiển thị lời nhắc đúng. Sau khi Core restart, vẫn cần telemetry mới và xử lý lỗi reconnect; không tự mở ca từ file trạng thái.

Cập nhật 08/10/2026: ngay cả khi Core cấu hình trạm mặc định cho cả 01 và 02, nếu mới có **một xe** trong fleet thì xe đó có thể mở ca tại S1/S2 tương ứng sau khi online, quét RFID mới, hết lỗi/đường dở/vùng chiếm. Xe chưa từng gửi telemetry không chặn ca; xe đã xuất hiện rồi offline vẫn chặn. Với đúng một xe và pin telemetry `?`, không tự gửi GOTO/CHARGE_START khi tập kết; người thử đặt xe tại trạm. Xe duy nhất có pin đo và đã ở trạm cũng không nhận CHARGE_START; nếu ở ngoài trạm, Core vẫn dùng luồng tự về trạm. Nếu cả hai xe xuất hiện, điều kiện cũ áp dụng cho cả hai và pin điều khiển phải đo ≥40%. Nút mở ca không thay đổi điều kiện nhận task/GOTO của FleetManager; thử chuyển động với pin `?` vẫn dùng chế độ GOTO thử đã được cấu hình riêng.

Sau khi xác nhận xe đứng yên, chạy một Core thử mới từ gốc workspace bằng `./scripts/run-one-robot-trial.ps1 -Build`; nếu dữ liệu/counter vận hành đang nằm trong `src/data`, thêm `-DataRoot src/data`. Dừng các Core cũ trước khi chạy để không trùng cổng 8080 hoặc dùng chung counter. Script giữ nguyên map, counter, runtime và shift state trong DataRoot đã chọn. Web/gateway tiếp tục ở cổng 3000; cần thấy `shift.single_robot_trial=true` trước khi giao lệnh.

Hàng đợi giữ trong RAM của phiên core khi đóng/mở ca. Sau restart, SQLite giữ lịch sử nhưng core chưa tự khôi phục task/mutex từ lịch sử. Khởi động lại giữa lúc robot chuyển động, xác nhận dừng vật lý và phục hồi đường chiếm vẫn OPEN trong hợp đồng MCU hiện tại.

## Kiểm chứng

- [C++ unit](../tests/core_tests.cpp): tuyến kho/trạm; không gán vị trí giả; gate task, task cuối, queue; pin thấp về trạm riêng; đường bị chặn, MQTT down, vị trí sai, offline, lỗi và mutex chưa trả.
- [MQTT integration](../tests/shift.integration.js): gateway/core/Mosquitto thật và hai robot giả lập; mở/đóng ca, ACK khác đến đích, về trạm qua mutex, pin/lỗi và nhận RFID sau restart; dữ liệu/broker riêng trong `.qa`.
- [Thử một xe](../tests/solo-shift.integration.js): broker/Core/gateway và DB riêng; chế độ mặc định đã thử riêng 01/S1 và 02/S2 với pin `?`, mở ca qua HTTP mà không phát lệnh sạc/di chuyển; chế độ trial 01 cũng được chạy lại.
- [Browser](../tests/browser.integration.js): nút ca, trạm, khóa task, vẫn quản lý vật cản, ẩn robot chưa định vị, desktop/mobile.

Các fixture trên không chứng minh chuyển động, sạc hoặc độ chính xác RFID trên xe thật. Kết quả nền tại [hồ sơ kiểm thử](testing-web-server-2026-09-29.md).

Core test01 10/10/2026 (ADR-036): clear_error/RESET_FAULT không yêu cầu ở S1; Core chờ diagnostic khỏe tại RFID hiện tại mới IDLE, ACK không đủ. Test01 mở/đóng ca tại node đã xác nhận, không ép về S1; at_home vẫn dùng trạm thực. Robot02/both giữ home gate. Không đổi command payload/firmware, không cần nạp lại. MCU vẫn kiểm tra line/mạng/vị trí/hướng; không trả mutex chỉ vì reset giữa tuyến.
