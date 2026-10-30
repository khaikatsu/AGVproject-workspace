# ADR-098 — Một lần bấm hoàn tác trên sổ kho

Ngày 30/10/2026. SOURCE: người dùng yêu cầu bấm Hoàn tác là hàng dang dở trở về kho, không cần đối soát; xác nhận rõ đây là hoàn tác trên sổ kho. Cargo theo dõi tình trạng hàng. Quyết định này thay quy trình hoàn tác/xác nhận cargo của ADR-089 và nhánh mở form đối soát cho đơn thiếu move.

## Quyết định

- Nút **Hoàn tác về K…** gọi API ngay, không hộp xác nhận hoặc form đối soát. Áp dụng chung xe01/02, gồm đơn đã ẩn và đơn closed cũ còn move gián đoạn. Chỉ hoàn tác đơn gián đoạn, không dùng để hủy đơn đang chạy.
- `POST /api/orders/:id/resolve` nhận `expected_revision`; trường `physically_clear` cũ không còn bắt buộc. Không cần Core/MQTT, RFID, trạng thái xe hoặc cargo để sửa sổ. Không gửi `reconcile_test_cargo`, lệnh MQTT hay điều khiển xe/servo. Cargo cảm biến và cargo ảo của Core giữ giá trị quan sát; hoàn tác không xác nhận xe trống hoặc hoàn thành vận chuyển thật. Chốt điều khiển/nâng hạ/mở ca hiện có nằm ngoài phạm vi thao tác sổ này.
- Trong cùng transaction SQLite: đưa kệ chưa giao về đúng kho lấy, bỏ reservation, đánh dấu move `rolled_back`, đóng đơn với `resolution:rollback_to_pickup`, `rollback_scope:inventory_only`, `physical_completion_verified:false`. Event ghi `evidence:operator_requested`, `scope:inventory_only`, không ghi `operator_verified` hoặc bằng chứng xe trống. Retry đơn đã hoàn tác trả kết quả cũ.
- Đơn cũ mất move có thể dùng mã kệ/xe/kho lấy đã lưu trong item cùng vị trí sổ kho hiện tại để hoàn tác. Chỉ chấp nhận kệ đang ở nguồn hoặc trên đúng xe; ghi move kết thúc có `legacy_reconstructed:true`. Không tạo lại mã kệ thiếu, không chuyển kệ đang ở vị trí mới của lượt sau. Event `dropoff`/`missing`/`rollback` của chính item chặn việc suy hoàn tác lại khi move đã được dọn.
- Giữ kiểm tra revision, kho nguồn bị chiếm, kệ/vị trí không khớp, trùng kệ/kho nguồn hoặc reservation của lượt khác. Lượt đã giao/hạ được xác nhận giữ kho giao. Kệ có vị trí unknown/move missing giữ nguyên ghi nhận thiếu hàng. Báo cáo ca cũ và lịch sử task giữ nguyên; đóng đơn không cộng một chuyến thành công.

## Kiểm chứng

Bổ sung30/10/2026, lỗi đơn mới chờ đội xe phục hồi: resolve ghi `fleet_recovery_hold:false` cùng lần lưu đóng đơn. Scheduler và trạng thái web dùng chung điều kiện, chỉ tính cờ của đơn active/needs_attention chưa được đối soát và chưa kết ca; cờ còn sót trong đơn closed cũ không giữ đội xe. Không sửa bản ghi legacy để bỏ chốt, không bỏ chốt lỗi/offline/task/cargo thực tế của xe. Kiểm thử orders/gateway50/50 PASS với Core giả và SQLite riêng, gồm01/02, đơn đã hoàn tác, closed cũ, lỗi xe thật và đơn chưa giải quyết đã ẩn. Chưa restart gateway vận hành trong lượt sửa vì xe01 còn task đang chạy; không sửa firmware/Core hoặc gửi lệnh xe.

VERIFIED30/10/2026: orders/inventory43/43 và gateway24/24 PASS. Có kiểm tra transaction lỗi ghi event/đơn, revision cũ, nguồn bị chiếm, giữ giao đã xác nhận/báo cáo cũ, đơn thiếu move cho01/02 khi Core không có, retry và không tái tạo hàng thiếu. Browser Edge headless desktop/mobile PASS một lần bấm/không dialog/không command Core/cargo giữ nguyên, xe02 offline, đơn đã ẩn quá50 mục và restart gateway/DB riêng. Bằng chứng `.qa/warehouse-rollback-iV59LM`, log `.qa/ledger-undo-unit.log`, `.qa/ledger-undo-gateway.log`, `.qa/ledger-undo-browser.log`.

Không sửa firmware, build/nạp MCU, restart dịch vụ hoặc sửa DB vận hành trong lượt này. Cần restart gateway Node.js đang chạy và tải lại web để áp dụng source mới; không cần build Core.
