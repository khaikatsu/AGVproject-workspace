#pragma once
// Điều phối đội robot: Dijkstra có trọng số, task_id và trạng thái từ telemetry.
// Command MQTT giữ JSON cmd/cmd_id; ACK text chỉ xác nhận nhận lệnh.
// Mất telemetry/ACK/lỗi: task gián đoạn, giữ vùng/đường chưa xác minh; không tự giao lại.
// Notify/event gọi sau khi nhả mtx_; server serialize mutation bằng g_stateMtx.
// Command ID allocator trên server lưu block trước khi cấp số (CommandIds.hpp).
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <functional>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include "utils/json.hpp"
#include "map/MapGraph.hpp"
#include "core/NodeManeuver.hpp"
#include "core/EncoderOdometry.hpp"
#include "utils/Mutexarbiter.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

enum class AGVState { IDLE, WORKING, CHARGING_EN_ROUTE, CHARGING, BLOCKED, AGV_ERROR, OFFLINE, UNLOCALIZED };

inline std::string toString(AGVState s) {
    switch (s) {
        case AGVState::IDLE: return "IDLE";
        case AGVState::WORKING: return "WORKING";
        case AGVState::CHARGING_EN_ROUTE: return "CHARGING_EN_ROUTE";
        case AGVState::CHARGING: return "CHARGING";
        case AGVState::BLOCKED: return "BLOCKED";
        case AGVState::AGV_ERROR: return "ERROR";
        case AGVState::UNLOCALIZED: return "UNLOCALIZED";
        default: return "OFFLINE";
    }
}

struct AGV {
    std::string id;
    std::string currentNode;
    int battery = 100;
    AGVState state = AGVState::OFFLINE;
    std::vector<std::string> activePath; // các node còn lại phía trước
    std::string taskId;
    int taskPriority = 0;
    bool hasTelemetry = false;
    bool positionValid = false;
    bool batteryMeasured = false;
    int batteryDisplayPercent = -1;
    bool batteryDisplayAvailable = false;
    bool needsRecharge = false;
    std::string lastError;
    std::vector<std::string> uncertainPath;
    std::string finalTarget;             // đích cuối cùng của task hiện tại
    bool isPriorityVehicle = false;      // xe ưu tiên (VD: chở hàng gấp)
    AGVState blockedResume = AGVState::IDLE; // trạng thái sẽ quay lại khi hết BLOCKED
    bool autoResetEligible = false;       // chỉ lỗi mạng mới được thử RESET_FAULT tự động
    bool autoResetRequested = false;
    Clock::time_point autoResetCandidateAt{};
    Clock::time_point lastSeen = Clock::now();
    std::string heading, headingNode, headingSource;
    bool headingSupported = false;
    bool stationDockingSupported = false, stationArrivalRequired = false, stationReady = false;
    Clock::time_point diagnosticSeen{};
    long routeCommandId = 0;
    bool nodeOrdersSupported = false;
    bool straightThroughSupported = false;
    std::string routeStart, lastNodeOrderKey, lastNodeOrderPayload;
    std::vector<std::string> commandPath;
    Clock::time_point lastNodeOrderAt{};
    EncoderObservation encoder;
    json estimatedPosition = nullptr;
    bool sonarObstacle = false;
    double sonarDistanceCm = 0;
    std::string sonarDirection;
    std::string sonarNextNode;
    long sonarRouteCommandId = 0;
    Clock::time_point sonarSeen{};
};

struct MotionDiagnostic {
    std::string node, state, error, motor;
    bool taskActive = true;
    bool wifi = false, mqtt = false;
    std::uint32_t rfidAgeMs = UINT32_MAX;
    int leftDuty = INT_MAX, rightDuty = INT_MAX;
    bool headingSupported = false;
    bool stationDockingSupported = false, stationReady = false;
    std::string heading, headingNode, headingSource;
    bool nodeOrdersSupported = false, awaitingNodeOrder = false, stationTurnRequired = false;
    long routeCommandId = 0;
    int pathIndex = -1;
    std::string nextNode;
    bool straightThroughSupported = false;
    bool sonarPresent = false, sonarObstacle = false;
    double sonarDistanceCm = 0;
    std::string sonarDirection;
};

struct Task {
    std::string targetNode;
    int priority = 0; // cao hơn = ưu tiên hơn
    std::string taskId;
    long seq = 0;      // thứ tự tạo, dùng để FIFO khi bằng priority
};

class FleetManager {
public:
    FleetManager(MapGraph& map, MutexArbiter& arbiter) : map_(map), arbiter_(arbiter) {}

    // Gán bởi Server: gửi lệnh MQTT xuống AGV, topic dạng "agv/{id}/command"
    void setPublishCallback(std::function<void(const std::string&, const std::string&)> cb) { publish_ = std::move(cb); }
    // Gán bởi Server: được gọi mỗi khi state đổi -> Server đẩy lên WebSocket
    void setStateChangedCallback(std::function<void()> cb) { onChanged_ = std::move(cb); }
    // Gán bởi Server: nhận thông báo sự kiện dạng text (offline, lỗi, trả task...) để log/cảnh báo
    void setEventCallback(std::function<void(const std::string&)> cb) { onEvent_ = std::move(cb); }
    void setCommandIdAllocator(std::function<long()> allocator) { commandIdAllocator_ = std::move(allocator); }
    void setTaskIdAllocator(std::function<std::string()> allocator) { taskIdAllocator_ = std::move(allocator); }
    std::string createTaskId() { std::lock_guard<std::mutex> lk(mtx_);return nextTaskIdNoLock(); }
    json durableSnapshot() {
        auto state = toJson();
        for (auto& a : state["agvs"]) {
            a.erase("last_seen_s");a.erase("online");
            for (const auto* key : {"encoder_supported","speed_cm_s","odometry","estimated_position","obstacle_detection","obstacle_alert"}) a.erase(key);
        }
        return state;
    }
    void restore(const json& state) {
        std::lock_guard<std::mutex> lk(mtx_);
        fleet_.clear();tasks_ = {};taskRecords_.clear();pendingCmd_.clear();
        seqCounter_ = state.at("queue_sequence").get<long>();
        for (auto record : state.at("tasks")) {
            const auto id = record.at("task_id").get<std::string>();
            if (!validIdentifier(id) || !map_.nodeExists(record.at("target").get<std::string>())) throw std::invalid_argument("Checkpoint task sai");
            if (record.at("status") == "dispatched") {
                record["status"] = "interrupted";record["reason"] = "Core khoi dong lai; can xac minh robot";
            }
            taskRecords_[id] = record;
        }
        for (const auto& item : state.at("pending_tasks")) {
            const auto id = item.at("task_id").get<std::string>();
            if (!taskRecords_.count(id) || taskRecords_[id]["status"] != "pending") throw std::invalid_argument("Checkpoint queue sai");
            tasks_.push(Task{item.at("target").get<std::string>(),item.at("priority").get<int>(),id,item.at("seq").get<long>()});
        }
        for (const auto& item : state.at("agvs")) {
            AGV a;a.id = item.at("id").get<std::string>();a.currentNode = item.at("node").get<std::string>();
            if (!validIdentifier(a.id) || (!a.currentNode.empty() && !map_.nodeExists(a.currentNode))) throw std::invalid_argument("Checkpoint robot sai");
            a.battery = item.at("battery").get<int>();a.needsRecharge = item.value("needs_recharge",false);
            a.hasTelemetry = true;a.positionValid = false;a.batteryMeasured = false;a.state = AGVState::OFFLINE;
            a.lastSeen = Clock::now()-offlineTimeout_-std::chrono::seconds(1);
            a.uncertainPath = item.at("uncertain_path").get<std::vector<std::string>>();
            for (const auto& node : item.at("path")) if (std::find(a.uncertainPath.begin(),a.uncertainPath.end(),node.get<std::string>()) == a.uncertainPath.end()) a.uncertainPath.push_back(node);
            a.lastError = "Core khoi dong lai; can RFID moi va xac minh robot";fleet_[a.id] = a;
        }
        chargeReservation_ = state.at("charge_reservations").get<decltype(chargeReservation_)>();
        acceptingTasks_ = false;
    }
    void reportOffline(const std::string& id) {
        {
            std::lock_guard<std::mutex> lk(mtx_);auto it = fleet_.find(id);if (it == fleet_.end()) return;
            interruptWorkNoLock(it->second,"MQTT LWT offline");it->second.state = AGVState::OFFLINE;
            it->second.lastSeen = Clock::now()-offlineTimeout_-std::chrono::seconds(1);pendingCmd_.erase(id);
            it->second.lastError = "MQTT LWT; chuyen dong chua xac minh";
            it->second.autoResetEligible = true;it->second.autoResetRequested = false;
            it->second.autoResetCandidateAt = {};
        }
        arbiter_.cancelWaiting(id);
        emit("AGV_"+id+" MAT KET NOI (LWT), giu mutex");notify();
    }
    void setNetworkAvailable(bool available) { std::lock_guard<std::mutex> lk(mtx_); networkAvailable_ = available; }
    // Broker loss can hide LWT messages. A working robot must not silently resume.
    void reportNetworkLoss() {
        std::vector<std::string> interrupted;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            networkAvailable_ = false;pendingCmd_.clear();
            for (auto& kv : fleet_) {
                auto& a = kv.second;
                if (!a.hasTelemetry || a.state == AGVState::OFFLINE || a.state == AGVState::UNLOCALIZED) continue;
                interruptWorkNoLock(a,"Mat MQTT broker; can xac minh robot");
                a.state = AGVState::AGV_ERROR;a.lastError = "Mat MQTT broker; can RFID moi va xoa loi";
                a.autoResetEligible = true;a.autoResetRequested = false;a.autoResetCandidateAt = {};
                interrupted.push_back(kv.first);
            }
        }
        for (const auto& id : interrupted) arbiter_.cancelWaiting(id);
        notify();
    }
    void setAcceptingTasks(bool value) { std::lock_guard<std::mutex> lk(mtx_); acceptingTasks_ = value; }
    void setSingleRobotTrial(bool value) { std::lock_guard<std::mutex> lk(mtx_); singleRobotTrial_ = value; }
    void setTwoRobotTrial(bool value) { std::lock_guard<std::mutex> lk(mtx_); twoRobotTrial_ = value; }
    void setTrialRobot(const std::string& id) {
        if (!id.empty() && id != "01" && id != "02") throw std::invalid_argument("Xe thu phai la 01 hoac 02");
        std::lock_guard<std::mutex> lk(mtx_); trialRobotId_ = id;
    }
    void setMonitorOnlyRobot(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_);
        monitorOnlyRobots_.insert(id);
    }
    bool acceptingTasks() { std::lock_guard<std::mutex> lk(mtx_); return acceptingTasks_; }
    bool networkAvailable() { std::lock_guard<std::mutex> lk(mtx_); return networkAvailable_; }
    void setHomeStations(const std::unordered_map<std::string,std::string>& homes) {
        std::lock_guard<std::mutex> lk(mtx_); homeStations_ = homes;
    }

    // Về trạm riêng khi rảnh; không thay task đang chạy, không sửa vị trí đo.
    std::string returnToHome(const std::string& id, const std::string& station) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (monitorOnlyRobots_.count(id)) return "";
            if (!trialRobotId_.empty()) return "Thu rieng mot xe: cho RFID tai tram, khong tu ve sac";
            if (singleRobotTrial_) return "Thu mot xe chi nhan lenh GOTO truc tiep";
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return "Chua nhan telemetry";
            auto& a = it->second;
            if (!networkAvailable_) return "Mat ket noi MQTT";
            if (!a.hasTelemetry || !a.positionValid || Clock::now()-a.lastSeen > offlineTimeout_)
                return "Cho vi tri RFID moi";
            if (a.state == AGVState::OFFLINE || a.state == AGVState::AGV_ERROR || !a.uncertainPath.empty())
                return "Can xac minh xe/xoa loi";
            if (hasWork(a)) return "Cho hoan thanh task hien tai";
            if (!isChargingStationNode(station)) return "Tram sac khong ton tai";
            const auto reservation = chargeReservation_.find(station);
            if (reservation != chargeReservation_.end() && reservation->second != id)
                return "Tram sac dang duoc xe khac giu";
            if (a.currentNode == station) {
                if (a.state == AGVState::CHARGING) return "";
                chargeReservation_[station] = id;
                a.activePath.clear(); a.finalTarget.clear(); a.state = AGVState::CHARGING;
                sendCommand(a,"CHARGE_START");
            } else {
                if (a.state == AGVState::CHARGING_EN_ROUTE && a.finalTarget == station) return "";
                if (a.state == AGVState::CHARGING) return "Xe dang sac o tram khac";
                auto route = computePathNoLock(a,station);
                if (route.empty()) {
                    a.state = AGVState::BLOCKED; a.blockedResume = AGVState::CHARGING_EN_ROUTE;
                    a.finalTarget = station;
                    return "Khong co duong ve tram sac";
                }
                chargeReservation_[station] = id;
                assignPath(a,route,station,AGVState::CHARGING_EN_ROUTE);
            }
        }
        notify();
        return "";
    }

    // ShiftController đã xác minh mọi xe ở đúng trạm và đủ pin trước khi gọi.
    void beginWorkFromHome(const std::unordered_map<std::string,std::string>& homes) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for (const auto& home : homes) {
                auto& a = fleet_.at(home.first);
                a.state = AGVState::IDLE; a.blockedResume = AGVState::IDLE;
                releaseStationReservationNoLock(a.id);
                sendCommand(a,"WAIT");
            }
            acceptingTasks_ = true;
        }
        dispatchPending(); notify();
    }

    // Tương thích ngược: chỉ đặt ngưỡng "pin thấp -> đi sạc". Nên dùng setChargeThresholds() thay thế.
    void setBatteryThresholds(int lowPercent) {
        if (lowPercent < 0 || lowPercent >= 100) throw std::invalid_argument("Nguong pin sai");
        std::lock_guard<std::mutex> lk(mtx_);
        lowBattery_ = lowPercent;
        if (resumeBattery_ <= lowBattery_) resumeBattery_ = lowBattery_ + 1;
    }
    // lowPercent: pin <= mức này -> tự động về sạc.
    // resumePercent: CHỈ nhận task mới khi pin >= mức này (phải > lowPercent, tránh nhận-rồi-lại-huỷ
    //   liên tục quanh 1 ngưỡng, VD pin 21% nhận task rồi vài phút sau lại bị huỷ để đi sạc).
    // targetPercent: pin coi là "sạc xong" nếu ESP32 không báo kèm % pin thật lúc charging_done.
    void setChargeThresholds(int lowPercent, int resumePercent, int targetPercent = 90) {
        if (lowPercent < 0 || lowPercent >= resumePercent || resumePercent > targetPercent || targetPercent > 100)
            throw std::invalid_argument("Can 0 <= low < resume <= target <= 100");
        std::lock_guard<std::mutex> lk(mtx_);
        lowBattery_ = lowPercent;
        resumeBattery_ = (resumePercent > lowPercent) ? resumePercent : lowPercent + 1;
        chargeTargetPercent_ = targetPercent;
    }
    // Bao lâu chờ ACK trước khi gửi lại cùng 1 lệnh (mặc định 3000ms)
    void setAckTimeout(int milliseconds) {
        std::lock_guard<std::mutex> lk(mtx_);
        ackTimeout_ = std::chrono::milliseconds(milliseconds > 0 ? milliseconds : 100);
    }
    // Số lần gửi lại tối đa trước khi coi là mất liên lạc lệnh (mặc định 3)
    void setMaxRetries(int n) {
        std::lock_guard<std::mutex> lk(mtx_);
        maxRetries_ = n >= 0 ? n : 0;
    }
    // Sau bao nhiêu giây không nhận telemetry thì coi xe là OFFLINE (mặc định 10s)
    void setOfflineTimeout(int seconds) {
        std::lock_guard<std::mutex> lk(mtx_);
        offlineTimeout_ = std::chrono::seconds(seconds > 0 ? seconds : 1);
    }
    // true (mặc định): telemetry từ xe chưa đăng ký sẽ tự tạo xe. false: bỏ qua xe lạ.
    void setAutoRegister(bool on) {
        std::lock_guard<std::mutex> lk(mtx_);
        autoRegister_ = on;
    }
    // false (mặc định): giữ quyền zone khi mất telemetry; vị trí vật lý chưa được xác minh.
    // Lưu ý: xe offline có thể vẫn đứng trong zone -> đặt false nếu ưu tiên an toàn hơn thông suốt.
    void setReleaseZonesOnOffline(bool on) {
        std::lock_guard<std::mutex> lk(mtx_);
        releaseZonesOnOffline_ = on;
    }

    // ---------------- Quản lý xe ----------------
    void addAGV(const std::string& id, const std::string& startNode, bool isPriority = false) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            AGV a; a.id = id; a.currentNode = startNode; a.state = AGVState::IDLE;
            a.isPriorityVehicle = isPriority; a.lastSeen = Clock::now();
            fleet_[id] = a;
        }
        notify();
        dispatchPending();
    }

    void removeAGV(const std::string& id) {
        bool requeued = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it != fleet_.end()) {
                requeued = requeueWorkNoLock(it->second);
                fleet_.erase(it);
            }
        }
        releaseAgvZones(id);
        if (requeued) emit("AGV_" + id + " bi xoa khi dang co task -> tra task ve hang doi");
        notify();
        dispatchPending();
    }

    // ---------------- Nhận dữ liệu từ ESP32 (telemetry) ----------------
    // obstacleAhead = true nếu cảm biến AGV phát hiện vật cản ngay phía trước
    bool updateTelemetry(const std::string& id, const std::string& rfid, int battery, bool obstacleAhead) {
        const bool nodeValid = map_.nodeExists(rfid);
        std::vector<std::string> toReplan;
        bool justFinishedCharging = false;
        bool recovered = false;
        bool unknownNode = false;
        bool positionChanged = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (singleRobotTrial_ && id != "01") {
                auto solo = fleet_.find("01");
                if (solo != fleet_.end() && !solo->second.finalTarget.empty()) {
                    interruptWorkNoLock(solo->second, "Xe khac xuat hien trong thu mot xe");
                    solo->second.state = AGVState::AGV_ERROR;
                    solo->second.lastError = "Xe khac xuat hien; dung va xac minh duong";
                    sendCommand(solo->second, "WAIT");
                }
            }
            auto it = fleet_.find(id);
            if (it == fleet_.end()) {
                if (!autoRegister_) return false;
                AGV a; a.id = id;               // state = OFFLINE -> nhánh recovered bên dưới đưa về IDLE
                it = fleet_.emplace(id, a).first;
            }
            AGV& agv = it->second;
            const std::string prevNode = agv.currentNode;

            const bool seenBefore = agv.hasTelemetry;
            positionChanged = !seenBefore || agv.positionValid != nodeValid || (nodeValid && rfid != prevNode);
            agv.hasTelemetry = true;
            agv.positionValid = nodeValid;
            agv.batteryMeasured = battery >= 0;
            if (agv.batteryMeasured) {
                agv.battery = std::clamp(battery, 0, 100);
                if (agv.battery <= lowBattery_) agv.needsRecharge = true;
                else if (agv.battery >= resumeBattery_) agv.needsRecharge = false;
            } else agv.battery = -1;
            agv.lastSeen = Clock::now();
            if (nodeValid) agv.currentNode = rfid; else unknownNode = rfid != "UNKNOWN";

            // UNKNOWN: có heartbeat nhưng chưa xác nhận vị trí. Không gán trạm/node giả.
            if (rfid == "UNKNOWN" && agv.state != AGVState::AGV_ERROR) {
                if (!agv.finalTarget.empty() || !agv.taskId.empty() || !agv.uncertainPath.empty() ||
                    (agv.state == AGVState::OFFLINE && seenBefore)) {
                    interruptWorkNoLock(agv,"Mat xac nhan RFID / robot khoi dong lai");
                    agv.state = AGVState::AGV_ERROR;
                    agv.lastError = "Can xac minh RFID va duong robot truoc khi xoa loi";
                    sendCommand(agv,"WAIT");
                } else if (agv.state != AGVState::UNLOCALIZED) {
                    agv.state = AGVState::UNLOCALIZED;
                    sendCommand(agv,"WAIT");
                }
            } else if (nodeValid && agv.state == AGVState::UNLOCALIZED) {
                agv.state = AGVState::IDLE; recovered = true;
                sendCommand(agv,"WAIT");
            }

            // Xe mất kết nối nay đã quay lại. (ERROR KHÔNG tự xoá — dùng clearError.)
            if (agv.state == AGVState::OFFLINE) {
                agv.state = seenBefore ? AGVState::AGV_ERROR : AGVState::IDLE;
                if (seenBefore) agv.lastError = "Ket noi lai: can xac minh robot va xoa loi thu cong";
                agv.blockedResume = AGVState::IDLE;
                recovered = true;
                // Kế hoạch cũ đã bị huỷ khi xe offline -> bảo xe dừng chờ lệnh mới.
                // Gửi trong lúc đang giữ mtx_ (như assignPath) để pendingCmd_ nhất quán.
                sendCommand(agv, "WAIT");
            }

            // Cập nhật tiến độ đường đi (chịu được việc bỏ lỡ thẻ RFID)
            if (nodeValid && !agv.activePath.empty()) {
                auto pit = std::find(agv.activePath.begin(), agv.activePath.end(), rfid);
                if (pit != agv.activePath.end()) {
                    agv.activePath.erase(agv.activePath.begin(), pit + 1); // cắt tới node vừa qua
                } else if (rfid != prevNode) {
                    toReplan.push_back(id); // xe đã dịch chuyển nhưng không nằm trên đường -> lệch đường
                }
            }

            if (obstacleAhead && !agv.activePath.empty()) {
                map_.setObstacle(agv.activePath.front(), true, obstacleTtlSeconds_);
                if (std::find(toReplan.begin(), toReplan.end(), id) == toReplan.end())
                    toReplan.push_back(id);
            }

            if (nodeValid) completeArrivalNoLock(agv);
        }
        if (unknownNode) emit("AGV_" + id + " gui RFID khong ton tai tren map: " + rfid);
        if (recovered) emit("AGV_" + id + " ket noi lai (ONLINE)");
        (void)justFinishedCharging; // đã gửi lệnh ở trên trong lúc còn giữ mtx_
        for (auto& id2 : toReplan) replanAGV(id2); // né vật cản / lệch đường

        checkBatteryOne(id);
        dispatchPending();
        notify();
        return positionChanged;
    }

    // Display-only percentage from firmware diagnostics; it never changes task or charge decisions.
    void updateBatteryDisplay(const std::string& id, int percent) {
        if (percent < -1 || percent > 100) throw std::invalid_argument("Phan tram pin hien thi sai");
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end() || !it->second.hasTelemetry) return;
            AGV& agv = it->second;
            const bool available = percent >= 0;
            changed = agv.batteryDisplayAvailable != available ||
                (available && agv.batteryDisplayPercent != percent);
            agv.batteryDisplayAvailable = available;
            agv.batteryDisplayPercent = available ? percent : -1;
        }
        if (changed) notify();
    }

    // Quan sát hiển thị chỉ ở Core test 01. Không notify/checkpoint, không cập nhật node/task/mutex.
    bool encoderEnabled(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_);
        return id == "01" && trialRobotId_ == "01";
    }
    bool bindEncoderSession(const std::string& id, uint32_t boot, uint32_t connection, uint32_t uptime) {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = fleet_.find(id);
        if (id == "01" && trialRobotId_ == "01" && it != fleet_.end())
            return it->second.encoder.bind(boot, connection, uptime);
        return false;
    }
    // QoS0 error events may be lost. A current, bound diagnostic must not leave
    // Core IDLE while the MCU reports a latched fault. It never clears a fault.
    void observeEncoderFirmwareFault(const std::string& id, const MotionDiagnostic& d) {
        if (d.error.empty() || d.error == "NONE" || (d.state != "AGV_SAFE_STOP" && d.state != "AGV_ERROR")) return;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            const auto it = fleet_.find(id);
            if (id != "01" || trialRobotId_ != "01" || it == fleet_.end() || !encoderReadyNoLock(it->second) ||
                !it->second.encoder.boot || d.node != it->second.currentNode || !d.wifi || !d.mqtt) return;
        }
        reportError(id,d.error);
    }
    bool observeOdometry(const std::string& id, const EncoderSample& s) {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = fleet_.find(id);
        if (id != "01" || trialRobotId_ != "01" || it == fleet_.end()) return false;
        auto& a = it->second;
        if (!encoderReadyNoLock(a) || a.currentNode != s.from) return false;
        if (s.tracking && (a.routeCommandId != s.route || a.activePath.empty() || a.activePath.front() != s.to ||
            a.commandPath.size() <= static_cast<size_t>(s.index) || a.commandPath[s.index] != s.to)) return false;
        if (!s.tracking && !a.activePath.empty()) return false;
        json estimate = nullptr;
        if (s.tracking) {
            const auto layout = map_.toJson();
            bool edge = false;
            for (const auto& e : layout["edges"]) if ((e["from"] == s.from && e["to"] == s.to) ||
                (e.value("two_way",false) && e["to"] == s.from && e["from"] == s.to)) {edge = true;break;}
            json from, to;
            for (const auto& n : layout["nodes"]) {if (n["id"] == s.from) from = n;if (n["id"] == s.to) to = n;}
            if (!edge || from.is_null() || to.is_null()) return false;
            const double ax = from["x"], ay = from["y"], bx = to["x"], by = to["y"];
            const double length = std::hypot(bx-ax,by-ay);
            if (!std::isfinite(length) || length <= 0) return false;
            // Map test 01 dùng cm. Giữ marker trước đích cho tới khi có RFID thật.
            const double progress = std::clamp(s.distance / length,0.0,0.98);
            estimate = {{"source","encoder"},{"from",s.from},{"to",s.to},{"progress",progress},
                {"x",ax+(bx-ax)*progress},{"y",ay+(by-ay)*progress},{"distance_cm",s.distance},
                {"edge_length_cm",length},{"route_cmd_id",s.route},{"path_index",s.index},{"reverse",s.reverse}};
        }
        if (!a.encoder.accept(s)) return false;
        a.estimatedPosition = estimate;
        return true;
    }

    // Diagnostic khong retained duoc gui ngay sau telemetry. Chi phuc hoi ca
    // thu hai xe tai tram rieng; telemetry pipe mot minh khong noi len loi MCU.
    void observeMotionDiagnostic(const std::string& id, const MotionDiagnostic& diagnostic) {
        std::string sonarEvent;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (id == "01" && trialRobotId_ == "01" && it != fleet_.end() &&
                diagnostic.sonarPresent && diagnostic.wifi && diagnostic.mqtt &&
                diagnostic.node == it->second.currentNode && networkAvailable_) {
                auto& a = it->second;
                if (a.sonarObstacle != diagnostic.sonarObstacle ||
                    (diagnostic.sonarObstacle && a.sonarDirection != diagnostic.sonarDirection))
                    sonarEvent = diagnostic.sonarObstacle
                        ? "AGV_01 VAT CAN PHIA " + std::string(diagnostic.sonarDirection == "rear" ? "SAU" : "TRUOC") +
                          " (" + std::to_string(static_cast<int>(std::lround(diagnostic.sonarDistanceCm))) + " cm); dang dung cho"
                        : "AGV_01 DA HET VAT CAN; tiep tuc tuyen neu cac chot an toan hop le";
                a.sonarObstacle = diagnostic.sonarObstacle;
                a.sonarDistanceCm = diagnostic.sonarDistanceCm;
                a.sonarDirection = diagnostic.sonarDirection;
                a.sonarNextNode = diagnostic.nextNode;
                a.sonarRouteCommandId = diagnostic.routeCommandId;
                a.sonarSeen = Clock::now();
            }
        }
        if (!sonarEvent.empty()) { emit(sonarEvent); notify(); }
        observeHeadingDiagnostic(id, diagnostic);
        observeNodeOrderRequest(id, diagnostic);
        observeRobot01Reset(id, diagnostic);
        std::vector<std::pair<std::string,std::string>> newGrants;
        std::vector<std::string> clearedZones;
        bool changed = false, recovered = false, resetRequested = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end() || !supervisedTrialNoLock() || !networkAvailable_) return;
            AGV& agv = it->second;
            const auto home = homeStations_.find(id);
            const bool expectedHome = (id == "01" && home != homeStations_.end() && home->second == "S1") ||
                (id == "02" && home != homeStations_.end() && home->second == "S2");
            if (!expectedHome || !agv.hasTelemetry || !agv.positionValid ||
                Clock::now() - agv.lastSeen > offlineTimeout_ || agv.currentNode != home->second ||
                diagnostic.node != home->second || !diagnostic.wifi || !diagnostic.mqtt ||
                diagnostic.motor != "STOP" || diagnostic.leftDuty != 0 || diagnostic.rightDuty != 0 ||
                diagnostic.taskActive || diagnostic.rfidAgeMs > 250 ||
                !agv.taskId.empty() || !agv.activePath.empty() || !agv.finalTarget.empty()) return;

            // Sai lech duong cu duoc giai toa boi RFID moi o tram, cach zone
            // it nhat 50 cm theo map da do. Khong nhan UNKNOWN/mot node gan zone.
            const auto zones = arbiter_.toJson();
            bool allClaimsClear = true;
            for (const auto& item : zones.items()) {
                const auto& zone = item.key();
                const auto& claim = item.value();
                if (claim.value("owner",std::string{}) != id) continue;
                if (!claim.contains("grant_id")) {allClaimsClear = false;continue;}
                const auto distance = map_.undirectedDistance(home->second, zone);
                const auto straight = map_.straightLineDistance(home->second, zone);
                if (distance == LLONG_MAX || distance < 50 || !std::isfinite(straight) || straight < 50.0) {
                    allClaimsClear = false;continue;
                }
                const auto grantId = claim.at("grant_id").get<long>();
                if (!arbiter_.confirmClear(zone,id,grantId)) {allClaimsClear = false;continue;}
                const auto next = arbiter_.currentOwner(zone);
                if (!next.empty()) newGrants.push_back({zone,next});
                clearedZones.push_back(zone);changed = true;
            }
            if (allClaimsClear) {
                if (!agv.uncertainPath.empty()) {agv.uncertainPath.clear();changed = true;}

                const bool healthy = diagnostic.state == "AGV_IDLE" && diagnostic.error == "NONE";
                if (agv.state == AGVState::AGV_ERROR && healthy) {
                    agv.state = AGVState::IDLE;agv.lastError.clear();
                    agv.autoResetEligible = false;agv.autoResetRequested = false;agv.autoResetCandidateAt = {};
                    recovered = changed = true;
                } else if (agv.state == AGVState::AGV_ERROR && agv.autoResetEligible &&
                           !agv.autoResetRequested && diagnostic.state == "AGV_SAFE_STOP" &&
                           (diagnostic.error == "NETWORK_UNAVAILABLE" ||
                            diagnostic.error == "WIFI_TIMEOUT" || diagnostic.error == "MQTT_TIMEOUT")) {
                    const auto now = Clock::now();
                    if (agv.autoResetCandidateAt.time_since_epoch().count() == 0)
                        agv.autoResetCandidateAt = now;
                    else if (now - agv.autoResetCandidateAt >= std::chrono::milliseconds(300)) {
                        agv.autoResetRequested = true;
                        sendCommand(agv,"RESET_FAULT");resetRequested = true;
                    }
                }
            }
        }
        for (const auto& grant : newGrants) {
            const auto payload = arbiter_.grant(grant.first,grant.second);
            if (!payload.is_null() && publish_)
                publish_("agv/" + grant.second + "/mutex_grant",payload.dump());
        }
        for (const auto& zone : clearedZones)
            emit("AGV_" + id + " tai tram: tu tra mutex " + zone + " sau RFID/diagnostic moi");
        if (resetRequested) emit("AGV_" + id + " tai tram: gui RESET_FAULT mot lan sau loi mang");
        if (recovered) emit("AGV_" + id + " diagnostic binh thuong: tu xoa loi Core");
        if (changed || resetRequested) {notify();dispatchPending();}
    }

    // Gọi khi AGV báo đã sạc xong (topic agv/{id}/charging_done).
    // finalBatteryPercent: % pin thật lúc ESP32 báo xong, nếu ESP32 không gửi kèm (payload rỗng)
    // thì truyền -1 để dùng chargeTargetPercent_ (mặc định 90) thay vì ép cứng 100 như bản cũ.
    void reportCharged(const std::string& id, int finalBatteryPercent = -1) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return;
            AGV& agv = it->second;
            if (agv.state != AGVState::CHARGING) return;
            agv.battery = std::clamp(finalBatteryPercent >= 0 ? finalBatteryPercent : chargeTargetPercent_, 0, 100);
            agv.batteryMeasured = finalBatteryPercent >= 0;
            agv.needsRecharge = !agv.batteryMeasured || agv.battery < resumeBattery_;
            agv.state = AGVState::IDLE;
            releaseStationReservationNoLock(id); // trả chỗ trạm sạc cho xe khác
        }
        notify();
        dispatchPending();
    }

    // AGV báo lỗi: gián đoạn nhiệm vụ và giữ owner vùng; chỉ diagnostic khỏe tại
    // trạm đủ xa (ca thử hai xe) hoặc clearError thủ công mới xóa ERROR.
    void reportError(const std::string& id, const std::string& msg) {
        std::vector<std::string> toReplan;
        bool requeued = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return;
            AGV& agv = it->second;
            requeued = interruptWorkNoLock(agv, msg);
            agv.lastError = msg;
            agv.state = AGVState::AGV_ERROR;
            agv.autoResetEligible = msg == "NETWORK_UNAVAILABLE" || msg == "WIFI_TIMEOUT" || msg == "MQTT_TIMEOUT";
            agv.autoResetRequested = false;agv.autoResetCandidateAt = {};
            toReplan = agvsRoutedThroughNoLock(agv.currentNode, id); // xe lỗi đang chắn node này
            sendCommand(agv, "STOP");
        }
        arbiter_.cancelWaiting(id);
        emit("AGV_" + id + " BAO LOI: " + msg + (requeued ? " -> task bi gian doan, can xac minh" : ""));
        for (auto& o : toReplan) replanAGV(o);
        notify();
        dispatchPending();
    }

    void observeHeadingDiagnostic(const std::string& id, const MotionDiagnostic& d) {
        bool changed = false;
        bool finished = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id); if (it == fleet_.end()) return;
            auto& a = it->second;
            const bool supported = d.headingSupported && d.wifi && d.mqtt && networkAvailable_;
            const bool valid = supported && a.positionValid && a.currentNode == d.node && d.headingNode == d.node &&
                (d.heading == "N" || d.heading == "E" || d.heading == "S" || d.heading == "W") &&
                Clock::now() - a.lastSeen <= offlineTimeout_;
            const std::string heading = valid && (d.headingSource == "bay_default" || d.headingSource == "route") ? d.heading : "";
            changed = a.heading != heading || a.headingSupported != supported || a.headingNode != d.headingNode ||
                      a.headingSource != d.headingSource;
            a.heading = heading; a.headingNode = d.headingNode; a.headingSource = d.headingSource;
            a.headingSupported = supported; a.diagnosticSeen = Clock::now();
            const bool station = a.currentNode == "S1" || a.currentNode == "S2";
            const bool capability = d.stationDockingSupported && supported;
            // Invalidate every command's old ready sample. Completion needs a
            // fresh stopped diagnostic at the confirmed RFID, facing the GS gate.
            // The front reader can leave the tag during the parking pivot;
            // RFID age is an anchor age, not the diagnostic packet's freshness.
            const bool ready = capability && valid && station && d.stationReady &&
                heading == (a.currentNode == "S1" ? "S" : "N") &&
                d.state == "AGV_IDLE" && d.error == "NONE" && !d.taskActive &&
                d.motor == "STOP" && d.leftDuty == 0 && d.rightDuty == 0 && d.rfidAgeMs != UINT32_MAX;
            changed = changed || a.stationDockingSupported != capability || a.stationReady != ready;
            a.stationDockingSupported = capability;
            a.stationReady = ready;
            if (capability && (a.finalTarget == "S1" || a.finalTarget == "S2"))
                a.stationArrivalRequired = true;
            finished = completeArrivalNoLock(a);
        }
        if (changed || finished) notify();
        if (finished) dispatchPending();
    }

    void observeNodeOrderRequest(const std::string& id, const MotionDiagnostic& d) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = fleet_.find(id); if (it == fleet_.end()) return;
        auto& a = it->second;
        a.nodeOrdersSupported = d.nodeOrdersSupported && d.wifi && d.mqtt && networkAvailable_;
        a.straightThroughSupported = a.nodeOrdersSupported && d.straightThroughSupported;
        if (!d.nodeOrdersSupported || !d.awaitingNodeOrder || !d.taskActive ||
            d.state != "AGV_WAITING" || d.error != "NONE" || d.motor != "STOP" ||
            d.leftDuty != 0 || d.rightDuty != 0 || !d.wifi || !d.mqtt || !networkAvailable_ ||
            d.rfidAgeMs == UINT32_MAX || !a.positionValid || a.currentNode != d.node || a.headingNode != d.node ||
            a.heading.empty() || Clock::now()-a.lastSeen > offlineTimeout_ ||
            (a.state != AGVState::WORKING && a.state != AGVState::CHARGING_EN_ROUTE) ||
            d.routeCommandId <= 0 || d.routeCommandId != a.routeCommandId ||
            d.pathIndex < 0 || static_cast<size_t>(d.pathIndex) > a.commandPath.size()) return;
        const size_t step = static_cast<size_t>(d.pathIndex);
        const std::string node = step ? a.commandPath[step-1] : a.routeStart;
        if (node != d.node || a.commandPath.size()-a.activePath.size() != step) return;
        const std::string next = d.stationTurnRequired ? node : step < a.commandPath.size() ? a.commandPath[step] : "";
        if (next.empty() || (!d.stationTurnRequired && d.nextNode != next) ||
            (d.stationTurnRequired && node != "S1" && node != "S2")) return;
        const std::string key = std::to_string(a.routeCommandId)+":"+std::to_string(step)+(d.stationTurnRequired ? ":park" : ":route");
        if (key == a.lastNodeOrderKey) {
            // Retry the exact same command even if its receipt ACK arrived but
            // the queued action has not yet changed the diagnostic phase.
            if (Clock::now()-a.lastNodeOrderAt >= std::chrono::seconds(1) && !a.lastNodeOrderPayload.empty()) {
                if (publish_) publish_("agv/"+id+"/command",a.lastNodeOrderPayload);
                a.lastNodeOrderAt = Clock::now();
            }
            return;
        }
        auto order = NodeManeuver::plan(map_.toJson(),node,next,a.heading,d.stationTurnRequired);
        if (order.is_null()) return;
        order["node"] = node; order["next_node"] = next;
        order["route_cmd_id"] = a.routeCommandId; order["path_index"] = step;
        const auto commandId = sendCommand(a,"NODE_ORDER",order);
        if (commandId > 0) {
            a.lastNodeOrderKey = key; a.lastNodeOrderAt = Clock::now();
            a.lastNodeOrderPayload = pendingCmd_.at(id).payload;
        }
    }

    // Explicit robot01 test recovery may finish at the measured current node.
    // ACK does not clear the fault; mutex ownership is never released here.
    void observeRobot01Reset(const std::string& id, const MotionDiagnostic& d) {
        bool cleared = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (trialRobotId_ != "01" || id != "01" || it == fleet_.end() || !networkAvailable_) return;
            auto& a = it->second;
            if (!a.autoResetRequested || a.state != AGVState::AGV_ERROR ||
                !a.hasTelemetry || !a.positionValid || Clock::now()-a.lastSeen > offlineTimeout_ ||
                a.currentNode != d.node || d.rfidAgeMs == UINT32_MAX ||
                !d.wifi || !d.mqtt || d.taskActive || d.state != "AGV_IDLE" || d.error != "NONE" ||
                d.motor != "STOP" || d.leftDuty != 0 || d.rightDuty != 0 ||
                !a.taskId.empty() || !a.finalTarget.empty() || !a.activePath.empty()) return;
            a.uncertainPath.clear();
            a.state = AGVState::IDLE; a.lastError.clear();
            a.autoResetEligible = false; a.autoResetRequested = false; a.autoResetCandidateAt = {};
            cleared = true;
        }
        if (cleared) {
            emit("AGV_01 test: da reset tai RFID hien tai, cho phep nhan nhiem vu moi");
            notify(); dispatchPending();
        }
    }

    // Other supervised modes still reset at home; robot01 may reset at its current RFID.
    // ACK RESET_FAULT chi la bien nhan; khong mo IDLE trong clearError().
    bool clearError(const std::string& id) {
        bool awaitingDiagnostic = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return false;
            // An earlier Core could clear its own ERROR while the ESP32 kept
            // a latched fault. Permit an explicit repeat while safely IDLE.
            const bool latched = it->second.state == AGVState::AGV_ERROR;
            const bool idleRepair = supervisedTrialNoLock() && it->second.state == AGVState::IDLE &&
                it->second.taskId.empty() && it->second.activePath.empty() &&
                it->second.uncertainPath.empty();
            if (!latched && !idleRepair) return false;
            if (!it->second.positionValid || Clock::now() - it->second.lastSeen > offlineTimeout_) return false;
            if (!networkAvailable_) return false;
            awaitingDiagnostic = supervisedTrialNoLock();
            if (awaitingDiagnostic) {
                const auto home = homeStations_.find(id);
                const bool robot01Reset = trialRobotId_ == "01" && id == "01";
                if (!robot01Reset && (home == homeStations_.end() ||
                    (id == "01" ? home->second != "S1" : id != "02" || home->second != "S2") ||
                    it->second.currentNode != home->second)) return false;
                it->second.state = AGVState::AGV_ERROR;
                if (it->second.lastError.empty()) it->second.lastError = "Dang cho diagnostic sau RESET_FAULT";
            } else {
                it->second.uncertainPath.clear();
                it->second.lastError.clear();
                it->second.state = AGVState::IDLE;
            }
            it->second.autoResetEligible = false;it->second.autoResetRequested = awaitingDiagnostic;
            it->second.autoResetCandidateAt = {};
            sendCommand(it->second,"RESET_FAULT");
        }
        emit("AGV_" + id + (awaitingDiagnostic ? " da gui RESET_FAULT; cho diagnostic khoe tai RFID hien tai" : " da duoc xoa loi (IDLE)"));
        notify();
        dispatchPending();
        return true;
    }

    // ESP32 xác nhận đã nhận lệnh (topic agv/{id}/ack, payload = cmd_id). Ngăn resend không cần thiết.
    // Ack trễ (đến sau khi đã gửi lại hoặc đã hết hạn/đã báo lỗi) bị bỏ qua vì cmd_id không còn khớp.
    void reportAck(const std::string& id, long cmdId) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = pendingCmd_.find(id);
        if (it != pendingCmd_.end() && it->second.cmdId == cmdId) pendingCmd_.erase(it);
    }

    // Vật cản báo qua topic riêng (agv/{id}/obstacle), payload = nodeId phát hiện vật cản
    void setObstacleTtl(int seconds) { obstacleTtlSeconds_ = std::max(1,seconds); }
    void reportObstacle(const std::string& agvId, const std::string& nodeId, int ttlSeconds = -1) {
        map_.setObstacle(nodeId, true,ttlSeconds >= 0 ? ttlSeconds : obstacleTtlSeconds_);
        replanAGV(agvId);
        // Các xe khác đang có node này trên đường đi cũng phải replanning
        std::vector<std::string> others;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            others = agvsRoutedThroughNoLock(nodeId, agvId);
        }
        for (auto& id : others) replanAGV(id);
        notify();
    }
    void clearObstacle(const std::string& nodeId) {
        map_.setObstacle(nodeId, false);
        notify();
    }

    // The simulator defers route replacement until a moving point reaches RFID.
    void simulationReplan(const std::vector<std::string>& deferred = {}) {
        std::vector<std::string> ids;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for (const auto& item : fleet_)
                if (std::find(deferred.begin(),deferred.end(),item.first)==deferred.end()) ids.push_back(item.first);
        }
        for (const auto& id : ids) replanAGV(id);
        notify();dispatchPending();
    }

    // ---------------- Giao task ----------------
    void addTask(const std::string& targetNode, int priority = 0, const std::string& taskId = "") {
        if (!map_.nodeExists(targetNode)) throw std::invalid_argument("Node dich khong ton tai");
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (singleRobotTrial_) throw std::invalid_argument("Thu mot xe: chon truc tiep xe 01 va node dich");
            if (!acceptingTasks_) throw std::invalid_argument("Ca chua bat dau hoac dang ket thuc");
            if (tasks_.size() >= 1000) throw std::invalid_argument("Hang doi da dat gioi han 1000 task");
            if (!taskId.empty() && taskRecords_.count(taskId)) throw std::invalid_argument("task_id da ton tai");
            addTaskInternal(targetNode, priority, taskId);
        }
        dispatchPending();
        notify();
    }

    // ---------------- Ra lệnh trực tiếp 1 xe cụ thể (bấm chọn trên map) ----------------
    // Khác addTask(): KHÔNG tự tìm "xe gần nhất", mà điều khiển ĐÚNG agvId do
    // người dùng chỉ định. Nếu xe đang làm task khác, task cũ được TRẢ VỀ hàng đợi.
    // Trả về "" nếu thành công, hoặc chuỗi mô tả lỗi nếu thất bại.
    std::string directGoto(const std::string& agvId, const std::string& targetNode, const std::string& taskId = "", int priority = 0) {
        if (!map_.nodeExists(targetNode)) return "Node dich khong ton tai: " + targetNode;
        bool requeued = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (!acceptingTasks_) return "Ca chua bat dau hoac dang ket thuc";
            auto it = fleet_.find(agvId);
            if (it == fleet_.end())
                return "Khong tim thay AGV: " + agvId;
            AGV& agv = it->second;
            if (monitorOnlyRobots_.count(agvId)) return "Xe chi giam sat; khong nhan lenh di chuyen";
            if (!trialRobotId_.empty() && agvId != trialRobotId_) return "Che do thu chi cho phep xe " + trialRobotId_;
            if (twoRobotTrial_ && agvId != "01" && agvId != "02") return "Thu hai xe chi cho phep 01 va 02";
            if (!taskId.empty() && taskRecords_.count(taskId)) return "task_id da ton tai";
            if (!agv.positionValid || !agv.hasTelemetry || Clock::now() - agv.lastSeen > offlineTimeout_) return "Vi tri/telemetry cua xe chua hop le";
            if (!networkAvailable_) return "Mat ket noi MQTT broker";
            if (agv.stationArrivalRequired && agv.currentNode == agv.finalTarget && !agv.stationReady)
                return "Xe dang quay dau tai tram; cho xe dung va huong ra cua truoc khi giao tuyen moi";
            if (agv.nodeOrdersSupported && (agv.state == AGVState::WORKING || agv.state == AGVState::BLOCKED))
                return "Xe dang thuc hien tuyen; khong giao GOTO moi khi xe dang re hoac bam line";
            if (singleRobotTrial_ && agvId != "01")
                return "Thu mot xe chi cho phep xe 01";
            if (singleRobotTrial_ && (agv.state != AGVState::IDLE || !agv.taskId.empty() ||
                                      !agv.finalTarget.empty() || !agv.activePath.empty() ||
                                      !agv.uncertainPath.empty()))
                return "Thu mot xe can xe 01 dung yen va khong co hanh trinh do dang";
            if (singleRobotTrial_ && std::any_of(fleet_.begin(), fleet_.end(), [&](const auto& entry) {
                    return entry.first != "01";
                }))
                return "Thu mot xe can doi soat va dua xe khac ra khoi ban do";
            if (singleRobotTrial_) {
                const auto zones = arbiter_.toJson();
                for (const auto& zone : zones.items())
                    if (!zone.value().value("owner", std::string{}).empty())
                        return "Thu mot xe can xac minh va giai phong mutex cu";
            }
            if ((!agv.batteryMeasured || agv.battery < resumeBattery_) && !singleRobotTrial_ && !supervisedTrialNoLock())
                return "Pin chua do hoac khong du de nhan task";
            if (agv.state == AGVState::CHARGING || agv.state == AGVState::CHARGING_EN_ROUTE)
                return "AGV dang sac, khong the ra lenh thu cong luc nay";
            if (agv.state == AGVState::OFFLINE) return "AGV dang OFFLINE, khong the ra lenh";
            if (agv.state == AGVState::AGV_ERROR) return "AGV dang LOI, hay xoa loi (clear_error) truoc";

            auto path = computePathNoLock(agv, targetNode);
            if (singleRobotTrial_ && (path.size() < 2 || path.size() - 1 > 32))
                return "Thu mot xe can duong co huong toi node khac, toi da 32 chang";
            const bool waitingForRoute = path.empty();
            if (waitingForRoute && map_.findShortestPath(agv.currentNode,targetNode,buildOccupiedNodesNoLock(agv.id)).empty())
                return "Khong tim duoc duong tu " + agv.currentNode + " den " + targetNode;

            requeued = requeueWorkNoLock(agv); // không làm mất task đang dở
            agv.taskId = taskId.empty() ? nextTaskIdNoLock() : taskId;
            agv.taskPriority = priority;
            taskRecords_[agv.taskId] = {{"task_id", agv.taskId}, {"target", targetNode}, {"priority", priority}};
            setTaskStatusNoLock(agv.taskId, "dispatched", agv.id);
            if (waitingForRoute) {
                agv.finalTarget = targetNode;agv.state = AGVState::BLOCKED;agv.blockedResume = AGVState::WORKING;
                sendCommand(agv,"WAIT");
            } else assignPath(agv, path, targetNode, AGVState::WORKING);
            if (agv.state == AGVState::AGV_ERROR) return agv.lastError;
        }
        if (requeued) emit("AGV_" + agvId + " bi ghi de bang lenh thu cong -> task cu duoc tra ve hang doi");
        notify();
        dispatchPending();
        return "";
    }

    // ---------------- Huỷ task theo đích đến (nút "Huỷ" trên hàng đợi task ở dashboard) ----------------
    // Huỷ MỌI task đang CHỜ trong hàng đợi có cùng targetNode, và nếu có xe nào đang WORKING
    // (hoặc BLOCKED, sắp làm lại) hướng tới đích đó thì cho xe dừng lại (về IDLE) -- KHÔNG trả
    // task đó về hàng đợi (khác với lỗi/mất kết nối, vì đây là người dùng CHỦ ĐỘNG huỷ).
    // Trả về tổng số mục bị ảnh hưởng (task trong hàng đợi + xe bị dừng); 0 nếu không có gì để huỷ.
    int cancelTask(const std::string& targetNode, const std::string& taskId = "") {
        int removedFromQueue = 0;
        std::vector<std::string> interruptedAgvs;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            std::priority_queue<Task, std::vector<Task>, TaskCmp> remaining;
            while (!tasks_.empty()) {
                Task t = tasks_.top(); tasks_.pop();
                if ((!taskId.empty() && t.taskId == taskId) || (taskId.empty() && t.targetNode == targetNode)) {
                    setTaskStatusNoLock(t.taskId, "cancelled", "", "Nguoi dung huy");
                    removedFromQueue++;
                }
                else remaining.push(t);
            }
            tasks_ = std::move(remaining);

            for (auto& kv : fleet_) {
                AGV& a = kv.second;
                bool goingThere = ((!taskId.empty() && a.taskId == taskId) || (taskId.empty() && a.finalTarget == targetNode)) &&
                    (a.state == AGVState::WORKING ||
                     (a.state == AGVState::BLOCKED && a.blockedResume == AGVState::WORKING));
                if (!goingThere) continue;
                setTaskStatusNoLock(a.taskId, "cancelled", a.id, "Nguoi dung huy");
                a.taskId.clear();
                a.activePath.clear();
                a.finalTarget.clear();
                a.blockedResume = AGVState::IDLE;
                a.state = AGVState::IDLE;
                sendCommand(a, "WAIT"); // huỷ kế hoạch cũ, KHÔNG addTaskInternal lại (khác requeueWorkNoLock)
                interruptedAgvs.push_back(a.id);
            }
        }
        for (auto& id : interruptedAgvs) emit("Task toi " + targetNode + " bi nguoi dung huy -> AGV_" + id + " dung lai");
        if (removedFromQueue > 0) emit("Da huy " + std::to_string(removedFromQueue) + " task dang cho trong hang doi toi " + targetNode);
        if (removedFromQueue > 0 || !interruptedAgvs.empty()) { notify(); dispatchPending(); }
        return removedFromQueue + (int)interruptedAgvs.size();
    }
    //  - phát hiện xe mất kết nối
    //  - gửi lại lệnh chưa được ACK / coi là mất liên lạc lệnh nếu quá số lần thử
    //  - cho xe BLOCKED thử lại
    //  - giao task đang chờ cho xe rảnh gần nhất
    //  - kiểm tra pin toàn đội xe
    void tick() {
        pruneTaskRecords();
        if (map_.expireObstacles()) notify();
        checkTimeouts();
        checkAcks();
        resumeBlocked();
        dispatchPending();
        checkBatteryAll();
    }

    json toJson() {
        std::lock_guard<std::mutex> lk(mtx_);
        json j;
        json agvsJ = json::array();
        const auto now = Clock::now();
        for (auto& kv : fleet_) {
            const AGV& a = kv.second;
            const auto& encoder = a.encoder;
            const long long encoderAge = encoder.received ? std::chrono::duration_cast<std::chrono::milliseconds>(now-encoder.seen).count() : 0;
            const auto& sample = encoder.sample;
            const bool context = !sample.tracking ? a.activePath.empty() :
                (sample.route == a.routeCommandId && !a.activePath.empty() && a.activePath.front() == sample.to);
            const bool encoderFresh = encoder.received && encoderReadyNoLock(a) && encoderAge <= EncoderObservation::staleMs &&
                sample.from == a.currentNode && context;
            json detected = nullptr;
            if (a.id == "01" && a.sonarObstacle && now-a.sonarSeen <= std::chrono::seconds(3) &&
                encoderFresh && a.estimatedPosition.is_object() && sample.tracking &&
                a.sonarRouteCommandId == sample.route && a.sonarNextNode == sample.to &&
                a.sonarDirection == (sample.reverse ? "rear" : "front") &&
                std::isfinite(a.sonarDistanceCm) && a.sonarDistanceCm >= 0 && a.sonarDistanceCm <= 20) {
                const auto& p = a.estimatedPosition;
                const double length = p.value("edge_length_cm",0.0);
                if (length > 0 && sample.from == p.value("from",std::string{}) && sample.to == p.value("to",std::string{})) {
                    const double projected = std::clamp(sample.distance+a.sonarDistanceCm,0.0,length);
                    const double ratio = projected/length;
                    const auto layout = map_.toJson();
                    for (const auto& e : layout["edges"]) {
                        if (!((e["from"] == sample.from && e["to"] == sample.to) ||
                            (e.value("two_way",false) && e["from"] == sample.to && e["to"] == sample.from))) continue;
                        json from, to;
                        for (const auto& n : layout["nodes"]) {
                            if (n["id"] == sample.from) from=n;
                            if (n["id"] == sample.to) to=n;
                        }
                        if (!from.is_null() && !to.is_null())
                            detected = {{"from",sample.from},{"to",sample.to},{"x",from["x"].get<double>()+(to["x"].get<double>()-from["x"].get<double>())*ratio},
                                {"y",from["y"].get<double>()+(to["y"].get<double>()-from["y"].get<double>())*ratio},
                                {"range_cm",a.sonarDistanceCm},{"direction",a.sonarDirection},{"source","sonar_encoder_estimate"}};
                        break;
                    }
                }
            }
            long long silentSec = std::chrono::duration_cast<std::chrono::seconds>(now - a.lastSeen).count();
            agvsJ.push_back({
                {"id", a.id}, {"node", a.currentNode}, {"battery", a.battery},
                {"battery_display_percent", a.batteryDisplayAvailable ? json(a.batteryDisplayPercent) : json(nullptr)},
                {"battery_display_available", a.batteryDisplayAvailable},
                {"state", toString(a.state)}, {"path", a.activePath},
                {"target", a.finalTarget}, {"priority", a.isPriorityVehicle},
                {"last_seen_s", a.hasTelemetry ? json(silentSec) : json(nullptr)},
                {"task_id", a.taskId}, {"position_valid", a.positionValid},
                {"battery_measured", a.batteryMeasured}, {"error", a.lastError},
                {"needs_recharge", a.needsRecharge},
                {"encoder_supported", a.id == "01" && trialRobotId_ == "01" && encoder.boot != 0},
                {"speed_cm_s", encoderFresh ? json(sample.speed) : json(nullptr)},
                {"odometry", encoder.received ? json{{"valid",encoderFresh},{"age_ms",encoderAge},
                    {"stale_after_ms",EncoderObservation::staleMs},{"seq",sample.seq},{"boot_id",sample.boot},
                    {"connection_session",sample.connection},{"tracking",sample.tracking},{"translating",sample.translating},
                    {"distance_cm",sample.distance},{"left_ticks",sample.leftTicks},{"right_ticks",sample.rightTicks},
                    {"pulses_per_100cm",sample.pulsesPer100Cm}} : json(nullptr)},
                {"estimated_position", encoderFresh ? a.estimatedPosition : json(nullptr)},
                {"obstacle_alert", a.id == "01" && a.sonarObstacle && networkAvailable_ &&
                    now-a.sonarSeen <= std::chrono::seconds(3) && a.hasTelemetry && now-a.lastSeen <= offlineTimeout_
                    ? json{{"direction",a.sonarDirection},{"range_cm",a.sonarDistanceCm}} : json(nullptr)},
                {"obstacle_detection", detected},
                {"uncertain_path", a.uncertainPath},
                {"heading_supported", a.headingSupported && now-a.diagnosticSeen <= std::chrono::seconds(3) && networkAvailable_},
                {"heading", a.headingNode == a.currentNode && a.positionValid && now-a.diagnosticSeen <= std::chrono::seconds(3) &&
                    a.hasTelemetry && now-a.lastSeen <= offlineTimeout_ && a.state != AGVState::OFFLINE && networkAvailable_
                    ? json(a.heading.empty() ? "UNKNOWN" : a.heading) : json("UNKNOWN")},
                {"heading_source", a.headingSource},
                {"online", a.hasTelemetry && now - a.lastSeen <= offlineTimeout_ && a.state != AGVState::OFFLINE}
            });
        }
        j["agvs"] = agvsJ;
        json tasksJ = json::array();
        for (auto& t : taskQueueSnapshotNoLock())
            tasksJ.push_back({{"task_id", t.taskId}, {"target", t.targetNode}, {"priority", t.priority},{"seq",t.seq}});
        j["pending_tasks"] = tasksJ;
        j["tasks"] = json::array();
        for (const auto& kv : taskRecords_) j["tasks"].push_back(kv.second);
        j["offline_timeout_s"] = offlineTimeout_.count();
        j["charge_reservations"] = chargeReservation_; // station -> agvId đang giữ/đặt chỗ
        j["accepting_tasks"] = acceptingTasks_;
        j["home_stations"] = homeStations_;
        j["trial_robot_id"] = trialRobotId_;
        j["queue_sequence"] = seqCounter_;
        return j;
    }

private:
    bool encoderReadyNoLock(const AGV& a) const {
        return networkAvailable_ && a.hasTelemetry && a.positionValid && Clock::now()-a.lastSeen <= offlineTimeout_ &&
            a.state != AGVState::OFFLINE && a.state != AGVState::AGV_ERROR && a.state != AGVState::UNLOCALIZED;
    }
    bool completeArrivalNoLock(AGV& agv) {
        if (!agv.positionValid || !agv.activePath.empty() || agv.finalTarget.empty() ||
            agv.currentNode != agv.finalTarget ||
            (agv.state != AGVState::WORKING && agv.state != AGVState::CHARGING_EN_ROUTE)) return false;
        if (agv.stationArrivalRequired && (!agv.stationReady ||
            agv.headingNode != agv.currentNode || Clock::now() - agv.diagnosticSeen > std::chrono::seconds(3)))
            return false;
        const bool charging = agv.state == AGVState::CHARGING_EN_ROUTE;
        if (!charging && !agv.taskId.empty()) setTaskStatusNoLock(agv.taskId, "completed", agv.id);
        agv.taskId.clear(); agv.finalTarget.clear();
        agv.stationArrivalRequired = false;
        agv.state = charging ? AGVState::CHARGING : AGVState::IDLE;
        if (charging) sendCommand(agv, "CHARGE_START");
        return true;
    }
    void pruneTaskRecords() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::pair<long,std::string>> terminal;
        for (const auto& kv : taskRecords_) {
            const auto status = kv.second.value("status","");
            if (status == "completed" || status == "cancelled" || status == "interrupted") terminal.push_back({kv.second.value("updated_sequence",0L),kv.first});
        }
        if (terminal.size() <= 1000) return;
        std::sort(terminal.begin(),terminal.end());
        for (size_t i=0;i<terminal.size()-1000;++i) taskRecords_.erase(terminal[i].second);
    }
    struct TaskCmp {
        bool operator()(const Task& a, const Task& b) const {
            if (a.priority != b.priority) return a.priority < b.priority; // priority cao hơn lên top
            return a.seq > b.seq; // seq nhỏ hơn (tạo trước) lên top -> FIFO cùng priority
        }
    };

    static constexpr int kRequeuePriority = 5; // task bị trả lại được ưu tiên hơn task thường

    // 1 lệnh đang chờ ESP32 xác nhận (ACK) — xem [5] trong ghi chú đầu file.
    struct PendingCmd {
        long cmdId;
        std::string topic;
        std::string payload; // gửi lại NGUYÊN VĂN chuỗi này nếu hết ackTimeout_
        Clock::time_point sentAt;
        int retries = 0;
    };

    std::string nextTaskIdNoLock() { return taskIdAllocator_ ? taskIdAllocator_() : "core-" + std::to_string(seqCounter_++); }

    void setTaskStatusNoLock(const std::string& id, const std::string& status,
                             const std::string& agvId = "", const std::string& reason = "") {
        if (id.empty()) return;
        auto& record = taskRecords_[id];
        record["status"] = status; record["assigned_agv"] = agvId; record["reason"] = reason;
        record["updated_sequence"] = ++seqCounter_;
    }

    void addTaskInternal(const std::string& targetNode, int priority, const std::string& taskId = "") {
        const auto id = taskId.empty() ? nextTaskIdNoLock() : taskId;
        tasks_.push(Task{targetNode, priority, id, seqCounter_++});
        taskRecords_[id] = {{"task_id", id}, {"target", targetNode}, {"priority", priority}};
        setTaskStatusNoLock(id, "pending");
    }

    // An uncertain robot can still be moving. Do not automatically dispatch its task elsewhere.
    bool interruptWorkNoLock(AGV& a, const std::string& reason) {
        bool interrupted = !a.taskId.empty();
        if (interrupted) setTaskStatusNoLock(a.taskId, "interrupted", a.id, reason);
        if (a.uncertainPath.empty()) a.uncertainPath = a.activePath;
        a.taskId.clear(); a.activePath.clear(); a.finalTarget.clear(); a.blockedResume = AGVState::IDLE;
        return interrupted;
    }

    // PHẢI gọi khi đã giữ mtx_
    std::vector<Task> taskQueueSnapshotNoLock() {
        auto copy = tasks_;
        std::vector<Task> out;
        while (!copy.empty()) { out.push_back(copy.top()); copy.pop(); }
        return out;
    }

    // ---------------- Các hàm phụ (PHẢI gọi khi đã giữ mtx_) ----------------

    // Xe đang có "việc dở" cần được giao lại nếu xe bị loại khỏi cuộc chơi?
    static bool hasWork(const AGV& a) {
        if (a.finalTarget.empty()) return false;
        return a.state == AGVState::WORKING ||
               (a.state == AGVState::BLOCKED && a.blockedResume == AGVState::WORKING);
    }

    // Trả task đang dở (nếu có) về hàng đợi chung, rồi xoá kế hoạch của xe.
    // Trả về true nếu có task được trả lại.
    bool requeueWorkNoLock(AGV& a) {
        bool requeued = false;
        if (hasWork(a)) {
            addTaskInternal(a.finalTarget, std::max(a.taskPriority, kRequeuePriority), a.taskId);
            requeued = true;
        }
        releaseStationReservationNoLock(a.id); // huỷ mọi kế hoạch (kể cả kế hoạch đi sạc) -> nhả chỗ trạm
        a.taskId.clear();
        a.activePath.clear();
        a.finalTarget.clear();
        a.blockedResume = AGVState::IDLE;
        return requeued;
    }

    // Trạm sạc đang được xe id đặt chỗ hoặc chiếm giữ -> gỡ hết (dùng khi kế hoạch/việc sạc bị huỷ)
    void releaseStationReservationNoLock(const std::string& id) {
        for (auto it = chargeReservation_.begin(); it != chargeReservation_.end(); ) {
            if (it->second == id) it = chargeReservation_.erase(it); else ++it;
        }
    }

    bool isChargingStationNode(const std::string& node) const {
        auto stations = map_.chargingStations();
        return std::find(stations.begin(), stations.end(), node) != stations.end();
    }

    // Chọn trạm sạc gần nhất còn TRỐNG (chưa bị xe khác đặt chỗ/chiếm) mà agv tới được.
    // Khác map_.nearestCharging(): loại các trạm đã có agv KHÁC đặt chỗ, để 2 xe pin thấp
    // không cùng lúc nhắm tới 1 trạm còn đang trống (trước khi ai đó thực sự tới nơi).
    std::string pickChargingStationNoLock(const AGV& agv) {
        std::string best; long long bestCost = LLONG_MAX;
        for (auto& cs : map_.chargingStations()) {
            auto home = homeStations_.find(agv.id);
            if (home != homeStations_.end() && cs != home->second) continue;
            auto rit = chargeReservation_.find(cs);
            if (rit != chargeReservation_.end() && rit->second != agv.id) continue; // trạm đã có chủ khác
            auto path = computePathNoLock(agv, cs);
            auto cost = map_.pathCost(path);
            if (!path.empty() && (cost < bestCost || (cost == bestCost && cs < best))) { bestCost = cost; best = cs; }
        }
        return best;
    }

    // Các node "bận" với xe excludeId: đường đi của xe khác + vị trí đang đứng của xe khác
    // (xe IDLE / đang sạc / lỗi / offline vẫn chiếm chỗ thật ngoài đời).
    std::unordered_set<std::string> buildBusyNodesNoLock(const std::string& excludeId) {
        auto busy = buildHardBlockedNodesNoLock(excludeId);
        for (auto& kv : fleet_) {
            if (kv.first == excludeId) continue;
            const AGV& o = kv.second;
            if (!o.currentNode.empty()) busy.insert(o.currentNode);
            for (auto& n : o.activePath) busy.insert(n);
            for (auto& n : o.uncertainPath) busy.insert(n);
        }
        return busy;
    }

    // Ưu tiên né cả route dự kiến; fallback vẫn giữ node/đích đang chiếm và vùng chưa xác minh.
    std::vector<std::string> computePathNoLock(const AGV& agv, const std::string& target) {
        auto busy = buildBusyNodesNoLock(agv.id);
        auto path = map_.findShortestPath(agv.currentNode, target, busy);
        if (path.empty()) path = map_.findShortestPath(agv.currentNode, target, buildHardBlockedNodesNoLock(agv.id));
        return path;
    }

    std::unordered_set<std::string> buildOccupiedNodesNoLock(const std::string& excludeId) const {
        std::unordered_set<std::string> blocked;
        for (const auto& kv : fleet_) {
            if (kv.first == excludeId) continue;
            const auto& a = kv.second;
            if (!a.currentNode.empty()) blocked.insert(a.currentNode);
            if (!a.finalTarget.empty()) blocked.insert(a.finalTarget);
            if (a.state == AGVState::OFFLINE || a.state == AGVState::AGV_ERROR || !a.positionValid) {
                if (!a.currentNode.empty()) blocked.insert(a.currentNode);
                blocked.insert(a.uncertainPath.begin(), a.uncertainPath.end());
                blocked.insert(a.activePath.begin(), a.activePath.end());
            }
        }
        return blocked;
    }

    std::unordered_set<std::string> buildHardBlockedNodesNoLock(const std::string& excludeId) {
        auto blocked = buildOccupiedNodesNoLock(excludeId);
        const auto zones = map_.mutexZones();
        const std::unordered_set<std::string> mutex(zones.begin(),zones.end());
        for (const auto& kv : fleet_) {
            if (kv.first == excludeId) continue;
            const auto& a = kv.second;
            // Ordinary gates/corners have no grant protocol: reserve their whole route.
            for (const auto& node : a.activePath) if (!mutex.count(node)) blocked.insert(node);
            auto reserveCorridor = [&](const std::string& node) {
                if (!mutex.count(node)) return;
                const auto component = map_.mutexComponent(node);
                if (component.size() > 1) blocked.insert(component.begin(),component.end());
            };
            reserveCorridor(a.currentNode);
            for (const auto& node : a.activePath) reserveCorridor(node);
            for (const auto& node : a.uncertainPath) reserveCorridor(node);
        }
        return blocked;
    }

    std::vector<std::string> agvsRoutedThroughNoLock(const std::string& node, const std::string& exceptId) const {
        std::vector<std::string> out;
        if (node.empty()) return out;
        for (auto& kv : fleet_) {
            if (kv.first == exceptId) continue;
            const auto& p = kv.second.activePath;
            if (std::find(p.begin(), p.end(), node) != p.end()) out.push_back(kv.first);
        }
        return out;
    }

    // Dùng resumeBattery_ (không phải lowBattery_) để tránh xe pin 21% nhận task rồi
    // vài phút sau bị huỷ để đi sạc — xem hysteresis ở [6] trong ghi chú đầu file.
    bool dispatchableNoLock(const AGV& a) const {
        return !monitorOnlyRobots_.count(a.id) && (trialRobotId_.empty() || a.id == trialRobotId_) &&
               (!twoRobotTrial_ || a.id == "01" || a.id == "02") &&
               acceptingTasks_ && networkAvailable_ && a.state == AGVState::IDLE &&
               (supervisedTrialNoLock() || (a.batteryMeasured && a.battery >= resumeBattery_)) && a.hasTelemetry && a.positionValid &&
               Clock::now() - a.lastSeen <= offlineTimeout_ && !a.currentNode.empty();
    }

    // Tìm & giao AGV IDLE gần nhất (theo tổng trọng số cạnh) cho các task đang chờ,
    // ưu tiên xử lý task có priority cao trước (và tạo trước, nếu bằng priority).
    void dispatchPending() {
        bool assignedAny = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (singleRobotTrial_) return;
            if (tasks_.empty() || !networkAvailable_ || !acceptingTasks_) return;
            bool anyFree = false;
            for (auto& kv : fleet_) if (dispatchableNoLock(kv.second)) { anyFree = true; break; }
            if (!anyFree) return; // không có xe rảnh -> khỏi tốn Dijkstra

            std::priority_queue<Task, std::vector<Task>, TaskCmp> remaining;
            while (!tasks_.empty()) {
                Task t = tasks_.top(); tasks_.pop();

                std::string bestId; std::vector<std::string> bestPath; long long bestCost = LLONG_MAX;
                // pass 0: né node bận (xe khác đang đứng / sắp đi qua). pass 1: không né.
                for (int pass = 0; pass < 2 && bestId.empty(); ++pass) {
                    for (auto& kv : fleet_) {
                        const AGV& agv = kv.second;
                        if (!dispatchableNoLock(agv)) continue;
                        std::vector<std::string> path;
                        if (pass == 0) path = map_.findShortestPath(agv.currentNode, t.targetNode, buildBusyNodesNoLock(kv.first));
                        else           path = map_.findShortestPath(agv.currentNode, t.targetNode, buildHardBlockedNodesNoLock(kv.first));
                        auto cost = map_.pathCost(path);
                        if (!path.empty() && (cost < bestCost || (cost == bestCost && kv.first < bestId))) {
                            bestCost = cost;
                            bestId = kv.first;
                            bestPath = path;
                        }
                    }
                }

                if (!bestId.empty()) {
                    auto& chosen = fleet_[bestId];
                    chosen.taskId = t.taskId; chosen.taskPriority = t.priority;
                    setTaskStatusNoLock(t.taskId, "dispatched", bestId);
                    assignPath(chosen, bestPath, t.targetNode, AGVState::WORKING);
                    assignedAny = true;
                } else {
                    remaining.push(t); // chưa có xe phù hợp -> giữ lại cho lần tick sau
                }
            }
            tasks_ = std::move(remaining);
        }
        if (assignedAny) notify();
    }

    void assignPath(AGV& agv, std::vector<std::string> path, const std::string& target, AGVState newState) {
        if (!path.empty() && path.front() == agv.currentNode) path.erase(path.begin());
        agv.activePath = path;
        agv.commandPath = path; agv.routeStart = agv.currentNode;
        agv.lastNodeOrderKey.clear(); agv.lastNodeOrderPayload.clear();
        agv.finalTarget = target;
        agv.stationArrivalRequired = agv.stationDockingSupported && (target == "S1" || target == "S2");
        agv.stationReady = false;
        agv.state = newState;
        agv.blockedResume = AGVState::IDLE;
        json command = {{"path",path},{"target",target}};
        if (agv.straightThroughSupported && Clock::now()-agv.diagnosticSeen <= offlineTimeout_) {
            auto through = json::array();
            const auto layout = map_.toJson();
            for (size_t i = 0; i+1 < path.size(); ++i)
                if (NodeManeuver::straightThrough(layout,i ? path[i-1] : agv.currentNode,path[i],path[i+1]))
                    through.push_back(i);
            command["pass_through"] = through;
        }
        agv.routeCommandId = sendCommand(agv, "GOTO", command);
    }

    // Gửi 1 lệnh JSON có cmd_id xuống xe, theo dõi để gửi lại nếu không có ACK (xem [5]).
    // PHẢI gọi khi đang giữ mtx_. Lệnh mới cho cùng 1 xe thay thế hẳn lệnh cũ đang chờ ACK.
    long sendCommand(AGV& agv, const std::string& cmdName, json extra = json::object()) {
        // Chot cuoi cung: khong phat chuyen dong/sac cho xe khong duoc chon.
        if (!trialRobotId_.empty() && agv.id != trialRobotId_ &&
            (cmdName == "GOTO" || cmdName == "CHARGE_START" || cmdName == "NODE_ORDER")) return 0;
        long cmdId;
        try {cmdId = commandIdAllocator_ ? commandIdAllocator_() : ++cmdSeq_;}
        catch (const std::exception& e) {
            interruptWorkNoLock(agv,e.what());agv.state = AGVState::AGV_ERROR;agv.lastError = e.what();
            pendingCmd_.erase(agv.id);return 0;
        }
        json j = extra;
        j["cmd"] = cmdName;
        j["cmd_id"] = cmdId;
        std::string topic = "agv/" + agv.id + "/command";
        std::string payload = j.dump();
        pendingCmd_[agv.id] = PendingCmd{cmdId, topic, payload, Clock::now(), 0};
        if (publish_) publish_(topic, payload);
        return cmdId;
    }

    // Gửi lại lệnh chưa được ACK sau ackTimeout_; sau maxRetries_ lần mà vẫn im lặng
    // thì coi là mất liên lạc lệnh: gián đoạn nhiệm vụ và chuyển xe sang AGV_ERROR.
    void checkAcks() {
        std::vector<std::pair<std::string, bool>> failed; // {agvId, co_requeue_task_khong}
        {
            std::lock_guard<std::mutex> lk(mtx_);
            const auto now = Clock::now();
            for (auto it = pendingCmd_.begin(); it != pendingCmd_.end(); ) {
                const std::string& id = it->first;
                PendingCmd& pc = it->second;
                if (now - pc.sentAt < ackTimeout_) { ++it; continue; }

                auto ait = fleet_.find(id);
                if (ait == fleet_.end() || ait->second.state == AGVState::OFFLINE) {
                    it = pendingCmd_.erase(it); // xe đã mất kết nối/bị xoá -> heartbeat đã/sẽ xử lý riêng
                    continue;
                }
                if (pc.retries < maxRetries_) {
                    pc.retries++;
                    pc.sentAt = now;
                    if (publish_) publish_(pc.topic, pc.payload); // gửi lại NGUYÊN VĂN, cùng cmd_id
                    ++it;
                } else {
                    AGV& agv = ait->second;
                    bool requeued = interruptWorkNoLock(agv, "Het thoi gian ACK");
                    agv.lastError = "Khong nhan duoc ACK; can xac minh robot";
                    agv.state = AGVState::AGV_ERROR;
                    failed.push_back({id, requeued});
                    it = pendingCmd_.erase(it);
                }
            }
        }
        for (auto& f : failed)
            emit("AGV_" + f.first + " KHONG ACK lenh sau " + std::to_string(maxRetries_) +
                 " lan gui lai -> nghi mat lien lac, chuyen LOI" + (f.second ? " (task bi gian doan)" : ""));
        if (!failed.empty()) { notify(); dispatchPending(); }
    }

    // Tính lại đường cho 1 xe — gọi khi phát hiện vật cản, lệch đường hoặc đường bị chặn
    void replanAGV(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = fleet_.find(id);
        if (it == fleet_.end() || it->second.finalTarget.empty()) return;
        if (!networkAvailable_) return;
        AGV& agv = it->second;
        if (agv.state != AGVState::WORKING && agv.state != AGVState::CHARGING_EN_ROUTE &&
            agv.state != AGVState::BLOCKED) return;

        AGVState resume = (agv.state == AGVState::BLOCKED) ? agv.blockedResume
                        : (agv.state == AGVState::CHARGING_EN_ROUTE ? AGVState::CHARGING_EN_ROUTE : AGVState::WORKING);
        if (resume != AGVState::WORKING && resume != AGVState::CHARGING_EN_ROUTE) resume = AGVState::WORKING;

        auto path = computePathNoLock(agv, agv.finalTarget);
        if (path.empty()) {
            if (agv.state != AGVState::BLOCKED) {
                agv.blockedResume = resume;
                agv.state = AGVState::BLOCKED; // hết đường -> chờ, tick() sẽ thử lại (resumeBlocked)
                agv.activePath.clear();
                sendCommand(agv, "WAIT");
            }
        } else {
            assignPath(agv, path, agv.finalTarget, resume);
        }
    }

    // Phát hiện xe mất kết nối
    void checkTimeouts() {
        std::vector<std::string> lost, requeuedIds, toReplan;
        bool release = true;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            const auto now = Clock::now();
            for (auto& kv : fleet_) {
                AGV& a = kv.second;
                if (a.state == AGVState::OFFLINE) continue;
                if (now - a.lastSeen <= offlineTimeout_) continue;
                if (interruptWorkNoLock(a, "Mat telemetry")) requeuedIds.push_back(kv.first);
                a.lastError = "Mat telemetry; vi tri va chuyen dong chua xac minh";
                a.state = AGVState::OFFLINE;
                a.autoResetEligible = true;a.autoResetRequested = false;a.autoResetCandidateAt = {};
                lost.push_back(kv.first);
                pendingCmd_.erase(kv.first); // đừng phí công gửi lại lệnh cho xe đã coi là mất kết nối
            }
            for (auto& id : lost)
                for (auto& o : agvsRoutedThroughNoLock(fleet_[id].currentNode, id)) toReplan.push_back(o);
            release = releaseZonesOnOffline_;
        }
        if (lost.empty()) return;

        std::sort(toReplan.begin(), toReplan.end());
        toReplan.erase(std::unique(toReplan.begin(), toReplan.end()), toReplan.end());
        for (auto& id : lost) {
            bool requeued = std::find(requeuedIds.begin(), requeuedIds.end(), id) != requeuedIds.end();
            arbiter_.cancelWaiting(id);
            emit("AGV_" + id + " MAT KET NOI (OFFLINE)" + (requeued ? " -> task bi gian doan, can xac minh" : ""));
            if (release) releaseAgvZones(id);
        }
        for (auto& o : toReplan) replanAGV(o);
        notify();
        dispatchPending();
    }

    // Cho các xe BLOCKED thử lại mỗi tick
    void resumeBlocked() {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (!networkAvailable_) return;
            for (auto& kv : fleet_) {
                AGV& agv = kv.second;
                if (!trialRobotId_.empty() && agv.id != trialRobotId_) continue;
                if (agv.state != AGVState::BLOCKED) continue;
                if (!agv.positionValid || Clock::now()-agv.lastSeen > offlineTimeout_) continue;
                if (agv.finalTarget.empty()) {
                    // Không còn việc dở. Nếu pin thấp thì để checkBatteryOne tìm đường tới trạm sạc.
                    if (singleRobotTrial_ || supervisedTrialNoLock() || (!agv.needsRecharge && agv.battery >= resumeBattery_)) {
                        agv.state = AGVState::IDLE;
                        agv.blockedResume = AGVState::IDLE;
                        changed = true;
                    }
                    continue;
                }
                auto path = computePathNoLock(agv, agv.finalTarget);
                if (path.empty()) continue; // vẫn kẹt, thử lại tick sau (không gửi lại WAIT)
                AGVState resume = (agv.blockedResume == AGVState::CHARGING_EN_ROUTE)
                                  ? AGVState::CHARGING_EN_ROUTE : AGVState::WORKING;
                assignPath(agv, path, agv.finalTarget, resume);
                changed = true;
            }
        }
        if (changed) notify();
    }

    // Trả về true nếu có thay đổi state/hàng đợi
    bool checkBatteryOne(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (singleRobotTrial_ || supervisedTrialNoLock() || monitorOnlyRobots_.count(id)) return false; // che do thu khong tu ve sac
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return false;
            AGV& agv = it->second;
            if (!networkAvailable_ || !agv.batteryMeasured || !agv.positionValid || !agv.hasTelemetry ||
                Clock::now()-agv.lastSeen > offlineTimeout_) return false;
            if (agv.state == AGVState::OFFLINE || agv.state == AGVState::AGV_ERROR ||
                agv.state == AGVState::CHARGING_EN_ROUTE || agv.state == AGVState::CHARGING) return false;
            if (!agv.needsRecharge || agv.currentNode.empty()) return false;

            // Nếu đang làm task dở -> trả ngay vào hàng đợi chung, ưu tiên cao,
            // để XE KHÁC nhận và tiếp tục ngay (không đợi xe này sạc xong).
            // (Cũng nhả mọi chỗ trạm sạc cũ đang giữ, để pickChargingStationNoLock bên dưới sạch sẽ.)
            bool requeued = requeueWorkNoLock(agv);
            changed = requeued;

            const auto home = homeStations_.find(id);
            if (isChargingStationNode(agv.currentNode) && (home == homeStations_.end() || home->second == agv.currentNode)) {
                // Đã đứng sẵn tại 1 trạm sạc -> đặt chỗ cho chính mình rồi sạc luôn
                chargeReservation_[agv.currentNode] = agv.id;
                agv.state = AGVState::CHARGING;
                sendCommand(agv, "CHARGE_START");
                changed = true;
            } else {
                // Chọn trạm gần nhất CHƯA bị xe khác đặt chỗ (xem pickChargingStationNoLock)
                std::string chg = pickChargingStationNoLock(agv);
                std::vector<std::string> path;
                if (!chg.empty()) path = computePathNoLock(agv, chg);
                if (path.empty()) {
                    if (agv.state != AGVState::BLOCKED) { // không có đường tới trạm sạc nào (trống hoặc đặt được)
                        agv.state = AGVState::BLOCKED;
                        agv.blockedResume = AGVState::IDLE;
                        changed = true;
                    }
                } else {
                    chargeReservation_[chg] = agv.id; // đặt chỗ NGAY khi cử đi, trước khi tới nơi
                    assignPath(agv, path, chg, AGVState::CHARGING_EN_ROUTE);
                    changed = true;
                }
            }
        }
        if (changed) {
            notify();
            dispatchPending(); // có thể có xe khác rảnh để nhận ngay task vừa trả lại
        }
        return changed;
    }

    void checkBatteryAll() {
        std::vector<std::string> ids;
        { std::lock_guard<std::mutex> lk(mtx_); for (auto& kv : fleet_) ids.push_back(kv.first); }
        for (auto& id : ids) checkBatteryOne(id);
    }

    // Nhả mọi zone xe đang giữ và báo "mutex_grant" cho xe kế tiếp đang xếp hàng ở từng zone.
    // (Cần MutexArbiter::forceRelease bản mới, trả về danh sách {zone, xe_được_cấp}.)
    // KHÔNG gọi khi đang giữ mtx_.
    void releaseAgvZones(const std::string& id) {
        auto grants = arbiter_.forceRelease(id);
        if (!publish_) return;
        for (auto& g : grants) {
            const auto payload = arbiter_.grant(g.first,g.second);
            if (!payload.is_null()) publish_("agv/" + g.second + "/mutex_grant", payload.dump());
            emit("Zone " + g.first + " duoc cap cho AGV_" + g.second + " (xe " + id + " roi khoi he thong)");
        }
    }

    void notify() { if (onChanged_) onChanged_(); }
    void emit(const std::string& msg) { if (onEvent_) onEvent_(msg); }

    MapGraph& map_;
    MutexArbiter& arbiter_;
    std::unordered_map<std::string, AGV> fleet_;
    std::priority_queue<Task, std::vector<Task>, TaskCmp> tasks_;
    std::unordered_map<std::string, json> taskRecords_;
    long seqCounter_ = 0;
    int lowBattery_ = 20;    // % pin coi là "thấp" -> tự động về sạc
    int resumeBattery_ = 40; // % pin tối thiểu để được nhận task mới (hysteresis, phải > lowBattery_)
    int chargeTargetPercent_ = 90; // % pin coi là "sạc xong" nếu ESP32 không báo kèm số liệu thật
    std::chrono::seconds offlineTimeout_{10};
    bool autoRegister_ = true;
    bool singleRobotTrial_ = false;
    bool twoRobotTrial_ = false;
    std::string trialRobotId_;
    bool supervisedTrialNoLock() const { return twoRobotTrial_ || !trialRobotId_.empty(); }
    std::unordered_set<std::string> monitorOnlyRobots_;
    bool networkAvailable_ = true;
    bool acceptingTasks_ = true;
    std::unordered_map<std::string,std::string> homeStations_;
    bool releaseZonesOnOffline_ = false;
    std::unordered_map<std::string, std::string> chargeReservation_; // station -> agvId đang giữ/đặt chỗ
    std::unordered_map<std::string, PendingCmd> pendingCmd_;         // agvId -> lệnh đang chờ ACK
    long cmdSeq_ = 0;
    std::function<long()> commandIdAllocator_;
    std::function<std::string()> taskIdAllocator_;
    int obstacleTtlSeconds_ = 30;
    std::chrono::milliseconds ackTimeout_{3000};
    int maxRetries_ = 3;
    std::function<void(const std::string&, const std::string&)> publish_;
    std::function<void()> onChanged_;
    std::function<void(const std::string&)> onEvent_;
    std::mutex mtx_;
};
