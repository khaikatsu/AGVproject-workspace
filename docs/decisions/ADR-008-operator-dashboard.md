# ADR-008: Dashboard theo công việc của người vận hành

Ngày: 01/10/2026. Trạng thái: **ACCEPTED/IMPLEMENTED giao diện**; chưa khảo sát người vận hành thật.

## Nguồn và vấn đề

Người dùng yêu cầu thiết kế lại web dựa trên dashboard đội xe mã nguồn mở, dễ đọc/dễ dùng, giảm nút dư và giữ chức năng khớp server. Bố cục cũ dồn giám sát, điều khiển, sửa bản đồ, snapshot và KPI vào ba cột cùng lúc; mobile phải cuộn qua mọi công cụ trước khi xem hết thông tin vận hành.

## Quyết định

Giữ HTML/CSS/JS và các ID/form/REST/WS hiện có. Tách **Tổng quan**, **Nhiệm vụ**, **Bản đồ**, **Báo cáo** bằng thanh điều hướng, chỉ thay phần hiển thị chứ không mở thêm kết nối. Tổng quan có trạng thái Web–Core–MQTT, tóm tắt đội xe/task/vùng/ca, map và thao tác giao việc. Kết nối thủ công chuyển vào mục `Thiết lập kết nối`; web vẫn tự reconnect. Bỏ nút tải lại snapshot khỏi bề mặt chính, tải lại khi vào mục Bản đồ và sau khi lưu. Khóa/mở vật cản dùng một nút dựa trên dữ liệu map core mới nhất. Form node dùng danh sách node từ core; bản đồ có chế độ phóng to/cuộn trong khung trên mobile.

Không gán ý nghĩa mới cho ACK, telemetry hay KPI. `CHARGING` trên web ghi “Ở trạm sạc” vì chưa có phép đo sạc điện. Trạng thái trạm riêng lấy từ core, không gán cứng 01/S1 nếu cấu hình thay đổi. Map sửa cấu trúc và apply snapshot tiếp tục bị khóa theo điều kiện core cũ.

## Tham khảo và giới hạn

Tham khảo phân khu map/robots/tasks trong [Open-RMF Web `packages/rmf-dashboard-framework/examples/demo/main.tsx`](https://github.com/open-rmf/rmf-web/blob/main/packages/rmf-dashboard-framework/examples/demo/main.tsx), truy cập 01/10/2026. Không sao chép mã, component hoặc CSS. Thiết kế và code giao diện trong workspace là bản riêng; không thêm React hoặc dependency runtime.

Giao diện còn phụ thuộc dữ liệu telemetry: sketch test có pin/vật cản giả lập; việc đo pin, sạc điện và vị trí liên tục chưa được giải quyết bằng đổi bố cục. Không tuyên bố dùng được cho vận hành công nghiệp hoặc đạt chuẩn khả dụng chỉ từ ảnh trình duyệt.

## Kiểm chứng

Cập nhật 02/10/2026 theo yêu cầu trực tiếp: tăng diện tích map bằng cột phụ hẹp hơn, header/thống kê/chú thích gọn hơn và viewBox ôm graph có chừa khoảng cho nhãn/xe. Giữ tỷ lệ hình học, tọa độ và trọng số map, cùng nút Phóng to/Vừa màn hình.

Browser regression 02/10/2026 PASS desktop/mobile, không tràn chiều ngang và giữ thao tác map/routes/editor/ca/login. Đã xem ảnh map desktop/mobile mới tại `.qa/browser-test-AI8ywh/`; chưa thay dịch vụ vận hành hoặc dữ liệu map.

Sau phản hồi “vẫn vậy”, bố cục desktop mặc định chuyển sang map toàn chiều ngang, tối thiểu720px cao, bảng phụ xuống dưới; nút Thu gọn/Mở rộng khôi phục bố cục ba cột. Node/nhãn/marker xe lớn hơn; asset có phiên bản URL để refresh lấy đúng mã. Mobile giữ bố cục vừa khung và Phóng to. Không thay map, API hoặc điều khiển robot.

Browser PASS bản cuối tại `.qa/browser-test-hL53Kd/`, gồm kiểm tra chiều rộng map mặc định và khôi phục ba cột. Trang localhost:3000 cũng được mở chỉ đọc, xác nhận HTML/CSS mới và map1250px trên viewport1280px.

Yêu cầu tiếp theo 02/10: kết hợp kích thước map đã duyệt với bảng hai bên của giao diện trước. Giữ canvas/camera/nhãn lớn; dùng khoảng trống thật quanh graph để đặt lại left-panel/right-panel trong cùng khung. ResizeObserver và phép đo node group cập nhật vị trí; chỉ dock nếu hai khoảng trống đều >=260px, chừa khoảng an toàn. Panel cuộn độc lập; khi zoom hoặc màn hình/graph không đủ chỗ, bảng xuống dưới thay vì che node. Reports giữ bố cục riêng.

Browser PASS tại `.qa/browser-test-7JR33o/`: thao tác các chức năng cũ, desktop1600×1050, laptop1366×768 và mobile390×844; kiểm tra panel không đè node, cùng hàng với canvas và không tràn ngang. Đã xem ảnh map/editor và tổng quan có xe tại trạm; giữ nguyên dữ liệu/thuật toán/giao thức server.

`node tests/browser.integration.js` qua Edge headless với core/DB thử riêng: PASS trên desktop 1600×1050 và mobile 390×844. Kiểm tra bốn khu vực, map v3/zoom, giao việc/hủy, lịch sử, ca, vật cản khóa/mở, snapshot/editor/KPI, dữ liệu cũ, mất MQTT và robot chưa định vị. Ảnh tại `.qa/browser-test-LvQBTu/`. Không truy cập DB vận hành hoặc robot thật.
