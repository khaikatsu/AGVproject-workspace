# ADR-056 — Xe01 xoay tại GS1 rồi lùi vào S1

Ngày15/10/2026. Trạng thái: đã duyệt theo xác nhận line của người dùng; đã triển khai và kiểm thử phần mềm, chưa nạp/thử xe thật.

## Yêu cầu mới

`SOURCE`: riêng xe01, khi lệnh về S1 đi qua GS1: dừng ở GS1, xoay để đuôi hướng S1 và đầu hướng ra ngoài (hướng S trên map), rồi lùi GS1→S1. Tại RFID S1 dừng, không quay tiếp tại trạm. Chỉ hoàn thành task/hết Đang chạy sau khi đọc RFID đích thật và có diagnostic mới khỏe, motor STOP/duty0/0, không task nhúng, hướng ra ngoài đúng phiên. ACK lệnh không xác nhận đã đỗ.

Yêu cầu này thay vị trí xoay/cách đi vào S1 của ADR-033 cho xe01; xe02 và kho K giữ thiết kế hiện tại. Giữ sonic sau khi lùi, STOP/mất mạng/lỗi line và mutex. Không thay đổi dữ liệu vận hành hoặc nạp/chạy xe để thử trong bước tiếp nhận.

## Bằng chứng và quyết định

`OBSERVED`: RoutePlanner hiện chỉ cho dockingReverse khi ra kho; khi vào S1, CoreNavigator dùng StationDwell→StationTurning sau RFID, rồi mới stationReady. Map/ADR-033 mô tả GS1 là nhánh ngang M1–M2 cùng nhánh S1; không có nhánh line chạy ra phía đối diện S1. Mắt line hiện có ở phía trước. Nếu mặt sàn giống mô tả đó, quay đầu hướng S tại GS1 làm mắt trước nằm ngoài line; điều kiện bắt nhánh hiện tại không chứng minh được góc xoay đúng và không có line để bắt đầu bám lùi ngay.

`SOURCE`: người dùng đã xác nhận có đường line nối dài qua GS1 về phía đối diện S1. Vướng mắc vật lý ở bước tiếp nhận đã được giải quyết. Đây là đoạn dẫn hướng để mắt trước bắt nhánh hướng S, không thêm node/cạnh điều phối vào bản đồ. Không dùng thời gian hoặc encoder chưa hiệu chuẩn để tự kết luận đã xoay đủ góc.

`OBSERVED` sau sửa: profile01 báo diagnostic `reverseHomeSupported:true`. Chỉ với ID01/capability này, Core thêm `reverse_after_turn:true` trong NODE_ORDER GS1→S1. Maneuver vẫn là góc xoay vật lý: đầu E→RIGHT, W→LEFT, N→TURN_AROUND_RIGHT (hai lần bắt line: E rồi S), S→STRAIGHT. RoutePlanner MCU kiểm tra độc lập cùng góc và cờ; parser/dedup bao gồm cờ, sai route/index/node/hướng/crossings bị từ chối. Cần cập nhật cả Core và firmware; Core cũ không phát cờ tương ứng sẽ không được MCU mới cho chạy.

Xoay tại GS1 vẫn dùng PWM210, chốt rời line000/bắt lại010/101/111, tối thiểu100ms và timeout2500ms. Khi xoay dùng sonar trước; bắt nhánh đúng xong trả STOP một vòng, chuyển sonar sau và đợi mẫu mới theo cửa sổ thu nhận hiện có trước khi cấp PWM lùi. Nếu đã hướng S, lấy sonar sau ngay từ lúc chờ. Lùi có bám line/còi/encoder như chặng lùi kho, giữ ngưỡng vật cản15/20cm; vật cản giữ route và tiếp tục quét phía sau để nhả. STOP, mất mạng, mất line, mẫu hỏng và timeout vẫn khóa motor. RFID S1 đọc trong lúc còn chờ/xoay không được coi là đến đích.

Sau khi đang bám lùi và đọc đúng RFID S1, MCU dừng, không vào StationDwell/StationTurning, báo IDLE/taskActivefalse/STOP/duty0 và hướng S. Core giữ điều kiện stationReady/phiên/telemetry/diagnostic mới của ADR-053; không hoàn thành chỉ bởi ACK hoặc target. Xe02, S2, kho K và mutex không đổi. M2→GS1 không phải cạnh đi trực tiếp hợp lệ trên map hiện tại; hướng đầu W được kiểm tra độc lập, không tạo đường tắt ngược chiều.

Kiểm thử PASS: native01 28 ca, native02 27 ca, audit650 tuyến mỗi profile, Core unit, host sonar GS1→S1 và hai fixture MQTT/Core/gateway (capability mới và firmware cũ) dùng cổng/map/counter/DB riêng. Kết quả build được ghi ở tests/README.md. Còn cần nghiệm thu line, khoảng dừng và RFID trên xe thật trước vận hành.

## Tham chiếu

- REQ-ROBOT01-REVERSE-HOME-01 trong sổ yêu cầu.
- [ADR-033](ADR-033-stations-face-outward.md), [ADR-053](ADR-053-station-completion-rfid-anchor.md).
- `firmware/NCKHAGV/NCKHAGV/src/core/RoutePlanner.h`, `CoreNavigator.cpp` và `data/map_layout.json`.
