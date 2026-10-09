# Chẩn đoán khởi động gateway và core — 29/09/2026

## Bằng chứng OBSERVED

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
