# ADR-045 — Thử lấy/hạ tại kho, chưa quản lý đơn hàng

15/10/2026: [ADR-057](ADR-057-robot01-servo-lift-test.md) bổ sung một servo GPIO17 và kiện ảo cho test01 có capability mới: chờ nhúng hoàn tất chu kỳ5s thay vì timer gateway, sau đó mới lùi gate. Không tạo đơn hay xác nhận hàng vật lý. Firmware cũ vẫn giữ luồng mô phỏng dưới đây với nhãn rõ.

Ngày10/10/2026. Source và kiểm thử hoàn tất; chờ gateway nạp lại.

`SOURCE`: người dùng đính chính xe luôn không có đơn hàng thực, chưa làm chức năng đơn; yêu cầu chỉ tới kho lấy/hạ, đứng chờ5s, lùi gate rồi xoay theo hướng của chặng tiếp. Yêu cầu mới thay riêng cách dùng đơn hàng/giữ xe trong chế độ test01 của ADR-043/044. Không cần đóng/đối soát bản ghi đơn cũ để thử.

## Hành vi

- Thêm hai nút Lấy hàng/Hạ hàng, chọn khoK1–K6, mặc định01 trong Core mode robot01. GUI test01 ẩn form/lịch sử đơn hàng; APIorder từ chối tạo đơn và scheduler không phát các đơn queued cũ trong mode này. Lịch sử giữ nguyên.
- POST `/api/agvs/01/warehouse-action` body `{action:"pickup"|"dropoff",warehouse:"K5"}`. Không tạo order hoặc gán kiện/cargo.
- Core GOTO tới kho; chỉ sau taskcompleted + RFID đúng kho/IDLE mới chờ5000ms. Hết5s khi xe vẫn đứng đúng kho thì GOTO gate tương ứng; firmware/Core hồi sáng đã có REVERSE kho→gate. Chờ taskcompleted + RFIDgate/IDLE mới báo kết thúc thao tác.
- Tại gate, xe đứng chờ thao tác/chặng tiếp. Core NODE_ORDER tính pivot từ hướng hiện tại và tuyến tiếp; không tự xoay khi chưa có hướng đích.
- Chỉ thao tác đang thực hiện hoặc trạng thái Core thật làm xe bận. Lịch sử đơn cũ không giữ xe; lỗi/mất mạng/đổi phiên/rời kho trong lúc chờ vẫn ngắt thao tác. ACK/hết timer không được dùng để xác nhận đã tới gate.
- Action history tối đa20 trong RAM, không replay sau gateway restart. Core taskhistory vẫn dùng task_id. Snapshot thêm warehouse_actions; APIhealth thêm warehouse_test_actions=true để nhận biết backend mới.

## Kiểm chứng

`VERIFIED`: unit orders/gateway/security/warehouse30/30 PASS, gồm thứ tựtask/RFID5s/gate, actualbusy, Core lỗi/đổi phiên/saiRFID, scheduler không xử lý đơncũ mode01.

`VERIFIED`: Edge/mobile qua hai nút PASS, cả khi DB có đơn giấy cũ needs_attention: K5/5s/G5 rồiK3/5s/G3, không thêm order và không pageerror; `.qa/warehouse-actions-browser-UxM41F/warehouse-actions-mobile.png`.

`VERIFIED`: `tests/warehouse-actions.integration.js` dùng chính Core hồi sáng `--mode robot01`, broker/map/DB/counter/xeảo riêng ởTemp; hai thao tác đầy đủ,5s thời gian thực mỗi kho, cuối tạiG5/G3, APIorders luônrỗng. `.qa/warehouse-actions-integration-3kfEUf/verification.json`. Firmware native20/20 đã kiểm chứng reverse/pivot; hash binary Core/firmware giữ nguyên.

`OPEN`: gateway thật chưa nạp source mới; restart tự động trước đó bị auto-review chặn `blocked by policy`. Script người dùng chạy `.qa/apply-warehouse-actions.ps1` chỉ kiểm tra không tuyến/task thật, restart riênggateway và xác minh feature/CoreReady; không đóng order, không gửi GOTO. Script cũ apply-five-second-cargo được đổi thành alias sang script này để không thực hiện quyết định đóng đơn đã bị thay thế. Chưa thử motor/RFID trên xe thật theo hai nút mới.
