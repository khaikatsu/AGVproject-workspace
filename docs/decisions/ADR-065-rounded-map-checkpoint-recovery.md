# ADR-065 — Khôi phục checkpoint map v3 khi chuyển sang map bo v4

Ngày: 17/10/2026. SOURCE: người dùng báo dashboard mất map và F5 dừng tại kiểm tra checkpoint dòng883, yêu cầu sửa. Bổ sung phần áp dụng của ADR-064; không thay thiết kế mutex.

OBSERVED: data/map_layout.json có22node/35cạnh nhưng data/runtime_state.json lưu26node/39cạnh, hai xe ở C1/C3 và còn owner/token M1/M5/M6. Listener8080 được tạo trước khi đọc checkpoint; startup lỗi hoặc bị debugger dừng khiến gateway chưa nhận snapshot. Dòng breakpoint không tự chứng minh lỗi; hai graph thực tế không tương thích mới là nguyên nhân lần này.

IMPLEMENTED: RuntimeStore nhận diện riêng phép chuyển bốn chuỗi G1→C1→M1, M2→C2→G6, M3→C3→G1, G6→C4→M4 thành bốn cạnh bo135cm. Kiểm tra26→22node,39cạnh cũ, node góc/tọa độ, hướng/trọng số cũ70/50, geometry85/50 mới, và mọi node/cạnh/mutex/home còn lại khớp. Không áp dụng cho graph khác hoặc checkpoint có obstacle; trường hợp đó dừng sạch với thông báo thay vì exception chưa bắt.

Trước khi ghi checkpoint mới, sao lưu nguyên byte file cũ thành runtime_state.json.before-rounded-map-v4-N.json, không ghi đè bản sao trước. Giữ nguyên lịch sử nhiệm vụ, owner và grant ID. Mục tiêu C đã bỏ được đánh dấu target_retired chỉ để đọc lịch sử terminal. Task pending/dispatched chuyển interrupted, hàng chờ task/mutex cũ không được chạy lại. Không tạo task hoàn thành hoặc giải phóng vùng từ việc đổi map.

Xe ở C không được đặt giả sang G/M: node để trống, position_valid=false; hai đầu cạnh mới và mọi đường chưa xác minh cũ được giữ trong uncertain_path. Fleet restore vẫn OFFLINE và chờ RFID/đối soát. Các owner phục hồi vẫn recovery_required. File counter command/task giữ cơ chế tiếp nối hiện có; không xóa/reset checkpoint, counter hoặc DB.

VERIFIED: xem tests/README.md. Kiểm thử dùng file/broker/cổng riêng; không phải bằng chứng xe thật đã rời vùng hoặc firmware đã nạp mapv4. Triển khai binary vận hành cần dừng đúng phiên Core đang chạy; người dùng đã xác nhận cho phép dừng và cập nhật trong hội thoại17/10/2026.
