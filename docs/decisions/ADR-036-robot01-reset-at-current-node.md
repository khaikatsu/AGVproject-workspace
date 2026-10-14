# ADR-036 — Core test01 reset/mở ca tại RFID hiện tại

Ngày10/10/2026. IMPLEMENTED trong mã; đang build/kiểm thử.

SOURCE: người dùng yêu cầu bỏ thông báo/chốt bắt về S1 trước khi reset giữa tuyến trong Core test xe01, để thử nhận lệnh server đi lấy/trả hàng rồi về kho. Quyết định này thay riêng manual reset/home-start của robot01 ở ADR-026; robot02/both giữ quy tắc cũ.

Core test --mode robot01 bỏ yêu cầu currentNode=S1 tại cả WebCommands và FleetManager::clearError. Xóa lỗi gửi RESET_FAULT ở node hiện tại và giữ ERROR tới diagnostic AGV_IDLE/NONE/STOP, PWM0/0, hết task, cùng node/vị trí hợp lệ và có lần đọc RFID trong phiên. ACK không mở IDLE. Xóa kế hoạch uncertain tương lai sau reset khỏe; không trả owner/grant mutex tại node giữa tuyến.

Test01 có thể mở/đóng ca tại RFID hiện tại khi xe online/IDLE/vị trí hợp lệ, không task/path/uncertain; không bắt về S1 sau Core restart hoặc mỗi lượt thử. all_home/at_home vẫn phản ánh trạm thật, không giả lập xe ở S1. Xe không được chọn vẫn phải dừng ở trạm; mutex và điều phối tuyến giữ nguyên. Nhiệm vụ gián đoạn không tự phát lại; lượt mới theo task/order hiện có.

Không đổi firmware, không cần nạp lại ảnh grant-buffer-fix. Chốt local sensor/mạng/cấu hình và hướng UNKNOWN sau reboot vẫn tồn tại trong MCU; IR không đo góc tuyệt đối. Snapshot lúc bắt đầu sửa báo GS1/UNEXPECTED_NODE/headingUNKNOWN/line000, cần đặt mắt trên vạch trước reset. Không giả định bỏ Core home gate chứng minh xe đã bám line được hoặc biết hướng sau reboot.

OPEN: Core unit/fixture đang chạy, cập nhật binary test01 và kiểm tra HTTP reset tại node. Không đổi stack, counter/map/runtime/DB; chưa thử tuyến vật lý lấy/trả hàng.