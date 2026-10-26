#pragma once
#include "utils/json.hpp"
#include "utils/AtomicFile.hpp"
#include <fstream>
#include <algorithm>
#include <optional>
#include <vector>

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
    // Only the known v3 -> rounded v4 change is eligible. Other graph changes
    // still require reconciliation; never silently restore the old graph.
    static bool migrateRoundedMap(const nlohmann::json& current, nlohmann::json& checkpoint) {
        using json = nlohmann::json;
        const auto& old = checkpoint.at("map");
        if (current.value("map_name",std::string{}) != "Kho AGV v4 - bo goc (cm)" || current.at("nodes").size() != 22 ||
            old.at("nodes").size() != 26 || old.at("edges").size() != 39) return false;
        struct Corner {const char* id;const char* from;const char* to;int x;int y;int first;int second;};
        const Corner corners[] = {{"C1","G1","M1",0,0,70,50}, {"C2","M2","G6",200,0,50,70},
            {"C3","M3","G1",0,140,50,70}, {"C4","G6","M4",200,140,70,50}};
        auto candidate = old;
        auto findNode = [](const json& map,const std::string& id) -> const json* {
            for (const auto& n : map.at("nodes")) if (n.at("id") == id) return &n;
            return nullptr;
        };
        auto findEdge = [](const json& map,const std::string& from,const std::string& to) -> const json* {
            for (const auto& e : map.at("edges")) if (e.at("from") == from && e.at("to") == to) return &e;
            return nullptr;
        };
        for (const auto& c : corners) {
            const auto* n = findNode(old,c.id);const auto* a = findEdge(old,c.from,c.id);
            const auto* b = findEdge(old,c.id,c.to);const auto* replacement = findEdge(current,c.from,c.to);
            if (!n || n->at("type") != "waypoint" || n->at("x") != c.x || n->at("y") != c.y ||
                !a || !b || !replacement || a->at("weight") != c.first || b->at("weight") != c.second ||
                a->value("two_way",false) || b->value("two_way",false) ||
                replacement->at("weight") != 135 || replacement->value("two_way",false) ||
                !replacement->contains("geometry")) return false;
            const auto& g = replacement->at("geometry");
            const json lengths = c.first == 70 ? json::array({85,50}) : json::array({50,85});
            if (g.at("via").at("x") != c.x || g.at("via").at("y") != c.y ||
                g.at("segment_lengths_cm") != lengths) return false;
            auto& nodes = candidate["nodes"];
            nodes.erase(std::remove_if(nodes.begin(),nodes.end(),[&](const json& v){return v.at("id") == c.id;}),nodes.end());
            auto& edges = candidate["edges"];
            edges.erase(std::remove_if(edges.begin(),edges.end(),[&](const json& v){
                return v.at("from") == c.id || v.at("to") == c.id;
            }),edges.end());
            edges.push_back(*replacement);
        }
        if (!mapsCompatible(current,candidate)) return false;
        auto migrated = checkpoint;
        auto expand = [&](const json& values) {
            json out = json::array();
            auto append = [&](const std::string& id) {
                if (!id.empty() && std::find(out.begin(),out.end(),json(id)) == out.end()) out.push_back(id);
            };
            for (const auto& value : values) {
                const auto id = value.get<std::string>();bool retired = false;
                for (const auto& c : corners) if (id == c.id) {append(c.from);append(c.to);retired = true;break;}
                if (!retired) {if (!findNode(current,id)) throw std::runtime_error("Checkpoint co node khong biet");append(id);}
            }
            return out;
        };
        migrated["map"] = current;
        // Nonempty/structured obstacle state needs a separate, explicit migration.
        if (!old.value("obstacles",json::array()).empty()) return false;
        auto& fleet = migrated.at("fleet");
        fleet["accepting_tasks"] = false;
        for (auto& task : fleet.at("tasks")) {
            const bool retired = !findNode(current,task.at("target").get<std::string>());
            if (retired) {
                bool known = false;for (const auto& c : corners) if (task.at("target") == c.id) known = true;
                if (!known) return false;
                task["target_retired"] = true;
            }
            if (task.at("status") == "pending" || task.at("status") == "dispatched") {
                task["status"] = "interrupted";task["reason"] = "Map v3 -> v4; can doi soat va giao lai";
                task["updated_sequence"] = fleet.at("queue_sequence").get<long>() + 1;
                fleet["queue_sequence"] = task["updated_sequence"];
            }
        }
        fleet["pending_tasks"] = json::array();
        for (auto& robot : fleet.at("agvs")) {
            auto uncertain = robot.at("uncertain_path");
            for (const auto& n : robot.value("stopped_nodes",json::array())) uncertain.push_back(n);
            for (const auto& n : robot.at("path")) uncertain.push_back(n);
            const auto node = robot.at("node").get<std::string>();
            if (!node.empty()) uncertain.push_back(node);
            const auto target = robot.value("target",std::string{});
            if (!target.empty()) uncertain.push_back(target);
            robot["uncertain_path"] = expand(uncertain);
            robot["stopped_nodes"] = json::array(); // Restart cannot restore a live stop proof.
            if (!node.empty() && !findNode(current,node)) robot["node"] = "";
            robot["position_valid"] = false;robot["state"] = "OFFLINE";
            robot["path"] = json::array();robot["target"] = "";robot["task_id"] = "";
        }
        // Keep all owners and grant IDs. Old wait requests cannot authorize movement.
        for (auto& zone : migrated.at("mutex").items()) zone.value()["waiting"] = json::array();
        checkpoint = std::move(migrated);
        return true;
    }
    std::filesystem::path backupBeforeRoundedMigration() const {
        auto backup = path_;backup += ".before-rounded-map-v4";
        for (unsigned i = 0; ; ++i) {
            auto target = backup;target += "-" + std::to_string(i) + ".json";
            if (std::filesystem::exists(target)) continue;
            std::filesystem::copy_file(path_,target); // Never overwrite a prior backup.
            return target;
        }
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
