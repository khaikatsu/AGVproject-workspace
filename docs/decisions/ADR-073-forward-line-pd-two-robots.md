# ADR-073 — PD bám line đi tiến cho hai robot

Ngày19/10/2026. Trạng thái: IMPLEMENTED trong source; kiểm chứng phần mềm tại tests/README.md, chưa nạp/thử sàn.

## Bổ sung v7 — Trợ bám cua riêng xe01, không sửa quay tại node

`SOURCE` 19/10/2026: người dùng báo xe01 lệch khi100/110/001/011/111, yêu cầu tự chỉnh nhẹ theo cua và nhấn mạnh không sửa code quay trái/phải tại node. `OBSERVED`: các mẫu này đã hợp lệ trong navigator, không phải tất cả đều bị xem là mất line. PDbase01 hiện210; khi110/011 vàD đổi dấu, chênh bánh có thể nhỏ trong khi tốc độ còn cao. Chưa có trace chứng minh ngưỡng cảm biến/chiều dây hoặc lực motor trên xe thật.

`IMPLEMENTED`: thêm bộ chọn PWM thuần `ForwardLineAssist` trước PD, chỉ bật mặc định trong profile01 bằng `AGV_FORWARD_LINE_ASSIST_ENABLED=1`. Nó chỉ được gọi qua `applyFollowing` sau navigator cấp quyền đi tiến; không phát PivotLeft/PivotRight và không khởi tạo task hoặc cấp quyền vùng. Hai bánh vẫn tiến, tạo cung bằng chênh duty. Mẫu mới đổi phía được xử lý ngay, không đợi intervalPD10ms. Khi về010 resetD và dùng lại base/trimPD hiện tại210/0/0.

| Mẫu chuẩn hóa Config | Duty ENA / ENB mặc định | Hành vi bám line |
| --- | --- | --- |
| 110 | 180 / 120 | Bo trái nhẹ theo ánh xạ cũ |
| 100 | 180 / 80 | Bo trái rõ hơn khi chỉ còn mắt bên |
| 011 | 120 / 180 | Bo phải nhẹ theo ánh xạ cũ |
| 001 | 80 / 180 | Bo phải rõ hơn khi chỉ còn mắt bên |
| 111 hoặc101 | 120 / 120 | Chậm thẳng, không đoán phía hay giữ lệch cũ qua giao điểm |
| 010 | 210 / 210 hiện tại | PD đi thẳng, không mangD từ nhánh trợ bám |
| 000 | 0 / 0 lúc phát hiện | Giữ phục hồi có giới hạn của v6, không tiến mù |

Các mức trợ bám được cap theo basePD nếu người dùng giảm base sau này, cộng trimPD rồi clip0…255. GPIO39/34/35 và chiều ENA/ENB giữ như trước. Không coi111 là bằng chứng chọn nhánh trái/phải: ba mắt cùng thấy vạch không cung cấp hướng duy nhất. Sau000 vẫn cần xác nhận line theo v5/v6 trước khi áp trợ bám; lỗi đã chốt không tựclear.

Không sửa `CoreNavigator.cpp`, `AGVController.cpp`, `MovementModule.cpp` (gồm turnLeft/turnRight), `MotorModule.cpp` hoặc `Robot02Config.h` trong lượt này; đối chiếuSHA256 trước/sau và kiểm hồi quy. Lùi, Turning/TurnCapture, PWMxoay/settling, grant, sonar/STOP/mạng, timeout tìm line vàCore/MQTT giữ nguyên. Profile02 không bật assist và giữ buildIDv6; profile01 `20261019-robot01-curve-assist-v7`. Đặt cờassist0 để vềPDv6; tắtPD vẫn dùng bảngPWM cũ.

Kiểm chứng tại [tests](../../tests/README.md): GPIO→pattern→duty, đổi phía trong1ms/cùng timestamp, wide không lệch cũ, về010 khôngDkick, STOP/hazard, chặngM2→G6 cần grant,000 rồi phục hồi ở1300ms, hướng quay/lùi cũ và native cảhai. `OPEN`: chưa nạp/chạy xe thật; PWM180/120/80/120 là mức thử, cần kê bánh xác minh hướng và lực tạiPWM80, sau đó thử cung riêng xe01 có giám sát. Không khẳng định sửa phần mềm đã loại hết mất line vật lý.

## Bổ sung v6 — Tách hạn quét motor khỏi hạn xác nhận lỗi

`SOURCE` 19/10/2026: người dùng đồng ý tách thời gian tìm line và thời hạn báo lỗi, để xe01 có thời gian xác nhận đã bắt lại vạch. `OBSERVED`: v5 xe01 dùng chung hạn600ms cho quét và báo lỗi; ứng viên xuất hiện ở550ms cần100ms xác nhận nên chưa thể hoàn tất trước hạn. Đây là khả năng từ mã, chưa có trace chứng minh là nguyên nhân duy nhất trên sàn.

`IMPLEMENTED`: profile01 thêm `AGV_LINE_RECOVERY_FAULT_MS=2000`, tính từ lần đầu000 của lần mất line có hướng tìm. `LINE_RECOVERY_TIMEOUT_MS=600` vẫn là hạn quét motor, không phải2s được phép quay. Hết600ms luôn STOP, nhưng tiếp tục nhận bằng chứng line hợp lệ và đủ100ms/≥3 mẫu trước hạn2s để bám tiếp cùng task/route/index/grant. Không gia hạn deadline theo ứng viên, không thêm lượt quét, không reset timeout chặng. Ví dụ001 ở550ms có thể được xác nhận tại650ms;001 ở1200ms có thể được xác nhận tại1300ms. Hết2s chưa đủ bằng chứng vẫn chốtLINE_LOST, không tự mở lỗi khi line trở lại.

Xe02 giữ `AGV_LINE_LOSS_GRACE_MS=7000` như trước. Xe01 không có hướng đủ mới hoặc đã dùng lượt quét vẫn giữ hạn `LINE_LOSS_CONFIRM_MS` hiện tại7s, không rút xuống2s. Profile khác không định nghĩa macro mới giữ hạn cũ600ms; tắtPD vẫn dùng quy tắc xác nhận010. Hạn Turning/TurnCapture600ms, PWM/PD/trim, grant/STOP/sonar/mạng/RFID và luồng lỗi Core/MQTT không đổi. Trong chờ, `following=false`, không cộng pivot vào encoder hoặc trả vùng vì timeout.

BuildID01/02 `20261019-robot01-line-confirm-v6` / `20261019-robot02-line-confirm-v6`. Native kiểm550→650ms,1200→1300ms cả rollover/tiến/lùi hợp lệ, không xuất pivot sau600ms, deadline tuyệt đối và hazard khi chờ. Host nối navigator→driver tạiM2→G6 kiểm duty0/0 lúc600/1000ms rồi PD sau xác nhận1300ms. Kết quả ghi tại [tests](../../tests/README.md). `OPEN`: chưa upload hoặc thử xe thật;2s là mức thử phần mềm, cần kê bánh rồi thử riêng xe01 có giám sát.

## Bổ sung v5 — Nhận lại line bên sau 000 khi dùng PD tiến

`SOURCE`: người dùng báo xe đang001 nhưng bịLINE_LOST và Core không cho chạy tiếp. `OBSERVED`:001 khi Following bình thường là mẫu hợp lệ trong cả navigator/PD. Tuy nhiên sau000, `recoverLine` cũ chỉ nhận010 đủ100ms;001/011/100/110 không thoát tìm line và có thể chạm deadline dù đã thấy vạch. Diagnostic đọc trực tiếp ởxe01 đangv4 cho thấySAFE_STOP/LINE_LOST với pattern010 vàSTOP0/0, chứng minh lỗi cũ được giữ; không có trace đủ nhanh để khẳng định mẫu tại thời điểm phát lỗi đầu tiên.

`IMPLEMENTED`: riêng Following đi tiến bậtPD, sau000 chấp nhận nhóm001/011 hoặc100/110 khi cùng phía≥100ms,≥3 mẫu khác thời điểm, gap giữa mẫu≤50ms. Trong xác nhận giữSTOP, không pivot khi đã có ứng viên line bên. Đổi phía,000,101/111 hoặc gap quá50ms xóa bằng chứng; không trộn với mẫu010 để đủ ngưỡng. Đủ bằng chứng trước deadline mới trả action chỉnh theo mẫu hiện tại đểPD bám lại, giữroute/index/task/grant. Thiếu mutex xóa mẫu đang xác nhận. Không gia hạn deadline quét/grace/chặng, không thêm lượt quét. Đi lùi/tắtPD vẫn cần010; phaTurning/TurnCapture không đổi. Lỗi đã chốt không được tự xóa chỉ vì001/010 xuất hiện; người vận hành vẫn xác minh vị trí vàclear_error theo luồng hiện có.

BuildID01/02 `20261019-robot01-pd-side-recovery-v5` / `20261019-robot02-pd-side-recovery-v5`. Giữ tuning hiện tại của người dùng: PDbase01=210,02=160. Native/host kiểm001 bình thường không lỗi,000→line bên đủ mẫu mới tiếp tục, nhiễu/timeout/hazard/mutex không bỏ chốt, lùi giữ cũ; kiểm chứng tại tests/README.md. Đây là mở rộng hẹp quy tắc centered-only củaADR-062 cho PDtiến; chưa upload/đo xe thật.

## Nguồn và phạm vi

`SOURCE`: người dùng cung cấp `C:/Users/DELL/Downloads/dolinexengu2 (1).ino`, xác nhận sketch từng chạy trên xe01, yêu cầu tích hợp vào dự án và mở rộng cho cả01/02. SHA256 tệp đầu vào `6CB644BE1944388880D5D1443EECE34CB51F70F52D107EB3D1A6E8263A5FEAA6`. Đây là mã người dùng cung cấp, không phải bằng chứng robot02 đã chạy thành công.

`OBSERVED`: sketch dùng ba mắt, base160/Kp1/Kd0,6/chu kỳ danh định10ms. Sketch gọi GPIO35 là trái và39 là phải; Config dự án đặt tên ngược nhưng đầu ra ENA/ENB tương đương khi quy đổi theo tín hiệu GPIO. Trước đó profile01/02 dùng bảng PWM cố định. Các mức chỉnh tay cũ phải tiếp tục dùng được khi tắt PD.

## Quyết định

- Thêm `ForwardLinePD` thuần tính toán và `MovementModule::applyFollowing`. Controller chỉ gọi sau navigator cho phép Following tiến; cả lượt sớm trước RFID và lượt đầy đủ dùng cùng cách, cùng mẫu line/time với navigator. Không cho lượt đầy đủ ghi đè PD bằng bảng PWM cũ.
- Bật riêng `AGV_FORWARD_PD_ENABLED=1` trong cả hai profile. Mỗi profile có base160/Kp1/Kd0,6/trimPD0/0 độc lập. Trim cũ tiếp tục áp cho fallback/lùi/pivot; PD có trim riêng nhằm tái hiện160/160 của sketch đã chạy.
- Bảng error theo pattern của **Config**:010=0,110=+50,100=+100,011=−50,001=−100. DutyENA=base+error*Kp+D, dutyENB=base−error*Kp−D, clip0…255. Cùng GPIO35/34/39 kích hoạt phải cho cùng chiều ưu tiên motor như sketch nguồn; không đảo pin.
- PD cập nhật tối thiểu10ms, không delay/catch-up. D=`Kd*(error-lastError)*10/dt_ms` để giữ ý nghĩa hệ số gốc tại10ms; bỏ D ở mẫu đầu hoặc gap>50ms. Chu kỳ thực vẫn phụ thuộc RFID/network; không tuyên bố vòng thời gian thực10ms.
- D reset khi STOP, mẫu000/không hợp lệ, đổi route/index, RFID node mới (kể cả đi xuyên), đổi sang lùi/pivot/tìm line/capture/settling, mất quyền hoặc hazard.101/111 giữ hành vi đi thẳng và ngắt lịch sử D. Mỗi robot/module có bộ nhớ riêng.
- Các chốt wifi/MQTT/config/sonar/servo/STOP/000 được kiểm tra trước dùng PWM cache, kể cả chưa tới10ms. Quyền mutex/node-order/task vẫn do navigator/Core; PD không cấp chuyển động hoặc hoàn tất task.
- Chỉ lấy bộ điều khiển đi tiến từ sketch. Không ghép tự chạy sau boot, SerialG, tiến mù100ms khi000, quay tìm1500ms hoặc tự chạy sau timeout. Hướng tìm line cuối đã do navigator nhớ theo action; giữ bộ phục hồi, giới hạn và chốt cũ.
- Lùi, quay, bắt line sau rẽ, servo, RFID, encoder-clearance và MQTT không đổi giao thức/logic. Tuning PD khi tiến áp cho mọi scope sử dụng firmware tương ứng, không chỉ Core scope01+02.

## Quay lại và triển khai

Đặt `AGV_FORWARD_PD_ENABLED=0` trong profile xe cần quay lại rồi build/nạp đúng env; cũng hỗ trợ override build flag0. Đây là cờ compile-time, không đổi trực tiếp trên dashboard. Binary trước PD được giữ tại `.qa/forward-pd-baseline/robot01-before-pd.bin` và `robot02-before-pd.bin`.

Firmware mới: `20261019-robot01-forward-pd-v4` / `20261019-robot02-forward-pd-v4`, env `esp32dev_two_robot_bench_01` / `_02`. Core/gateway không cần thay chỉ để dùng PD. Servo, mạng và counter giữ nguyên; robot không tự nhận lại nhiệm vụ cũ sau nạp.

## Kiểm chứng và giới hạn

Bổ sung19/10/2026 sau log lỗi biên dịch người dùng: `Config.h` hiện hành thiếu toàn bộ khai báo `Config::FORWARD_PD_*` và mang buildID cũ, trong khi profile/Movement/Controller đã dùngPD. Khôi phục cầu nối macro profile→namespaceConfig, cả nhánh tắtPD và interval10ms/reset-gap50ms, đồng bộ lại buildIDv4. Giữ tuning người dùng hiện có (fallback base01=230,02=220; PDbase160 mỗi xe). Không thay controller hoặc thuật toán. Kiểm chứng lại host bật/tắt cảhai và buildESP32 tại tests/README.md; chưa upload.

Host probe kiểm hướng theo GPIO, P/D theo10/20ms, mẫu cùng thời điểm, gap dài, millis rollover, clip, controller độc lập, reset route/index/STOP/blank, giữ lùi/pivot và quyền mutex ở chínhM2→G6; chạy cảhai profile và cả cờ bật/tắt. Native hồi quy navigator kiểm node-order/RFID/capture/mất line/mạng/sonar/mutex. ESP32 build kiểm đường gọi Controller thật. Hồ sơ tại [tests](../../tests/README.md).

Sửa assertion test cũ cố định5s để dùng giới hạn timeout cấu hình: SharedRobotConfig hiện là7s do tuning trước lượt này; không sửa thời gian runtime để làm test qua.

`OPEN`: chưa upload hoặc đo xe thật trong lượt tích hợp. Hệ số từ xe01 là điểm khởi đầu cho02; cần kê bánh xác nhận cùng GPIO→kênh motor, rồi thử riêng từng xe trên đường thẳng/cung trước khi chạy cùng. PD ba mắt digital vẫn có thể mất vạch vì hình học, ngưỡng mắt hoặc độ trễ; kết quả host/build không chứng minh hếtLINE_LOST.
