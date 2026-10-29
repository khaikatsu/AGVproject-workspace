---
name: git-friend-push
description: Quy định bắt buộc khi push code lên Git: chỉ push sang remote của bạn (khaikatsu/AGVproject-workspace), luôn viết comment/commit message rõ ràng, chi tiết bằng tiếng Việt mô tả mọi thay đổi trong firmware và server.
---

# Kỹ năng Push Git lên Repo Bạn (khaikatsu)

## 1. Mục đích & Nguyên tắc bất biến
- **Chỉ push lên repository của bạn**: `https://github.com/khaikatsu/AGVproject-workspace.git` (trong `HETHONG_AGV` remote là `origin`, hoặc remote `friend` ở root nếu có).
- **Tuyệt đối KHÔNG tự ý push lên bất kỳ repo nào khác** (như `mrbitvn169/NCKH_2025`).
- **Chỉ thực hiện khi người dùng yêu cầu rõ ràng**: ("push lên git", "push code").
- **Luôn viết commit message đầy đủ, rõ ràng và có tâm**:
  - Tóm tắt ngắn gọn ở dòng tiêu đề (conventional commit: `feat(...)`, `fix(...)`, `refactor(...)`).
  - Phần body liệt kê chi tiết từng file, từng module server và firmware đã chỉnh sửa những gì, lý do thay đổi và cách hoạt động mới.

## 2. Quy trình thực hiện
1. Kiểm tra trạng thái git:
   ```powershell
   git status
   git diff
   ```
2. Thêm các file thay đổi (chú ý bỏ qua các artifact tạm hoặc file nhị phân không cần thiết):
   ```powershell
   git add <cac_file_thay_doi>
   ```
3. Tạo commit với nội dung chi tiết:
   - Liệt kê các chỉnh sửa ở Gateway / Webserver (APIs, timeout, reconcile).
   - Liệt kê các chỉnh sửa ở C++ Core (`NodeManeuver.hpp`, mutex, etc.).
   - Liệt kê các chỉnh sửa ở Firmware ESP32 (`Config.h`, `CoreNavigator`, `RoutePlanner`, `CoreProtocol`).
4. Push lên đúng nhánh `main` của repo `khaikatsu/AGVproject-workspace`:
   ```powershell
   git push origin main
   ```
5. Báo cáo lại cho người dùng commit SHA và nội dung đã commit.
