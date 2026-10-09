# Firmware NCKHAGV (ESP32 DevKit)

## Bản thử một xe trên toàn bản đồ 05/10/2026

`src/core/CoreNavigator.cpp` nối GOTO với bám line và quét từng RFID trên đường ngắn nhất do Core tính bằng Dijkstra. Bản thử xe 01 dừng **ít nhất 1 giây** tại mỗi thẻ trước khi đi tiếp hoặc quay (bản thường vẫn 200 ms); mỗi đoạn tối đa 15 s và mỗi lần quay tối đa 2,5 s theo giới hạn phần mềm chưa hiệu chuẩn. Mất line, mất mạng, quá hạn hoặc quét sai thẻ đều dừng và báo lỗi. Topic `agv/01/diagnostic` bổ sung `linePattern` (0–7, bit trái/giữa/phải) để đối chiếu lúc rẽ; đây không phải vị trí RFID. Bản thường còn dừng khi siêu âm lỗi hoặc gặp vật cản; bản trial tạm bỏ chốt này theo mục dưới.

Cập nhật 07/10/2026: trong trial, một mẫu `000` khi bám line làm motor dừng ngay; chỉ sau 120 ms vẫn `000` mới chốt `LINE_LOST`. Mẫu hợp lệ như `010` trở lại trước hạn thì tiếp tục. Serial Monitor ghi `[LINE] blank/recovered/fault` để xác định mẫu lúc xử lý; trạng thái lỗi trên web được giữ lại dù mắt đọc `010` sau khi xe đã dừng. Bản thường không dùng khoảng xác nhận này.

Firmware ước tính pin 2S 18650 từ ADC GPIO36 theo mạch chia áp 30 kΩ/7.5 kΩ (hệ số 5), với dải cấu hình 6.4–8.4 V. Trong giai đoạn thử hiển thị, phần trăm chỉ nằm trong JSON `agv/{id}/diagnostic`; trường pin của telemetry điều khiển luôn là `?`, nên chưa tác động ngưỡng 20%/40% của Core. Đây là nội suy tuyến tính theo điện áp, chưa được đối chiếu đồng hồ trên xe và không thay thế bảo vệ điện áp thấp của BMS. Bản `esp32dev` thường vẫn khóa PWM. Bản `esp32dev_trial`, cùng Core opt-in `AGV_ONE_ROBOT_TRIAL=1`, cho **duy nhất xe 01** đi tới các node có đường trên map qua lệnh GOTO trực tiếp từ web; không giao task tự động và không điều phối hai xe. [ADR-017](../../../docs/decisions/ADR-017-solo-full-map-trial.md) thay phạm vi trial một đoạn của [ADR-016](../../../docs/decisions/ADR-016-single-robot-server-motion-trial.md).

Cập nhật 08/10/2026 sau quan sát pin dao động 65–70%: `BatteryModule` lấy 16 mẫu ADC rải trong khoảng 0,5 giây, bỏ mẫu cao nhất/thấp nhất, lọc tiếp giữa các đợt và chỉ đổi số phần trăm khi vượt biên 1,5 điểm. Không chèn `delay` vào vòng điều khiển. `batteryAdcMv` trong diagnostic là trung bình đợt trước khi lọc theo thời gian; `batteryVoltage` và `batteryPercentage` là giá trị đã làm mượt. Vẫn cần so với đồng hồ tại chân chia áp và điện áp pack để kiểm tra sai số thực tế.

Đây là project nhúng do nhóm bổ sung, dùng PlatformIO với Arduino framework. `src/main.cpp` khởi tạo `AGVController`; các module trong `src/core`, `motion`, `sensors`, `actuators`, `network` được biên dịch cùng nhau. `AGV_Connection_Test` vẫn là sketch kiểm tra kết nối/bench riêng; không chạy đồng thời hai firmware cho cùng robot ID. Bản thường giữ xin/ACK mutex và chưa tự release. Chỉ bản trial một xe bỏ chờ grant khi qua M1–M6; xe thứ hai phải ở ngoài đường thử.

## Thử xe 01 chạy bằng server tới các node

Đây là bản chạy thử **một xe**, chưa xác nhận vận hành vật lý toàn kho. Cập nhật 07/10/2026: lần chạy thật cho thấy giả định lùi S1→GS1 đưa xe ra khỏi map. Đặt xe 01 ở S1 với **đầu xe hướng về GS1**; đoạn này nay được lệnh đi tiến. Kiểm tra trên giá kê trước: hai bánh cùng quay tiến, sau đó kiểm tra rẽ trái/phải, mẫu line `010/101/111` và nút ngắt nguồn. Đưa xe thứ hai khỏi đường thử. Quét thẻ S1 mới sau khi nạp; hướng đầu xe ở node khác sau reboot chưa được xác nhận và firmware sẽ từ chối tuyến mới.

Tạm thời theo yêu cầu 05/10/2026, bản `esp32dev_trial` **không khởi tạo/đo siêu âm và không dùng nó để cho phép/dừng chuyển động**. Chẩn đoán `ultrasonicCm` sẽ là `null`; giá trị này không khóa lệnh. Bản `esp32dev` thường vẫn chặn khi siêu âm lỗi hoặc gặp vật cản. Chỉ tháo/cắm dây cảm biến khi đã ngắt nguồn; khi thử bản trial, giữ đường trống, nhấc bánh lên giá kê trước và có người trực tiếp ngắt nguồn motor. Muốn dùng cảm biến trở lại phải đổi `Config::ULTRASONIC_MOTION_GUARD` cho bản trial, đo được khoảng cách hợp lệ, kiểm tra ngưỡng dừng 15 cm trên bàn rồi nạp lại. Không dùng bản này để chạy không giám sát.

Mở PowerShell mới từ gốc workspace để chạy đúng **một Core** với công tắc thử (dừng Core cũ trước; giữ `data/command_sequence.json`):

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject'
$env:AGV_ONE_ROBOT_TRIAL = '1'
$env:AGV_HOME_STATIONS = '{}'
./scripts/run-core.ps1 -Build
```

Giữ gateway `AGVwebserver` chạy riêng. Từ thư mục firmware, kiểm tra cổng ESP32 rồi build/nạp **env trial** (thay `COMx` bằng cổng thật):

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject\firmware\NCKHAGV\NCKHAGV'
$pio = "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe"
& $pio run -e esp32dev_trial
& $pio device list
& $pio run -e esp32dev_trial -t upload --upload-port COMx
& $pio device monitor -p COMx -b 115200
```

Đợi web hiện xe `01` tại S1, `online=true`, `position_valid=true`, Core/MQTT kết nối. Chọn **xe 01**, giao đích **GS1** trước; sau khi telemetry GS1 xác nhận hoàn thành, thử M2 rồi tăng dần đến các node khác. Có thể giao một node xa; Core gửi cả đường Dijkstra và firmware dừng/quét từng RFID. Xe 01 mới dùng PWM trial danh định 210/220 khi đi thẳng và 200/200 khi pivot; `LEFT` cấp trái tiến/phải lùi, `RIGHT` cấp trái lùi/phải tiến. Sau khi nạp, xem `[MOTOR]` trên Serial Monitor và xác nhận cả hai bên quay đúng trên giá kê. Chế độ này từ chối lệnh mới khi xe đang chạy, xe 02 xuất hiện hoặc còn owner mutex cũ. Nếu bánh quay sai, lệch line hoặc cảm biến bất thường, ngắt nguồn motor và kiểm tra tại chỗ. ACK chỉ báo nhận lệnh; trạng thái hoàn thành cần RFID đích.

## Build và nạp từ PowerShell

Mở thư mục project chứa `platformio.ini`, rồi gọi trực tiếp PlatformIO đã cài trên máy này (lệnh `platformio` chưa có trong `PATH`):

```powershell
Set-Location 'C:\Users\DELL\OneDrive\noobcoder\AGVproject\firmware\NCKHAGV\NCKHAGV'
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev
```

File tạo ra là `.pio/build/esp32dev/firmware.bin`. Đây là ảnh ứng dụng; nếu dùng công cụ nạp `.bin` thủ công còn cần bootloader và bảng phân vùng đúng offset, vì vậy cách nạp dễ nhất là nối USB với ESP32, chọn cổng COM và để PlatformIO nạp đủ các phần:

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" device list
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev -t upload --upload-port COMx
```

Thay `COMx` bằng cổng của ESP32. Lệnh `esp32dev` ở mục này build/nạp **bản thường khóa motor**; muốn thử chạy phải dùng `esp32dev_trial` như mục trên. Điền cấu hình mạng riêng trước khi build/nạp, và dùng ID xe `01` cho trial.

## Kết nối Core

Sao chép `src/LocalNetworkConfig.example.h` thành `src/LocalNetworkConfig.h` rồi điền ID xe `01` hoặc `02`, WiFi và địa chỉ broker LAN. File cục bộ được ignore để tránh đưa mật khẩu vào mã chia sẻ. Cấu hình mới không có file này sẽ giữ xe offline. ESP32 bật tự kết nối lại sau khi AP rớt và còn thử kết nối định kỳ mỗi 5 giây; Serial Monitor in `[WiFi] Disconnected reason=...` và `[WiFi] Got IP` để phân biệt lỗi WiFi với MQTT. Với broker TLS, đặt port của listener, `AGV_MQTT_TLS true`, tài khoản/mật khẩu riêng từng xe và CA PEM; tên/IP broker phải khớp SAN của chứng chỉ. Firmware không dùng `setInsecure()` và chờ NTP trước khi xác minh chứng chỉ; LAN không có Internet cần NTP nội bộ qua `AGV_NTP_SERVER`. Broker/Core phải được cấu hình tương ứng theo [hướng dẫn gia cố](../../../docs/18-hardening.md).

### Xe 02 chỉ kết nối và giám sát

Nếu xe 02 dùng đúng ESP32 DevKit và sơ đồ chân ở `src/Config.h`, build riêng env `esp32dev_robot2_monitor`. Env này lấy WiFi/broker từ `LocalNetworkConfig.h`, đặt ID MQTT thành `02` dù file cục bộ đang dùng ID `01`, và giữ `ENABLE_CORE_MOTION=false`. Không nạp env `esp32dev_trial` cho xe 02. Xác định đúng cổng COM của xe 02 trước khi nạp:

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" device list
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev_robot2_monitor
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev_robot2_monitor -t upload --upload-port COMx
```

Khi Core và broker đang chạy, xe 01/02 phải có MQTT client ID khác nhau (`agv-01-<MAC>` và `agv-02-<MAC>`), cùng xuất hiện trên web sau telemetry. Quét thẻ S2 thật ở xe 02 để xác minh vị trí; trước đó node là `UNKNOWN`. Nếu Core vẫn bật `AGV_ONE_ROBOT_TRIAL=1`, telemetry xe 02 sẽ làm gián đoạn tuyến xe 01 đang chạy và chặn GOTO thử mới. Dừng xe 01 an toàn trước khi kết nối xe 02; chế độ này chỉ dùng để kiểm tra kết nối/giám sát, chưa cho hai xe di chuyển đồng thời. Nếu bo hoặc dây xe 02 khác xe 01, đối chiếu phần cứng rồi sửa cấu hình riêng trước khi nạp.

Core chế độ thường có thể tự gửi `CHARGE_START` cho xe 02 khi kết thúc ca tại S2. Để giữ xe 02 ở vai trò giám sát, bản Core mới hỗ trợ biến môi trường `AGV_MONITOR_ONLY_02=1`: không chọn xe 02 cho nhiệm vụ, từ chối GOTO trực tiếp, không tự đưa xe 02 về sạc. Chỉ áp dụng khi khởi động **một** Core mới sau khi dừng Core cũ và giữ nguyên map/runtime/counter đang dùng; không xóa dữ liệu hay mở hai Core cùng lúc. Firmware xe 02 vẫn khóa PWM độc lập với chế độ Core.

Sau khi đối soát trạng thái hai xe và dừng Core cũ, chạy `./scripts/run-robot2-monitor.ps1 -Build` từ gốc workspace. Script mặc định dùng `src/data` đang chứa map, runtime, shift và counter của Core vận hành; thay `-DataRoot` chỉ khi đã xác định một bộ dữ liệu khác cần tiếp tục.

MQTT dùng `agv/{id}/command` JSON từ Core, `agv/{id}/ack` là cmd_id dạng text, `agv/{id}/telemetry` là `node|battery|obstacle`, `agv/{id}/error` là text, `agv/{id}/status` online/LWT offline retained. Firmware theo dõi `agv/server/heartbeat`; chỉ gửi telemetry khi heartbeat mới và khóa điều khiển khi quá 3 giây, mất WiFi/MQTT, worker mạng ngừng hoặc phiên Core đổi. `cmd_id` cao nhất lưu vào NVS namespace `agv-rfid`, key `last-cmd`, tương thích sketch thử nghiệm; lệnh trùng gần nhất với cùng nội dung chỉ ACK lại, không thực hiện lại. ACK gửi lỗi được thử lại sau reconnect; ACK chỉ xác nhận đã lưu/nhận lệnh, không xác nhận đã tới đích. Không xóa NVS để “sửa” lỗi thứ tự lệnh; giữ file counter của Core.

Diagnostic NCKHAGV mới có `rfidAgeMs`: thời gian từ lần RC522 đọc được thẻ đang nằm dưới đầu đọc trong phiên MQTT; chưa có lần đọc hợp lệ thì `null`. Core thử hai xe dùng tuổi ≤250 ms cùng motor `STOP`, `leftDuty=rightDuty=0`, `taskActive=false` và RFID trạm S1/S2 để đối soát quyền vùng/lỗi mạng theo [ADR-026](../../../docs/decisions/ADR-026-safe-automatic-recovery-at-home.md). Telemetry node cũ, ACK hoặc chỉ MQTT online không kích hoạt phục hồi. Cần build/nạp firmware mới cho cả hai xe và thử trên giá kê trước khi dùng tự phục hồi; bản cũ vẫn cần xác minh thủ công.

Trong bản `esp32dev_trial`, mẫu line `110`/`011` (mắt giữa và một mắt bên cùng thấy vạch) dùng chỉnh nhẹ: cả hai bánh vẫn chạy tiến với duty danh định tương ứng 210/140 hoặc 130/220. Mẫu lệch hẳn `100`/`001` giữ chỉnh mạnh một bánh; `010`, `101`, `111` đi thẳng; `000` dừng ngay và chỉ chốt lỗi sau 120 ms không bắt lại line. Chỉnh khi lùi và bản `esp32dev` thường chưa đổi. Đây là thông số thử phần mềm, chưa được xác minh hướng chỉnh trên xe thật. Kê bánh, đặt lần lượt ba mắt ở trạng thái `110`, `010`, `011`, xem `GENTLE_CORRECT_LEFT`/`FORWARD`/`GENTLE_CORRECT_RIGHT` cùng duty trên Serial/diagnostic; xác nhận bánh tạo hướng kéo xe về vạch trước khi thử một đoạn thẳng ngắn trên mặt đất.

26 UID RC522 đã đồng bộ với bản đồ v3 và sketch thử nghiệm. Khi chưa quét RFID trong phiên kết nối hiện tại, node là `UNKNOWN`; telemetry/hàng đợi của phiên cũ bị loại, không suy ra vị trí từ đích lệnh. Firmware ước tính phần trăm pin từ điện áp ADC theo cấu hình ở trên. Core lấy `batteryPercentage` trong diagnostic để hiện và lưu lịch sử qua `battery_display_percent`; `battery`/`battery_measured` của điều phối vẫn giữ nguyên giá trị từ telemetry `?`. Khi cấu hình hoặc phép đo không hợp lệ, dashboard hiện “Chưa đo”. Cần đối chiếu ADC/điện áp thực bằng đồng hồ; việc bật ngưỡng pin là thay đổi riêng sau giai đoạn kiểm tra hiển thị.

## Hướng rẽ theo bản đồ và khóa motor

Theo bổ sung của người dùng ngày 04/10/2026, chiều cạnh và tọa độ RFID đã nằm trong bản đồ. `src/core/RoutePlanner.h` dùng `path` của lệnh `GOTO` và đồ thị một chiều để kiểm tra từng cạnh; từ ba node liên tiếp nó phân loại trái/phải/thẳng/lùi. Các nhánh như M1 có thể đi GS1 hoặc G2; **node kế trong path** quyết định nhánh. Điều chỉnh theo quan sát 07/10/2026: xe 01 đi tiến S1→GS1 khi đầu hướng về GS1; các đoạn sau vẫn có thể cần lùi hoặc quay theo path. Hướng trái/phải sau khi lùi được xét theo đầu xe chứ không theo véc-tơ di chuyển lùi. Khi lùi, 010/101/111 giữ thẳng, mắt lệch thì giảm tốc một bên bánh, 000 dừng an toàn; hướng chỉnh thực tế còn phải kiểm tra trên giá kê. Tại RFID đầu tiên khác S1 sau boot/reconnect, nếu chưa thấy RFID trước đó trong cùng phiên, hướng đầu xe là `UNKNOWN_HEADING`; không suy đoán từ mũi tên trên bản đồ. Những tuyến dùng nhánh kho/sạc có thể phải lùi và vẫn cần kiểm chứng thao tác vật lý.

`src/core/RouteMap.generated.h` được sinh từ `data/map_layout.json` bằng `node scripts/generate-firmware-route.js` tại gốc workspace; `--check` phát hiện bản sinh lệch bản đồ. Khi sửa bản đồ vận hành, cần sinh và nạp firmware tương ứng. Core v0 chưa gửi phiên bản map trong `GOTO`, nên không thể xác nhận bản đồ đang chạy có cùng phiên bản chỉ bằng payload lệnh. Không sửa/apply snapshot map trong lúc thử xe nếu chưa đồng bộ lại firmware.

Trong env `esp32dev` thường, `GOTO` được ACK ở mức nhận lệnh, kiểm tra cạnh/hướng, rồi **vẫn dừng** với `MOTION_LOCKED_PENDING_ZONE_CLEARANCE_CALIBRATION`. Cạnh không hợp lệ bị báo `INVALID_COMMAND`; chưa quét RFID trong phiên hiện tại bị báo `NOT_AT_ROUTE_NODE`. `ENABLE_CORE_MOTION=false` và `MotorModule::drive()` chặn PWM. `STOP`/`WAIT` giữ dừng; `CHARGE_START` báo `CHARGER_NOT_CONFIGURED`. Bản thường đã xin/ACK grant vùng M trước khi chạy nhưng chưa tự trả quyền mutex; bản trial một xe bỏ chờ grant khi qua M1–M6 và tuyệt đối không dùng cùng xe thứ hai. Servo chưa được Core điều khiển qua các lệnh này. Ước lượng xe dài 18 cm, rộng 12 cm; đầu đọc nằm giữa ngang xe và cách đuôi khoảng 12 cm. ESP32 DevKit V1 38 chân, L298N và dây theo Config đã được người dùng xác nhận. Line đen đọc 1; `010`, `101`, `111` là nhóm bám thẳng. Sketch độc lập trên xe cũ xác nhận polarity W tiến/S lùi; xe 01 mới cần xác nhận lại trên giá kê. Sau thử GS1, pivot trái trial cấp trái tiến/phải lùi ở 200/200; pivot phải trái lùi/phải tiến. Chỉnh line khi tiến dùng bánh trái để chỉnh trái, bánh phải để chỉnh phải. Khi tới GS1, Serial Monitor in `[MOTOR]` với action, duty và line pattern để phân biệt pivot với chỉnh line một bên. Duty danh định trial đi thẳng là 220/220, với `LEFT_SPEED_TRIM=-30` nên duty trái giảm 30; chế độ thường dùng trim `-40`. Trim mới chưa được đo lại trên xe. Khi quay, firmware chờ mắt rời vạch cũ rồi bắt lại một mẫu bám thẳng; thời hạn quay thử đang giới hạn 2,5 giây và chưa được đo trên mặt đất. Xe không có encoder. Chưa xác nhận biên vùng M1–M6, bám line khi lùi, điện áp pin/B25, thời gian quay hoặc bám line thực. Không tự trả mutex trong bản thường sau khi chỉ đọc một thẻ.

## Chuẩn bị thử một xe với ngưỡng tạm 28 cm

Cập nhật 09/10/2026 cho hướng thử hai xe: xe 02 tại S2 hướng đầu về GS2, nên firmware nhận S2→GS2 là đoạn đi thẳng đầu tiên khi chưa có RFID trước đó. Người dùng đo đầu đọc đến đuôi xe 10 cm (thay ước lượng 12 cm ở đoạn lịch sử phía trên), tâm thẻ M đến mép cuối vùng 10 cm, tâm thẻ kế tiếp ngắn nhất 35 cm, RC522 đọc sớm 8 cm. Tại **lần đọc sớm** thẻ 35 cm, khoảng hở dọc đường danh nghĩa chỉ là 7 cm. Người dùng xác nhận các đoạn 35/50/100 cm trên map là số đo thật và bốn chuỗi 35+35 cm qua G2/G5/G3/G4 đi thẳng. Firmware trong workspace chỉ queue `mutex_release` JSON đúng grant ID khi đọc thẻ dự kiến cách M ≥50 cm hoặc thẻ thứ hai của đúng chuỗi thẳng 35+35 cm. Tuyến khác giữ mutex. Firmware hiện vẫn khóa motor trong `esp32dev` và `esp32dev_robot2_monitor`; binary mới chưa nạp lên xe. Xe 02 chưa có siêu âm; không áp dụng kiểm thử tuyến thật khi chưa có cách dừng vật cản đã kiểm chứng. [ADR-025](../../../docs/decisions/ADR-025-two-robot-supervised-trial.md).

### Bản bench hai xe trên giá kê

Hai env `esp32dev_two_robot_bench_01` và `esp32dev_two_robot_bench_02` mở PWM riêng cho đúng ID 01/02, dùng Core `AGV_TWO_ROBOT_TRIAL=1`, giữ xin/ACK/release mutex. Cập nhật 09/10/2026: hai env bench 01/02 cùng đặt tốc độ cơ sở 230/255; trim trái −40 nên lệnh thẳng/lùi có trái 190, phải 230. Ảnh motor probe riêng của xe 01 vẫn cố định 255/255 và giới hạn 2 giây. Không có ngoại lệ bỏ grant như bản một xe. Bản bench tạm bỏ chốt siêu âm và chốt pin điều khiển chưa hiệu chuẩn, nhưng chỉ cho PWM khi ADC ước lượng pack trong khoảng 6,4–8,6 V; điện áp ra ngoài khoảng này khóa lệnh và dừng khi đang chạy. Đây chỉ là chốt phần mềm, chưa thay phép đo trực tiếp bằng đồng hồ. Chỉ dùng khi **cả hai xe trên giá kê và có thể ngắt nguồn motor ngay**. Vẫn dừng khi mất line, thẻ RFID sai, mất mạng/heartbeat hoặc quá 15 s/đoạn; đường cần lùi còn bị chặn vì chưa xác nhận lái lùi. Servo lấy/trả hàng vật lý qua Core chưa được nghiệm thu. Bản `esp32dev`/`esp32dev_robot2_monitor` vẫn khóa PWM.

Nếu `SEGMENT_TIMEOUT` làm ESP32 báo `AGV_SAFE_STOP`, Core/web xóa lỗi phải gửi thêm `RESET_FAULT`; chỉ `WAIT` không mở được chốt firmware. Cần nạp firmware hỗ trợ lệnh này cho cả hai xe và dùng Core mới đồng bộ. Xe phải online, vừa xác minh RFID đúng vị trí, line hợp lệ, không có tuyến đang chạy và chốt điện áp đạt yêu cầu. Sau thao tác xóa lỗi, xem `agv/{id}/diagnostic`: `state=AGV_IDLE`, `error=NONE`, `motor=STOP` trước khi thử lệnh mới. ACK reset chỉ là nhận lệnh. Còi lùi chỉ chứng minh firmware đã đặt duty khác 0; nếu bánh vẫn đứng, đọc duty lúc lệnh đang hoạt động và kiểm tra nguồn motor/ENA/ENB/OUT của L298N trước khi tăng PWM tiếp.

Build từ thư mục chứa `platformio.ini` bằng `& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -e esp32dev_two_robot_bench_01 -e esp32dev_two_robot_bench_02`. Trước khi nạp, xác định ID xe/MAC qua Serial hoặc MQTT và đúng cổng COM. Nạp từng xe bằng `run -e esp32dev_two_robot_bench_01 -t upload --upload-port COMx` hoặc env `_02` tương ứng. Mỗi lần nạp/đọc Serial có thể reboot ESP32; quét lại S1/S2, xác nhận Core online/vị trí hợp lệ rồi mới xử lý lỗi và thử lệnh. Không dùng ảnh bench để chạy trên sàn khi chưa có cảm biến dừng vật cản và kiểm tra hình học thật.

Người dùng chọn **28 cm** làm mốc thử trước khi đo lại đường thật. `Config::MUTEX_EXIT_TRIAL_CM=28.0f` lưu mốc này trong firmware; hiện **chưa được dùng để phát `mutex_release`**. Bản trial mở PWM độc lập với mốc này vì chỉ có một xe. Vì đã bỏ encoder, firmware hiện không tự đo được 28 cm; cần cách xác nhận khác hoặc mốc RFID vật lý đã đo. Map v3 có cạnh ngắn nhất rời node M dài 35 đơn vị mô hình, nhưng chưa xác nhận 1 đơn vị bằng 1 cm thực tế.

Để lấy số đo còn thiếu, nạp project rồi mở Serial Monitor 115200 với bánh xe **nhấc khỏi mặt đất và nguồn motor đã tách/ngắt**. Mỗi giây dòng `[BENCH]` hiện ba giá trị mắt line, khoảng cách siêu âm và ADC pin; không còn trường encoder. Đưa vật cản ở khoảng cách đã đo để đối chiếu ngưỡng dừng 15 cm; đặt mắt line trên/vắng vạch để xác nhận mức active. Bề rộng vạch được báo là 4,5 cm, đường kính bánh 6,5 cm, đầu mắt cách trục bánh trước 5 cm và bánh sau 10 cm. Phần trăm pin hiện được nội suy từ dải điện áp cấu hình, nhưng ADC và cầu chia vẫn cần đối chiếu bằng đồng hồ trên xe.

Có thể thử **kết nối và nhận lệnh** với riêng xe `01` trước khi cho motor chạy: dùng ID MQTT `01`, quét RFID S1 thật và kiểm tra heartbeat, ACK, log `[ROUTE]`, telemetry. ACK và log route **không chứng minh xe đã chạy**. Để thử phần trăm pin, kiểm tra `batteryVoltage`, `batteryPercentage`, `batteryCalibrated` trên diagnostic và số hiển thị trên dashboard; telemetry điều khiển vẫn là `?` nên chưa áp dụng ngưỡng 20%/40%. Chế độ trial vẫn giới hạn ở GOTO trực tiếp xe 01. Để vận hành hai xe về sau vẫn cần xác minh pin trên cả hai xe, cơ chế grant/ACK/release mutex và kiểm tra dừng khi mất server.

`platformio run -e esp32dev` build firmware với phiên bản thư viện trong `platformio.ini`; `platformio test -e native` chạy các test logic, gồm `test/test_core_protocol`. Kiểm thử parser độc lập ở `tests/nckhagv_protocol.cpp` dùng g++ và ArduinoJson. `CommandCodec` cũ còn trong source để giữ test logic legacy nhưng không còn được MQTTModule sử dụng. Các bài này không thay thế upload và thử trên xe thật. Xem [giao thức MQTT](../../../docs/07-network.md) và [ADR-014](../../../docs/decisions/ADR-014-nckhagv-core-sync.md).
