#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <chrono>
#include <atomic>
#include <cstdio>

struct AtomicCommitMetrics {
    inline static std::atomic<long long> lastMs{0}, maxMs{0}, slowCount{0};
};
struct AtomicCommitTimer {
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    ~AtomicCommitTimer() noexcept {
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        AtomicCommitMetrics::lastMs=elapsed;
        auto maximum=AtomicCommitMetrics::maxMs.load();
        while(elapsed>maximum && !AtomicCommitMetrics::maxMs.compare_exchange_weak(maximum,elapsed)) {}
        if(elapsed>500) {
            ++AtomicCommitMetrics::slowCount;
            std::fprintf(stderr,"[STORAGE] Atomic commit cham: %lld ms (nguong 500 ms)\n",static_cast<long long>(elapsed));
        }
    }
};
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// Caller closes the temporary file before committing. Never remove the old file first.
inline void durableReplaceFile(const std::filesystem::path& temporary,const std::filesystem::path& destination) {
    const AtomicCommitTimer timer;
#if defined(_WIN32)
    const auto started=GetTickCount64();
    auto retry = [&](auto operation) {
        DWORD error = ERROR_SUCCESS;
        for (int attempt=0;attempt<41;++attempt) {
            if (operation()) return;
            error = GetLastError();
            if (attempt == 40 || GetTickCount64()-started >= 2000 ||
                (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION && error != ERROR_ACCESS_DENIED)) break;
            Sleep(50); // One 2 s retry budget across open/replace, within the 3 s Core heartbeat watchdog.
        }
        throw std::runtime_error("Atomic file commit failed; Windows error="+std::to_string(error));
    };
    HANDLE file = INVALID_HANDLE_VALUE;
    retry([&] {file = CreateFileW(temporary.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);return file != INVALID_HANDLE_VALUE;});
    const BOOL flushed = FlushFileBuffers(file);const DWORD flushError = GetLastError();CloseHandle(file);
    if (!flushed) throw std::runtime_error("File flush failed; Windows error="+std::to_string(flushError));
    retry([&] {return MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);});
#else
    int fd = ::open(temporary.c_str(),O_RDONLY);
    if (fd < 0 || ::fsync(fd) != 0) {if (fd >= 0) ::close(fd);throw std::runtime_error("File fsync failed");}
    ::close(fd);std::filesystem::rename(temporary,destination);
    fd = ::open((destination.parent_path().empty() ? std::filesystem::path(".") : destination.parent_path()).c_str(),O_RDONLY);
    if (fd < 0 || ::fsync(fd) != 0) {if (fd >= 0) ::close(fd);throw std::runtime_error("Directory fsync failed");}
    ::close(fd);
#endif
}
