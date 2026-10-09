# Kiểm thử và nghiệm thu

Đây là kế hoạch, chưa có kết quả đo hoặc robot thật. Mỗi ca ghi phiên bản mã, bản đồ, phần cứng/simulator, input, kết quả mong đợi, log thực tế và kết luận. Dùng DB/broker riêng cho thao tác thay đổi trạng thái.

| ID | Yêu cầu liên quan | Ca kiểm thử | Bằng chứng cần có |
| --- | --- | --- | --- |
| T-01 | SRC-01 | Hai robot đi lộ trình định trước | Log node đích và quan sát chuyển động thật |
| T-02 | SRC-02 | Web cập nhật vị trí/pin/lỗi | Video hoặc snapshot cùng log thời gian và telemetry |
| T-03 | SRC-03 | Map/lộ trình hiển thị đúng | Đối chiếu node, hướng cạnh, scale và bản đồ thực |
| T-04 | PROP-01 | Mất ACK và gửi lại command | Một hành động, nhiều ACK hợp lệ, không chạy lặp |
| T-05 | PROP-02 | Hai robot xin cùng zone | Timeline owner/grant/release, không cùng chiếm |
| T-06 | PROP-02 | Offline khi còn trong zone | Vùng xử lý theo policy, không cấp vì timeout đơn thuần |
| T-07 | OPEN-06 | Reconnect/restart core và MCU | Command phiên cũ/mới được xử lý rõ ràng |
| T-08 | GAP-01 | Save/apply snapshot | Map core đúng revision; sai dữ liệu bị từ chối |
| T-09 | GAP-04 | Hủy task/lỗi/đổi đích | Không lưu nhầm hoàn thành; task ID xuyên suốt |
| T-10 | OPEN-01 | Vật cản và dừng tại robot | Quãng dừng đo thật trong cấu hình test |

## Chỉ tiêu chưa chốt

- Độ trễ từ telemetry tới web: cần mốc đo ở producer và consumer, thống nhất clock hoặc nêu sai số.
- Độ trễ nhận lệnh và ACK: ghi p50/p95/max, packet loss và điều kiện mạng.
- Thời gian hoàn thành, tỷ lệ hoàn thành và số lần chờ/replan.
- Thời gian hoạt động pin, sai số pin và phục hồi sau mất liên kết.
- Khoảng dừng và kích thước vùng chiếm chỗ theo tốc độ/tải thực.

Không dùng nhịp broadcast 700 ms như chỉ tiêu realtime đã đạt. Không coi mô phỏng S1 là kiểm thử tránh va chạm vật lý. Test được chọn theo thay đổi; sửa tài liệu không yêu cầu chạy cả hệ thống.

Mẫu ghi kết quả: [tests/README.md](../tests/README.md). [Sổ yêu cầu](requirements/register.md) lưu trạng thái các chỉ tiêu.

## Kiểm thử đã triển khai 29/09/2026

Đã có test C++ logic, gateway REST/SQLite với mock core, browser và tích hợp core C++/broker riêng. Xem [hồ sơ kiểm thử](../tests/README.md) để biết lệnh chạy và giới hạn bằng chứng. Các tiêu chí nghiệm thu robot thật phía trên vẫn chờ số đo thực nghiệm.
