# ADR-023 — Bỏ chốt pin cho xe 01 trong chế độ thử

Ngày 08/10/2026. Trạng thái: triển khai phần mềm; chưa xác nhận trên xe thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng yêu cầu vẫn hiển thị phần trăm pin nhưng server phải cho xe đi, và chọn phạm vi **chỉ xe 01 đang thử**. Chưa muốn áp dụng ngưỡng 20%/40% cho lần thử này.

`OBSERVED`: firmware NCKHAGV gửi `?` trong telemetry điều khiển và phần trăm pin qua diagnostic. Core thường từ chối GOTO khi pin chưa đo. Core thử một xe đã miễn pin `?` nhưng vẫn khóa nếu telemetry báo số dưới 40%; ca thử cũng khóa pin thấp.

## Quyết định

- Khi `AGV_ONE_ROBOT_TRIAL=1`, bỏ điều kiện pin đã đo và ngưỡng 40% ở bước mở ca và `direct_goto` của xe 01. Không tự kích hoạt sạc ở chế độ này. Giữ % pin diagnostic để hiển thị, không chuyển nó thành pin điều khiển.
- Giữ nguyên giới hạn một xe, RFID/telemetry mới, trạng thái IDLE, mạng, lỗi, tuyến, vùng mutex và giới hạn 32 chặng. Xe 02 hoặc Core thường vẫn dùng chính sách pin/sạc hiện có.
- Script thử một xe dùng binary riêng `build/trial/agv_core.exe` để không nhầm với `build/agv_core.exe` của chế độ thường. Thay đổi chỉ có hiệu lực sau khi build và khởi động lại Core bằng script thử một xe. Không tự dừng Core đang chạy hoặc nạp lại firmware.

## Rủi ro và kiểm chứng

Không có chốt phần mềm dừng xe 01 do pin thấp trong chế độ thử. Ước tính % theo điện áp chưa đối chiếu đồng hồ; người vận hành phải giám sát pin thực, giữ nút ngắt nguồn trong tầm tay và chỉ thử khi xe đứng ở vị trí đã xác minh. Unit test kiểm tra pin `?`, pin báo thấp, % hiển thị và ca trial. `VERIFIED` phần mềm: build binary trial và web vận hành báo đúng chế độ thử, hiển thị 54% pin sau khi chuyển Core với sự cho phép của người dùng. Core giữ ERROR xác minh do reconnect; người dùng tự kiểm tra và xóa lỗi, chưa có GOTO hay kiểm tra chuyển động vật lý.
