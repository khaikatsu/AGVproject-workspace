# ADR-088 — Gộp giao diện báo cáo và nhật ký

Ngày 26/10/2026. Trạng thái: IMPLEMENTED theo REQ-WEB-REPORTS-COMPACT-01; browser báo cáo/dashboard/kho PASS trên dữ liệu thử riêng.

SOURCE: người dùng gửi ảnh năm khối báo cáo/nhật ký dài, yêu cầu làm gọn hoặc gộp để đỡ rối.

SOURCE bổ sung 26/10/2026, REQ-WEB-REPORTS-ROBOT-IDS-01: chỉ hiện ID hiện tại `01` và `02` trong Hoạt động xe; các alias cũ `1`, `AGV_01`, `AGV_02` và ID khác bị bỏ khỏi bảng/số đếm. Không cộng thời gian alias và không ghi lại báo cáo lưu. Báo cáo thiếu dữ liệu hai ID này hiển thị trạng thái trống. Quyết định này thay phần hiển thị tất cả ID lịch sử trước đó.

Quyết định: đưa các khối vào một khu Báo cáo với ba mục Hiệu suất, Kho cuối ca, Nhật ký. Chỉ mở một mục tại một thời điểm. Danh sách báo cáo đã lưu thành ô chọn ca, giữ báo cáo đang chọn khi tải lại; không ghép KPI và báo cáo kho theo ngày hoặc đoán chúng là cùng ca. Các chức năng tạo KPI, tải lại và tải JSON giữ nguyên.

Hiệu suất hiển thị bốn chỉ số chính, các số liệu còn lại và hoạt động xe nằm trong phần mở rộng. Không cộng/gộp các ID robot lịch sử như 1/01/AGV_01 vì chưa có chứng cứ đó là một bản ghi. Kho cuối ca hiển thị bốn chỉ số và bảng đối chiếu đầu/cuối ca, nhật ký của ca thu gọn. Nhật ký chung có lựa chọn Chuyển kệ/Hệ thống. Render bằng text an toàn, xử lý thiếu dữ liệu và không tràn ngang mobile.

Chỉ sửa frontend/tài liệu/fixture browser. Giữ API, cách tính KPI, lưu trữ, báo cáo bất biến, Core và firmware. Trạng thái xe/kho và bảng nhiệm vụ đang có ở trang Báo cáo không bị xóa trong yêu cầu này. Kết quả kiểm chứng ghi tại tests/README.md.
