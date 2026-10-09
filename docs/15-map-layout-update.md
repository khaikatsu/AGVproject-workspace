# Bản đồ kho v3 — RFID giữa map và tên node ngắn

Cập nhật cùng ngày cho tự về sạc: hai cạnh ngoài trái đã đổi thành **C3 → G1 → C1**, khép vòng có hướng. Đã kiểm tra mọi S1/S2 ↔ K1…K6. Xem [quản lý ca và trạm riêng](17-shifts-and-home-charging.md). Hình/snapshot trước cập nhật có thể vẫn chứa hướng cũ.

Ngày: 29/09/2026. Nguồn `SOURCE`: người dùng yêu cầu thêm hai RFID tại hai ngã tư giữa map, rút gọn tên node, sửa trong `src` và giao diện, đưa các kho phía trong về giữa đoạn đường tới ngã tư.

## Bản đồ đã triển khai

`OBSERVED`: map có 26 node, 39 cạnh có hướng (gồm hai hướng của nhánh kho/sạc), 6 vùng RFID/mutex. Hai ngã tư giữa là `M5=(50,70)` và `M6=(150,70)`. Đường ngang đi theo `G1 → M5 → M6 → G6`, trọng số lần lượt 50, 100, 50. Cột trái đi xuống; cột phải đi lên. Không còn cạnh vượt thẳng qua hai ngã tư.

| Nhóm | ID cũ → ID mới | Ý nghĩa |
| --- | --- | --- |
| RFID có sẵn | RFID_M1…RFID_M4 → M1…M4 | Vùng chung trên/dưới |
| Hai RFID giữa | CROSS_1/CROSS_2 → M5/M6 | Ngã tư giữa trái/phải; trước thay đổi chỉ một file JSON có CROSS |
| Kho | KHO_1…KHO_6 → K1…K6 | Điểm đích của nhiệm vụ |
| Cửa kho | GATE_K1…GATE_K6 → G1…G6 | Điểm nối kho với đường chính |
| Trạm sạc | CHG_1/CHG_2 → S1/S2 | Trạm trên/dưới |
| Cửa sạc | GATE_S1/GATE_S2 → GS1/GS2 | Điểm nối trạm sạc |
| Góc | CORNER_TL/TR/BL/BR → C1/C2/C3/C4 | Trên trái, trên phải, dưới trái, dưới phải |

| Kho / cửa | Đoạn đường cần chia đôi | Tọa độ kho | Tọa độ cửa |
| --- | --- | --- | --- |
| K2 / G2 | M1–M5: y=0…70 | (30,35) | (50,35) |
| K3 / G3 | M5–M3: y=70…140 | (30,105) | (50,105) |
| K4 / G4 | M2–M6: y=0…70 | (170,35) | (150,35) |
| K5 / G5 | M6–M4: y=70…140 | (170,105) | (150,105) |

Mỗi đoạn dọc từ RFID tới cửa kho dài 35 đơn vị; nhánh kho dài 20. Tổng chiều dài cột trong vẫn 140, đường ngang giữa vẫn 200. Tọa độ/trọng số dùng cm theo mô hình đang có; kích thước thực và UID thẻ RFID vẫn `OPEN`, cần đo và ánh xạ trước khi chạy phần cứng.

## Tệp và giao diện

Đồng bộ [map vận hành](../data/map_layout.json), [map mẫu trong src](../src/map_layout.json), [map khi chạy từ src](../src/data/map_layout.json) và `loadDefaultDemoMap()` trong [MapGraph.hpp](../src/MapGraph.hpp). Map mặc định và các file JSON có cùng node/cạnh, hướng, trọng số, vùng chung và trạm sạc. Entry point được build vẫn là `src/AGVserver.cpp`.

[Dashboard](../AGVwebserver/public/dashboard.js) lấy graph từ core, dùng cùng tọa độ và một phép co giãn đều cho cả hai trục. ID ngắn nằm trong vòng tròn; nhãn trọng số nằm ngoài node. Tooltip giải thích loại node; chú giải phân biệt M/K/G/S/GS/C. Cách vẽ này áp dụng cả map chính và bản xem trước snapshot. Tên đích mẫu ở [HTML](../AGVwebserver/index.html) đã đổi thành `K3`, node chặn mẫu là `G2`.

MQTT vẫn dùng `node|pin|vật_cản`; command vẫn là JSON. **Giá trị ID node đã đổi**, nên firmware/simulator phải dùng ID mới ở telemetry, path, target và mutex. Ví dụ hợp đồng trong [tài liệu mạng](07-network.md) và [fixture MQTT](../contracts/examples/current-mqtt.json) đã cập nhật. Không ánh xạ UID thẻ thật khi chưa có bảng thẻ từ nhóm.

Snapshot/lịch sử SQLite cũ được giữ nguyên. Nếu người vận hành áp dụng snapshot có ID cũ, core sẽ dùng lại map cũ; reconnect không tự áp dụng snapshot. Không xóa hoặc lùi counter `command_sequence.json`. Bản sao các file trước sửa nằm trong `.qa/map-update-2026-09-29/`; thư mục này không phải bản sao counter hay database vận hành.

## Chạy và kiểm chứng

Từ gốc workspace, dùng core mới đã build:

```powershell
./scripts/build-core.ps1 -Tests
./build/agv_core.exe
```

Terminal gateway:

```powershell
npm --prefix AGVwebserver start
```

Nếu có tiến trình core cũ đang mở, dừng tiến trình đó trước khi chạy bản mới; tải lại trang `http://localhost:3000`. Khi chạy từ thư mục `src`, core vẫn đọc `src/data/map_layout.json`, nhưng cách chạy được hỗ trợ là từ gốc workspace.

Đã chạy thành công trên Windows:

- Build core và C++ test: ba file JSON khớp map mặc định; trọng số đúng độ dài; đường dọc/ngang qua M5/M6; chặn M5 làm K3 không thể tới; kiểm tra hai robot xin/trả cùng vùng.
- `node tests/map.integration.js`: core C++ và broker Mosquitto thật trên cổng riêng; MQTT robot giả lập; GOTO tới K4 qua M5/M6; hai robot xếp hàng, không cho xe khác trả quyền; telemetry trung gian chưa hoàn thành, telemetry đích hoàn thành đúng task.
- `node tests/browser.integration.js` với Edge headless: map có 26 node/6 zone; cửa kho ở đúng trung điểm trên SVG; tên ngắn vừa vòng tròn, các nhãn node không đè nhau; snapshot và mobile 390 px. Ảnh tại `.qa/browser-test-oPyqAo/warehouse-map-v3.png` và `warehouse-map-v3-mobile.png`.

Các test dùng map, counter, broker và database riêng. Chưa kiểm thử robot thật hoặc động học. Quyết định tại [ADR-003](decisions/ADR-003-map-rfid-layout.md).
