#pragma once
// ============================================================================
// FILE 2/4 — MutexArbiter.hpp
// Phụ trách: NGƯỜI 2 — Trọng tài khoá chéo (Mutex Zone Arbiter)
// Vai trò:
//   - Đảm bảo tại một thời điểm chỉ 1 AGV được đi vào 1 ngã tư (mutex zone)
//     để tránh va chạm giữa nhiều xe chạy trên nhiều board (ESP32) khác nhau,
//     giao tiếp qua MQTT (không dùng std::mutex thường được vì mỗi xe là
//     1 tiến trình/thiết bị vật lý riêng biệt).
//   - Thread-safe để dùng chung an toàn giữa luồng MQTT (khi request/release)
//     và luồng WebSocket (khi hiển thị trạng thái lên dashboard).
//
// Cách dùng (do Người 4 - Server - gọi trong on_message của MQTT):
//   - requestZone(zone, agvId) -> true: được cấp quyền ngay, publish "grant".
//                                 false: đã xếp hàng chờ, không publish gì.
//   - releaseZone(zone, agvId) -> nếu có xe kế tiếp trong hàng đợi, trả về
//     ID xe đó (đã được cấp quyền) để server publish "grant" báo xe đó chạy.
// ============================================================================
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <algorithm>
#include <utility>
#include <functional>
#include <chrono>
#include "utils/Identifiers.hpp"
#include "utils/json.hpp"

using json = nlohmann::json;

class MutexArbiter {
public:
    void setGrantIdAllocator(std::function<long()> allocator) { allocator_ = std::move(allocator); }
    void setRetryTimeout(int milliseconds) { retryMs_ = std::max(1,milliseconds); }
    explicit MutexArbiter(const std::vector<std::string>& zoneIds = {}) {
        for (const auto& id : zoneIds) owner_[id] = "";
    }

    // Đăng ký thêm 1 zone mới lúc runtime (khi map được chỉnh sửa từ web)
    void registerZone(const std::string& zoneId) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!owner_.count(zoneId)) owner_[zoneId] = "";
    }

    // Xe agvId muốn vào zoneId.
    // true = được cấp quyền ngay lập tức. false = phải chờ (đã xếp hàng đợi).
    bool requestZone(const std::string& zoneId, const std::string& agvId) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = owner_.find(zoneId);
        if (it == owner_.end() || !validIdentifier(agvId)) return false;
        if (it->second.empty()) { assignNoLock(zoneId,agvId); return true; }
        if (it->second == agvId) {
            if (grants_[zoneId].recovery) assignNoLock(zoneId,agvId);
            return true;
        }

        auto& q = queue_[zoneId];
        for (const auto& w : q) if (w == agvId) return false; // đã xếp hàng sẵn, tránh trùng
        q.push_back(agvId);
        return false;
    }

    // Xe agvId rời khỏi zoneId.
    // Trả về ID xe kế tiếp (đã được cấp quyền) nếu có ai đang chờ, ngược lại "".
    std::string releaseZone(const std::string& zoneId, const std::string& agvId, long grantId = 0) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = owner_.find(zoneId);
        if (it == owner_.end() || it->second != agvId) return ""; // không phải chủ sở hữu -> bỏ qua
        auto grant = grants_.find(zoneId);
        if (grant != grants_.end() && (grant->second.recovery || grant->second.id != grantId)) return "";

        auto qit = queue_.find(zoneId);
        if (qit != queue_.end() && !qit->second.empty()) {
            std::string next = qit->second.front();
            qit->second.pop_front();
            assignNoLock(zoneId,next);
            return next;
        }
        it->second.clear();
        grants_.erase(zoneId);
        return "";
    }

    json grant(const std::string& zone, const std::string& robot) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!owner_.count(zone) || owner_.at(zone) != robot || !grants_.count(zone) || grants_[zone].recovery) return nullptr;
        auto& g = grants_.at(zone);g.sent = std::chrono::steady_clock::now();
        return {{"zone",zone},{"grant_id",g.id}};
    }
    bool acknowledge(const std::string& zone,const std::string& robot,long id) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = grants_.find(zone);
        if (it == grants_.end() || owner_[zone] != robot || it->second.id != id || it->second.recovery) return false;
        it->second.acked = true;return true;
    }
    std::vector<std::pair<std::string,json>> retryGrants() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::pair<std::string,json>> out;
        const auto now = std::chrono::steady_clock::now();
        for (auto& kv : grants_) {
            auto& g = kv.second;
            if (g.acked || g.recovery || now-g.sent < std::chrono::milliseconds(retryMs_)) continue;
            g.sent = now;out.push_back({owner_.at(kv.first),{{"zone",kv.first},{"grant_id",g.id}}});
        }
        return out; // Missing ACK never releases ownership.
    }
    void restore(const json& state) {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& kv : state.items()) {
            if (!owner_.count(kv.key())) throw std::invalid_argument("Checkpoint mutex khong khop map");
            const auto owner = kv.value().at("owner").get<std::string>();
            if (!owner.empty() && !validIdentifier(owner)) throw std::invalid_argument("Checkpoint owner sai");
            owner_[kv.key()] = owner;queue_[kv.key()].clear();
            for (const auto& item : kv.value().at("waiting")) {
                const auto waiter = item.get<std::string>();
                if (!validIdentifier(waiter) || waiter == owner) throw std::invalid_argument("Checkpoint waiter sai");
                queue_[kv.key()].push_back(waiter);
            }
            if (!owner.empty()) grants_[kv.key()] = Grant{kv.value().value("grant_id",0L),false,true,{}};
        }
    }

    std::string currentOwner(const std::string& zoneId) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = owner_.find(zoneId);
        return it == owner_.end() ? "" : it->second;
    }
    bool confirmClear(const std::string& zone,const std::string& owner,long grantId) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!owner_.count(zone) || owner_.at(zone) != owner || !grants_.count(zone) || grants_.at(zone).id != grantId) return false;
        auto& q = queue_[zone];
        if (q.empty()) {owner_[zone].clear();grants_.erase(zone);}
        else {const auto next = q.front();q.pop_front();assignNoLock(zone,next);}
        return true;
    }

    // Xe loi/offline khong duoc tiep tuc xep hang xin vung; neu dang la owner
    // thi van giu quyen den khi co chung cu da roi vung.
    void cancelWaiting(const std::string& agvId) {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& item : queue_)
            item.second.erase(std::remove(item.second.begin(),item.second.end(),agvId),item.second.end());
    }

    // Gỡ AGV khỏi mọi zone + mọi hàng đợi (khi xe bị xoá khỏi hệ thống, mất kết nối hoặc lỗi nặng).
    // Với mỗi zone xe này đang giữ mà CÓ xe đang xếp hàng, quyền được trao luôn cho xe đầu hàng.
    // Trả về danh sách {zoneId, xe_mới_được_cấp} để Server publish "mutex_grant" cho các xe đó
    // (nếu không, xe đang chờ sẽ đứng mãi vì không ai báo cho nó biết zone đã trống).
    // Nơi gọi cũ không dùng giá trị trả về vẫn biên dịch bình thường.
    std::vector<std::pair<std::string, std::string>> forceRelease(const std::string& agvId) {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::pair<std::string, std::string>> grants;
        // Bỏ xe này khỏi mọi hàng đợi trước, để nó không bị "cấp quyền cho chính mình"
        for (auto& kv : queue_) kv.second.erase(std::remove(kv.second.begin(), kv.second.end(), agvId), kv.second.end());
        for (auto& kv : owner_) {
            if (kv.second != agvId) continue;
            auto qit = queue_.find(kv.first);
            if (qit != queue_.end() && !qit->second.empty()) {
                const auto next = qit->second.front();
                qit->second.pop_front();
                assignNoLock(kv.first,next);
                grants.push_back({kv.first, kv.second});
            } else {
                kv.second.clear();
                grants_.erase(kv.first);
            }
        }
        return grants;
    }

    void resetZones(const std::vector<std::string>& ids) {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& kv : owner_) if (!kv.second.empty()) throw std::runtime_error("Van co xe giu mutex zone");
        owner_.clear(); queue_.clear(); grants_.clear();
        for (const auto& id : ids) owner_[id] = "";
    }

    json toJson() {
        std::lock_guard<std::mutex> lk(mtx_);
        json j = json::object();
        for (auto& kv : owner_) {
            json z;
            z["owner"] = kv.second;
            json waiters = json::array();
            auto qit = queue_.find(kv.first);
            if (qit != queue_.end()) for (auto& w : qit->second) waiters.push_back(w);
            z["waiting"] = waiters;
            const auto grant = grants_.find(kv.first);
            if (grant != grants_.end()) {
                z["grant_id"] = grant->second.id;z["grant_acked"] = grant->second.acked;
                z["recovery_required"] = grant->second.recovery;
            }
            j[kv.first] = z;
        }
        return j;
    }

private:
    struct Grant {long id = 0;bool acked = false;bool recovery = false;std::chrono::steady_clock::time_point sent{};};
    void assignNoLock(const std::string& zone,const std::string& robot) {
        owner_[zone] = robot;
        // Unit-only fallback 0 preserves legacy direct Arbiter tests; core always installs allocator.
        grants_[zone] = Grant{allocator_ ? allocator_() : 0,false,false,{}};
    }
    std::unordered_map<std::string,Grant> grants_;
    std::function<long()> allocator_;
    int retryMs_ = 1000;
    std::unordered_map<std::string, std::string> owner_;             // zoneId -> agvId đang giữ ("" = trống)
    std::unordered_map<std::string, std::deque<std::string>> queue_; // zoneId -> danh sách agvId đang chờ
    std::mutex mtx_;
};
