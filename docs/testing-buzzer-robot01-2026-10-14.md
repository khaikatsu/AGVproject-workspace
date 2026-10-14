# Kiểm tra còi lùi xe01 — 14/10/2026

`SOURCE`: người dùng báo hai bánh lùi mà còi không kêu, dây vẫn giữ như cũ.

`OBSERVED`: BuzzerModule/MovementModule vẫn điều khiển activeLOW GPIO0; ba action lùi có duty âm bật còi, tiến/pivot/STOP tắt. MotorModule lưu duty theo chiều logic nên inversion bridge không đổi điều kiện còi. Encoder dùngGPIO16/4, không trùngGPIO0. Các sửa encoder/giảm PWM/mạng không loại nhánh bật còi. Chưa có bằng chứng đủ để kết luận lỗi firmware hoặc hỏng module.

Trace thụ động19:11:50–19:12:36 tại `.qa/encoder-network-live/run-2026-10-14T12-11-50-970Z`: firmware20261014-robot01-network-clock-fix, boot2736130448/session1,43diagnostic, toàn bộ STOP/PWM0/0/AGV_IDLE và RFID UNKNOWN. Không phát lệnh thử/reset/nạp. Không có mẫu lùi để đối chiếu còi.

## Chuẩn bị phép đo

FirmwareBuild `20261014-robot01-buzzer-diagnostic` thêm diagnostic01 `buzzerOn` là yêu cầu theo action/duty, `buzzerPinLevel` là mức đọc GPIO0 lúc snapshot. Serial115200 in `[BUZZER] ... requested=... gpio=0 level=... activeLevel=0` khi action đổi. Không thay pin/polarity/PWM/timeout, điều kiện motor/còi hoặc cơ chế mạng hiện có.

Build ESP3201/02 PASS. Host module thật MotorModule/MovementModule/BuzzerModule/LineFollowModule với I/O stub PASS cả hai profile: duty lùi01−150/−190; lùi/chỉnh line đọcLOW, pivot/tiến/STOP đọcHIGH,50vòng lùi liên tục không tắt còi. Đây là kiểm chứng tín hiệu driver qua stub, chưa nghe âm thanh trên phần cứng. Mẫu diagnostic tăng độ dài chuỗi/node/task/session tới giới hạn dùng1310byte, dưới buffer1536; không coi đây là phép đo toàn bộ biến thể của serializer trên bo.

## Đối chiếu sau nạp

- `buzzerOn=true/buzzerPinLevel=0` khi bánh lùi nhưng im: đo SIG/VCC/GND tại module và kiểm tra giắc/module; đọcLOW chưa chứng minh có điện áp nguồn hay âm thanh.
- `buzzerOn=true/buzzerPinLevel=1`: GPIO không có mức mong đợi tại lúc đọc; cần đo dây SIG và đối chiếu Serial, output mode/tải ngoài.
- `buzzerOn=false` dù xe thật lùi: đối chiếu motor/duty/route và firmwareBuild; không thay cực còi theo suy đoán.

`OPEN`: chưa nạp bản diagnostic hoặc đo GPIO/nguồn/âm thanh lúc lùi. Artifact mới trong `build/robot01-buzzer-diagnostic`; ảnh clock-fix trước giữ riêng.
