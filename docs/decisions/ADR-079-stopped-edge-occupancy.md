# ADR-079 — Xe lỗi đã dừng chặn một node hoặc một đoạn đường

Ngày 23/10/2026. Trạng thái: IMPLEMENTED và VERIFIED bằng unit/fixture MQTT; kết quả và một test tuning cũ không khớp tại [tests/README.md](../../tests/README.md), chưa nạp hoặc thử xe thật.

## Nguồn và phạm vi

23/10/2026, REQ-FIRMWARE-SERVER-SYNC-01: sau RAR ghi đè mất producer proof, người dùng duyệt khôi phục riêng phần tương thích. `Types.h`, controller và codec lại giữ/phát `stopped_position`; dùng `StoppedPosition.h` đã có, hai bánh ổn định ≥600 ms, đúng phiên và RFID mới cho proof node. BuildID `20261023-robot0x-server-sync-v16`; giữ nguyên tuning và network pause của RAR. Các ca native legacy quay/mất mạng chưa khớp nền RAR, không sửa chúng trong phần đồng bộ này. Kiểm chứng tại [hồ sơ](../../tests/README.md).

`SOURCE`: người dùng chốt mô hình như vật cản: xe dừng giữa A–B thì chặn A, B và cạnh hai chiều; xe được xác nhận tại A thì chặn A. Không cộng khoảng đệm hình học. Trả mutex ngoài vùng, replan xe còn lại; mất mạng hoặc dữ liệu cũ thì giữ vùng chưa xác minh.

`OBSERVED`: odometry cũ mất ngữ cảnh chặng khi MCU fault/STOP, Core không nhận encoder khi ERROR. `tracking:false`, speed0 hoặc chấm cuối trên web không chứng minh xe đã về RFID gần nhất. [ADR-078](ADR-078-failure-footprint-replanning.md) mới bổ sung kích hoạt replan, vẫn chặn toàn bộ đường chưa xác minh.

## Quyết định

1. Firmware01/02 giữ from/to, GOTO/index và quãng đường khi lỗi. Driver STOP/duty0 và cả hai raw encoder không đổi ít nhất600ms mới có `stopped_position` phiên bản1. Xung còn tăng thì bắt đầu lại thời gian đứng yên; xung trôi được cộng để Core phát hiện vượt chiều dài cạnh. Lỗi sau pivot không suy ra chặng nếu thiếu RFID mới. Thẻ mới trong trạng thái dừng mới chứng minh một node; không suy node từ distance/clamp marker.
2. Core cần telemetry≤1000ms, diagnostic cùng Core/boot/MQTT session, đúng node, SAFE_STOP/ERROR, motorSTOP/duty0 và không task MCU. Diagnostic≤1500ms và uptime khớp cửa sổ. Hai proof tăngseq/uptime cách200–1000ms giữ cùng vùng/rawticks/distance; một proof, ACK hoặc speed0 không đủ.
3. Proof cạnh khớp GOTO/index/from/to đã giao, distance không vượt weight cạnh. Proof node cần diagnostic RFID tuổi≤250ms. Firmware cũ không có proof giữ chính sách bảo thủ.
4. Planner dùng duy nhất một node hoặc hai đầu cạnh thay `uncertain_path`/hành lang cũ khi proof còn mới. Chặn hai đầu cũng ngăn đi qua cạnh cả hai chiều; không ghi obstacleTTL chung lên map. Không cộng vùng đệm theo yêu cầu. Dijkstra/taskID giữ nguyên; xe lỗi vẫnERROR, task gián đoạn không tự giao lại.
5. Arbiter có mặt nạ `blocked_by_stopped_robot` độc lập owner: vùng bị chặn không cấp/nhắc grant kể cả owner rỗng hoặc waiter cũ. Cập nhật mặt nạ, rút lượt chờ/replan rồi trả đúng owner/token các mutex ngoài vùng. Mutex thuộc tập node vẫn giữ owner nếu của xe lỗi. FIFO chỉ chạy lại khi hết bị chặn.
6. Giữ `uncertain_path` cũ để dự phòng; lưu `stopped_nodes` vào checkpoint. Thiếu proof, stale, telemetry cũ, đổi node/phiên, offline hoặc restart làm mất quyền thu hẹp: planner và arbiter trở lại tập bảo thủ gồm đường cũ và node đã xác nhận, replan xe bị ảnh hưởng. Không tạo lại owner cho mutex đã trả. Publish command/grant chỉ sau checkpoint thành công.
7. Trả mutex không xóaERROR hoặc tựRESET_FAULT. Luồng RFID/diagnostic phục hồi cũ vẫn áp dụng. Restart không khôi phục proof còn mới từ đĩa.
8. Firmware hiện từ chối GOTO khi navigator còn active. Core không phát lại tuyến nếu path vẫn an toàn/không đổi; nếu phải thay tuyến đang chạy trên firmware có NODE_ORDER, gửi WAIT, giữ BLOCKED và `replan_waiting_for_node:true`. Chỉ diagnostic mới hơn mốc WAIT, cùng phiên, motor STOP/duty0/task inactive, lỗi NONE và RFID mới tại currentNode mới mở bước giao tuyến thay thế. Dừng giữa hai thẻ thì tiếp tục chờ; không dùng RFID lưu cũ làm gốc tuyến mới.

## Tương thích và giới hạn

Mở rộng tùy chọn odometry schema1; telemetry pipe/command/ACK/payload mutex giữ nguyên. Core cũ bỏ qua proof; Core mới với firmware cũ giữ bảo thủ. Cần firmwarev15 và Core mới để sử dụng.

Đây là mô hình đồ thị do người dùng chọn, chưa nghiệm thu hình học hoặc sai số encoder. Không đảm bảo đi vòng nếu A/B là lối duy nhất. Mốc600ms và cửa sổ tuổi mẫu là thông số phần mềm, cần thử bàn/sàn trước vận hành.
