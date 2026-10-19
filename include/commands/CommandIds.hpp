#pragma once
#include "utils/json.hpp"
#include "utils/AtomicFile.hpp"
#include <fstream>
#include <filesystem>
#include <mutex>
#include <climits>
#include <stdexcept>
#include <algorithm>
#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
#include <windows.h>
#endif
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

// Reserve a block on disk before exposing any ID. Process restarts skip unused IDs.
// Keep IDs in signed 32-bit range for compatibility with the existing numeric ACK contract.
class CommandIds {
public:
    explicit CommandIds(std::filesystem::path file) : file_(std::move(file)) {
        if (!file_.parent_path().empty()) std::filesystem::create_directories(file_.parent_path());
        if (std::filesystem::exists(file_)) {
            std::ifstream input(file_);nlohmann::json data;input >> data;
            const auto& value = data.at("reserved_through");
            if (!value.is_number_integer()) throw std::runtime_error("File command ID khong hop le");
            auto end = value.get<long long>();
            if (end < 0 || end > INT_MAX) throw std::runtime_error("Command ID ngoai gioi han");
            next_ = end + 1;
        }
        reserve();
    }
    long next() {
        std::lock_guard<std::mutex> lk(mtx_);
        if (next_ > reserved_) reserve();
        return static_cast<long>(next_++);
    }
    // Firmware persists the greatest accepted command ID in NVS. If that
    // value is ahead of this process' counter (for example after starting
    // Core with an older/moved counter file), skip the unsafe range. Never
    // rewind either side and never reuse an ID merely to recover an ACK.
    void ensureAbove(long highwater) {
        if (highwater < 0) throw std::invalid_argument("Command high-water khong hop le");
        std::lock_guard<std::mutex> lk(mtx_);
        if (next_ > highwater) return;
        if (highwater >= INT_MAX) throw std::runtime_error("Da het mien command ID; can doi hop dong truoc khi tiep tuc");
        next_ = static_cast<long long>(highwater) + 1;
        if (next_ > reserved_) reserve();
    }
private:
    void reserve() {
        if (next_ > INT_MAX) throw std::runtime_error("Da het mien command ID; can doi hop dong truoc khi tiep tuc");
        const long long through = std::min<long long>(INT_MAX,next_ + 1023);
        auto temporary = file_;temporary += ".tmp";
        std::ofstream output(temporary);
        output << nlohmann::json{{"reserved_through",through}}.dump();output.flush();
        if (!output) throw std::runtime_error("Khong luu duoc command ID; dung phat lenh");
        output.close();
        durableReplaceFile(temporary,file_);
        reserved_ = through;
    }
    std::filesystem::path file_;
    long long next_ = 1,reserved_ = 0;
    std::mutex mtx_;
};
