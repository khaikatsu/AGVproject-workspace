# ADR-024 — Chỉnh nhẹ khi xe 01 lệch line

Ngày 08/10/2026. Trạng thái: đã build và kiểm thử logic; chưa nạp hoặc kiểm chứng chuyển động thật.

## Nguồn và hiện trạng

`SOURCE`: người dùng yêu cầu xe tự chỉnh để trở lại line khi lệch nhẹ. `OBSERVED`: firmware trước đó coi `110`/`011` giống `100`/`001`, dừng hẳn một bánh khi chỉnh; việc này có thể làm xe đổi hướng quá mạnh. Hướng chỉnh trái/phải hiện dùng mapping đã chọn sau các phép thử xe 01, nhưng chưa được xác nhận lại với vạch thực hiện tại.

## Quyết định

- Chỉ env `esp32dev_trial`: `110` dùng `GentleCorrectLeft` với duty trái/phải danh định 210/140; `011` dùng `GentleCorrectRight` với 130/220. Giữ cả hai bánh tiến, dựa trên mapping chỉnh trái/phải hiện có. Khi mắt giữa thấy lại line (`010`), trở về tốc độ thẳng 210/220.
- `100`/`001` vẫn chỉnh mạnh một bánh; `000` vẫn dừng motor ngay và giữ xác nhận mất line 120 ms. Chỉnh khi lùi, pivot, GPIO, cực tính motor, map và bản thường không đổi.
- Chưa nạp firmware vì thay đổi tốc độ tác động lên xe thật; cần thử bánh trên giá kê và xác nhận chiều kéo về line trước khi chạy ngắn có giám sát.

## Kiểm chứng và mở

`VERIFIED`: `native_trial` 8/8 test PASS, `native` 54/54 PASS, build `esp32dev_trial` PASS. `OPEN`: nạp bản mới khi người dùng sẵn sàng, đo mẫu mắt/duty và quan sát xe có tự bắt lại line hay không; nếu hướng chỉnh sai hoặc lực ở duty 130–140 không đủ, dừng và hiệu chỉnh theo quan sát thực, không tăng tốc tùy đoán.
