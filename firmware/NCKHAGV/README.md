# Firmware ESP32 AGV

29/10/2026 — Source v25 đồng bộ01/02: hạ hết chu kỳ5s chuyển LOWERED, cho lùi K→G cùng kho dù cảm biến còn HIGH. Chỉ DONE_DOWN khi dừng tại gate và LM393 LOW ổn định150ms; quá2s tại gate còn hàng/không ổn định thì lỗi, không tự lặp servo. Giữ HIGH/raw1=có hàng và tuning riêng. Cần Core/gateway tương ứng [ADR-093](../../docs/decisions/ADR-093-dropoff-exit-cargo-proof.md). BuildID `20261029-robot01-dropoff-exit-v25` / `20261029-robot02-dropoff-exit-v25`. Chỉ sửa/rà source, chưa build/test/nạp; người dùng tự thử. Quy tắc sau hạ/LOW=có hàng ghi ở các mốc cũ bên dưới đã được thay thế.

29/10/2026 — Source v24 thêm CONFIRM_HEADING cho cả01/02: xác nhận hướng thực tế khi xe đã dừng/hết lỗi, không tự quay hoặc chạy. Diagnostic trả đúng headingCommandId để Core mở khóa sau phục hồi. BuildID `20261029-robot01-recovery-heading-v24` / `20261029-robot02-recovery-heading-v24`. Config01/02, mạng và hiệu chuẩn pin giữ nguyên. Chưa build/test/nạp; người dùng tự nạp và thử thực tế. [ADR-092](../../docs/decisions/ADR-092-unified-recovery-heading.md).

28/10/2026 — Source v23 ghép cập nhật firmware từ `origin/main` commit `061b1d6ba995cbfd1d776462b081a1af38e05364`: bù PWM theo mức pin, cứu line quay180° bị lố và LM393 INPUT_PULLUP/LOW=có hàng cho cả01/02. Giữ sửa đổi LM393 tại máy, toàn bộ hai file Robot01Config.h/Robot02Config.h, hiệu chuẩn pin và mạng cục bộ. BuildID `20261028-robot01-remote-sync-v23` / `20261028-robot02-remote-sync-v23`; chỉ rà diff/hash, chưa build/test/nạp. Các mốc INPUT/HIGH và buildID bên dưới là lịch sử.

28/10/2026 — Source v22 thêm chẩn đoán LM393 cho cả01/02, buildID `20261028-robot01-lm393-raw-v22` / `20261028-robot02-lm393-raw-v22`; chưa build/test/nạp. Khi xe dừng, Serial115200 hiện `[CARGO] xe=02 gpio=2 raw=0 active=1 stable=1 present=0`; diagnostic thêm cargoRawLevel/cargoActiveLevel/cargoPin. Đặt/rút hàng và đối chiếu raw: đổi mức nhưng ý nghĩa ngược mới có căn cứ chỉnh polarity; raw không đổi thì kiểm tra tín hiệu/đường DO. INPUT/HIGH=có hàng và debounce150ms giữ nguyên. Các buildID bên dưới là mốc cũ.

28/10/2026 — LM393 GPIO2 cho cảhai profile01/02; HIGH=có hàng theo xác nhận người dùng, debounce150ms. BuildID source `20261028-robot01-lm393-v21` / `20261028-robot02-lm393-v21`. Chưa build/test/nạp firmware; người dùng tự thử theo AGENTS.md. Cần Core/gateway tương ứng [ADR-091](../../docs/decisions/ADR-091-lm393-warehouse-proof.md); trước nâng phải có hàng, sau nâng có/sau hạ không hàng mới cho rời kho. Nếu cảm biến vẫn nhìn thấy kệ sau hạ, xe giữ dừng và cần kiểm tra bố trí, không tự lùi.

Phiên bản source hiện tại: `20261027-robot01-g1-turn3000-v20` và `20261027-robot02-g1-turn3000-v20`. Bản v20 tăng tổng timeout quay180° và xác nhận line từ2700ms lên3000ms; rẽ90° giữ2500ms. Giữ logic đếm nhánh, mẫu line, PWM và thông số riêng từng xe hiện có. Chưa build/nạp hoặc chạy kiểm thử cho v20. Theo yêu cầu27/10/2026, người dùng tự nạp và kiểm thử thực tế; giữ nguyên trạng thái các file test hiện có/đã xóa.

## Cấu trúc source

```text
NCKHAGV/
  platformio.ini
  src/
    main.cpp
    Config.h
    Types.h
    core/
      AGVController.h / .cpp
      ControlLogic.h / .cpp
      CommandCodec.h / .cpp
    motion/
      MotorModule.h / .cpp
      MovementModule.h / .cpp
      LineFollowModule.h / .cpp
    sensors/
      SensorModule.h / .cpp
      EncoderModule.h / .cpp
      RFIDModule.h / .cpp
      CargoModule.h / .cpp
      BatteryModule.h / .cpp
    actuators/
      ServoModule.h / .cpp
      BuzzerModule.h
    network/
      WiFiModule.h / .cpp
      MQTTModule.h / .cpp
  test/test_control/test_main.cpp
```

[main.cpp](src/main.cpp) chỉ khởi tạo và cập nhật controller. Mỗi module có header khai báo API và source triển khai đặt cùng nhau trong nhóm chức năng; còi nhỏ nên triển khai ngay trong header.

[Config.h](src/Config.h) là nguồn GPIO/cấu hình duy nhất; [Types.h](src/Types.h) chứa kiểu dùng chung. Hai header này giữ nguyên vị trí để các project thử nghiệm độc lập tiếp tục dùng cấu hình hiện tại.

PlatformIO tự biên dịch source trong các thư mục con của `src/`. Thư viện bên ngoài vẫn được quản lý bằng `lib_deps` trong [platformio.ini](platformio.ini); các module của xe không phải thư viện tải thêm.

Các lệnh build/test và quy trình kiểm chứng nằm trong [hướng dẫn phát triển](../docs/05-development.md). Việc chia thư mục không thay đổi thuật toán, GPIO hay giao thức và không bao gồm nạp firmware lên xe.
