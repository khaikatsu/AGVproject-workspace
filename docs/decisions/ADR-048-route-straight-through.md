# ADR-048 — Đi xuyên RFID thẳng theo tuyến

Ngày 13/10/2026. IMPLEMENTED và VERIFIED phần mềm; thử xe thật OPEN.

`SOURCE`: người dùng muốn bỏ dừng tại node trung gian đi thẳng trên lộ trình, vẫn đọc và ghi đủ node trên server, ví dụ C4→M4→GS2→M3→C3. Không phân loại cố định một node là đi xuyên trên map. Người dùng đã yêu cầu triển khai.

`OBSERVED`: mỗi RFID đang dừng motor, CoreNavigator chờ dwell và NODE_ORDER. Capability nodeOrdersSupported và quyền grant hiện tại không cấp phép tiếp tục chặng mới khi xe đang chạy. M4/M3 có nhánh nhưng tuyến ví dụ đi thẳng qua chúng.

`IMPLEMENTED`: capability straightThroughSupported; GOTO thêm mảng pass_through là index 0-based của path còn lại, chỉ node trung gian waypoint/mutex có cạnh đúng chiều, cùng hướng vào/ra. Không cấp tại gate ngay sau khi lùi ra kho: K1→G1→M5 có ba điểm thẳng về hình học nhưng đầu xe đang hướng ngược, phải dừng và quay ở G1. MCU từ chối index trùng/sai/đích, bộ ba rẽ/quay đầu/kho/trạm hoặc đang lùi; dedup so cả quyền đi xuyên. Metadata thuộc cmd_id của GOTO và phiên kết nối, không phải node global.

Khi tới đúng RFID trong pha Following, MCU tiếp tục nếu mọi cảm biến/mạng tốt, không chờ line phục hồi, còn trong hạn chặng và đã có grant cho current/arrival/next. RFID không bị xóa khỏi path; tiến độ, hướng, clearance và telemetry vẫn cập nhật. Khi thiếu quyền, dừng tại RFID và chờ NODE_ORDER/grant như trước. Xin trước tối đa một zone khi đang tiến tới node được cấp phép đi xuyên; không xin nhiều zone của toàn tuyến. Các zone khác giữ owner/FIFO/recovery cũ. Release vẫn đúng token sau thẻ rời vùng đo đủ xa, không nhả tại thẻ M.

RFID vẫn dùng HALT/WUPA và poll20ms như bản đang có để thẻ lặp có thể đọc mới. UID lặp không tăng index; thẻ tới đích/sai/unknown hoặc node cần dừng xử lý khi motor đã dừng. Tại RFID đi xuyên hợp lệ, motor tiếp tục trong lúc cleanup RC522; cần đo trên xe thật thời gian giao dịch đọc/HALT và độ ổn định line.

Firmware cũ chưa capability không nhận metadata. Firmware mới nhận GOTO cũ vẫn dừng từng node. Core/firmware đều phải nâng cấp để có lợi ích. Profile xe01 giữ PWM lùi/correction190 ở ADR-047; tiến/quay không tăng. firmwareBuild01=20261013-through-slow-reverse, các profile khác=20261013-straight-through.

`VERIFIED`: Core build/unit PASS, gồm legacy không có capability, GOTO path đầy đủ và pass_through[0,1,2], không hoàn thành từ ACK/node trung gian, đích mới hoàn thành, rẽ/kho/trạm/lùi không đi xuyên. Native profile01/02 52/52 PASS: parser/dedup/max32, grants, không dwell ở M4/GS2/M3 khi đủ quyền, thiếu M3 thì dừng GS2/chờ NODE_ORDER, mất mạng/line/vật cản/timeout/STOP/sai RFID và clearance đúng token. Contract schema/ví dụ PASS. MQTT/Core/gateway fixture riêng PASS tại .qa/straight-through-test-HxZGVR/verification.json; 02 giữ M3/offline vẫn giữ vùng, token sai không release, token đúng chuyển quyền01, mọi RFID vẫn có tiến độ. NODE_ORDER regression với Core cuối PASS tại .qa/node-order-test-ZYoD6b/verification.json. Hai ảnh ESP32 build PASS; profile01 RAM56540/Flash948961, profile02 RAM56540/Flash948865. Bản sao Core/ảnh01/ảnh02 và SHA256 nằm trong .qa/straight-through-release-20261013/artifacts.json.

`OPEN`: chưa nạp firmware/chuyển Core vận hành hoặc thử xe thật. Cần chạy tuyến C4→C3 và tuyến rẽ C4→M4→G5, đo đọc RFID liên tục, chờ grant khi xe khác giữ M3, STOP/mất line/mất heartbeat, giữ vùng lúc offline; không suy test logic chứng minh quãng phanh/độ ổn định vật lý.
