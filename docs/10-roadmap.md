# Lộ trình triển khai sau khi có yêu cầu

Thời gian đề tài trong phiếu: tháng 2 đến tháng 10/2026. Không suy ra tiến độ hiện tại. Các giai đoạn dưới đây là thứ tự phụ thuộc đề xuất; nhóm tự bổ sung ngày và người phụ trách.

| Giai đoạn | Đầu vào | Công việc | Điều kiện chuyển tiếp |
| --- | --- | --- | --- |
| P0 Chốt yêu cầu nền | Phiếu đề tài, mô tả mới, mã có sẵn | Sổ yêu cầu, BOM, bản đồ, tiêu chí đo | Những lựa chọn ảnh hưởng module tiếp theo đã rõ |
| P1 Chuẩn hóa tích hợp | Toolchain và hợp đồng v0 | Build core, sửa case header, phân loại AGVtest, xử lý load_map_data | Core/gateway chạy bằng môi trường test tái lập |
| P2 Robot giả lập | Broker riêng, map test | S1 hai robot, ACK, telemetry, lỗi mạng | Web/core/simulator đi hết luồng task và phục hồi |
| P3 Thuật toán hai xe | Hình học kho, vùng chung | Xung đột cạnh, mutex, offline, trạm sạc | Các tình huống xung đột có log kiểm chứng |
| P4 Firmware và phần cứng | BOM/pinout/chính sách dừng | Sensor, motor, MQTT, watchdog, bench test | Một robot thật qua ca kiểm thử bàn và tuyến |
| P5 Tích hợp hai robot | Các phần trước đạt yêu cầu | Điều phối hai xe, pin/lỗi, demo và đo | Bằng chứng theo tiêu chí nghiệm thu đã chốt |
| P6 Báo cáo | Log, kết quả, ảnh, nguồn | Phương pháp, số liệu, giới hạn, tài liệu bàn giao | Báo cáo đúng mẫu học viện do nhóm cung cấp |

Mô phỏng ROS 2/Gazebo là nhánh bổ sung khi S1/S2 không trả lời được câu hỏi cần nghiên cứu. Không cài hoặc chuyển platform chỉ để hoàn thành bộ nền.

## Công việc có thể bắt đầu tiếp theo

1. Nhập yêu cầu phần cứng và bản đồ vào [inbox](requirements/inbox/README.md).
2. Ưu tiên GAP-01/GAP-02/GAP-03 để có đường build/tích hợp rõ.
3. Chốt chính sách robot offline trong zone và ID/session trước chạy hai xe.
4. Tạo simulator đúng hợp đồng rồi kiểm thử vòng task.
