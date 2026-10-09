# ADR-015: Điều khiển GOTO theo từng RFID và hiển thị vị trí khi pin chưa đo

Ngày 04/10/2026. Trạng thái: mã nguồn đã triển khai, chưa nạp hoặc kiểm thử trên xe thật.

## Nguồn và bối cảnh

`SOURCE`: người dùng muốn quét RFID để biết vị trí, ra lệnh trên server để xe bám line tới các RFID và hiện lại vị trí trên map; các thông số còn thiếu sẽ cung cấp sau. Xe 01 lùi S1→GS1, mắt line vẫn thấy vạch. Motor trước đó được giữ khóa vì chưa kiểm chứng rẽ, pin và toàn thân rời vùng M1–M6.

`OBSERVED`: Core chỉ dùng telemetry `node|battery|obstacle` để cập nhật map và hoàn thành GOTO. ACK chỉ báo đã nhận lệnh. NCKHAGV trước thay đổi chỉ kiểm tra route rồi dừng; pin B25 chưa hiệu chuẩn nên không gửi telemetry pipe. Core từ chối giao task khi `battery_measured=false`.

## Quyết định triển khai

Firmware NCKHAGV thêm `CoreNavigator`: nhận path có hướng từ GOTO, xử lý từng đoạn, bám line tiến/lùi, quay tới khi rời vạch cũ và bắt lại vạch, dừng ngắn tại RFID, kiểm tra node kế, dừng khi mất line, siêu âm lỗi/vật cản, mạng lỗi hoặc quá thời hạn đoạn. Trước đoạn chạm vùng M, firmware yêu cầu grant và chỉ chạy sau khi nhận đúng `{zone,grant_id}` rồi ACK. Không tự gửi `mutex_release`: RFID kế tiếp và 28 cm chưa chứng minh đuôi xe đã ra ngoài vùng. Mất phiên Core xóa grant cục bộ và dừng.

Giới hạn thử trong mã là 2,5 giây cho quay và 15 giây cho một đoạn, nhằm dừng nếu không thấy thẻ/vạch. Đây là giá trị bảo vệ phần mềm, chưa phải hiệu chuẩn tốc độ của xe; quá hạn phải dừng và kiểm tra tại chỗ.

Telemetry v0 chấp nhận `node|?|obstacle` khi pin chưa đo. Core vẫn cập nhật RFID để map hiển thị node thực, nhưng giữ `battery_measured=false` và từ chối giao nhiệm vụ. Không phát phần trăm pin giả. Sketch cũ và simulator gửi số 0–100 như trước. Khi pin được hiệu chuẩn, firmware tiếp tục dùng payload số theo hợp đồng hiện có.

`ENABLE_CORE_MOTION=false` vẫn khóa PWM và `GOTO` trả lỗi khóa. Cờ này không được mở riêng lẻ: đường S1→GS1 cần xác nhận lái lùi; mọi đoạn rẽ cần timeout đã đo; pin B25 cần đo; vùng M cần xác minh cách rời vùng. Cơ chế giữ grant chưa dùng được cho vận hành hai xe dài hạn vì không tự trả quyền.

## Bằng chứng và giới hạn

Kiểm thử host xác nhận parser/route, đoạn lùi S1→GS1, dừng ở RFID, dừng khi mất line, từ chối node sai, chờ grant trước vùng M. Core unit xác nhận RFID có pin `?` không bị tính là pin đo. Build Core/test, kiểm thử MQTT tích hợp với broker/Core thật trong fixture, PlatformIO ESP32 build và 52 native test đã PASS. Chưa có upload hoặc thử GPIO/RC522/MQTT/motor trên ESP32 thật. `OPEN`: xác nhận chiều motor, timeout rẽ/đoạn, hệ số B25, vùng quét RFID, cách xác nhận rời M, kiểm thử bàn và tuyến một xe trước khi mở PWM.
