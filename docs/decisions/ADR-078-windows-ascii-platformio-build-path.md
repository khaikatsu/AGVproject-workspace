# ADR-078 — Đường dẫn build PlatformIO không dấu trên Windows

Ngày23/10/2026. Phạm vi: công cụ biên dịch trên máyWindows hiện tại, không đổi hànhvi robot.

## Nguồn và bằng chứng

Người dùng gửi log môi trường02 báo `xtensa-esp32-elf-g++` khôngmởđược `tmp*.tmp` tại đường dẫn dựán có dấu `D:/Nghiên cứu khoa học/AGVproject-workspace-main/.../.pio/build/...`; đường dẫn hiểnthị ký tự sai, tiếp theoInvalidargument/noinputfiles. TênServoModule.cpp.o/AGVController.cpp.o chỉ là object đang bị chặncompile, log chưa chỉra lỗi logic tronghai fileđó.

## Quyết định

- Giữ project ở thưmục người dùng. Trong `[platformio]`, đặt `build_dir=D:/AGV-build-cache/AGVproject-workspace-main/build` và `libdeps_dir=D:/AGV-build-cache/AGVproject-workspace-main/libdeps`.
- Đặt chung để cảhai môi trường01/02 và các môi trường kếthừa dùng đường dẫnASCII. PlatformIO vẫn tách thư mụccon theo tên môi trường, không dùng chungobject của hai xe.
- Không đổi platform6.13.0, framework, toolchainversion, uploadport/speed, compileflags, firmwarePWM/PD, mạng hoặcMQTT. Không xóa project/.pio cũ hoặc nạpbo.
- Tái sử dụng thưviện hiện có bằng sao chép vào cachemới, không chuyểnđi hoặc xóa cachecũ. Build kiểmchứng phải chạy từ projectthật códấu và bỏ biếnmôitrường ghiđèbuild/libdeps trong riêng tiếntrình thử; không chỉbuild bảnstagedASCII.
- Đây là đường dẫn tuyệtđối dành chomáy hiện tại; khi chuyển máy cần điều chỉnh gốcASCII có quyềnghi và dunglượng. Cache/build cóthể được tạo lại, không phải source cần sửa trênxe.

## Kiểm chứng và khôi phục

Kết quả thực tế ghi tại `tests/README.md`, log/hashes/backup ở workspacechat `output/fix-platformio-ascii-path-20261023`. Chỉ build, khôngupload hoặc điều khiểnxe. Kiểmhash mọi nguồnnhúng để chứngminh không đổi thuậttoán/thôngsố.

Muốn hoàn tác: khôi phụcplatformio.ini từbackup hoặc bỏ hai dòngbuild_dir/libdeps_dir và chúthích mới. Khi đóPlatformIO trởlại`.pio` tươngđối trongproject; lỗiUnicode cóthể xuấthiệnlại. Không cần phục hồi source hoặc cậpnhậtcode trênbo.
