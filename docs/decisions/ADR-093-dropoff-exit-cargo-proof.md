# ADR-093 — Hạ tại kho, xác nhận kệ rời xe sau lùi ra gate

Ngày 29/10/2026. Yêu cầu REQ-DROPOFF-EXIT-PROOF-20261029. IMPLEMENTED trong source; nghiệm thu hai xe OPEN.

## Nguồn và quyết định

SOURCE: người dùng xác nhận servo hạ xong thì xe vẫn nằm dưới kệ, LM393 còn báo HIGH/raw1; lùi ra kho mới LOW/raw0. Người dùng duyệt sửa luồng này cho cả01/02. Quyết định này thay chốt sau hạ trong ADR-091 và phần chờ DONE_DOWN tại kho trong ADR-069, không đổi quy ước HIGH=có hàng/debounce150ms.

OBSERVED trước sửa: firmware chờ cảm biến không hàng sau5000ms hạ, tối đa7000ms mới lỗi; Core/gateway cũng chờ DONE_DOWN/không hàng tại RFID kho trước GOTO ra gate. Vì vậy kệ đúng vị trí vẫn làm luồng đứng/lỗi.

## Luồng source mới

1. Trước LIFT_DOWN giữ proof có hàng ổn định, xe dừng đúng kho/phiên/RFID, không lỗi/task/servo; lệnh đúng ID và không replay.
2. Hết chu kỳ điều khiển servo5000ms, MCU báo `LOWERED`, `liftBusy=false`, `virtualCargo=true`; đây là đã chạy hết chu kỳ, không phải phép đo cơ khí. Cảm biến vẫn có thể HIGH. Core chờ diagnostic/cảm biến mới ổn định tại kho, đủ5000ms ở cả hai phía, rồi mở riêng chặng lùi.
3. Khi chờ ra kho, MCU và Core chỉ chấp nhận đường một cạnh K1→G1…K6→G6; MCU kiểm maneuver REVERSE. Không phát servo lần nữa, nhận task khác, về trạm hoặc suy mất hàng từ LM393 chuyển LOW trong chặng này. Chốt line, sonar sau, mạng, STOP, RFID, grant và timeout chuyển động giữ nguyên.
4. Tại gate tương ứng, xe dừng/PWM0, RFID mới≤250ms, không lỗi và cùng phiên: LM393 LOW ổn định150ms mới phát `DONE_DOWN`, cargo=false. Nếu vẫn HIGH hoặc không ổn định sau2000ms tại gate thì lỗi `CARGO_RESULT_MISMATCH`/`CARGO_SENSOR_UNSTABLE`, không lùi thêm hoặc lặp servo. Thiếu proof RFID/dừng/mạng không được suy hoàn tất.
5. Core nhận DONE_DOWN đúng lift ID/boot/kết nối và cảm biến mới tại gate mới xóa hold. Gateway chờ thêm task ra gate hoàn tất đúng xe; lúc đó transaction mới chuyển kệ trong sổ từ xe về kho giao, ghi sự kiện đúng một lần. Giữ reservation/cargo/sổ kho chờ xác minh cho tới mốc này. Nâng hàng vẫn DONE_UP/có hàng tại kho như trước.

## Giao thức và tương thích

Diagnostic thêm boolean `dropoffExitSupported:true`; `liftState` thêm `LOWERED`, vẫn dùng LIFT_DOWN/GOTO/NODE_ORDER và topic/ACK/counter hiện có. Snapshot `lift` thêm `dropoff_exit_supported`, `dropoff_exit_pending`. `lift.node` giữ kho giao; `lift.observed_node` đi theo vị trí quan sát hiện tại, nên DONE_DOWN mới được quan sát tại gate. `virtual_cargo=true` trong giai đoạn LOWERED/lùi là chốt chưa xác minh kệ rời xe, không khẳng định servo còn đang nâng.

Core mới từ chối hạ nếu MCU thiếu capability; MCU mới với Core cũ giữ dừng ở LOWERED, Core cũ không nhận nhầm DONE_DOWN. Gateway chỉ giữ nhánh cũ cho Core không yêu cầu cargo_sensor; không fallback timer/MCU cũ khi required. Cần cập nhật Core/gateway và nạp cả hai profile cùng đợt.

STOP/lỗi/mất mạng/kết ca không tự chạy tiếp. Đổi boot/kết nối vô hiệu proof cũ. Checkpoint giữ hold/kho/lift ID đang chờ ra gate nhưng không khôi phục quyền điều khiển/boot; sau restart cần đối soát, không nhận DONE cũ làm hoàn tất. Hoàn tác cargo khi còn hold cần xe đã ra gate và có proof trống; không đoán từ timeout. Nhiệm vụ lùi gián đoạn chỉ tiếp tục theo cơ chế xác minh hiện có, không phát lại hạ servo.

## Kiểm chứng và giới hạn

BuildID source01/02: `20261029-robot01-dropoff-exit-v25` / `20261029-robot02-dropoff-exit-v25`. Rà mã/diff cho hai profile; không thay file profile, hiệu chuẩn pin, mạng hoặc dữ liệu vận hành. Cập nhật ca kiểm tra server `tests/servo_core.cpp` và `tests/cargo-sensor.test.js` cho LOWERED khi HIGH, đúng/sai gate/ID, proof trống mới/cũ và sổ kho chờ tới gate. JavaScript `node --check` (chỉ parse), parse JSON hai ví dụ và `git diff --check` thành công. Chưa chạy ca kiểm thử; chưa build/test/nạp MCU, build/chạy Core/gateway hoặc thử xe thật theo quy trình người dùng.

OPEN: người dùng nạp/thử01/02, kiểm RFID gate, kệ còn che sensor khi hạ, raw đổi khi lùi và tình huống vẫn kẹt/còn hàng ở gate. LM393 không đo tải/góc servo/ID kệ; vật lý vẫn phải được nghiệm thu. Không dùng tín hiệu hàng để nhả mutex.
