# ADR-090 — Kết ca sạch, phục hồi nhiệm vụ và kho nhị phân

Ngày 28/10/2026. SOURCE: người dùng duyệt mục 1, 2, phục hồi S2, giao diện nhị phân và điều kiện mutex trong đánh giá cùng ngày. Trạng thái: IMPLEMENTED trong source; gateway/browser đã kiểm chứng, Core nguồn cuối chưa build lại.

## Quyết định

- Bổ sung REQ-RECONCILED-ORDER-HOLD-20261028: tách hold vật lý chưa đối soát khỏi bản ghi lịch sử ca đã kết. Nếu đơn needs_attention có closed_by_shift, thời điểm shift_closed_at (legacy dùng updated) trước một event initialized/reconciled có evidence=operator_verified và không còn move reserved/carrying/needs_attention của đơn, scheduler bỏ hold cũ. List/HTTP/WS trình bày đơn này như closed với resolution=warehouse_reconciled, closed_at và warehouse_reconciliation={event_id,created_at}; DB lịch sử giữ trạng thái gốc, không migration/ghi lại lúc khởi động, không đụng cargo hoặc phát MQTT. Giữ mốc shift_closed_at qua thao tác ẩn để xóa giao diện không thay hiệu lực bằng chứng. Thiếu bằng chứng, cùng timestamp, đơn chưa kết ca hoặc còn move mở thì vẫn khóa. nearest hiển thị nguyên nhân cụ thể, gồm ID đơn kể cả đã ẩn. Restart gateway vẫn không replay đơn active/queued cũ.

- Bổ sung 28/10, REQ-WAREHOUSE-FAULT-RECOVERY-20261028: phục hồi lỗi tại K1–K6 cho cả01/02 không đòi heading hoặc task treo. Giữ ca RUNNING, xác nhận vật lý, RFID≤250ms, telemetry≤1000ms, diagnostic dừng≤1500ms đúng boot/kết nối, PWM0/0, hết task/tuyến/nâng hạ; không áp quy tắc nới này cho node giữa tuyến. Chỉ gửi RESET_FAULT khi MCU còn lỗi và chờ diagnostic khỏe sau lệnh đúng high-water trước IDLE. Không tự gán hướng, node, cargo hoặc tạo quyền tiếp tục nhiệm vụ đã hết hiệu lực. Resume yêu cầu heading hợp lệ mới trước GOTO; thiếu heading báo rõ thay vì giao tuyến rồi đứng chờ NODE_ORDER. Web bỏ câu yêu cầu chỉnh hướng trong xác nhận phục hồi chung, nút ghi node đang báo.

- Kết ca chặn nhận việc, hủy hàng đợi và công việc đang treo của Core/gateway, vô hiệu quyền tiếp tục. Giữ lịch sử, counter, tồn kho thực tế và footprint chưa xác minh; WAIT cần proof dừng. Kệ còn trên xe hoặc chu kỳ lift bị ngắt chưa rõ kết quả phải đối soát trước khi tự về trạm/mở ca. Ca sau không chạy lại công việc cũ.
- Đối soát gom lỗi, task, mutex và tồn kho. Chỉ xác nhận các lỗi trong bản chụp đã xem, giữ audit; lỗi mới hoặc trạng thái vật lý chưa xử lý vẫn hiện.
- Lỗi trong ca giữ đích, xe, pha và RFID cuối để tiếp tục bằng lệnh mới sau xác minh dừng tại đúng RFID. Không tự phát lại servo, không tự tiếp tục sau restart/kết ca hoặc khi cargo chưa rõ.
- RFID mới cập nhật vị trí ngay cả khi ERROR. S1/S2 có diagnostic dừng mới, đúng boot/kết nối sẽ thu hồi các vùng cũ; xác nhận thủ công cần cùng điều kiện dừng và xác nhận thân xe đã ra hết các vùng. RESET_FAULT vẫn chờ MCU khỏe.
- Phân biệt ưu tiên né tuyến dự kiến với mutex thực. Chỉ cấp mutex kế tiếp phù hợp tuyến hiện tại; chưa xác minh thì không nhả vùng vì timeout. Proof dừng node/cạnh theo ADR-079 được giữ.
- Mỗi K1–K6 có sức chứa một kệ: Trống hoặc Có kệ + mã kệ. API đơn hàng từ chối quantity khác 1; kết quả servo đúng lệnh cập nhật kho↔xe duy nhất một lần. Chưa triển khai kho nhiều kệ.
- OPEN: bỏ “trạng thái tin cậy” khi lấy hàng. Giữ nguyên các chốt RFID/lift/command/session/cargo tới khi xác định nguyên nhân trễ.

## Kiểm chứng

Bổ sung REQ-WAREHOUSE-FAULT-RECOVERY-20261028: hồi quy server `shift_recovery.cpp` PASS cho01/02 và K1–K6, gồm kho thiếu heading/task, proof không hợp lệ, ACK/command high-water và giữ chốt hướng khi resume giữa tuyến. Build Core source cuối PASS tại `.qa/warehouse-recovery-20261028/agv_core.exe`; chưa thay Core vận hành hoặc thử xe thật, firmware không đổi. Chi tiết/hash ở [hồ sơ](../../tests/README.md). Các đoạn dưới giữ mốc kiểm chứng của lượt triển khai ban đầu.

Core nguồn cuối có chốt kệ treo trước khi về trạm/mở ca; chốt này chưa được build lại.

Gateway/kho64/64 PASS, gồm lỗi mới khi resume lần hai trùng event_key đã sửa, stale token đối soát, chặn tồn kho/cargo sai và đóng queue sau restart/kết ca. Browser form kho desktop/mobile và luồng S2→resume→kết ca→hoàn tác→đối soát PASS với Core giả, DB/cổng riêng. [Kết quả và đường dẫn bằng chứng](../../tests/README.md).

Binary kiểm thử Core01/02 đã chạy PASS ở một bản trung gian. Sau đó source siết thêm cửa sổ xin mutex và chặn diagnostic cũ mở IDLE trong lúc RESET_FAULT đang chờ. Bộ duyệt chặn build; người dùng chọn **Chỉ kiểm thử đã biên dịch**, nên không build lại source cuối hoặc chạy lại test C++ đã cập nhật. Binary core_tests cũ dừng ở kỳ vọng cũ “end_shift vẫn giữ đích K3”; source kiểm thử đã sửa theo yêu cầu hủy task, chưa biên dịch lại. Đây không phải bằng chứng Core cuối đã PASS.

Chưa thay executable/dịch vụ vận hành, chưa thử xe thật. Firmware không sửa/build/nạp. Việc xác nhận thủ công tại S1/S2 vẫn đòi firmware đọc đúng RFID và phát STOP mới; chưa có lệnh MCU để đặt vị trí bằng click. Kho nhiều kệ/cảm biến tải và mục bỏ “trạng thái tin cậy” vẫn OPEN.
