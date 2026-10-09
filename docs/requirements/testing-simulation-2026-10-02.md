# Kết quả web clone mô phỏng 02/10/2026

REQ-SIM-01, SOURCE người dùng: clone từ workspace, hai xe ảo, giữ thuật toán/chức năng, thêm injection pin, lấy/trả hàng, giao riêng từng xe, quan sát thời gian/quãng đường/chi phí. Mô hình và hướng dẫn: [simulation](../../simulation/README.md), [ADR-010](../decisions/ADR-010-isolated-simulation.md).

OBSERVED — chạy trên Windows với Node v24, Core `build/agv_core.exe` đã build từ source hiện có, Mosquitto riêng và Edge headless/Playwright:

| Bộ kiểm tra | Kết quả cuối | Bằng chứng |
| --- | --- | --- |
| `node tests/simulation.robot.test.js` | PASS | Dedup/same-ID khác payload, cache/stale, ID int32, empty path tại node, cạnh không có/sạc ngoài trạm, trả grant chưa dùng nhưng giữ khi đang vào vùng, low pin tại RFID/zero dừng |
| `node tests/simulation.integration.js` | PASS | `.qa/run-oSiHRO`; DB/map/counter/broker riêng; hai trạm/ca, retry ACK không lặp đường, 2 chuỗi hàng, pin thấp/sạc, hết pin/clear-error, LWT giữ M5/phục hồi, kết thúc ca về trạm |
| `node tests/simulation.mutex.js` | PASS | `.qa/simulation-mutex-5RRwH3/run-OENrPl`; FIFO M5 hai xe, xe 02 chờ tại A2, chỉ nhận quyền sau xe 01 trả, cả hai task completed; cost35/41 khác khoảng đường |
| `node tests/simulation.browser.js` | PASS | `.qa/run-Usc8sG`; snapshot homes/moving/laptop/mobile, 2 marker ảo, cargo, giao riêng xe02, 0%/recover, báo cáo download, không có pageerror/overflow ngang mobile |
| Script mở/dừng | PASS | Phiên `simulation/runs/run-86lXnV`: web3100, Core64496/MQTT64495, 01S1/02S2; phát hiện đã chạy không spawn trùng; stop lưu report và đóng ba tiến trình mô phỏng |
| `python scripts/check_workspace.py` | PASS tài liệu | 50 Markdown/248 link ở lần chạy trước thêm báo cáo này; JSON Schema chưa có module, không dùng kết quả này để xác nhận runtime/hardware |

Trong quá trình chạy đã sửa lỗi model giữ grant chưa vào sau hủy tuyến/grant đến trễ. Mô hình chỉ trả quyền khi biết xe điểm chưa chiếm vùng; không sửa MutexArbiter hoặc nhả vùng đang chiếm khi mất mạng. Test hash mã/map/counter vận hành trước/sau; DB vận hành đang có gateway3000 ghi độc lập nên chỉ xác nhận DB simulator nằm ngoài đường dẫn đó, không yêu cầu hash DB live đứng yên.

OPEN: robot/motor/RFID/sạc vật lý, va chạm thân xe, độ chính xác vận tốc/hao pin, thời gian phần cứng và chứng minh tối ưu toàn đội. Khoảng đường giả định cm, chi phí chỉ là trọng số; tăng hệ số thời gian không thay timeout thật của Core. Lưu đầy đủ input/output JSONL để đọc lại sự kiện, chưa có replay deterministic Core/clock/network.

## Sửa tình huống pin15 sau hết pin/mất mạng

SOURCE: người dùng báo xe01 bấm15% không về sạc. OBSERVED trong log phiên `run-sdHCKO`: sau task K4→K6, đã bấm0% trước15%; Core gửi STOP và giữ ERROR. Sau đó reconnect/clear-error từ giao diện gốc có GOTO về S1 nhưng simulator vẫn giữ pause/failsafe. UI cũ chỉ báo nhận thao tác nên thiếu hướng dẫn phục hồi. Đây là vấn đề đồng bộ trạng thái và phản hồi của mô phỏng, không thay thuật toán Core.

IMPLEMENTED: nút pin trả message/warning theo trạng thái; hiển thị chỉ dẫn cụ thể; **Phục hồi xe / RFID** tự reconnect trong mô hình, đợi telemetry/RFID/pin và heartbeat mới, clear-error được Core chấp nhận rồi mới bỏ pause. Không auto-clear lỗi bằng pin15 hoặc GOTO. Cờ charging_done reset khi lệnh/chu kỳ sạc mới để không kẹt ở lần sạc tiếp theo.

VERIFIED: `node tests/simulation.low-battery.js` PASS, `.qa/run-07XCOp`: K6→S1 tại15%, lặp sạc tại trạm, 0→15 còn giữ dừng trước phục hồi, pause/resume và offline/reconnect/phục hồi. Unit robot và browser regression cũng PASS. Nhật ký phiên người dùng được lưu khi khởi động lại riêng simulator; mã/dữ liệu vận hành không được sửa.
