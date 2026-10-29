# Chẩn đoán khởi động gateway và core — 29/09/2026

### Build báo thiếu `mosquitto/mqtt_protocol.h` — 29/10/2026

Sau khi hợp nhất `main`, header trung gian `include/mosquitto.h` che header Mosquitto của MSYS2 UCRT64 vì script truyền `-I include`. Header này gọi `mosquitto/mqtt_protocol.h`, không có trong bộ thư viện trên máy. Đã bỏ header trung gian để `#include <mosquitto.h>` trong Core dùng trực tiếp header của compiler, giữ nguyên liên kết `-lmosquitto`.

Tái hiện đúng lỗi trước sửa; sau sửa, `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-core.ps1 -OutputDirectory .qa/core-build-fix-20261029` build thành công (exit0). Chưa thay hoặc khởi chạy binary vận hành, không sửa firmware hay dữ liệu. Có thể build/F5 lại; nếu binary đích đang chạy thì dừng đúng phiên cũ hoặc dùng thư mục đầu ra riêng như hướng dẫn bên dưới.

17/10/2026: dừng tại kiểm tra checkpoint không luôn là breakpoint vô hại. Khi đổi map26node sang22node, Core cũ từ chối runtime_state.json v3 dù mapv4 trên đĩa còn nguyên. Bản sửa [ADR-065](decisions/ADR-065-rounded-map-checkpoint-recovery.md) nhận diện riêng phép chuyển này, backup checkpoint và giữ owner/token/lịch sử trước khi phục vụ mapv4. Xe phải có RFID mới và đối soát vùng như trước; không xóa runtime/counter để gỡ lỗi. Cần build lại binary và dừng đúng phiên cũ trước khi F5 bản mới.

CURRENT 16/10/2026: menu F5 1/2/3 chọn scope `active01`, `active02`, `active-both` của cùng Core thống nhất; không còn chọn luật trial bỏ qua pin. Tự động hóa có thể dùng `build/agv_core.exe --mode active01` (tương tự `active02`, `active-both`, `normal`). Các tham số `robot01|robot02|both` bên dưới được giữ tạm cho hồi quy lịch sử. Chọn scope không tự mở ca hoặc phát GOTO/CHARGE_START. [ADR-058](decisions/ADR-058-unified-core-active-robot-scope.md).

### Build báo không tìm thấy `rg` — 25/10/2026

Ảnh lỗi cho thấy `scripts/check-vendor.ps1` gọi `rg` trước compiler, trong khi Windows PowerShell của task VS Code không có ripgrep trong PATH. Script đã chuyển sang `Get-ChildItem` và `Select-String -SimpleMatch`, chỉ quét source/header ứng dụng và bỏ qua header JSON đã kiểm SHA256 riêng. Không cần cài thêm `rg` để build. `-VendorOnly` chỉ kiểm checksum và báo đúng phạm vi đó.

Kiểm chứng bằng Windows PowerShell 5 với PATH chỉ có thư mục Windows: source hợp lệ PASS, include hỏng bị từ chối, JSON sai checksum bị từ chối cả với và không có `-VendorOnly`. Kết quả build tại [hồ sơ kiểm thử](../tests/README.md).

### Telemetry treo, lỗi/phục hồi lặp khi F5 — 14/10/2026

Cấu hình F5 đã thêm `_NO_DEBUG_HEAP=1`: debug heap Windows làm cấp phát JSON lịch sử chậm, giữ lock MQTT và khiến heartbeat vượt watchdog3s. Đối chứng400task:314ms dưới GDB mặc định so với16ms khi tắt debug heap. Fixture1000task/encoder5Hz qua GDB PASS heartbeat max278ms. Không tăng timeout hoặc xóa lịch sử. [ADR-052](decisions/ADR-052-core-debug-heap-heartbeat.md).

Core mới đã chạy nền mode robot01 ngoài debugger lúc18:48; muốn F5 lại phải dừng phiên Core nền trước vì cùng executable/cổng8080. Breakpoint vẫn có thể làm xe dừng theo watchdog. Trace thật45s sau áp có heartbeat max269ms, telemetry1047ms, không đổi boot/session/offline; xe còn SAFE_STOP/RFID UNKNOWN nên đọc RFID hợp lệ/phục hồi lỗi trước thử chuyển động.

### F5 đang dùng bản Core khôi phục — 10/10/2026

Theo yêu cầu khôi phục Core xe 01 hồi sáng, **AGV core (workspace root)** hiện chạy binary đã lưu tại `build/robot01-morning/agv_core.exe` và **không build trước F5**. Kết thúc phiên debug cũ nếu còn bằng Shift+F5, rồi F5/chọn 1. `build/agv_core.exe` cũng đã được chép lại từ cùng binary 03:14:03. Không dùng task Build để tái tạo bản này từ source hiện tại; source có thay đổi mới hơn. [Bằng chứng và phạm vi khôi phục](decisions/ADR-040-restore-morning-core-binary.md). Các hướng dẫn F5 luôn build bên dưới mô tả cấu hình trước lần khôi phục này.

### Build và cổng bận — 10/10/2026

VERIFIED sau xử lý ảnh tiếp theo: build bằng chính Windows `powershell.exe` với compiler UCRT64 đã PASS, tạo lại `build/agv_core.exe`. Bấm F5 với cấu hình **AGV core (workspace root)** rồi chọn chế độ trong console. Lỗi tiếng Việt của script đã sửa bằng BOM; chưa thử nút F5 trực tiếp trong UI VS Code.

Ảnh tiếp theo xác nhận task F5 chạy bằng Windows PowerShell 5 và chốt executable đang chạy báo PID 22344. Script UTF-8 không BOM làm chuỗi tiếng Việt bị đọc sai; `scripts/build-core.ps1` đã lưu UTF-8 có BOM để tương thích trình chạy này. Trước khi giải phóng binary, đối soát Core có fleet trống, không task pending/dispatched và không mutex owner; đã dừng riêng PID 22344 sau kiểm tra lại đường dẫn `build/agv_core.exe`. Gateway 3000, launcher 3100 và broker 1883 vẫn chạy. Core được để dừng để người dùng F5/chọn chế độ, không tự mở lại hoặc phát lệnh robot.

Ảnh người dùng chỉ có lỗi tổng kết build và Node listen/EADDRINUSE. Kiểm tra chỉ đọc: Core PID 22344 đang chạy từ `build/agv_core.exe`, gateway 3000 khỏe, launcher mô phỏng 3100 đã hoạt động. Script build hiện chặn trước linker nếu executable đích đang chạy, báo PID/đường dẫn; không tự dừng Core. Dừng đúng phiên cũ trước F5/build, hoặc build riêng bằng `./scripts/build-core.ps1 -OutputDirectory .qa/core-build`. Build riêng đã PASS; chưa xác nhận lỗi GCC/linker cụ thể của lần trong ảnh vì phần đó bị cắt.

Gateway bắt lỗi cổng trùng và báo rõ thay cho unhandled WebSocketServer error. Nếu 3000 đã có gateway khỏe, mở web hiện hữu; web mô phỏng dùng `./scripts/run-simulation.ps1` và [cổng 3100](http://127.0.0.1:3100/simulation), không cần chạy thêm gateway/Core vận hành. Kiểm thử cổng trùng dùng DB/cổng riêng và giữ gateway đầu khỏe. [ADR-039](decisions/ADR-039-simulation-browser-reconnection.md).

## Bằng chứng OBSERVED

### Menu chọn chế độ khi mở Core (09/10/2026)

VERIFIED: build/Core unit, menu + MQTT + mutex + counter, browser, startup integration, solo cũ và đơn hàng kho 7/7 PASS. Binary thử ở `.qa/menu-build`; fixture menu `.qa/core-mode-test-9rQiWI`, browser `.qa/browser-test-sNaMX6`. Chưa kiểm tra xe thật hoặc F5 trực tiếp trong UI; Core vận hành hiện hữu không bị dừng, F5 sẽ build bản mới sau khi bạn dừng phiên cũ.

Mở `AGVproject.code-workspace`, chọn **AGV core (workspace root)**, dừng phiên cũ bằng Shift+F5 rồi F5. Sau build, console riêng hiện:

```text
1. Thu rieng xe 01 (tram S1, mutex bat)
2. Thu rieng xe 02 (tram S2, mutex bat)
3. Thu ca hai xe 01 + 02 (mutex bat)
4. Che do van hanh thuong
0. Thoat
```

Nhập số và Enter. Lựa chọn có hiệu lực đến khi dừng Core; muốn đổi thì dừng và mở lại. Core chờ chọn trước khi mở cổng/ghi counter/kết nối MQTT, nhập sai hỏi lại, EOF/0 thoát. Sau khi chọn, vẫn quét RFID trạm và bắt đầu ca/giao việc trên web; chọn chế độ không tự cho xe chạy.

Hai lựa chọn thử riêng 01/02 giữ mutex và firmware bench hiện tại, không dùng đường tắt của solo 01 cũ. Chỉ xe đã chọn được nhận GOTO/task/sạc; xe còn lại vẫn được theo dõi. Nếu xe còn lại đã xuất hiện, nó cần RFID mới tại trạm riêng, dừng/rảnh và hết lỗi/task/vùng chưa xác minh trước khi mở ca. Cả ba lựa chọn thử giữ chính sách pin của thử hai xe: không dùng pin điều khiển để giao việc, không tự về sạc. Map/counter/checkpoint đang có được giữ; không xóa/reset để đổi chế độ.

Chạy `scripts/run-core.ps1` hoặc executable trực tiếp trong terminal cũng hiện menu khi stdin tương tác và chưa đặt cờ chế độ cũ. Tự động hóa/mô phỏng không có stdin tương tác giữ cơ chế môi trường cũ; có thể chọn rõ bằng `build/agv_core.exe --mode robot01` (tương tự `robot02`, `both`, `normal`). F5 dùng `--select-mode` nên luôn hỏi lại, kể cả có cờ cũ trong môi trường. [ADR-028](decisions/ADR-028-core-startup-mode-menu.md).

### Sửa build/F5 ngày 09/10/2026

Ảnh F5 người dùng gửi sau lần sửa đầu cho thấy `ParserError: Unexpected token` ở dòng bắt đầu bằng `447727 ${env:DEBUGINFOD_URLS}=''; ...WindowsDebugLauncher.exe...`. Số đứng trước phép gán biến khiến PowerShell không phân tích được lệnh; lỗi xảy ra trước khi GDB/Core khởi chạy. Chưa xác định vì sao số đó xuất hiện trong terminal. Đã đổi `externalConsole` sang `true` trong `.vscode/launch.json` để F5 mở console riêng, tránh đưa lệnh launcher vào terminal PowerShell đang dùng. Dừng phiên debug cũ bằng Shift+F5 rồi F5 lại. Cấu hình đã kiểm tra JSON và schema của extension C/C++ đang cài; chưa kiểm tra thao tác F5 trong UI sau thay đổi này.

SOURCE: người dùng báo server không chạy khi bấm Run and Debug/F5, yêu cầu chỉ sửa phần build/khởi động và giữ cấu trúc code hiện tại.

OBSERVED: entrypoint hiện nằm ở `src/core/AGVserver.cpp`, header ở `include/`. Script Windows đã dùng đúng đường dẫn; build bằng PATH hiện tại thành công. Khi thử với PATH chỉ có Windows System32, script cũ lỗi `g++ is not recognized`; executable cũ thoát ngay với mã `3221225781` (`0xC0000135`, thiếu DLL runtime). Đây là lỗi tái hiện khi thiếu PATH, chưa có log F5 của phiên người dùng để xác nhận môi trường VS Code lúc lỗi.

- `.vscode/tasks.json`: task F5 gọi compiler UCRT64 bằng đường dẫn đầy đủ; cả hai task đều build đúng entrypoint vào `build/agv_core.exe`, kể cả đang mở header/file thử. Dùng task `process` để giữ nguyên đối số đường dẫn.
- `.vscode/launch.json`: thêm `C:/msys64/ucrt64/bin` vào PATH của chương trình debug để tìm libmosquitto và DLL GCC. Giữ working directory gốc workspace.
- `scripts/build-core.ps1`: tìm GCC UCRT64 khi `g++` chưa có trong PATH; thêm thư mục compiler trong lúc build/test và thêm `-g` cho breakpoint.
- `scripts/run-core.ps1`: thêm PATH runtime khi chạy qua terminal, khôi phục biến môi trường sau khi kết thúc.
- `.vscode/c_cpp_properties.json`: đổi cấu hình IntelliSense từ MSVC `cl.exe` sang GCC UCRT64/C++17. Sai IntelliSense có thể gây báo lỗi trong editor; bản thân file này không quyết định linker.
- `.github/workflows/ci.yml`, `deploy/Dockerfile.core`: sửa đường dẫn nguồn cũ và thêm include path; Docker chép thêm `include/`. Chưa chạy CI/Docker trong lần sửa này.

Mở workspace, chọn **AGV core (workspace root)** rồi F5. F5 khởi động Core C++; dashboard cần thêm `npm start` trong `AGVwebserver`. Không chạy đồng thời F5 và `run-core.ps1` trên cùng cổng/counter. Không đổi thuật toán, MQTT, dữ liệu map/counter/SQLite hoặc cấu trúc thư mục.

VERIFIED 09/10/2026: build Core và unit binary thành công với PATH chỉ có System32; GDB dùng executable mới dừng đúng ở `main` (`src/core/AGVserver.cpp:652`). `build/core_tests.exe` và `node tests/startup.integration.js` PASS ngoài sandbox; fixture `.qa/startup-test-edtxzT` kiểm tra Core/MQTT, chạy trùng, wrapper, restart và giữ counter. Trong sandbox, test từng lỗi timeout/atomic file Windows error 5; không sửa thuật toán để xử lý hạn chế của môi trường test. Chưa trực tiếp thao tác nút F5 trong UI VS Code.

Kiểm tra chỉ đọc cuối phiên: Mosquitto đang listen 1883; cổng 8080/3000 chưa có listener, không thấy tiến trình Core chạy trùng.

### Hồ sơ chẩn đoán 29/09/2026

- `EADDRINUSE`, port 3000: gateway đã chạy, lần chạy mới không mở được HTTP listener. Trong phiên chẩn đoán, một gateway do trợ lý mở giữ cổng; đã dừng gateway đó và core do trợ lý mở.
- Dashboard có map nhưng `mqtt_connected:false`: gateway nhận state từ core, còn core mất kết nối broker. Đã thấy ba core cùng listen 8080, gồm executable cũ trong src và bản build.
- Các core có cùng client ID `AGV_MasterServer`. [MQTT 3.1.1 §3.1.4](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html) yêu cầu broker ngắt client cũ khi client mới trùng ID. Đây là nguyên nhân có căn cứ cho vòng reconnect; không đồng nghĩa broker service bị tắt.
- Windows `SO_REUSEADDR` có thể cho nhiều socket bind cùng địa chỉ/cổng. [Microsoft](https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse) mô tả hành vi không xác định và tùy chọn `SO_EXCLUSIVEADDRUSE` để giữ độc quyền.

## Sửa đã triển khai

Core giữ cổng độc quyền trên Windows trước khi đọc/ghi counter và kết nối MQTT; bản chạy trùng báo lỗi rồi thoát. Gateway dùng IPv4 127.0.0.1 cho core. [run-core.ps1](../scripts/run-core.ps1) đặt working directory về gốc, chọn executable mới và giữ file counter cũ khi chuyển từ src. Core mặc định cũng nhận diện counter cũ nếu chưa có counter cạnh map; override môi trường được giữ nguyên.

## Chạy đúng

Dừng các phiên debug/core cũ trước. Mở [AGVproject.code-workspace](../AGVproject.code-workspace), giữ broker hiện có ở 1883. Terminal thứ nhất từ gốc:

```powershell
./scripts/run-core.ps1 -Build
```

Terminal thứ hai từ gốc:

```powershell
Set-Location AGVwebserver
$env:AGV_WS_URL = 'ws://127.0.0.1:8080'
npm start
```

Nếu dùng F5, chọn **AGV core (workspace root)** trong workspace vừa mở; nó build đúng entrypoint và dùng cwd gốc. Không đồng thời chạy wrapper và F5. File `.env.example` là mẫu, gateway không tự nạp `.env`.

Kiểm tra chỉ đọc:

```powershell
Invoke-RestMethod http://localhost:3000/api/health
```

Mong đợi `agv_connected`, `core_ready`, `protocol_supported`, `mqtt_connected` đều true. Fleet có thể trống khi chưa có robot gửi telemetry. Nếu core còn báo lỗi broker, kiểm tra service/listener và AGV_MQTT_HOST/PORT. Không xóa counter, SQLite hoặc áp snapshot để chữa lỗi kết nối.

## Kiểm chứng

[startup.integration.js](../tests/startup.integration.js) dùng broker/cổng/counter/map trong `.qa`, kiểm tra duplicate không sửa counter và không thay phiên MQTT đang hoạt động, wrapper chuẩn hóa cwd, restart dùng counter tiếp nối và fallback counter src trong workspace fixture. Kiểm thử không dùng broker 1883, core 8080 hoặc DB vận hành.

Kết quả 29/09/2026: **PASS** build/core tests, 7 ca gateway, startup integration, map integration và system integration. Startup fixture tại `.qa/startup-test-uCknOJ`, map fixture `.qa/map-test-xDDcTJ`, system fixture `.qa/system-test-GXrUmM`. Wrapper từ thư mục khác bị từ chối đúng khi đã có core; MQTT và phiên core hiện hữu vẫn ổn định, counter không đổi do lần chạy trùng. Restart và fallback counter src đều qua kiểm tra.

Kiểm tra tiến trình sau test: các listener 3000/8080 đã trống, broker service 1883 vẫn chạy. Đây là trạng thái tại thời điểm chẩn đoán, không phải giám sát định kỳ. Các giới hạn firmware, phục hồi vùng sau reboot và robot thật vẫn theo [bản cập nhật](14-web-server-update.md).

## ESP32 không kết nối broker qua Mobile Hotspot (03/10/2026)

Ảnh Serial của xe 01 cho thấy `MQTT_CONNECT_FAILED=-2` khi sketch thử `192.168.1.4:1883`. Trên laptop hiện tại, Mobile Hotspot là `192.168.137.1`; Mosquitto đang listen `0.0.0.0:1883`, cho anonymous trong cấu hình lab, và phép subscribe chỉ đọc vào `$SYS/broker/version` qua `192.168.137.1:1883` thành công. Đây là bằng chứng IP trong sketch đã cũ, không phải bằng chứng xe đã kết nối lại sau khi nạp firmware.

[Sketch](../firmware/AGV_Connection_Test/AGV_Connection_Test.ino) được sửa sang `192.168.137.1`; log lỗi bổ sung ESP IP và gateway. Sau khi nạp lại, ESP IP phải thuộc `192.168.137.0/24`, gateway thường là `192.168.137.1`; nếu còn `state=-2`, đối chiếu hai địa chỉ trong Serial rồi kiểm tra kết nối của client hotspot tới cổng 1883. Chưa nạp board hoặc thử kết nối từ xe thật trong lần sửa này.
