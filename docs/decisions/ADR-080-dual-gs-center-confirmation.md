# ADR-080 — Xác nhận tâm line khi docking GS1/GS2

> Lịch sử: quy tắc docking `010` trong ADR này đã được thay bằng line rộng `111` theo [ADR-081](ADR-081-dual-gs-wide-line-confirmation.md).

Ngày 24/10/2026. Trạng thái: IMPLEMENTED phần mềm; kết quả kiểm chứng tại [tests](../../tests/README.md), nạp/thử xe thật OPEN.

## SOURCE và OBSERVED

Người dùng duyệt sửa firmware theo phương án review: chuẩn hóa đường dẫn project, giữ tính năng giảm PWM tại GS và bắt đúng010, khôi phục tuning01 đã ghi nhận, tăng buildID và kiểm thử cả hai profile.

Bản đầu vào lồng thêm `firmware/NCKHAGV/NCKHAGV`, thiếu suite `test_core_protocol`. Có55 file tracked giống HEAD và5 file khác. Nhánh docking mới bỏ xác nhận line ổn định nhưng còn đường phiếu011/110→010, đồng thời bỏ chốt dừng quét600ms. BuildID vẫnv16; tuning01 đổi từ−5/205/500ms thành0/200/100ms. Các quan sát này là mã nguồn, chưa phải đo trên xe.

## Quyết định

- Project chuẩn là `firmware/NCKHAGV`. Di chuyển toàn bộ nội dung, gồm cấu hình riêng và cache tại máy; không ghi credential vào Git. Sao lưu source đầu vào trong `.qa/docking-fix-20261024-before` và so hash mọi file source khi di chuyển.
- Giữ `AGV_GS_TURN_PWM=180` trong cả hai profile. Áp dụng trên tuyến có `reverseAfterTurn` tạiGS1/GS2; duty pivot gồm trim riêng từng xe:01=180/175,02=180/180. Tham số này không điều chỉnh PWM rẽ thường ngoài docking.
- Docking rẽ90° vào `TurnCapture`: chỉ010 liên tục≥100ms/≥3 mẫu mới cách≥5ms/gap≤50ms mới hoàn tất. Mẫu khác010 làm lại bằng chứng; cùng timestamp không thêm mẫu. Dừng khi xác nhận010;011/110 chỉ dùng căn bằng xung có giới hạn, không được tính là phiếu hoàn tất.
- Giữ ngân sách quét600ms tính từ lúc bắt đầuCapture. Từ600ms trở đi luônSTOP, còn nhận bằng chứng đến deadline1500ms. Hết hạn giữ lỗiLineLost. Không kéo dài ngân sách vì cần căn chính xác hơn.
- Docking quay180° vẫn dừng ở nhánh cuối theo luồng hiện có, xác nhận tại chỗ chỉ010 trong500ms ở01/100ms ở02; giữ deadlinequay2500ms. Khi dừng lệch011/110 mà không trở lại010 sẽ chờ rồi timeout, không tự cấp lùi.
- Sau xác nhận trảSTOP một vòng; sonar trước dùng khi quay, sonar sau phải có mẫu mới trước PWM lùi. RFID đích thật mới chốt `stationOriented`. STOP/mạng/cảm biến/vật cản/quyền vùng vẫn được kiểm trước xác nhận. Network pause3000ms và các policy ngoài docking giữ nguyên.
- Khôi phục01 trim quay phải−5, G1PWM205, xác nhận180°500ms theo duyệt24/10.02 giữ G1PWM200/xác nhận100ms. Các giá trị PD/lùi/servo/mạng/hiệu chuẩn dùng bản đầu vào. Bảng thông số hiện hành ở [tuning](../firmware-tuning-01-02.md).
- BuildID mới `20261024-robot01-gs-center-v17` và `20261024-robot02-gs-center-v17`. Không đổi topic/payload/ACK/schema/counter.

## Kiểm chứng và giới hạn

Khôi phục suite51 ca/profile từHEAD. Trước sửa, chạy lại suite này với sourceHEAD trong snapshot riêng tái hiện7 lỗi01/6 lỗi02: fixture đòi fault mạng tức thì thay vì pause, nhận mắt biên trước80ms và nhịp đếm nhánh cũ. Cập nhật fixture theo mã/policy hiện tại, kiểm cảSTOP tức thì lẫn fault sau3000ms; không sửa policy mạng để làm testđạt. Test capture mới dùng đúng cửa sổ docking100ms, helper xác nhận180° dùng500/100ms. Các điều kiện dừng và lỗi vẫn được assert.

`scripts/test-docking.ps1` kiểm navigator→Movement→Motor với GPIO giả, cảS1/S2 trên cả01/02: trái/phải/180°, PWM cótrim,010 thoáng qua/nhiễu/mẫu cũ/timestamp trùng/rollover, hạnquét600ms/timeout1500ms, STOP/mạng/sonar/vật cản/RFID. Cùng script kiểm sonar trước→dừng→xóa mẫu trước→thu mẫu sau mới→vật cản sau→lùi→RFID. `test-core-protocol.ps1`, `test-line-confirm.ps1`, `test-turn180.ps1` kiểm hồi quy. Testline thường dùng50ms đúngConfig hiện tại; bảnfixture100ms cũ đã lỗi trước thay đổi docking.

Đây là kiểm chứng phần mềm; chưa chứng minh góc xoay/quãng trôi, độ chính xác line, lực motorPWM180/175 hoặc khoảng trống thân xe thật. Chưa nạp firmware, điều khiển xe hoặc đổi dữ liệu Core/gateway.

## Khôi phục

Source trước sửa và mạng cục bộ nằm trong `.qa/docking-fix-20261024-before`. Giữ cấu trúc một cấp khi đối chiếu từng file; không đưa secret vào Git, không xóa/rewind NVS hoặc `command_sequence.json`. Build lại đúng profile sau mọi rollback; không dùng binary cũ có cùngv16 để suy ra mã đã nạp.

Tham chiếu: REQ-DUAL-GS-CENTER-01; [ADR-056](ADR-056-robot01-reverse-home-request.md), [ADR-059](ADR-059-robot02-firmware-parity.md), [ADR-076](ADR-076-dual-narrow-line-confirm.md), [ADR-077](ADR-077-turn180-fast-stop-confirm.md).
