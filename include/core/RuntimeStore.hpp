#pragma once
#include "commands/CommandIds.hpp"
#include <algorithm>
#include <optional>
#include <vector>
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

// Checkpoint before publishing commands/grants. Invalid files stop startup.
class RuntimeStore {
public:
    explicit RuntimeStore(std::filesystem::path path) : path_(std::move(path)) {}
    static bool mapsCompatible(const nlohmann::json& current, const nlohmann::json& saved) {
        if (!current.is_object() || !saved.is_object()) return false;
        // Runtime obstacles and revision may differ; the physical graph must match.
        for (const char* key : {"nodes", "edges", "mutex_zones", "charging_stations"}) {
            if (!current.contains(key) || !saved.contains(key) ||
                !current.at(key).is_array() || !saved.at(key).is_array()) return false;
            auto canonical = [key](const nlohmann::json& map) {
                std::vector<std::string> values;
                for (const auto& item : map.at(key)) values.push_back(item.dump());
                std::sort(values.begin(), values.end());
                return values;
            };
            if (canonical(current) != canonical(saved)) return false;
        }
        return true;
    }
    std::optional<nlohmann::json> load() {
        if (!std::filesystem::exists(path_)) return {};
        std::ifstream in(path_); nlohmann::json data; in >> data;
        if (data.at("version") != 1 || !data.at("fleet").is_object() || !data.at("mutex").is_object())
            throw std::runtime_error("Runtime checkpoint khong hop le");
        return data;
    }
    void save(const nlohmann::json& data) {
        const auto serialized = data.dump();
        if (serialized == last_) return;
        if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
        auto temporary = path_; temporary += ".tmp";
        std::ofstream out(temporary,std::ios::binary);out << serialized;out.flush();
        if (!out) throw std::runtime_error("Khong ghi duoc runtime checkpoint; dung phat lenh");
        out.close();
        durableReplaceFile(temporary,path_);
        last_ = serialized;
    }
private:
    std::filesystem::path path_;
    std::string last_;
};
