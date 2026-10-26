# ADR-089 — Hoàn tác kệ về kho lấy khi đối soát lượt thử

Ngày 26/10/2026. Trạng thái: IMPLEMENTED phần mềm; nghiệm thu xe thật OPEN.

## Nguồn và hiện trạng

`SOURCE`: người dùng hiện thử hai xe đi tuyến và nâng/hạ, chưa cần luồng cảm biến hàng hoàn chỉnh. Người dùng duyệt nút đối soát trả dữ liệu kệ của lượt gián đoạn về kho lấy, sau khi xác nhận kệ đã về vị trí cũ và xe trống. Kho nguồn có kệ khác phải bị chặn. Phương án cảm biến vẫn là PROPOSED cho giai đoạn sau.

`OBSERVED`: `orders.resolve` cũ chỉ đóng đơn; sổ kho và cargo ảo của Core vẫn có thể giữ kệ trên xe. Cargo của Core chỉ đổi theo kết quả nâng/hạ tương quan lệnh đang active; diagnostic DONE cũ không tự cập nhật cargo khi không còn lệnh active. Firmware `LiftCycle` nhận chu kỳ mới độc lập với cargo cũ, do đó thao tác đối soát thử có thể thực hiện tại server, không cần lệnh hay firmware MCU mới.

## Quyết định

- Nút ở bảng nhiệm vụ và khu đối soát đơn đã xóa ghi `Hoàn tác về K…` khi có lượt kho dang dở; xác nhận ngắn kệ đã về kho lấy và xe đã trống. Chỉ áp dụng cho đơn `needs_attention`. Các lượt đã có kết quả hạ xác nhận giữ kho giao, kể cả lỗi ở chặng ra cửa; tìm bằng move hoặc sự kiện dropoff bền vững nếu ca đã đóng và move đã được dọn. Đơn cũ chưa có thay đổi kho chỉ đóng bản ghi, không suy đoán mã kệ.
- HTTP POST `/api/orders/:id/resolve` nhận `physically_clear:true` và `expected_revision` của sổ kho đã nhập. Từ chối revision cũ, kho nguồn bị chiếm, mã kệ/vị trí không khớp, lượt khác còn giữ kệ/xe/kho hoặc dữ liệu move mất mà không có kết quả hạ. Gửi lại một đơn đã hoàn tác trả bản ghi đã đóng, không phát thao tác mới.
- Gateway chỉ đối soát cargo của xe có move cần hoàn tác; lượt đã hạ không xóa cargo của xe đang dùng cho lượt sau. Xe phải online, RFID hợp lệ, không có task/tuyến/nâng hạ active, không giữ kệ hoặc lượt chuyển khác; chờ Core xác nhận trước khi đổi sổ kho. WS v1 mới `reconcile_test_cargo` có batch tối đa hai `{id,lift_command_id}` và `physically_clear:true`. Core kiểm tra cả batch trước khi đổi: chỉ ID01/02 trong scope, đúng lift command hiện tại, telemetry/diagnostic mới (diagnostic ≤3s), diagnostic đúng boot/kết nối encoder, cùng node, motor STOP/duty0, không task/servo busy. Cho phép IDLE/CHARGING/ERROR đã dừng; giữ nguyên ERROR và vùng/path chưa xác minh.
- Core chỉ đặt `virtualCargoKnown=true, virtualCargo=false`, không gửi MQTT, không điều khiển servo/motor, không tự xóa lỗi hoặc nhả mutex. DONE cũ ở MCU không ghi đè trạng thái đối soát. Chu kỳ nâng/hạ mới tiếp tục được xác nhận theo hợp đồng cũ. Không sửa firmware hoặc thông số riêng hai xe.
- Sau xác nhận Core, gateway ghi cùng transaction SQLite: kệ trở về source của chính move; status stored; move rolled_back; sự kiện `rollback` với `evidence:operator_verified`; đơn closed có `resolution:rollback_to_pickup` và `rolled_back`. Các đặt chỗ của move hết hiệu lực. Lỗi ghi DB giữ nguyên đơn/sổ kho; Core có thể đã ghi nhận xe trống, người dùng gửi lại để hoàn tất. Revision đổi trong lúc chờ cũng giữ đơn cần đối soát và yêu cầu tải lại. Không tự replay lượt cũ.
- Báo cáo ca đã lưu vẫn bất biến; thao tác hoàn tác là nhật ký mới, không sửa lịch sử nâng/hạ hoặc cộng nhiệm vụ thành công. Nút chỉ thay dữ liệu; việc đưa kệ thật về nguồn/hạ cơ cấu do người giám sát thực hiện trước xác nhận.

## Kiểm chứng và giới hạn

Kiểm chứng bằng DB/Core giả, trình duyệt và Core unit cho cả01/02: xem [tests/README.md](../../tests/README.md). Build Core ở `.qa/rollback-core`, không ghi đè tiến trình vận hành. Cần dùng gateway/Core mới để chạy chức năng; Core cũ từ chối command mới và đơn/sổ kho vẫn giữ nguyên. Chưa chạy thao tác lên xe thật hoặc sửa dữ liệu vận hành. Khi có cảm biến hàng sẽ cần thiết kế lại nguồn bằng chứng và đối soát đầy đủ.
