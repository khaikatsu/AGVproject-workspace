# Vị trí phát triển firmware

Cập nhật 03/10/2026: sketch đồng bộ mutex JSON grant_id/ACK/retry với Core; có LWT, TLS tùy cấu hình, watchdog heartbeat/phiên Core và lưu cmd_id cao nhất trong NVS. [Driver L298N, line và HC-SR04](AGV_Connection_Test/driver.cpp) đã được nối vào lệnh/phanh cục bộ. Motor **khóa ở cấu hình mặc định** cho tới kiểm thử bàn và chốt cách rẽ/xác nhận rời vùng; xem [ADR-012](../docs/decisions/ADR-012-motor-firmware-integration.md).

Đã có [AGV_Connection_Test.ino](AGV_Connection_Test/AGV_Connection_Test.ino) cho **ESP32 DevKit + Arduino IDE + PubSubClient + RC522**. Sketch có sẵn [26 UID ↔ node](AGV_Connection_Test/README.md), giữ `learn` như override NVS khi thay thẻ, boot chờ quét mới và đồng bộ MQTT với luồng ca 01→S1/02→S2. Pin/sạc vẫn là test; siêu âm HC-SR04 được đưa vào gate motor và telemetry khi bật motion. [ADR-013](../docs/decisions/ADR-013-preloaded-rfid-uid-map.md).

Pinout mới từ [bảng người dùng](bang-phan-chan-agv.md): RC522 SS chuyển sang GPIO5; GPIO21 dành cho servo thứ ba, GPIO0 dành cho còi nên không dùng BOOT làm nút chạy. Cần đối chiếu dây thật trước khi nạp.

Đọc [firmware](../docs/08-firmware.md), [mạng](../docs/07-network.md) và [sổ yêu cầu](../docs/requirements/register.md). Dùng `$agv-firmware` cho phần MCU; giữ fixture giao tiếp đồng bộ với core/simulator.

## Firmware NCKHAGV mới

Thư mục [NCKHAGV/NCKHAGV](NCKHAGV/NCKHAGV/README.md) là project PlatformIO đầy đủ do nhóm mới bổ sung. Ngày 04/10/2026 đã đồng bộ giao thức command/ACK/telemetry/status/heartbeat với Core v0, bảng 26 UID, cấu hình mạng cục bộ, NVS high-water và chế độ khóa motor. Pin, sạc, bảng rẽ và điều kiện rời mutex chưa được hiệu chuẩn/xác minh; xem [ADR-014](../docs/decisions/ADR-014-nckhagv-core-sync.md).
