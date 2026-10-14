# Phát triển web giám sát AGV

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

Khi Core trả `shift.single_robot_trial=true`, form Đi tới node chọn xe 01 trực tiếp thay cho tùy chọn tự phân xe. Nếu pin chỉ có `battery_display_percent` mà `battery_measured=false` ở Core thường, form giải thích % này chưa được dùng để cấp nhiệm vụ. Giao diện không tự biến % hiển thị thành pin điều khiển hoặc thay ngưỡng 20%/40%.

Khung Ca làm việc hiện rõ Core đang ở chế độ thường hay thử một xe 01. Khi Core thường và xe chỉ có % pin display-only, khung này cảnh báo rằng số % chưa được dùng để cấp nhiệm vụ; người vận hành không cần suy ra trạng thái trial từ lời báo lỗi pin.
## Cảnh báo vật cản xe 01 (15/10/2026)

Core test 01 nhận hai sonar và encoder: khi dừng vì vật cản, cạnh đang đi được tô đỏ và một chấm đỏ biểu diễn vị trí ước tính của vật cản. Tooltip cho biết phía trước/sau và khoảng cách đo cm. Điểm dùng tọa độ map cm, không phải pixel cố định; thiếu mẫu encoder/sonar mới thì ẩn cảnh báo vị trí. Khi vật cản rời đi và firmware đọc >20 cm, trạng thái đỏ biến mất, xe tiếp tục tuyến cũ nếu mọi chốt an toàn khác còn hợp lệ. Độ chính xác điểm và quãng phanh cần thử xe thật; xem [ADR-055](decisions/ADR-055-robot01-dual-sonar-edge-warning.md).
