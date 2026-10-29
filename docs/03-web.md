# Phát triển web giám sát AGV

29/10/2026, REQ-UNIFIED-RECOVERY-HEADING-20261029: thẻ xe chỉ có Khôi phục xe thay ba nút Xóa lỗi/Phục hồi tại node/Xác nhận tại trạm. Dialog dùng RFID đang báo, yêu cầu xác nhận vùng cũ trống và xe có được di chuyển bằng tay; reset xong mới xác nhận hướng nếu cần. Hướng chọn theo mũi tên bản đồ; tại kho/trạm người vận hành đặt xe đúng hướng quy ước, không có lệnh tự quay. UI hiện trạng thái chờ reset/chờ hướng và chặn bước tiếp khi đổi node/Core session hoặc mất dữ liệu. Tiếp tục nhiệm vụ vẫn riêng, không tự phát lại servo. [ADR-092](decisions/ADR-092-unified-recovery-heading.md).

28/10/2026, REQ-WAREHOUSE-FAULT-RECOVERY-20261028: nút phục hồi ghi node đang báo, ví dụ **Phục hồi tại K2**. Xác nhận xe dừng và thân xe ra khỏi vùng cũ; bỏ yêu cầu chỉnh hướng trong hộp thoại chung. Tại kho K1–K6, Core cho phục hồi lỗi khi có proof RFID/STOP mới dù chưa biết hướng hoặc không còn task treo; chạy tiếp vẫn kiểm riêng hướng và quyền resume. [ADR-090](decisions/ADR-090-shift-recovery-and-binary-inventory.md).

28/10/2026, LM393: thẻ xe hiển thị Có hàng/Không có hàng/Chưa xác định, hoặc chưa có firmware cảm biến. Kệ nhập đầu ca vẫn giữ, nếu xe xác nhận kho trống thì hiển thị ô trống và mã kệ thiếu cần đối soát; lịch sử/báo cáo giữ danh tính kệ. [ADR-091](decisions/ADR-091-lm393-warehouse-proof.md).

28/10/2026: **Ca làm việc → Đối soát lỗi ca** gom cảnh báo đã phát sinh và các mục xe/task/mutex/kệ còn treo; xác nhận chỉ chốt bản chụp đã xem khi ca CLOSED và hết mục treo, giữ lịch sử. Thẻ xe có **Xác nhận tại S1/S2**, **Phục hồi tại RFID cũ**; chi tiết đơn có **Tiếp tục lượt N** cho chặng di chuyển bị gián đoạn cùng ca. RFID/diagnostic dừng mới vẫn bắt buộc; xác nhận không gán vị trí chưa đo. Mỗi ô kho hiển thị Có kệ/Trống và mã kệ, cảnh báo/đặt chỗ nằm ở chi tiết; form chọn hai trạng thái và mã kệ. Kết ca hủy nhiệm vụ cũ. [ADR-090](decisions/ADR-090-shift-recovery-and-binary-inventory.md).

27/10/2026, bổ sung REQ-WAREHOUSE-TEST-ROLLBACK-01: **Trạng thái kho → Kệ trên xe** có nút **Hoàn tác về K…** cho lượt còn cần đối soát. Xử lý được đơn cũ đã đóng/xóa nhưng kệ vẫn treo, không cần tìm lại đơn trong bảng nhiệm vụ. Vẫn phải đưa kệ về nguồn, làm xe trống và xác nhận; xe chưa kết nối/RFID thì báo hướng dẫn kết nối lại. [ADR-089](decisions/ADR-089-test-order-rollback.md).

26/10/2026, REQ-WAREHOUSE-TEST-ROLLBACK-01: đơn gián đoạn có nút **Hoàn tác về K…**. Sau khi người giám sát đưa kệ về kho lấy, làm xe trống và xác nhận, server hoàn tác lượt dang dở/đóng đơn; lượt đã hạ giữ kho giao. Kho nguồn có kệ khác hoặc xe chưa dừng sẽ báo lỗi. Chỉ một hộp xác nhận, không thêm form chọn vị trí; áp dụng cả đơn đã xóa đang chờ đối soát. [ADR-089](decisions/ADR-089-test-order-rollback.md).

26/10/2026, REQ-WEB-REPORTS-ROBOT-IDS-01: **Hoạt động xe** chỉ hiển thị ID `01`, `02`; bỏ các ID lịch sử khác khỏi bảng và số lượng. Giữ thời gian của đúng ID, không cộng alias, không thay dữ liệu báo cáo đã lưu. [ADR-088](decisions/ADR-088-compact-reports-workspace.md).

26/10/2026, REQ-WEB-REPORTS-COMPACT-01: năm khối báo cáo/nhật ký được gộp thành **Báo cáo & nhật ký**, gồm **Hiệu suất**, **Kho cuối ca**, **Nhật ký**, chỉ hiện một mục mỗi lần. Chọn báo cáo đã lưu bằng ô **Chọn ca**; Hiệu suất hiện bốn chỉ số, phần nhiệm vụ/hoạt động xe thu gọn. Kho cuối ca dùng một bảng đối chiếu vị trí kệ đầu/cuối ca, nhật ký ca thu gọn và nút **Tải JSON**. Nhật ký chọn **Chuyển kệ/Hệ thống**; giữ tạo KPI, tải lại, số liệu và lịch sử theo ID đã lưu. Không gộp các ID xe cũ khác nhau hoặc suy KPI và báo cáo kho là cùng ca. [ADR-088](decisions/ADR-088-compact-reports-workspace.md).

26/10/2026, REQ-WEB-LOCATION-NAMES-03: form hoán đổi tên chỉ còn tiêu đề, hai ô chọn và nút **Cập nhật**. Bỏ các đoạn giải thích, dòng xem trước và dòng thông báo dưới nút; lý do khóa xem trong tooltip nút. Các mô tả form có xem trước ở phần lịch sử bên dưới được thay thế bởi yêu cầu này.

26/10/2026, REQ-WEB-LOCATION-NAMES-02: bỏ toàn bộ phần **Cấu trúc vị trí cố định**, form NODE/CẠNH ĐƯỜNG và handler JS tương ứng. **Chỉnh cấu trúc** chỉ còn **Hoán đổi tên kho / trạm**. Chốt fleet trống vẫn áp dụng cho nạp snapshot; API sửa graph giữ nguyên. Ghi chú về form sửa node/cạnh phía dưới ở bản triển khai trước đã được thay thế bởi yêu cầu này. [ADR-087](decisions/ADR-087-server-location-names.md).

26/10/2026, REQ-WEB-LOCATION-NAMES-01: trong **Bản đồ → Chỉnh cấu trúc → Hoán đổi tên kho / trạm**, chọn hai tên K1–K6 hoặc hai tên S1/S2, xem trước vị trí rồi bấm **Cập nhật tên kho / trạm**. Chỉ mở khi ca đã đóng, xe rảnh, không công việc/đối soát/vùng đang giữ. Có thể giữ hai xe trong Core khi đổi tên; phần sửa node/cạnh phía dưới vẫn có chốt fleet trống riêng. Tên mới dùng ở bản đồ, chọn kho, giao việc, tồn kho, thẻ xe và trạm riêng; khi khác tên ban đầu hiển thị kèm “vị trí …” để đối chiếu thẻ. Gateway lưu tên qua restart và dịch lựa chọn về ID vật lý trước khi giao xe; không cần nạp firmware. Kho/kệ vẫn ở vị trí vật lý cũ, S1/S2 chỉ đổi nhãn, không chuyển trạm về của xe. Lịch sử giữ ảnh tên lúc ghi nhận; log/báo cáo kho và editor graph/snapshot dùng ID cố định. Cần khởi động lại gateway Node.js rồi tải lại web để dùng API mới. [ADR-087](decisions/ADR-087-server-location-names.md).

26/10/2026, REQ-WEB-OVERVIEW-NAV-01: bỏ trang/tab **Nhiệm vụ** bị trùng với Tổng quan. Điều hướng còn **Tổng quan – Bản đồ – Báo cáo**; tạo đơn, thao tác nhiệm vụ và mở lịch sử xe dùng Tổng quan. Bảng nhiệm vụ vẫn ở dưới bản đồ. [ADR-086](decisions/ADR-086-map-first-missions-and-single-inventory.md).

26/10/2026, REQ-WEB-LAYOUT-INVENTORY-01: **Nhiệm vụ và kết quả** nằm ngay dưới bản đồ, giữ bố cục desktop mở rộng/thu gọn và mobile. **Trạng thái kho** là một khối duy nhất trong bảng xe/kho, gồm K1–K6, kệ trên xe, các vị trí cần đối soát và form nhập kho; bỏ bảng Trạng thái kệ ở sidebar. Khối hiện tại vẫn có ở Báo cáo, không thay các bản chụp trạng thái ca đã lưu. [ADR-086](decisions/ADR-086-map-first-missions-and-single-inventory.md).

26/10/2026, REQ-WEB-UNIFIED-MISSIONS-01: **Nhiệm vụ và kết quả** là bảng duy nhất thay Các lượt lấy→giao và bảng lịch sử task cũ. Một đơn một hàng, mở Chi tiết xem từng lượt/xe/chặng; task độc lập là hàng riêng. Bộ lọc trạng thái có số lượng, tìm tuyến/xe/mã, checkbox xem đơn đã đối soát; mobile hiển thị hàng dạng thẻ. Hủy áp dụng task/chặng đúng phiên, Xóa vẫn dọn danh sách bền vững. Liên kết task theo ID/phiên hoặc khóa DB đã ghi; dữ liệu cũ không đoán ghép. [ADR-085](decisions/ADR-085-unified-mission-results-board.md).

26/10/2026, REQ-WEB-DELETE-RECORD-01: mỗi đơn trong **Các lượt lấy → giao** và mỗi dòng **Nhiệm vụ và kết quả** có nút **Xóa**. Xác nhận bỏ mục khỏi danh sách, lưu qua tải lại/khởi động lại gateway; không hủy nhiệm vụ hoặc đổi sổ kho/KPI. Xóa đơn active vẫn theo dõi xe; đơn đã xóa chuyển sang cần đối soát có nút xử lý dưới bảng xe/kho. Đối tượng là từng ID đầy đủ, không dùng mã task rút gọn. [ADR-084](decisions/ADR-084-delete-individual-dashboard-records.md).

26/10/2026, REQ-WEB-FIXED-VEHICLES-01: **Trạng thái xe và kho** thay bảng Đội xe chi tiết bằng hai thẻ cố định01/02. Thẻ có pin/tốc độ/tuổi telemetry, RFID/đích/kệ/tuyến, cảnh báo và Lịch sử/Chọn xe/Xóa lỗi/Gỡ khỏi Core. Xe chưa xuất hiện hoặc đã gỡ vẫn giữ thẻ, không tạo robot/marker/task giả. Gỡ thành công chỉ bỏ bản ghi Core; telemetry mới cập nhật lại cùng thẻ. Cache riêng từng xe và từng nhóm ô kho; xem [ADR-083](decisions/ADR-083-fixed-vehicle-cards-in-warehouse-overview.md).

25/10/2026, REQ-WEB-SERVER-MAINT-01: dashboard dùng chung quy tắc kho K1–K6 với server, tách helper/map renderer khỏi `dashboard.js`, giữ DOM node/cạnh tĩnh khi chỉ telemetry xe đổi và chỉ dựng lại lớp xe/tuyến động. Bảng kho dùng projection không chứa odometry, vòng cập nhật thời gian còn một timer 1 giây và animation chỉ dùng `requestAnimationFrame` khi marker đang chuyển. Màu trạng thái gom về biến CSS, thuật ngữ dùng kệ/nâng/hạ và chữ trạng thái kho tối thiểu 12 px. Không thay REST/WS, session hoặc audit.

25/10/2026: khi hủy hoặc đổi tuyến đang chạy, bảng xe hiện rõ đang chờ xác minh dừng/RFID và số giây chờ; chốt này chỉ mở bằng proof mới từ Core. Nếu diagnostic vượt giới hạn64boot, UI báo cần đối soát rồi restart Core. [ADR-082](decisions/ADR-082-core-cancellation-startup-and-durability.md).

21/10/2026, REQ-SHIFT-WAREHOUSE-01: bấm **Mở ca** để nhập/kiểm tra trực tiếp vị trí kệ trước khi ca chạy. Dữ liệu đã lưu được điền sẵn; để trống ô không có kệ, xác nhận đã kiểm tra thực tế và xe trống, rồi bấm **Xác nhận và bắt đầu ca**. Web lưu kho khi CLOSED trước, chỉ mở ca sau khi lưu thành công; lỗi mở ca không xóa lần lưu đã thành công. Hủy trước xác nhận không lưu/mở ca. Không cho chỉnh kho khi RUNNING; vẫn giữ form đối soát riêng. [ADR-074](decisions/ADR-074-warehouse-review-before-shift-start.md).

18/10/2026, bổ sung REQ-WAREHOUSE-UI-01: tiêu đề bảng đổi thành **Trạng thái kho hàng** theo ảnh người dùng. Danh sách lượt và bộ chọn tiến độ mặc định ẩn đơn `closed` (đã đóng sau đối soát); bật **Hiện đơn đã đối soát (N)** để xem lại. Đơn active/cần đối soát vẫn hiện, dữ liệu SQLite và báo cáo không bị xóa.

18/10/2026, REQ-WAREHOUSE-UI-01: bảng **Xe đang đi đâu · Kho đang có gì?** nằm phía trên map ở Tổng quan/Nhiệm vụ. Mỗi xe hiện LẤY TỪ→GIAO ĐẾN, pha hiện tại, RFID, đích lệnh và mã kệ; sáu ô kho hiện trạng thái hiện tại và đầu ca, xe lấy/nhận/giữ chỗ/đối soát. Form chuyển kệ bỏ ô số lượng1, hiện nguồn có kệ/đích trống và lý do nút giao việc chưa sẵn sàng. Tuyến/xe được ưu tiên trong danh sách đơn; mã/chi tiết đường nằm phần mở rộng. Dữ liệu cũ và lỗi không tiếp tục hiển thị bước như đang chạy. [ADR-071](decisions/ADR-071-warehouse-operations-overview.md).

18/10/2026: bảng **Trạng thái kệ** ở Tổng quan/Nhiệm vụ/Báo cáo hiển thị từng ô K trống/có kệ/đặt chỗ/cần đối soát và kệ trên xe. Mở **Nhập / đối soát vị trí kệ khi ca đã đóng**, nhập mã tại vị trí thực tế, để trống ô rỗng, kiểm tra xe đã trống rồi xác nhận lưu. Không gán vị trí mặc định. Đơn chuyển kệ cần nguồn có kệ và đích trống, một kệ mỗi vị trí. Khu Báo cáo thêm nhật ký chuyển kệ và báo cáo đầu/cuối ca tự lưu khi Core CLOSED; chọn xem lại hoặc tải JSON. Vị trí dựa vào kết quả servo, chưa có cảm biến tải. [ADR-070](decisions/ADR-070-warehouse-racks-and-shift-reports.md).

17/10/2026: mapv4 vẽ bốn cạnh bo theo geometry, gồm cả tuyến dự kiến và xem trước snapshot; không dựng nodeC hoặc đường chéo nối thẳng. Nhãn hai đoạn85/50, các node/kho/trạm giữ tên và mũi tên theo ảnh. Vị trí encoder/sonar do Core nội suy trên sơ đồ, chưa là tọa độ đo thật. [ADR-064](decisions/ADR-064-rounded-map-without-corner-tags.md).

16/10/2026, test tạm: dashboard đọc `fleet.battery_gate_enabled=false` để hiện rõ bỏ chốt pin/sạc thủ công, không còn báo ngưỡng20/40 đang có hiệu lực. Bộ chọn xe vẫn giữ scope; thiếu cờ tương thích Core cũ. Số đo và cảnh báo pin vẫn hiển thị, không tự chặn nút theo %. [ADR-060](decisions/ADR-060-temporary-battery-bypass-manual-charge.md).

16/10/2026, REQ-ROBOT02-PARITY-01: tốc độ/marker encoder, sonar và kiện ảo hiển thị cho cả01/02; scope riêng02 hiện cùng nút Lấy/Hạ, phát API tới02. Nhãn vị trí chi tiết theo xe scope/chọn; thanh tốc độ liệt kê cả hai. [ADR-059](decisions/ADR-059-robot02-firmware-parity.md).

16/10/2026: dashboard nhận `operation_profile`, `active_robot_ids` và `participating` từ Core thống nhất. Bộ chọn task vô hiệu xe ngoài scope; scope riêng 01 vẫn hiện thao tác Lấy/Hạ, còn scope hai xe dùng điều phối bình thường. Nhãn UI ghi rõ scope vẫn áp dụng pin/mutex như Core thật. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

16/10/2026, phần trăm pin diagnostic đã được người dùng chấp nhận làm chuẩn điều phối. Dashboard tô cảnh báo ở≤20%, ghi rõ nguồn diagnostic và mode thường dùng ngưỡng20/40. Các mode trial vẫn không chặn theo ngưỡng dù Core đã nhận số pin. [ADR-021](decisions/ADR-021-display-only-battery.md).

15/10/2026, test servo xe01: trạng thái LIFTING/LOWERING hiện Đang nâng hàng/Đang hạ hàng, lift.state=PENDING hiện Đang gửi lệnh servo. Sau nhúng hoàn tất chu kỳ5s, virtual_cargo=true vẽ đúng một kiện trên marker và nhãn1kiệnảo; DONE_DOWN bỏ kiện. Khi mất độ tin cậy, dấu kiện cũ mờ/chưa xác minh, không giả là hàng thật. Nút lấy/hạ kho giữ tuyến tới kho và lùi gate; có capability mới chờ kết quả servo đúng lệnh, firmware cũ ghi rõ mô phỏng5s/chưa điều khiển servo. [ADR-057](decisions/ADR-057-robot01-servo-lift-test.md).

14/10/2026, test 01: marker nét đứt là vị trí ước tính encoder giữa hai RFID, vòng nhỏ là RFID gần nhất. Tốc độ cm/s hiện ở đầu/bên dưới bản đồ và dòng xe 01; chưa đo/cũ là `—`. Web nối hai mẫu đã nhận bằng animation 180 ms, dừng/giữ marker và ghi dữ liệu cũ sau 1000 ms hoặc mất liên kết/lỗi. Chỉ RFID xác nhận node/hoàn thành. [ADR-050](decisions/ADR-050-robot01-encoder-position-speed.md) thay quy ước chấm chỉ ở node của xe 01 trong đoạn lịch sử bên dưới; mode khác giữ RFID.

Yêu cầu mới test01 ngày10/10/2026: hai nút Lấy/Hạ riêng, chọn kho, tới kho/chờ5s/lùi gate; ẩn form/lịch sử đơn và không giữ xe bởi bản ghi cũ. Gate đứng chờ rồi xoay khi có tuyến tiếp. [ADR-045](decisions/ADR-045-standalone-warehouse-tests-without-orders.md) thay luồng đơn hàng test01 bên dưới.

Ngày10/10/2026: luồng lấy/giao dừng5s trong mỗi kho rồi lùi ra gate; sau giao chỉ kết thúc lượt khi task exit completed và RFID/IDLE đúng gate. Lý do chờ xe nêu ID đơn cũ giữ xe; đơn chưa từng phát lệnh không giữ robot. [ADR-044](decisions/ADR-044-five-second-warehouse-service-and-gate-exit.md).

Nền `OBSERVED`: `AGVwebserver/index.html`, REST tại gateway và WebSocket `/ws`. Giữ HTML/JS hiện có; chuyển React/Vue/TypeScript chỉ khi nhu cầu bảo trì hoặc yêu cầu mới có căn cứ.

## Luồng người vận hành

- Xem bản đồ, node, cạnh một chiều/hai chiều, vật cản và vùng dùng chung.
- Theo dõi hai robot: node đo gần nhất, lộ trình, pin, trạng thái và thời điểm nhận dữ liệu.
- Tạo nhiệm vụ theo đích hoặc chọn robot cụ thể; theo dõi chờ, đang chạy và kết quả.
- Xem lỗi, lịch sử, snapshot và thông tin mất kết nối.

Đây là định hướng giao diện. Những trang/chức năng chưa được yêu cầu hoặc kiểm thử không được đánh dấu hoàn tất.

## Dữ liệu và ý nghĩa hiển thị

Cập nhật 10/10/2026: chấm xe là RFID gần nhất, mũi tên phía trước biểu diễn đầu xe. Kho K hướng vào trong khi xử lý hàng: tiến vào/lùi ra. Trạm S1/S2 hướng ra cửa: tiến vào trạm rồi quay đầu tại S, tiến ra bắt đầu tuyến mới. Lúc lùi mũi tên đầu xe giữ hướng. Không còn bộ chọn hướng thủ công. ESP32 gửi heading mặc định hoặc heading tuyến; khi chưa biết/mất mạng/vị trí chưa xác minh thì không vẽ hướng chắc chắn. S1 ↓ ra GS1, S2 ↑ ra GS2, K1–K3 ←, K4–K6 → theo map hiện tại. Đặt xe thực tế khớp quy ước trước thử. [ADR-033](decisions/ADR-033-stations-face-outward.md).

Web nhận state chứa `map`, `fleet`, `mutex`, `logs`, cùng `agv_connected` do gateway thêm. `fleet.agvs` có các trường như `id`, `node`, `battery`, `state`, `path`, `target` theo `toJson()` của core. Đọc implementation trước khi thêm type/schema; không mặc định state có timestamp của robot.

`agv_connected` phản ánh socket gateway-core, không chứng minh từng robot online. Biểu diễn tuổi dữ liệu cần bổ sung metadata rõ ràng; thời điểm browser nhận snapshot không bằng thời điểm cảm biến đo.

Nếu chỉ có RFID/node, vị trí chắc chắn là node đo gần nhất. Nội suy robot chạy trên cạnh phải ghi là ước lượng; không giả định tọa độ liên tục thật từ firmware.

## Quy ước thao tác

1. Xác nhận node đích tồn tại từ snapshot mới nhất.
2. Gửi API và hiển thị chờ kết quả.
3. HTTP 202 chuyển trạng thái “đã gửi”; kết quả từ core mới quyết định “đã nhận nhiệm vụ”.
4. Khi mất kết nối, hiển thị dữ liệu cũ và trạng thái liên kết; không giả định robot đã dừng.
5. Không để thao tác lặp tạo nhiều task ngoài ý muốn; thiết kế request ID khi sửa API.

Trong mã hiện tại chưa có đầy đủ kết quả tương quan từ core; nêu rõ khoảng trống khi cải tiến UI.

## Kiểm tra thay đổi

- Trạng thái bình thường, không có xe, hai xe, pin thấp, lỗi, offline.
- Mất WS, reconnect, payload thiếu trường, log dài và ID không chứa HTML an toàn.
- Không inject tên node/log vào `innerHTML` chưa escape; dùng text node cho dữ liệu.
- Zoom/resize không đổi tọa độ thật; chuyển đổi pixel và đơn vị bản đồ phải thống nhất.
- Thử thao tác chỉ trên môi trường mô phỏng hoặc dữ liệu riêng khi ảnh hưởng task/map.

Tham khảo tổ chức dashboard/API từ [rmf-web](https://github.com/open-rmf/rmf-web); chỉ áp dụng các ý tưởng phù hợp, không yêu cầu chuyển toàn bộ dự án sang RMF. Hợp đồng API: [server](04-server.md). Kỹ năng: [agv-web](../.agents/skills/agv-web/SKILL.md).

## Bản triển khai 29/09/2026

Giao diện hiện tách script tại `AGVwebserver/public/dashboard.js`. Hàm API giữ array; snapshot/KPI/history hiển thị đúng. Core WS v1 xác nhận thao tác trước HTTP thành công. Đã có route, tuổi telemetry, task/robot history và editor node/cạnh. Các đoạn mô tả khoảng trống phía trên thuộc khảo sát nền; xem [chức năng hiện tại](14-web-server-update.md) và [hợp đồng](../contracts/web-server.md).

## Bố cục người vận hành 01/10/2026

Theo yêu cầu `REQ-WEB-03`, dashboard chia bốn khu vực **Tổng quan**, **Nhiệm vụ**, **Bản đồ** và **Báo cáo**. Tổng quan ưu tiên đội xe, sơ đồ, vùng chung, ca và giao việc. Nhiệm vụ hiện lịch sử/task ID và lịch sử xe. Bản đồ chứa quản lý vật cản, snapshot và editor. Báo cáo chứa KPI và nhật ký. Các khu vực dùng cùng state/DOM; đổi khu vực không tạo socket hoặc phiên core mới.

Thanh đầu hiển thị riêng Web, core và MQTT; cấu hình URL/kết nối lại nằm trong `Thiết lập kết nối` vì web đã tự reconnect. Danh sách node trong form lấy từ map core; khóa/mở vật cản là một nút theo trạng thái node đang chọn. Mobile có nút phóng to và cuộn bên trong bản đồ để đọc node mà không làm trang tràn ngang. `CHARGING` hiển thị là **Ở trạm sạc**, tránh diễn giải là đã đo dòng sạc. Không đổi endpoint, payload hay điều kiện điều khiển của core.

Tham khảo cách phân khu chức năng của [Open-RMF Web demo/main.tsx](https://github.com/open-rmf/rmf-web/blob/main/packages/rmf-dashboard-framework/examples/demo/main.tsx), xem 01/10/2026. Chỉ lấy ý tưởng phân vùng map/robots/tasks; không sao chép component hoặc mã. Quyết định và giới hạn tại [ADR-008](decisions/ADR-008-operator-dashboard.md). Kết quả Edge headless desktop/mobile tại [hồ sơ kiểm thử](testing-web-server-2026-09-29.md); chưa phải kiểm thử với người vận hành thực tế.

## Đơn hàng trên màn Tổng quan 08/10/2026

Khung Giao nhiệm vụ có hai chế độ: Đơn hàng kho (mặc định, nhập nhiều kho lấy/giao và số kiện từng kho) và Đi tới node. Danh sách đơn hiện cả Tổng quan và Nhiệm vụ để người vận hành thấy tiến độ mà không phải tìm ở cuối cột điều khiển. Theo yêu cầu 09/10/2026, bấm node khi chưa chọn robot gửi task tự phân xe qua `/api/tasks`; Core chọn xe khả dụng gần nhất theo tổng trọng số đường. Bấm robot trước rồi bấm node vẫn giao riêng xe qua GOTO. Trường node và vật cản vẫn được điền để người vận hành xem/sửa trong form. HTTP trả `task_id` là đã nhận lệnh, chưa chứng minh xe tới đích.

Chế độ Đơn hàng kho hiện thanh tiến độ theo đơn được chọn: tử số là số kiện có `status=delivered`, mẫu số là tổng kiện của đơn. Khi có nhiều đơn, người vận hành chọn đơn trong danh sách ngay trên thanh; mặc định ưu tiên đơn đang hoạt động. Kho giao đang ở bước mô phỏng dỡ hàng 10 giây chưa được tính là đã giao. Đây là KPI tiến độ đơn, không phải cảm biến xác nhận hàng vật lý.

Khi Core trả `shift.single_robot_trial=true`, form Đi tới node chọn xe 01 trực tiếp thay cho tùy chọn tự phân xe. Từ REQ-BATTERY-06, Core mới tự chuyển diagnostic calibrated thành pin điều phối; giao diện chỉ trình bày `battery_control_source` và không tự thay ngưỡng 20%/40%.

Khung Ca làm việc hiện rõ Core đang ở chế độ thường hay thử. Mode thường ghi pin diagnostic dùng cho điều phối; mode trial ghi rõ ngoại lệ không chặn theo ngưỡng.
## Cảnh báo vật cản xe 01 (15/10/2026)

Core test 01 nhận hai sonar và encoder: khi dừng vì vật cản, cạnh đang đi được tô đỏ và một chấm đỏ biểu diễn vị trí ước tính của vật cản. Tooltip cho biết phía trước/sau và khoảng cách đo cm. Điểm dùng tọa độ map cm, không phải pixel cố định; thiếu mẫu encoder/sonar mới thì ẩn cảnh báo vị trí. Khi vật cản rời đi và firmware đọc >20 cm, trạng thái đỏ biến mất, xe tiếp tục tuyến cũ nếu mọi chốt an toàn khác còn hợp lệ. Độ chính xác điểm và quãng phanh cần thử xe thật; xem [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).
