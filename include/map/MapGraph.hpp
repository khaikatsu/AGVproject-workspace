#pragma once
// ============================================================================
// FILE 1/4 — MapGraph.hpp
// Phụ trách: NGƯỜI 1 — Bản đồ & thuật toán tìm đường
// Vai trò:
//   - Cấu trúc dữ liệu bản đồ (Node/Edge), nạp & lưu ra file JSON.
//   - Cho phép CHỈNH SỬA MAP LÚC RUNTIME (thêm/xoá node, cạnh) để phục vụ
//     tính năng "điều chỉnh map" từ dashboard web.
//   - Đánh dấu & gỡ vật cản (obstacle) theo từng node — phục vụ né vật cản.
//   - Dijkstra tìm đường ngắn nhất, có thể né tạm các node "đang bận"
//     (do AGV khác đang đi qua) MÀ KHÔNG sửa đổi trạng thái chung của đồ thị
//     (khác bản gốc: bản gốc dùng blockNodes()/clearBlockages() làm bẩn state
//     toàn cục -> nếu 2 luồng cùng gọi dispatch cùng lúc sẽ đá nhau / sai kết
//     quả. Bản này truyền tập node cần né dưới dạng tham số cục bộ).
//   - Thread-safe bằng std::mutex vì Web UI (luồng WebSocket) và Dispatcher
//     (luồng MQTT / tick định kỳ) có thể động vào map cùng lúc.
//
// Ghi chú build: cần thư viện header-only nlohmann/json (json.hpp) — tải tại
// https://github.com/nlohmann/json (file single_include/nlohmann/json.hpp),
// đặt cùng thư mục và đổi tên thành "json.hpp".
// ============================================================================
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <filesystem>
#include <cstdio>
#include <chrono>
#include "utils/Identifiers.hpp"
#include "utils/AtomicFile.hpp"
#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
#include <windows.h>
#endif
#include "utils/json.hpp"

using json = nlohmann::json;

struct Node {
    std::string id;
    double x = 0.0, y = 0.0;
    std::string type = "waypoint"; // waypoint | mutex | charging | station
    bool isMutex() const { return type == "mutex"; }
    bool isCharging() const { return type == "charging"; }
};

struct Edge {
    std::string from;
    std::string to;
    int weight = 1;
    std::string type = "main";  // main | branch | loop
    std::string path_class;     // boulevard | shortcut | loop | branch ...
    bool twoWay = false;        // true = nhánh chữ T 2 chiều (yêu cầu nhường đường)
};

class MapGraph {
public:
    // ---------------- Nạp / lưu file JSON ----------------
    void loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) throw std::runtime_error("Khong mo duoc file map: " + path);
        json j; in >> j;
        loadFromJson(j);
    }

    // Nạp bản đồ trực tiếp từ 1 đối tượng JSON đã có sẵn trong bộ nhớ (không qua file) --
    // dùng khi Web Dashboard gửi lệnh "load_map_data" (nạp lại 1 snapshot đã lưu trong SQLite,
    // hoặc nạp bản đồ mới nhất khi Node.js gateway vừa kết nối tới C++ Core). Cùng định dạng
    // JSON với toJson()/loadFromFile(), nên nạp lại đúng những gì đã lưu, bao gồm cả obstacles_
    // (bản gốc loadFromFile() vô tình BỎ QUÊN không khôi phục obstacles khi nạp file).
    void loadFromJson(const json& j) {
        // Parse/validate a candidate first. A rejected map must leave the live graph intact.
        if (!j.is_object() || !j.contains("nodes") || !j["nodes"].is_array() ||
            !j.contains("edges") || !j["edges"].is_array())
            throw std::invalid_argument("Ban do phai co nodes va edges dang mang");
        if (j["nodes"].empty() || j["nodes"].size() > 2000 || j["edges"].size() > 10000)
            throw std::invalid_argument("So node/canh ban do khong hop le");
        std::unordered_map<std::string, Node> nodes;
        std::vector<Edge> edges;
        std::vector<std::string> zones, stations;
        std::unordered_set<std::string> obstacles, edgeKeys;
        for (const auto& n : j["nodes"]) {
            Node node{n.at("id").get<std::string>(), n.value("x", 0.0),
                      n.value("y", 0.0), n.value("type", "waypoint")};
            validateNode(node);
            if (!nodes.emplace(node.id, node).second) throw std::invalid_argument("Node trung: " + node.id);
            if (node.isMutex()) zones.push_back(node.id);
            if (node.isCharging()) stations.push_back(node.id);
        }
        for (const auto& e : j["edges"]) {
            if (e.contains("weight") && (!e["weight"].is_number_integer() || e["weight"].get<long long>() < 0 || e["weight"].get<long long>() > 1000000))
                throw std::invalid_argument("Trong so phai la so nguyen trong 0..1000000");
            Edge edge{e.at("from").get<std::string>(), e.at("to").get<std::string>(),
                      e.value("weight", 1), e.value("type", "main"),
                      e.value("path_class", ""), e.value("two_way", false)};
            if (!nodes.count(edge.from) || !nodes.count(edge.to) || edge.from == edge.to ||
                edge.weight < 0 || edge.weight > 1000000)
                throw std::invalid_argument("Canh/node/trong so khong hop le");
            if (!edgeKeys.insert(edge.from + "\n" + edge.to).second)
                throw std::invalid_argument("Canh trung: " + edge.from + " -> " + edge.to);
            edges.push_back(edge);
        }
        // Snapshots export both directions; imports may describe a two-way edge once.
        const auto originals = edges;
        for (const auto& e : originals) if (e.twoWay) {
            auto reverse = std::find_if(edges.begin(), edges.end(), [&](const Edge& r) {
                return r.from == e.to && r.to == e.from;
            });
            if (reverse == edges.end()) edges.push_back({e.to, e.from, e.weight, e.type, e.path_class, true});
            else if (!reverse->twoWay || reverse->weight != e.weight)
                throw std::invalid_argument("Canh hai chieu khong nhat quan");
        }
        for (const auto& id : j.value("mutex_zones", std::vector<std::string>{})) {
            if (!nodes.count(id) || !nodes.at(id).isMutex()) throw std::invalid_argument("Mutex zone khong hop le");
            addUnique(zones, id);
        }
        for (const auto& id : j.value("charging_stations", std::vector<std::string>{})) {
            if (!nodes.count(id) || !nodes.at(id).isCharging()) throw std::invalid_argument("Tram sac khong hop le");
            addUnique(stations, id);
        }
        for (const auto& id : j.value("obstacles", std::vector<std::string>{})) {
            if (!nodes.count(id)) throw std::invalid_argument("Vat can khong ton tai: " + id);
            obstacles.insert(id);
        }
        auto name = j.value("map_name", std::string{});
        std::unordered_map<std::string,long long> deadlines;
        const auto deadlinesJson = j.value("obstacle_deadlines",json::object());
        for (const auto& kv : deadlinesJson.items()) {
            if (!obstacles.count(kv.key()) || !kv.value().is_number_integer() || kv.value().get<long long>() <= 0)
                throw std::invalid_argument("TTL vat can khong hop le");
            deadlines[kv.key()] = kv.value().get<long long>();
        }
        std::unordered_map<std::string, std::vector<Edge>> adjacency;
        for (const auto& e : edges) adjacency[e.from].push_back(e);
        std::unordered_set<std::string> blockedEdges;
        const auto blockedJson = j.value("blocked_edges",json::array());
        if (!blockedJson.is_array()) throw std::invalid_argument("blocked_edges phai la array");
        for (const auto& blocked : blockedJson) {
            const auto from = blocked.at("from").get<std::string>(), to = blocked.at("to").get<std::string>();
            if (!std::any_of(edges.begin(),edges.end(),[&](const Edge& e){
                return (e.from==from && e.to==to) || (e.from==to && e.to==from);
            })) throw std::invalid_argument("Vat can canh khong ton tai");
            blockedEdges.insert(edgeKey(from,to));
        }
        std::lock_guard<std::mutex> lk(mtx_);
        nodes_.swap(nodes); edges_.swap(edges); adjacency_.swap(adjacency);
        mutexZones_.swap(zones); chargingStations_.swap(stations); obstacles_.swap(obstacles);
        mapName_ = std::move(name); ++revision_;
        obstacleDeadlines_ = std::move(deadlines);
        blockedEdges_.swap(blockedEdges);
    }

    void saveToFile(const std::string& path) {
        auto snapshot = toJson();
        const auto temporary = std::filesystem::path(path + ".tmp");
        std::ofstream out(temporary);
        if (!out) throw std::runtime_error("Khong mo duoc file luu ban do: " + path);
        out << snapshot.dump(2); out.flush();
        if (!out) throw std::runtime_error("Khong ghi duoc file ban do: " + path);
        out.close();
        durableReplaceFile(temporary,std::filesystem::path(path));
    }

    // ---------------- Chỉnh sửa map lúc runtime (dùng cho Web UI) ----------------
    void addNode(const std::string& id, double x, double y, const std::string& type) {
        std::lock_guard<std::mutex> lk(mtx_);
        validateNode(Node{id, x, y, type});
        nodes_[id] = Node{id, x, y, type};
        mutexZones_.erase(std::remove(mutexZones_.begin(), mutexZones_.end(), id), mutexZones_.end());
        chargingStations_.erase(std::remove(chargingStations_.begin(), chargingStations_.end(), id), chargingStations_.end());
        ++revision_;
        if (type == "mutex")    addUnique(mutexZones_, id);
        if (type == "charging") addUnique(chargingStations_, id);
    }

    void removeNode(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_);
        nodes_.erase(id);
        adjacency_.erase(id);
        for (auto& kv : adjacency_) {
            auto& list = kv.second;
            list.erase(std::remove_if(list.begin(), list.end(),
                       [&](const Edge& e){ return e.to == id; }), list.end());
        }
        edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                     [&](const Edge& e){ return e.from == id || e.to == id; }), edges_.end());
        obstacles_.erase(id);
        obstacleDeadlines_.erase(id);
        for (auto it=blockedEdges_.begin();it!=blockedEdges_.end();) {
            const auto split=it->find('\n');
            if (it->substr(0,split)==id || it->substr(split+1)==id) it=blockedEdges_.erase(it);
            else ++it;
        }
        mutexZones_.erase(std::remove(mutexZones_.begin(), mutexZones_.end(), id), mutexZones_.end());
        chargingStations_.erase(std::remove(chargingStations_.begin(), chargingStations_.end(), id), chargingStations_.end());
        ++revision_;
    }

    void addEdge(const std::string& from, const std::string& to, int weight,
                 const std::string& type = "main", const std::string& pathClass = "",
                 bool twoWay = false) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!nodes_.count(from) || !nodes_.count(to) || from == to || weight < 0 || weight > 1000000)
            throw std::invalid_argument("Canh/trong so khong hop le");
        auto eraseDirection = [&](const std::string& a, const std::string& b) {
            auto& list = adjacency_[a];
            list.erase(std::remove_if(list.begin(), list.end(), [&](const Edge& e){ return e.to == b; }), list.end());
            edges_.erase(std::remove_if(edges_.begin(), edges_.end(), [&](const Edge& e){ return e.from == a && e.to == b; }), edges_.end());
        };
        bool wasPair = false;
        for (const auto& e : edges_) if (e.from == from && e.to == to) wasPair = e.twoWay;
        eraseDirection(from, to);
        if (twoWay || wasPair) eraseDirection(to, from);
        ++revision_;
        addEdgeInternal(Edge{from, to, weight, type, pathClass, twoWay});
        if (twoWay) addEdgeInternal(Edge{to, from, weight, type, pathClass, twoWay});
    }

    void removeEdge(const std::string& from, const std::string& to) {
        std::lock_guard<std::mutex> lk(mtx_);
        bool pair = false;
        for (const auto& e : edges_) if (e.from == from && e.to == to) pair = e.twoWay;
        auto matches = [&](const Edge& e) { return (e.from == from && e.to == to) || (pair && e.from == to && e.to == from); };
        for (auto& kv : adjacency_) {
            auto& list = kv.second;
            list.erase(std::remove_if(list.begin(), list.end(), matches), list.end());
        }
        edges_.erase(std::remove_if(edges_.begin(), edges_.end(), matches), edges_.end());
        if (!std::any_of(edges_.begin(),edges_.end(),[&](const Edge& e){
            return edgeKey(e.from,e.to)==edgeKey(from,to);
        })) blockedEdges_.erase(edgeKey(from,to));
        ++revision_;
    }

    // A physical obstacle blocks both travel directions on this segment.
    void setEdgeObstacle(const std::string& from,const std::string& to,bool blocked) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!std::any_of(edges_.begin(),edges_.end(),[&](const Edge& e){
            return edgeKey(e.from,e.to)==edgeKey(from,to);
        })) throw std::invalid_argument("Canh khong ton tai");
        const auto key=edgeKey(from,to);
        const bool changed=blocked ? blockedEdges_.insert(key).second : blockedEdges_.erase(key)>0;
        if (changed) ++revision_;
    }

    // Vật cản: cảm biến AGV báo vật cản tại 1 node -> chặn tạm thời cho Dijkstra
    void setObstacle(const std::string& nodeId, bool blocked, int ttlSeconds = 0) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!nodes_.count(nodeId)) throw std::invalid_argument("Node vat can khong ton tai");
        const bool wasPermanent = obstacles_.count(nodeId) && !obstacleDeadlines_.count(nodeId);
        bool changed = blocked ? obstacles_.insert(nodeId).second : obstacles_.erase(nodeId) > 0;
        if (blocked && ttlSeconds > 0 && !wasPermanent) {
            const auto expires = epochSeconds()+ttlSeconds;
            changed = changed || obstacleDeadlines_[nodeId] != expires;obstacleDeadlines_[nodeId] = expires;
        } else if (!blocked || ttlSeconds == 0) changed = obstacleDeadlines_.erase(nodeId) > 0 || changed;
        if (changed) ++revision_;
    }
    bool expireObstacles(long long now = epochSeconds()) {
        std::lock_guard<std::mutex> lk(mtx_);bool changed = false;
        for (auto it = obstacleDeadlines_.begin();it != obstacleDeadlines_.end();) {
            if (it->second <= now) {obstacles_.erase(it->first);it = obstacleDeadlines_.erase(it);changed = true;}
            else ++it;
        }
        if (changed) ++revision_;return changed;
    }
    static long long epochSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    bool hasObstacle(const std::string& nodeId) {
        std::lock_guard<std::mutex> lk(mtx_);
        return obstacles_.count(nodeId) > 0;
    }

    // Adjacent mutex nodes form one corridor: serial admission prevents hold-and-wait.
    std::unordered_set<std::string> mutexComponent(const std::string& seed) {
        std::lock_guard<std::mutex> lk(mtx_);
        std::unordered_set<std::string> zones(mutexZones_.begin(),mutexZones_.end()),result;
        if (!zones.count(seed)) return result;
        std::vector<std::string> pending{seed};result.insert(seed);
        while (!pending.empty()) {
            const auto node = pending.back();pending.pop_back();
            for (const auto& edge : edges_) {
                if (!zones.count(edge.from) || !zones.count(edge.to)) continue;
                const auto next = edge.from == node ? edge.to : (edge.to == node ? edge.from : std::string{});
                if (!next.empty() && result.insert(next).second) pending.push_back(next);
            }
        }
        return result;
    }

    // ---------------- Dijkstra ----------------
    // extraBlocked: các node cần né TẠM cho riêng lần gọi này (VD: đường đi
    // hiện tại của các AGV khác) — không sửa state chung -> an toàn đa luồng.
    std::vector<std::string> findShortestPath(const std::string& start, const std::string& target,
                                               const std::unordered_set<std::string>& extraBlocked = {}) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!nodes_.count(start) || !nodes_.count(target)) return {};

        std::unordered_map<std::string, int> dist;
        std::unordered_map<std::string, std::string> prev;
        for (auto& kv : nodes_) dist[kv.first] = INT_MAX;
        dist[start] = 0;

        using PQItem = std::pair<int, std::string>;
        std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;
        pq.push({0, start});

        while (!pq.empty()) {
            int d = pq.top().first; std::string u = pq.top().second; pq.pop();
            if (u == target) break;
            if (d > dist[u]) continue;
            auto it = adjacency_.find(u);
            if (it == adjacency_.end()) continue;
            for (const auto& edge : it->second) {
                if (obstacles_.count(edge.to) || extraBlocked.count(edge.to)) continue;
                if (blockedEdges_.count(edgeKey(edge.from,edge.to))) continue;
                if (dist[u] > INT_MAX - edge.weight) continue;
                int alt = dist[u] + edge.weight;
                if (dist.find(edge.to) != dist.end() && alt < dist[edge.to]) {
                    dist[edge.to] = alt;
                    prev[edge.to] = u;
                    pq.push({alt, edge.to});
                }
            }
        }
        std::vector<std::string> path;
        if (start != target && (dist.find(target) == dist.end() || dist[target] == INT_MAX)) return path;
        std::string cur = target;
        while (prev.count(cur)) { path.insert(path.begin(), cur); cur = prev[cur]; }
        path.insert(path.begin(), start);
        return path;
    }

    std::string nearestCharging(const std::string& from, const std::unordered_set<std::string>& extraBlocked = {}) {
        std::string best; long long bestCost = LLONG_MAX;
        for (auto& cs : chargingStations()) {
            auto p = findShortestPath(from, cs, extraBlocked);
            auto cost = pathCost(p);
            if (!p.empty() && (cost < bestCost || (cost == bestCost && cs < best))) { bestCost = cost; best = cs; }
        }
        return best;
    }

    long long pathCost(const std::vector<std::string>& path) {
        if (path.empty()) return LLONG_MAX;
        std::lock_guard<std::mutex> lk(mtx_);
        long long total = 0;
        for (size_t i = 1; i < path.size(); ++i) {
            auto it = adjacency_.find(path[i - 1]);
            if (it == adjacency_.end()) return LLONG_MAX;
            int weight = INT_MAX;
            for (const auto& e : it->second) if (e.to == path[i]) weight = std::min(weight, e.weight);
            if (weight == INT_MAX) return LLONG_MAX;
            total += weight;
        }
        return total;
    }

    // Khoảng cách vật lý nhỏ nhất trên các cạnh, bỏ chiều di chuyển. Chỉ dùng
    // với map đã đo bằng cm để chứng minh RFID trạm cách xa mutex zone.
    long long undirectedDistance(const std::string& start, const std::string& target) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!nodes_.count(start) || !nodes_.count(target)) return LLONG_MAX;
        std::unordered_map<std::string, long long> dist;
        for (const auto& node : nodes_) dist[node.first] = LLONG_MAX;
        dist[start] = 0;
        using Item = std::pair<long long, std::string>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pending;
        pending.push({0, start});
        while (!pending.empty()) {
            const auto [cost, node] = pending.top();pending.pop();
            if (cost != dist[node]) continue;
            if (node == target) return cost;
            for (const auto& edge : edges_) {
                const auto next = edge.from == node ? edge.to :
                    edge.to == node ? edge.from : std::string{};
                if (next.empty() || cost > LLONG_MAX - edge.weight) continue;
                const auto candidate = cost + edge.weight;
                if (candidate < dist[next]) {dist[next] = candidate;pending.push({candidate,next});}
            }
        }
        return LLONG_MAX;
    }

    // Giới hạn dưới theo tọa độ đo: đường đi dài không chứng minh hai điểm
    // cách xa nhau nếu tuyến vòng lại sát mutex.
    double straightLineDistance(const std::string& start, const std::string& target) {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto a = nodes_.find(start), b = nodes_.find(target);
        if (a == nodes_.end() || b == nodes_.end()) return std::numeric_limits<double>::infinity();
        return std::hypot(a->second.x - b->second.x, a->second.y - b->second.y);
    }

    std::vector<std::string> mutexZones() { std::lock_guard<std::mutex> lk(mtx_); return mutexZones_; }
    std::vector<std::string> chargingStations() { std::lock_guard<std::mutex> lk(mtx_); return chargingStations_; }
    bool nodeExists(const std::string& id) { std::lock_guard<std::mutex> lk(mtx_); return nodes_.count(id) > 0; }

    json toJson() {
        std::lock_guard<std::mutex> lk(mtx_);
        json j = {{"nodes", json::array()}, {"edges", json::array()}, {"revision", revision_}};
        j["map_name"] = mapName_;
        for (auto& kv : nodes_) {
            const Node& n = kv.second;
            j["nodes"].push_back({{"id", n.id}, {"x", n.x}, {"y", n.y}, {"type", n.type}});
        }
        for (auto& e : edges_) {
            j["edges"].push_back({{"from", e.from}, {"to", e.to}, {"weight", e.weight},
                                   {"type", e.type}, {"path_class", e.path_class}, {"two_way", e.twoWay}});
        }
        j["obstacles"] = std::vector<std::string>(obstacles_.begin(), obstacles_.end());
        j["blocked_edges"] = json::array();
        std::vector<std::string> blocked(blockedEdges_.begin(),blockedEdges_.end());
        std::sort(blocked.begin(),blocked.end());
        for (const auto& key : blocked) {
            const auto split=key.find('\n');
            j["blocked_edges"].push_back({{"from",key.substr(0,split)},{"to",key.substr(split+1)}});
        }
        if (!obstacleDeadlines_.empty()) j["obstacle_deadlines"] = obstacleDeadlines_;
        j["mutex_zones"] = mutexZones_;
        j["charging_stations"] = chargingStations_;
        return j;
    }

    // Map v3: 6 RFID/mutex M1..M6, cua kho G1..G6, kho K1..K6,
    // cua sac GS1/GS2, tram sac S1/S2 va goc C1..C4.
    // Toa do/trong so cung don vi cm theo mo hinh; can do lai tren kho that.
    // M5/M6 chia duong ngang giua; cua kho trong nam giua moi nua cot doc.
    // Dung khi chua co map_layout.json de he thong chay duoc ngay.
    void loadDefaultDemoMap() {
        std::lock_guard<std::mutex> lk(mtx_);
        clearInternal();
        mapName_ = "Kho AGV v3 (cm)";

        // Toa do mo hinh, don vi cm.
        addNodeNoLock("C1", 0, 0, "waypoint");
        addNodeNoLock("M1", 50, 0, "mutex");
        addNodeNoLock("GS1", 100, 0, "waypoint");
        addNodeNoLock("M2", 150, 0, "mutex");
        addNodeNoLock("C2", 200, 0, "waypoint");
        addNodeNoLock("S1", 100, -20, "charging");
        addNodeNoLock("S2", 100, 160, "charging");
        addNodeNoLock("G2", 50, 35, "waypoint");
        addNodeNoLock("G4", 150, 35, "waypoint");
        addNodeNoLock("K2", 30, 35, "station");
        addNodeNoLock("K4", 170, 35, "station");
        addNodeNoLock("G1", 0, 70, "waypoint");
        addNodeNoLock("G6", 200, 70, "waypoint");
        addNodeNoLock("K1", -20, 70, "station");
        addNodeNoLock("K6", 220, 70, "station");
        addNodeNoLock("M5", 50, 70, "mutex");
        addNodeNoLock("M6", 150, 70, "mutex");
        addNodeNoLock("G3", 50, 105, "waypoint");
        addNodeNoLock("G5", 150, 105, "waypoint");
        addNodeNoLock("K3", 30, 105, "station");
        addNodeNoLock("K5", 170, 105, "station");
        addNodeNoLock("C3", 0, 140, "waypoint");
        addNodeNoLock("M3", 50, 140, "mutex");
        addNodeNoLock("GS2", 100, 140, "waypoint");
        addNodeNoLock("M4", 150, 140, "mutex");
        addNodeNoLock("C4", 200, 140, "waypoint");

        // Duong chinh 1 chieu (weight = do dai canh trong mo hinh, cm)
        // Hang tren (trai -> phai)
        addEdgeInternal({"C1", "M1", 50, "main", "boulevard", false});
        addEdgeInternal({"M1", "GS1", 50, "main", "boulevard", false});
        addEdgeInternal({"GS1", "M2", 50, "main", "boulevard", false});
        addEdgeInternal({"M2", "C2", 50, "main", "boulevard", false});
        // Hang duoi (phai -> trai)
        addEdgeInternal({"C4", "M4", 50, "main", "boulevard", false});
        addEdgeInternal({"M4", "GS2", 50, "main", "boulevard", false});
        addEdgeInternal({"GS2", "M3", 50, "main", "boulevard", false});
        addEdgeInternal({"M3", "C3", 50, "main", "boulevard", false});
        // Cot ngoai trai/phai (tren -> duoi)
        addEdgeInternal({"G1", "C1", 70, "main", "outer", false});
        addEdgeInternal({"C3", "G1", 70, "main", "outer", false});
        addEdgeInternal({"C2", "G6", 70, "main", "outer", false});
        addEdgeInternal({"G6", "C4", 70, "main", "outer", false});
        // Cot trong trai (tren -> duoi)
        addEdgeInternal({"M1", "G2", 35, "main", "inner", false});
        addEdgeInternal({"G2", "M5", 35, "main", "inner", false});
        addEdgeInternal({"M5", "G3", 35, "main", "inner", false});
        addEdgeInternal({"G3", "M3", 35, "main", "inner", false});
        // Cot trong phai (duoi -> tren)
        addEdgeInternal({"M4", "G5", 35, "main", "inner", false});
        addEdgeInternal({"G5", "M6", 35, "main", "inner", false});
        addEdgeInternal({"M6", "G4", 35, "main", "inner", false});
        addEdgeInternal({"G4", "M2", 35, "main", "inner", false});
        // Duong ngang giua (trai -> phai)
        addEdgeInternal({"G1", "M5", 50, "main", "shortcut", false});
        addEdgeInternal({"M5", "M6", 100, "main", "shortcut", false});
        addEdgeInternal({"M6", "G6", 50, "main", "shortcut", false});

        // Nhanh 2 chieu ra diem sac / diem kho (weight = do dai mo hinh, cm)
        addTwoWay("S1", "GS1", 20, "branch");
        addTwoWay("S2", "GS2", 20, "branch");
        addTwoWay("K1", "G1", 20, "branch");
        addTwoWay("K2", "G2", 20, "branch");
        addTwoWay("K3", "G3", 20, "branch");
        addTwoWay("K4", "G4", 20, "branch");
        addTwoWay("K5", "G5", 20, "branch");
        addTwoWay("K6", "G6", 20, "branch");

        mutexZones_ = {"M1","M2","M3","M4","M5","M6"};
        chargingStations_ = {"S1", "S2"};
    }
private:
    static void validateNode(const Node& n) {
        if (!validIdentifier(n.id) ||
            !std::isfinite(n.x) || !std::isfinite(n.y) || std::abs(n.x) > 1000000 || std::abs(n.y) > 1000000 ||
            (n.type != "waypoint" && n.type != "mutex" && n.type != "charging" && n.type != "station"))
            throw std::invalid_argument("Node/toa do/type khong hop le");
    }
    static void addUnique(std::vector<std::string>& v, const std::string& id) {
        if (std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
    }
    void addNodeNoLock(const std::string& id, double x, double y, const std::string& type) {
        nodes_[id] = Node{id, x, y, type};
    }
    void addTwoWay(const std::string& a, const std::string& b, int w, const std::string& type) {
        addEdgeInternal({a, b, w, type, "", true});
        addEdgeInternal({b, a, w, type, "", true});
    }
    void addEdgeInternal(const Edge& e) {
        edges_.push_back(e);
        adjacency_[e.from].push_back(e);
    }
    void clearInternal() {
        nodes_.clear(); edges_.clear(); adjacency_.clear();
        mutexZones_.clear(); chargingStations_.clear(); obstacles_.clear();
        obstacleDeadlines_.clear();
        blockedEdges_.clear();
    }

    unsigned long revision_ = 0;
    std::string mapName_;
    std::unordered_map<std::string, Node> nodes_;
    std::vector<Edge> edges_;
    std::unordered_map<std::string, std::vector<Edge>> adjacency_;
    std::vector<std::string> mutexZones_;
    std::vector<std::string> chargingStations_;
    std::unordered_set<std::string> obstacles_;
    static std::string edgeKey(const std::string& from,const std::string& to) {
        return from<to ? from+"\n"+to : to+"\n"+from;
    }
    std::unordered_set<std::string> blockedEdges_;
    std::unordered_map<std::string,long long> obstacleDeadlines_;
    std::mutex mtx_;
};
