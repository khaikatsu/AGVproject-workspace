# ADR-004 — Một listener core và khởi động từ gốc workspace

Ngày: 29/09/2026. Trạng thái: IMPLEMENTED, sửa lỗi khởi động trong FIX-RUN-01.

## Bối cảnh OBSERVED

Ảnh người dùng báo `EADDRINUSE:3000` và dashboard báo mất liên kết MQTT. Kiểm tra máy thấy nhiều core lắng nghe cùng `127.0.0.1:8080`. Mã Windows đặt `SO_REUSEADDR`, mọi core dùng MQTT client ID `AGV_MasterServer` và kết nối broker trước khi bind WS. Gateway dùng localhost trong khi core bind IPv4. Cách F5 trước đó chạy executable trong src với working directory của thư mục cha.

## Quyết định triển khai

- Windows dùng `SO_EXCLUSIVEADDRUSE`, POSIX giữ `SO_REUSEADDR`. Core bind/listen trước mọi ghi map/counter và MQTT, chỉ bắt đầu accept sau khi cấu hình xong.
- Gateway mặc định `ws://127.0.0.1:8080`. Không đổi payload, MQTT client ID hay cơ chế FleetManager/MutexArbiter.
- Wrapper và cấu hình VS Code build/chạy `build/agv_core.exe` từ gốc workspace. Biến môi trường explicit được ưu tiên. Khi chạy mặc định mà counter mới chưa tồn tại, tiếp tục counter cũ ở src nếu có; không reset hoặc sao chép counter.

## Giới hạn

Khóa listener ngăn hai core cùng địa chỉ/cổng; không tạo HA hoặc khóa toàn broker trên nhiều máy/cổng khác nhau. Chỉ vận hành một core cho đội xe. Cần dừng executable cũ để dùng bản sửa. Counter hoặc map override phải tách riêng khi chạy môi trường thử.

Kiểm chứng phần mềm: [startup.integration.js](../../tests/startup.integration.js) với broker/cổng/file riêng; [cách chạy và kết quả](../16-startup-troubleshooting.md). Chưa kiểm thử robot thật.
