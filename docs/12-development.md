# Hướng dẫn phát triển và kiểm tra workspace

## Mở workspace

Mở `AGVproject.code-workspace` hoặc thư mục gốc. Tài liệu UTF-8, kỹ năng tại `.agents/skills`. Markdown là nguồn của hai bản Word trong `docs/exports`; những bản yêu cầu Word nhóm bổ sung được coi là nguồn riêng và cần ghi vào sổ yêu cầu.

```powershell
python scripts/check_workspace.py
```

Script chỉ đọc, kiểm tra đường dẫn nội bộ Markdown, JSON fixture/schema, sáu kỹ năng và hai Word. Nó không thay thế build, test robot hoặc visual QA của Word. Trên máy chưa có Python, chọn runtime Python đã cài hoặc bundled runtime của Codex.

## Chạy ứng dụng hiện có khi triển khai

Đã sửa include theo đúng tên header và thêm scripts/build-core.ps1. Hướng dẫn và kết quả kiểm tra bản mới ngày 29/09/2026 nằm tại [web/server cập nhật](14-web-server-update.md). Các bước dưới đây giữ quy ước môi trường nền; luôn dùng broker và DB riêng khi kiểm thử.

1. Chuẩn bị toolchain C++17, OpenSSL và libmosquitto; xác định Windows hay Linux.
2. Build entrypoint `src/AGVserver.cpp`; không compile mọi `.cpp` vào một executable vì có nhiều main và `AGVtest.cpp` chứa JavaScript.
3. Chuẩn bị broker thử riêng và cấu hình đúng network; core hiện nối local 1883.
4. Chạy `./scripts/run-core.ps1 -Build` từ gốc workspace. Script build executable mới và đặt working directory về gốc để đọc `data/map_layout.json`.
5. Trong terminal gateway, dùng lockfile để cài dependency khi cần, rồi chạy:

```powershell
Set-Location AGVwebserver
$env:AGV_WS_URL = 'ws://127.0.0.1:8080'
$env:PORT = '3000'
npm start
```

Gateway mở `AGVwebserver/agv.db` khi khởi tạo ứng dụng; import module để test không mở DB. Có thể đặt `AGV_DB_PATH` tới file riêng. Chuẩn bị bản test độc lập trước khi chạy nếu cần bảo toàn dữ liệu đang có. Không sao chép riêng file `.db` của phiên đang chạy rồi coi backup nhất quán; SQLite WAL cần cơ chế backup/checkpoint đúng thời điểm.

Endpoint xem health khi đã chủ động khởi chạy môi trường test: `http://localhost:3000/api/health`. Không gửi API điều khiển chỉ để kiểm tra server sống.

Chỉ chạy một core và một gateway cho đội xe. `EADDRINUSE` ở port 3000 nghĩa là gateway khác đang giữ cổng; dừng lần chạy cũ trước khi chạy lại. Mở `AGVproject.code-workspace`, chọn cấu hình **AGV core (workspace root)** rồi F5 nếu cần debug; cấu hình build đúng `src/AGVserver.cpp` và chạy `build/agv_core.exe` từ gốc, thay cho executable cũ trong `src`. Xem [chẩn đoán khởi động](16-startup-troubleshooting.md).

## Xuất Word

```powershell
python scripts/export_documents.py
```

Yêu cầu Python với `python-docx`. Trong Codex, dùng đường dẫn Python do `load_workspace_dependencies` trả về. Script xuất hai tài liệu từ `docs/00-project-brief.md` và `docs/requirements/template.md`, không dùng system runtime thay cho bundled runtime trong quy trình artifact của Codex.

Sau khi xuất, render Word thành ảnh bằng renderer của kỹ năng documents rồi kiểm tra từng trang. Nếu LibreOffice không có trên Windows, có thể dùng Word đã cài để xuất PDF trong môi trường QA rồi dùng Poppler render ảnh. Ghi rõ renderer thực tế và giới hạn nếu không render được; không báo đã kiểm tra bố cục chỉ từ XML.

## Kiểm tra kỹ năng

Các `SKILL.md` có frontmatter name/description và `agents/openai.yaml`. Khi sửa skill, dùng `quick_validate.py` của skill-creator nếu có sẵn; đồng thời kiểm tra link về tài liệu và đảm bảo chỉ dẫn không đổi ngầm hợp đồng.

Workspace chưa có Git repository tại khảo sát. `.gitignore` chuẩn bị cho lần quản lý phiên bản sau; chưa init, commit hoặc push repo trong công việc này.
