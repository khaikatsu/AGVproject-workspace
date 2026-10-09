# ADR-009: Điều khiển có xác thực và checkpoint an toàn

- Ngày: 01/10/2026.
- Trạng thái: Accepted trong phạm vi yêu cầu trực tiếp sáu nhóm của người dùng.
- Nguồn: SOURCE; kết quả mã/test là OBSERVED, an toàn vật lý và vận hành công nghiệp OPEN.

Giữ C++17/libmosquitto/Node CommonJS/Express/ws/SQLite/HTML. Core là nơi cấp task-ID và giữ queue/mutex; Node theo dõi lịch sử. Core lưu checkpoint trước outbound, file lỗi làm ngừng phát lệnh. Task pending khôi phục, task đang chạy interrupted; vị trí cũ không là telemetry mới. Không nhả vùng vì offline/LWT hoặc restart.

Mutex v0.1 dùng grant_id cùng allocator command hiện có; ACK/grant retry và release đúng generation. Firmware/server phải cập nhật đồng bộ. Owner khôi phục cần xác minh/request mới, token cũ không nhả quyền. Có thao tác xác minh vùng trống dành cho lỗi/offline, với xác nhận vật lý và kiểm tra token.

Web/API/WS dùng token chung và cookie HttpOnly; MQTT dùng account/ACL từng robot và CA/TLS. Mặc định local dev bind loopback; LAN cấu hình token, TLS/HTTPS và origin cụ thể. Không cài chứng chỉ hoặc thay broker vận hành tự động. Docker giữ WS Core trong network nội bộ.

Tối đa 5 state/giây, delta cấp trường, batch DB và retention. Lệnh/result/control loop không bị đợi throttle UI. TTL sensor không tự chứng minh đường trống; khóa thủ công không hết hạn. ESP32 failsafe là gate logic, chưa có motor/driver để khẳng định phanh vật lý.

Tradeoff: token chung chưa có RBAC; WS Core chưa TLS-native; batch SQLite chưa worker; terminal records Core giới hạn1000; mất gateway crash trước flush có thể mất event history. Scope kiểm thử gồm unit, real MQTT/TLS/ACL, tám robot logic, browser, host sketch và compile ESP32; Docker/CI và board thật chưa được xác nhận runtime. Hướng dẫn và contract tại [18-hardening](../18-hardening.md).
