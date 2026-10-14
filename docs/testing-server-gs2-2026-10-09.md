# Rà soát server khi xe 02 dừng tại GS2

Ngày 09/10/2026. Yêu cầu `REQ-SERVER-GS2-AUDIT-01`. Phạm vi: Core C++/MQTT, task và reset lỗi; giữ cấu trúc workspace. Không gửi lệnh cho robot thật, không sửa DB, map hoặc counter vận hành.

## Trạng thái quan sát

`OBSERVED`: danh sách tiến trình có một `agv_core.exe` tại `build/agv_core.exe` và một `mosquitto.exe`; chưa có bằng chứng hai Core chạy trùng. Snapshot trực tiếp có `shift.test_mode=robot02`, xe 02 OFFLINE tại GS2, M3 owner 02/grant 48134 đã ACK. Task trước đó gián đoạn vì LWT và INVALID_COMMAND. Core hiện giữ vùng/đường chưa xác minh khi xe offline; không được giải phóng bằng giả định xe đã rời vùng.

Diagnostic ở lượt trước có `resetReason=PANIC`; lần rà soát này xe offline nên không có diagnostic mới. Không kết luận lý do panic, hướng quay motor hoặc chất lượng nguồn từ kiểm thử server.

## Lỗi phục hồi đã sửa

`OBSERVED` trước sửa: `FleetManager::clearError` chuyển ERROR thành IDLE, xóa uncertain_path, gửi RESET_FAULT và gọi dispatchPending ngay. Task chờ có thể được giao trước khi MCU xác nhận reset thành công. Sau reboot tại GS2, firmware không còn previous RFID để biết hướng xuất phát, nên tuyến mới có thể bị INVALID_COMMAND. Đây là lỗi phục hồi sau sự cố, chưa chứng minh nguyên nhân xe reset/dừng lần đầu.

Regression thêm vào `tests/core_tests.cpp` thất bại trước sửa tại `!fleet.clearError("02")` khi xe ở GS2. Sau sửa:

- Ca thử robot01/robot02/both yêu cầu đúng trạm S1/S2 trước reset thủ công; WS/HTTP trả 409 kèm lý do khi xe ở giữa tuyến.
- Tại trạm, RESET_FAULT giữ ERROR, queue và phần đường chưa xác minh đến diagnostic khỏe. ACK chỉ là biên nhận.
- Điều kiện đối soát cũ giữ nguyên: RFID mới tại trạm, Wi-Fi/MQTT khỏe, IDLE/NONE/STOP, PWM 0/0, không task hoạt động; quyền vùng chỉ trả theo kiểm tra khoảng cách trong ADR-026.
- Giao diện báo đang chờ xe xác nhận khỏe, tránh báo đã xóa lỗi khi chỉ gửi reset.

## Kiểm chứng

Build riêng: `scripts/build-core.ps1 -Tests -OutputDirectory .qa/server-gs2-audit-build`. Core/unit PASS. Không thay binary vận hành đang chạy.

| Kiểm tra | Kết quả | Bằng chứng |
| --- | --- | --- |
| Core unit + regression reset | PASS | `.qa/server-gs2-audit-build/core_tests.exe` |
| MQTT GS2, chế độ robot02 và both | PASS | `.qa/server-gs2-test-VXswn1/events.json`, `robot02/final-state.json`, `both/final-state.json` |
| Menu, lọc đúng xe, counter tiếp tục | PASS | `.qa/core-mode-test-hmPSBW` |
| Khởi động trùng trước MQTT/counter | PASS | `.qa/startup-test-B2II1K` |
| Giữ vùng và phục hồi tại trạm | PASS | `build/recovery-check/mqtt-uycW4C` |
| Dashboard với Edge headless | PASS | `.qa/browser-test-3MRfrY` |

Fixture GS2 chạy Core/broker thật trên cổng riêng, mô phỏng MQTT hai xe với dữ liệu riêng. Xác nhận lệnh ban đầu từ S2 là path `[GS2,M3]`; sau telemetry GS2 Core giữ task WORKING/path `[M3]`, cấp/ACK M3 và trả cùng grant ID khi request trùng. Qua 3,4 giây sau ACK không có GOTO trùng hay STOP/WAIT tự phát; thêm task vào queue không ghi đè tuyến đang chạy. Khi inject LWT, Core giữ owner; clear giữa tuyến không gửi lệnh, reset tại S2 không giao GOTO dù đã ACK hoặc diagnostic vẫn SAFE_STOP; diagnostic khỏe mới giao task tiếp. Task cũ giữ interrupted.

`VERIFIED` ở mức S1 hợp đồng/logic. Chưa xác nhận chuyển động bánh, dòng motor, thời điểm panic hoặc RFID thật. Source đã sửa; để dùng, người vận hành dừng phiên Core cũ khi xe đã dừng, F5 build lại và chọn chế độ 2/3 phù hợp. Không khởi một Core mới cùng broker khi phiên cũ còn chạy.
