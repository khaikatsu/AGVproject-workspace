# ADR-010: Clone dashboard với hai robot MQTT ảo, dùng Core thật

- Ngày: 02/10/2026.
- Trạng thái: Accepted trong phạm vi yêu cầu mô phỏng trực tiếp của người dùng.
- SOURCE: clone web từ workspace, giữ chức năng/thuật toán, thêm pin/lấy trả hàng/giao riêng hai xe và số liệu thời gian/chi phí quãng đường.
- OBSERVED: Core quyết định tuyến, task, pin, ca và mutex; firmware hiện hành chỉ test kết nối, chưa chạy motor thật.

Clone sinh tại `/simulation` từ `AGVwebserver/index.html`; dùng đúng asset và gateway gốc, bổ sung CSS/JS/panel riêng. Không sửa file giao diện vận hành hay code thuật toán. Phục vụ trên 3100, gọi Core C++ binary trên cổng loopback riêng. Mỗi phiên có broker MQTT, map, SQLite, counter và checkpoint độc lập. Runner loại cấu hình AGV vận hành khỏi environment con; không gửi lệnh vào 1883/8080/3000.

Hai client MQTT 3.1.1 đóng vai robot 01/S1 và 02/S2. Telemetry pipe, command JSON/ACK và grant generation/ACK/release giữ [hợp đồng v0.1](../../contracts/examples/current-mqtt.json). Xe chỉ đi theo path Core gửi, xin token trước vào mutex và trả token sau khi đến node ngoài zone. Grant/ACK không thay kết quả task. Không nhả vì offline, hết pin hoặc STOP giữa cạnh. Nội suy trên cạnh chỉ phục vụ mô phỏng, vẫn giữ dấu RFID gần nhất từ Core.

Lấy/trả là chuỗi task simulator theo dõi bằng ID Core, không thêm loại task mới vào Core. Task bị hủy/trả queue/đổi xe, lỗi mạng hoặc kết thúc ca làm chuỗi interrupted; hàng không được báo trả thành công. Vì server hiện có thể phân công lại task khi pin thấp, đây là khác biệt cần quan sát giữa điều hướng và vận chuyển hàng khi nhóm mở rộng nghiệp vụ về sau, chưa tự thay thuật toán theo đề xuất đó.

Quãng đường từ tọa độ giả định cm, chi phí từ weight, thời gian từ clock thực/mô phỏng, chờ mutex từ mô hình robot. Log input/output, route plan, map và kết quả lưu JSONL; snapshot/report JSON riêng. S1 không chứng minh tối ưu toàn đội, va chạm hình học, vật lý phanh, chi phí tiền hoặc hoạt động MCU thật. Thử nghiệm tốc độ cao chịu độ phân giải tick/poll; việc dựng ROS/S2/S3 là PROPOSED ngoài phạm vi hiện tại.

Kiểm chứng 02/10/2026: real Core/gateway/MQTT với hai xe, shift/home/low pin/sạc, ACK retry/dedup, cargo và empty failsafe/offline giữ mutex; fixture tranh M5 chứng minh FIFO/quyền độc quyền và cost khác distance; browser desktop/laptop/mobile thao tác cargo/empty/recover/report. Source, map và counter vận hành được hash trước/sau test, database simulator ở thư mục fixture riêng. Không dùng hash DB vận hành đang được gateway 3000 ghi độc lập làm tiêu chí ổn định của test. Lệnh chạy và giới hạn tại [simulation README](../../simulation/README.md), [tests](../../tests/README.md).
