#pragma once
#include <tuple>
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
#include "utils/CoreTypes.hpp"
#include "map/MapGraph.hpp"
#include "core/NodeManeuver.hpp"
#include "core/EncoderOdometry.hpp"
#include "utils/Mutexarbiter.hpp"


enum class AGVState { IDLE, WORKING, CHARGING_EN_ROUTE, CHARGING, BLOCKED, AGV_ERROR, OFFLINE, UNLOCALIZED, LIFTING, LOWERING };

inline std::string toString(AGVState s) {
    switch (s) {
        case AGVState::IDLE: return "IDLE";
        case AGVState::WORKING: return "WORKING";
        case AGVState::CHARGING_EN_ROUTE: return "CHARGING_EN_ROUTE";
        case AGVState::CHARGING: return "CHARGING";
        case AGVState::BLOCKED: return "BLOCKED";
        case AGVState::AGV_ERROR: return "ERROR";
        case AGVState::UNLOCALIZED: return "UNLOCALIZED";
        case AGVState::LIFTING: return "LIFTING";
        case AGVState::LOWERING: return "LOWERING";
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
    bool batteryFromDiagnostic = false;
    Clock::time_point batteryDiagnosticSeen{};
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
    bool headingConfirmationSupported = false, recoveryHeadingRequired = false;
    long headingPendingId = 0, headingConfirmedId = 0;
    uint32_t headingRequestUptime = 0, headingRequestBoot = 0, headingRequestConnection = 0;
    Clock::time_point headingRequestedAt{};
    std::string headingRequested, headingRequestedNode;
    bool stationDockingSupported = false, stationArrivalRequired = false, stationReady = false;
    Clock::time_point diagnosticSeen{};
    long routeCommandId = 0;
    bool nodeOrdersSupported = false;
    bool replanNeedsNodeProof = false;
    uint32_t replanHoldUptime = 0;
    bool cancelNeedsNodeProof = false;
    long stopCommandId = 0;
    Clock::time_point nodeProofSince{}, nodeProofLastWarning{};
    bool encoderSessionLimitReported = false;
    bool straightThroughSupported = false;
    std::string routeStart, lastNodeOrderKey, lastNodeOrderPayload;
    std::vector<std::string> commandPath;
    Clock::time_point lastNodeOrderAt{};
    EncoderObservation encoder;
    json estimatedPosition = nullptr;
    std::vector<std::string> stoppedNodes;
    EncoderSample stopCandidate;
    bool hasStopCandidate = false, stoppedLocalized = false;
    Clock::time_point stoppedSeen{}, stoppedDiagnosticSeen{};
    uint32_t stoppedDiagnosticUptime = 0, stoppedDiagnosticRfidAge = UINT32_MAX;
    bool sonarObstacle = false;
    double sonarDistanceCm = 0;
    std::string sonarDirection;
    std::string sonarNextNode;
    long sonarRouteCommandId = 0;
    Clock::time_point sonarSeen{};
    bool liftSupported=false,liftReady=false,liftActive=false,virtualCargoKnown=false,virtualCargo=false;
    bool dropoffExitSupported=false,dropoffExitPending=false;
    bool liftRecoverySupported=false,liftWaitingRecovery=false;
    bool cargoSensorSupported=false,cargoSensorStable=false,cargoSensorPresent=false,cargoSensorStopped=false;
    bool cargoMonitoringOnly=false;
    bool liftDiagnosticSeen=false;
    uint32_t liftDiagnosticUptime=0,liftDiagnosticBoot=0,liftDiagnosticConnection=0;
    uint32_t cargoSensorSeq=0,cargoSensorUptime=0,cargoSensorBoot=0,cargoSensorConnection=0,liftCargoUptime=0;
    std::string cargoSensorNode;
    Clock::time_point cargoSensorSeen{};
    long liftCommandId=0;
    std::string liftState="IDLE",liftNode,liftAction,liftObservedNode;
    Clock::time_point liftSeen{},liftStarted{};
    uint32_t liftBoot=0,liftConnection=0;
    Clock::time_point cargoReconcileSeen{};
    uint32_t cargoReconcileBoot=0,cargoReconcileConnection=0;
    Clock::time_point nodeStopSeen{};
    std::string nodeStopNode;
    uint32_t nodeStopBoot=0,nodeStopConnection=0,nodeStopUptime=0,nodeStopRfidAge=UINT32_MAX;
    long nodeStopLastCommand=-1;
    bool nodeStopHealthy=false;
    bool operatorNodeReset=false;
    std::string recoveryExpectedNode;
    bool recoveryAutoResume=false;
    std::string recoveryChosenHeading;
    std::string recoveryChosenNode;
    Clock::time_point maintenanceStableSince{},maintenanceLastSample{},resetRequestedAt{};
    uint32_t resetBoot=0,resetConnection=0;
    bool maintenanceResetAttempted=false;
    bool maintenanceArrival=false;
    bool maintenanceNeedsReset=false;
    bool maintenanceSupported=false,maintenanceReady=false;
};

struct MotionDiagnostic {
    std::string node, state, error, motor;
    bool taskActive = true;
    bool wifi = false, mqtt = false;
    std::uint32_t rfidAgeMs = UINT32_MAX;
    int leftDuty = INT_MAX, rightDuty = INT_MAX;
    bool headingSupported = false;
    bool headingConfirmationSupported = false;
    long headingCommandId = 0;
    bool stationDockingSupported = false, stationReady = false;
    bool reverseHomeSupported = false;
    bool liftSupported=false,liftBusy=false,liftPresent=false,cargoKnown=false,virtualCargo=false;
    bool dropoffExitSupported=false;
    bool liftRecoverySupported=false;
    bool maintenanceRecoverySupported=false;
    bool maintenanceReady=false;
    bool cargoSensorSupported=false,cargoSensorStable=false,cargoSensorPresent=false;
    bool cargoMonitoringOnly=false;
    uint32_t cargoSensorSeq=0,cargoSensorAge=UINT32_MAX,cargoSensorStableMs=0;
    long liftCommandId=0;
    uint32_t liftElapsedMs=0;
    std::string liftState;
    std::string heading, headingNode, headingSource;
    bool nodeOrdersSupported = false, awaitingNodeOrder = false, stationTurnRequired = false;
    long routeCommandId = 0;
    int pathIndex = -1;
    std::string nextNode;
    bool straightThroughSupported = false;
    bool sonarPresent = false, sonarObstacle = false;
    double sonarDistanceCm = 0;
    std::string sonarDirection;
    long lastCommandId = -1;
    uint32_t encoderBoot = 0, encoderConnection = 0, encoderUptime = 0;
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
    void setCommandIdHighwaterCallback(std::function<void(long)> callback) {
        commandIdHighwaterCallback_ = std::move(callback);
    }
    void setTaskIdAllocator(std::function<std::string()> allocator) { taskIdAllocator_ = std::move(allocator); }
    std::string createTaskId() { std::lock_guard<std::mutex> lk(mtx_);return nextTaskIdNoLock(); }
    json durableSnapshot() {
        std::lock_guard<std::mutex> lk(mtx_);
        json state={{"queue_sequence",seqCounter_},{"tasks",json::array()},
            {"pending_tasks",json::array()},{"agvs",json::array()},
            {"charge_reservations",chargeReservation_},{"accepting_tasks",acceptingTasks_}};
        for(const auto& kv:taskRecords_) state["tasks"].push_back(kv.second);
        for(const auto& t:taskQueueSnapshotNoLock()) state["pending_tasks"].push_back(
            {{"task_id",t.taskId},{"target",t.targetNode},{"priority",t.priority},{"seq",t.seq}});
        for(const auto& kv:fleet_) {
            const auto& a=kv.second;
            state["agvs"].push_back({{"id",a.id},{"node",a.currentNode},{"battery",a.battery},
                {"needs_recharge",a.needsRecharge},{"state",toString(a.state)},
                {"position_valid",a.positionValid},{"path",a.activePath},{"target",a.finalTarget},
                {"task_id",a.taskId},{"uncertain_path",a.uncertainPath},{"stopped_nodes",a.stoppedNodes},
                {"dropoff_exit_pending",a.dropoffExitPending},{"dropoff_lift_node",a.liftNode},
                {"dropoff_lift_command_id",a.liftCommandId}});
        }
        return state;
    }
    void restore(const json& state) {
        std::lock_guard<std::mutex> lk(mtx_);
        fleet_.clear();tasks_ = {};taskRecords_.clear();pendingCmd_.clear();
        seqCounter_ = state.at("queue_sequence").get<long>();
        for (auto record : state.at("tasks")) {
            const auto id = record.at("task_id").get<std::string>();
            const auto status = record.at("status").get<std::string>();
            const bool retiredHistory = record.value("target_retired",false) &&
                (status == "completed" || status == "cancelled" || status == "interrupted");
            if (!validIdentifier(id) || (!map_.nodeExists(record.at("target").get<std::string>()) && !retiredHistory))
                throw std::invalid_argument("Checkpoint task sai");
            if (record.at("status") == "dispatched") {
                record["status"] = "interrupted";record["reason"] = "Core khoi dong lai; can xac minh robot";
            }
            // A restart never authorizes resuming a previous shift's command.
            record["resume_available"]=false;
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
            a.recoveryHeadingRequired=true; // A Core restart cannot retain proof of the physical front direction.
            a.dropoffExitPending=item.value("dropoff_exit_pending",false);
            if(a.dropoffExitPending) {
                a.liftNode=item.value("dropoff_lift_node",std::string{});
                a.liftCommandId=item.value("dropoff_lift_command_id",0L);
                if(!PhysicalLayout::warehouse(a.liftNode) || a.liftCommandId<=0)
                    throw std::invalid_argument("Checkpoint ha hang cho lui sai");
                a.liftState="INTERRUPTED";a.virtualCargo=true;
                // Keep the hold, never restore boot/session or actuator authorization.
            }
            a.lastSeen = Clock::now()-offlineTimeout_-std::chrono::seconds(1);
            a.uncertainPath = item.at("uncertain_path").get<std::vector<std::string>>();
            a.stoppedNodes = item.value("stopped_nodes",std::vector<std::string>{});
            if (a.stoppedNodes.size()>2) throw std::invalid_argument("Checkpoint stopped region sai");
            for (const auto& node : a.stoppedNodes) {
                if (!map_.nodeExists(node)) throw std::invalid_argument("Checkpoint stopped node sai");
                if (std::find(a.uncertainPath.begin(),a.uncertainPath.end(),node)==a.uncertainPath.end()) a.uncertainPath.push_back(node);
            }
            for (const auto& node : item.at("path")) if (std::find(a.uncertainPath.begin(),a.uncertainPath.end(),node.get<std::string>()) == a.uncertainPath.end()) a.uncertainPath.push_back(node);
            a.lastError = "Core khoi dong lai; can RFID moi va xac minh robot";fleet_[a.id] = a;
        }
        chargeReservation_ = state.at("charge_reservations").get<decltype(chargeReservation_)>();
        acceptingTasks_ = false;
    }
    void reportOffline(const std::string& id) {
        std::vector<std::string> toReplan;
        {
            std::lock_guard<std::mutex> lk(mtx_);auto it = fleet_.find(id);if (it == fleet_.end()) return;
            interruptWorkNoLock(it->second,"MQTT LWT offline");it->second.state = AGVState::OFFLINE;
            it->second.lastSeen = Clock::now()-offlineTimeout_-std::chrono::seconds(1);pendingCmd_.erase(id);
            it->second.lastError = "MQTT LWT; chuyen dong chua xac minh";
            it->second.autoResetEligible = true;it->second.autoResetRequested = false;
            it->second.autoResetCandidateAt = {};
            toReplan = agvsAffectedByFailureNoLock(id);
        }
        arbiter_.cancelWaiting(id);
        refreshStoppedOccupancy();
        appendMutexWaitersForFailedOwners({id},toReplan);
        replanAffectedAgvs({id},toReplan);
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
    // Pham vi van hanh thong nhat: cung mot Core/luat an toan, chi khac danh sach
    // robot duoc phep nhan lenh. Tap rong giu nghia tuong thich "tat ca robot".
    void setActiveRobots(const std::unordered_set<std::string>& ids) {
        for (const auto& id : ids)
            if (id != "01" && id != "02") throw std::invalid_argument("AGV active phai la 01 hoac 02");
        std::lock_guard<std::mutex> lk(mtx_); activeRobotIds_ = ids;
    }
    bool isActiveRobot(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_); return activeRobotNoLock(id);
    }
    void setMonitorOnlyRobot(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_);
        monitorOnlyRobots_.insert(id);
    }
    bool acceptingTasks() { std::lock_guard<std::mutex> lk(mtx_); return acceptingTasks_; }
    bool networkAvailable() { std::lock_guard<std::mutex> lk(mtx_); return networkAvailable_; }
    void setBatteryTestBypass(bool value) { std::lock_guard<std::mutex> lk(mtx_); batteryTestBypass_ = value; }
    bool batteryGateEnabled() { std::lock_guard<std::mutex> lk(mtx_); return !batteryTestBypass_; }
    void setAutomaticChargingEnabled(bool value) { std::lock_guard<std::mutex> lk(mtx_); automaticChargingEnabled_ = value; }
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
            if(a.cancelNeedsNodeProof || a.replanNeedsNodeProof) return "Cho xac minh dung tai RFID moi sau WAIT";
            if (!a.hasTelemetry || !a.positionValid || Clock::now()-a.lastSeen > offlineTimeout_)
                return "Cho vi tri RFID moi";
            if (a.state == AGVState::OFFLINE || a.state == AGVState::AGV_ERROR || !a.uncertainPath.empty())
                return "Can xac minh xe/xoa loi";
            if (a.virtualCargo || (a.liftCommandId>0 && !a.virtualCargoKnown))
                return "Can doi soat ke tren xe truoc khi ve tram";
            if (hasWork(a)) return "Cho hoan thanh task hien tai";
            if (!isChargingStationNode(station)) return "Tram sac khong ton tai";
            const auto reservation = chargeReservation_.find(station);
            if (reservation != chargeReservation_.end() && reservation->second != id)
                return "Tram sac dang duoc xe khac giu";
            if (a.currentNode == station) {
                if (a.state == AGVState::CHARGING) return "";
                chargeReservation_[station] = id;
                a.activePath.clear(); a.finalTarget.clear();
                a.state = automaticChargingEnabled_ ? AGVState::CHARGING : AGVState::IDLE;
                // sendCommand(a,"CHARGE_START"); // Luong sac tu dong goc, giu de bat lai.
                if (automaticChargingEnabled_) sendCommand(a,"CHARGE_START");
                else releaseStationReservationNoLock(id);
            } else {
                if (a.state == AGVState::CHARGING_EN_ROUTE && a.finalTarget == station) return "";
                if (a.state == AGVState::CHARGING) return "Xe dang sac o tram khac";
                const auto departureError=homeDepartureErrorNoLock(a);
                if(!departureError.empty())return departureError;
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
            const auto now = Clock::now();
            const bool diagnosticBatteryFresh = agv.batteryFromDiagnostic &&
                now - agv.batteryDiagnosticSeen <= diagnosticBatteryTimeout_;
            if (battery >= 0) {
                agv.batteryMeasured = true;
                agv.batteryFromDiagnostic = false;
                agv.batteryDiagnosticSeen = {};
                agv.batteryDisplayAvailable = false;
                agv.batteryDisplayPercent = -1;
                agv.battery = std::clamp(battery, 0, 100);
                if (agv.battery <= lowBattery_) agv.needsRecharge = true;
                else if (agv.battery >= resumeBattery_) agv.needsRecharge = false;
            } else if (!diagnosticBatteryFresh) {
                agv.batteryFromDiagnostic = false;
                agv.batteryDiagnosticSeen = {};
                agv.batteryDisplayAvailable = false;
                agv.batteryDisplayPercent = -1;
                agv.batteryMeasured = false;
                agv.battery = -1;
                agv.needsRecharge = true;
            }
            agv.lastSeen = now;
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
                sendCommand(agv, "WAIT",{{"preserve_lift",agv.liftWaitingRecovery}});
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
        refreshStoppedOccupancy();
        if (unknownNode) emit("AGV_" + id + " gui RFID khong ton tai tren map: " + rfid);
        if (recovered) emit("AGV_" + id + " ket noi lai (ONLINE)");
        for (auto& id2 : toReplan) replanAGV(id2); // né vật cản / lệch đường

        checkBatteryOne(id);
        dispatchPending(positionChanged);
        notify();
        return positionChanged;
    }

    // Người vận hành đã chấp nhận phần trăm diagnostic đã lọc làm nguồn pin điều phối.
    // Chỉ dữ liệu batteryCalibrated hợp lệ từ message diagnostic sống mới gọi hàm này.
    void updateBatteryDiagnostic(const std::string& id, int percent) {
        if (percent < -1 || percent > 100) throw std::invalid_argument("Phan tram pin hien thi sai");
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end() || !it->second.hasTelemetry) return;
            AGV& agv = it->second;
            const bool available = percent >= 0;
            const bool clearingDiagnosticControl = !available && agv.batteryFromDiagnostic;
            changed = agv.batteryDisplayAvailable != available ||
                (available && agv.batteryDisplayPercent != percent) ||
                (available && (!agv.batteryMeasured || agv.battery != percent || !agv.batteryFromDiagnostic)) ||
                clearingDiagnosticControl;
            agv.batteryDisplayAvailable = available;
            agv.batteryDisplayPercent = available ? percent : -1;
            if (available) {
                agv.batteryFromDiagnostic = true;
                agv.batteryDiagnosticSeen = Clock::now();
                agv.batteryMeasured = true;
                agv.battery = percent;
                if (agv.battery <= lowBattery_) agv.needsRecharge = true;
                else if (agv.battery >= resumeBattery_) agv.needsRecharge = false;
            } else if (clearingDiagnosticControl) {
                agv.batteryFromDiagnostic = false;
                agv.batteryDiagnosticSeen = {};
                agv.batteryMeasured = false;
                agv.battery = -1;
                agv.needsRecharge = true;
            }
        }
        if (changed) notify();
    }

    // Quan sát riêng từng xe. Encoder không xác nhận node/task/mutex.
    bool encoderEnabled(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx_);
        return (id == "01" || id == "02") && activeRobotNoLock(id);
    }
    bool bindEncoderSession(const std::string& id, uint32_t boot, uint32_t connection, uint32_t uptime) {
        std::unique_lock<std::mutex> lk(mtx_);
        const auto it = fleet_.find(id);
        if ((id == "01" || id == "02") && activeRobotNoLock(id) && it != fleet_.end()) {
            const bool changed=it->second.encoder.boot && (it->second.encoder.boot!=boot || it->second.encoder.connection!=connection);
            const bool bound=it->second.encoder.bind(boot,connection,uptime);
            if(!bound && it->second.encoder.retiredBoots.size()>64 && !it->second.encoderSessionLimitReported) {
                it->second.encoderSessionLimitReported=true;
                lk.unlock();
                emit("AGV_"+id+" vuot 64 lan doi boot; diagnostic bi tu choi, can khoi dong lai Core sau doi soat");
                return false;
            }
            if(bound && changed) {
                invalidatePendingHeadingNoLock(it->second,true);
                it->second.operatorNodeReset=false;it->second.autoResetRequested=false;
                it->second.recoveryAutoResume=false;it->second.recoveryChosenHeading.clear();
                it->second.maintenanceStableSince={};
                it->second.recoveryHeadingRequired=true;it->second.headingPendingId=0;it->second.headingConfirmedId=0;
                it->second.heading.clear();
                it->second.virtualCargoKnown=false;it->second.liftSupported=false;
                it->second.cargoSensorSeen={};it->second.cargoSensorSeq=0;it->second.cargoSensorSupported=false;
                it->second.cargoMonitoringOnly=false;
                lk.unlock();arbiter_.cancelWaiting(id);
            }
            return bound;
        }
        return false;
    }
    // QoS0 error events may be lost. A current, bound diagnostic must not leave
    // Core IDLE while the MCU reports a latched fault. It never clears a fault.
    void observeEncoderFirmwareFault(const std::string& id, const MotionDiagnostic& d) {
        if (d.error.empty() || d.error == "NONE" || (d.state != "AGV_SAFE_STOP" && d.state != "AGV_ERROR")) return;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            const auto it = fleet_.find(id);
            if ((id != "01" && id != "02") || !activeRobotNoLock(id) ||
                it == fleet_.end() || !encoderReadyNoLock(it->second) ||
                !it->second.encoder.boot || d.node != it->second.currentNode || !d.wifi || !d.mqtt) return;
        }
        reportError(id,d.error);
    }
    bool observeOdometry(const std::string& id, const EncoderSample& s) {
        if (s.stoppedPosition) return observeStoppedPosition(id,s);
        bool invalidate=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it=fleet_.find(id);
            if (it!=fleet_.end() && s.robotId==id && !it->second.stoppedNodes.empty() &&
                it->second.state==AGVState::AGV_ERROR && it->second.encoder.accept(s)) {
                it->second.stoppedSeen={};it->second.hasStopCandidate=false;invalidate=true;
            }
        }
        if (invalidate) {refreshStoppedOccupancy();return true;}
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = fleet_.find(id);
        if ((id != "01" && id != "02") || s.robotId != id || !activeRobotNoLock(id) ||
            it == fleet_.end()) return false;
        auto& a = it->second;
        if (!encoderReadyNoLock(a) || a.currentNode != s.from) return false;
        if (s.tracking && (a.routeCommandId != s.route || a.activePath.empty() || a.activePath.front() != s.to ||
            a.commandPath.size() <= static_cast<size_t>(s.index) || a.commandPath[s.index] != s.to)) return false;
        if (!s.tracking && !a.activePath.empty()) return false;
        json estimate = nullptr;
        if (s.tracking) {
            const auto layout = map_.edgeGeometry(s.from,s.to);
            if(layout.is_null()) return false;
            json edge = nullptr;
            for (const auto& e : layout["edges"]) if ((e["from"] == s.from && e["to"] == s.to) ||
                (e.value("two_way",false) && e["to"] == s.from && e["from"] == s.to)) {edge = e;break;}
            json from, to;
            for (const auto& n : layout["nodes"]) {if (n["id"] == s.from) from = n;if (n["id"] == s.to) to = n;}
            if (edge.is_null() || from.is_null() || to.is_null()) return false;
            const double length = edge.at("weight").get<double>();
            if (!std::isfinite(length) || length <= 0) return false;
            // Map test 01 dùng cm. Giữ marker trước đích cho tới khi có RFID thật.
            const double progress = std::clamp(s.distance / length,0.0,0.98);
            const auto point=EdgeGeometry::point(layout,edge,s.from,progress*length);
            estimate = {{"source","encoder"},{"from",s.from},{"to",s.to},{"progress",progress},
                {"x",point.at("x")},{"y",point.at("y")},{"distance_cm",s.distance},
                {"edge_length_cm",length},{"route_cmd_id",s.route},{"path_index",s.index},{"reverse",s.reverse}};
        }
        if (!a.encoder.accept(s)) return false;
        a.estimatedPosition = estimate;
        return true;
    }

    // Two increasing samples with identical raw counters, plus a bound STOP
    // diagnostic, authorize only coarse node/edge occupancy. ERROR stays latched.
    bool observeStoppedPosition(const std::string& id, const EncoderSample& s) {
        bool changed=false, waitingForStable=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it=fleet_.find(id);
            if (!s.stoppedPosition || s.robotId!=id || !activeRobotNoLock(id) || it==fleet_.end()) return false;
            auto& a=it->second;
            const auto now=Clock::now();
            if (!networkAvailable_ || a.state!=AGVState::AGV_ERROR || !a.hasTelemetry || !a.positionValid ||
                now-a.lastSeen>std::chrono::milliseconds(1000) || s.from!=a.currentNode || s.stopFrom!=a.currentNode ||
                now-a.stoppedDiagnosticSeen>std::chrono::milliseconds(1500) ||
                static_cast<int32_t>(s.uptime-a.stoppedDiagnosticUptime)<-500 ||
                static_cast<int32_t>(s.uptime-a.stoppedDiagnosticUptime)>1500 ||
                a.encoder.boot!=s.boot || a.encoder.connection!=s.connection) return false;
            if (s.stopTo.empty()) {
                if (a.stoppedDiagnosticRfidAge>250) return false;
            } else {
                if (!map_.nodeExists(s.stopTo) || s.stopRoute!=a.routeCommandId ||
                    a.commandPath.size()<=static_cast<size_t>(s.stopIndex) ||
                    a.commandPath[s.stopIndex]!=s.stopTo ||
                    (s.stopIndex ? a.commandPath[s.stopIndex-1] : a.routeStart)!=s.stopFrom) return false;
                const auto length=map_.pathCost({s.stopFrom,s.stopTo});
                if (length<=0 || length==LLONG_MAX || s.stopDistance>length) return false;
            }
            if (!a.encoder.accept(s)) return false;
            const auto previous=a.stopCandidate;
            const bool stable=a.hasStopCandidate && previous.boot==s.boot && previous.connection==s.connection &&
                previous.stopFrom==s.stopFrom && previous.stopTo==s.stopTo && previous.stopRoute==s.stopRoute &&
                previous.stopIndex==s.stopIndex && previous.leftTicks==s.leftTicks && previous.rightTicks==s.rightTicks &&
                previous.stopDistance==s.stopDistance &&
                s.stationaryMs>=previous.stationaryMs && s.uptime-previous.uptime>=200 && s.uptime-previous.uptime<=1000;
            a.stopCandidate=s;a.hasStopCandidate=true;
            if (!stable) {
                if (a.stoppedLocalized) a.stoppedSeen={};
                waitingForStable=true;
            } else {
                std::vector<std::string> nodes{s.stopFrom};if (!s.stopTo.empty()) nodes.push_back(s.stopTo);
                changed=!localizedStopNoLock(a) || a.stoppedNodes!=nodes;
                a.stoppedNodes=nodes;a.stoppedSeen=now;a.stoppedLocalized=true;
            }
        }
        refreshStoppedOccupancy();
        if (waitingForStable) return true;
        if (changed) {
            // Reroute/cancel old waiters before releasing any outside grant.
            replanAllForStoppedOccupancy();
            const auto zones=arbiter_.toJson();
            for (const auto& zone : zones.items()) {
                if (zone.value().value("owner",std::string{})!=id) continue;
                bool occupied=true;
                { std::lock_guard<std::mutex> lk(mtx_);
                  const auto& a=fleet_.at(id);
                  occupied=!localizedStopNoLock(a) || std::find(a.stoppedNodes.begin(),a.stoppedNodes.end(),zone.key())!=a.stoppedNodes.end(); }
                if (occupied || !zone.value().contains("grant_id")) continue;
                if (arbiter_.confirmClear(zone.key(),id,zone.value().at("grant_id").get<long>())) {
                    const auto owner=arbiter_.currentOwner(zone.key());
                    const auto grant=arbiter_.grant(zone.key(),owner);
                    if (!grant.is_null() && publish_) publish_("agv/"+owner+"/mutex_grant",grant.dump());
                    emit("AGV_"+id+" dung da xac minh: tu tra mutex "+zone.key());
                }
            }
            replanAllForStoppedOccupancy();
            notify();dispatchPending();
        }
        return true;
    }

    // May be called before any mutex request/release and every tick. No nested
    // Fleet/Arbiter lock; server serializes state mutation with g_stateMtx.
    void refreshStoppedOccupancy() {
        bool changed=false;
        std::unordered_set<std::string> blocked;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for (auto& kv : fleet_) {
                auto& a=kv.second;
                if (a.stoppedNodes.empty()) continue;
                if (a.state==AGVState::IDLE && a.uncertainPath.empty()) {
                    a.stoppedNodes.clear();a.stoppedLocalized=false;a.hasStopCandidate=false;changed=true;continue;
                }
                const bool fresh=localizedStopNoLock(a);
                if (a.stoppedLocalized && !fresh) {a.stoppedLocalized=false;a.hasStopCandidate=false;changed=true;}
                blocked.insert(a.stoppedNodes.begin(),a.stoppedNodes.end());
                if (!fresh) {
                    std::unordered_set<std::string> uncertain(a.uncertainPath.begin(),a.uncertainPath.end());
                    uncertain.insert(a.currentNode);uncertain.insert(a.stoppedNodes.begin(),a.stoppedNodes.end());
                    const auto copy=uncertain;
                    for (const auto& node : copy) {const auto component=map_.mutexComponent(node);uncertain.insert(component.begin(),component.end());}
                    blocked.insert(uncertain.begin(),uncertain.end());
                }
            }
        }
        arbiter_.setBlockedZones(blocked);
        if (changed) {replanAllForStoppedOccupancy();notify();}
    }

    // Diagnostic khong retained duoc gui ngay sau telemetry. Chi phuc hoi ca
    // thu hai xe tai tram rieng; telemetry pipe mot minh khong noi len loi MCU.
    void observeMotionDiagnostic(const std::string& id, const MotionDiagnostic& diagnostic) {
        if (diagnostic.lastCommandId >= 0 && commandIdHighwaterCallback_)
            commandIdHighwaterCallback_(diagnostic.lastCommandId);
        observeCargoSensor(id,diagnostic);
        observeLift(id,diagnostic);
        bool nodeProofAccepted=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it=fleet_.find(id);
            if (it!=fleet_.end()) {
                auto& a=it->second;
                if(diagnostic.maintenanceRecoverySupported) {
                    a.maintenanceSupported=true;a.maintenanceReady=diagnostic.maintenanceReady;
                    const bool home=PhysicalLayout::home(id)==a.currentNode;
                    const bool parked=home && a.taskId.empty() && a.finalTarget.empty() &&
                        a.activePath.empty() && !diagnostic.taskActive && !diagnostic.liftBusy;
                    if(parked&&!a.maintenanceArrival) {
                        a.recoveryHeadingRequired=true;a.heading.clear();a.maintenanceStableSince={};
                        a.recoveryAutoResume=false;
                    }
                    if(!home)a.maintenanceArrival=false;
                    else if(parked)a.maintenanceArrival=true;
                    a.maintenanceNeedsReset=home && (diagnostic.liftState=="WAITING_LIFT_RECOVERY"||diagnostic.liftState=="INTERRUPTED");
                    if(home && parked && diagnostic.liftState=="IDLE" && diagnostic.error=="NONE" && diagnostic.cargoMonitoringOnly) {
                        a.virtualCargoKnown=true;a.virtualCargo=false;
                    }
                }
                const bool nodeStopped=diagnostic.encoderBoot && a.encoder.boot==diagnostic.encoderBoot &&
                    a.encoder.connection==diagnostic.encoderConnection && diagnostic.node==a.currentNode &&
                    diagnostic.wifi && diagnostic.mqtt && diagnostic.motor=="STOP" &&
                    diagnostic.leftDuty==0 && diagnostic.rightDuty==0 && !diagnostic.taskActive && !diagnostic.liftBusy &&
                    (diagnostic.state=="AGV_IDLE" || diagnostic.state=="AGV_WAITING" ||
                     diagnostic.state=="AGV_SAFE_STOP" || diagnostic.state=="AGV_ERROR");
                a.nodeStopSeen=nodeStopped ? Clock::now() : Clock::time_point{};
                a.nodeStopNode=diagnostic.node;a.nodeStopBoot=diagnostic.encoderBoot;
                a.nodeStopConnection=diagnostic.encoderConnection;a.nodeStopUptime=diagnostic.encoderUptime;
                a.nodeStopRfidAge=diagnostic.rfidAgeMs;a.nodeStopLastCommand=diagnostic.lastCommandId;
                a.nodeStopHealthy=nodeStopped && diagnostic.error=="NONE" &&
                    (diagnostic.state=="AGV_IDLE" || diagnostic.state=="AGV_WAITING");
                const bool cargoStopped=diagnostic.encoderBoot && a.encoder.boot==diagnostic.encoderBoot &&
                    a.encoder.connection==diagnostic.encoderConnection && diagnostic.node==a.currentNode &&
                    diagnostic.wifi && diagnostic.mqtt && diagnostic.motor=="STOP" &&
                    diagnostic.leftDuty==0 && diagnostic.rightDuty==0 && !diagnostic.taskActive &&
                    diagnostic.liftPresent && !diagnostic.liftBusy && diagnostic.rfidAgeMs!=UINT32_MAX &&
                    (diagnostic.state=="AGV_IDLE" || diagnostic.state=="AGV_SAFE_STOP" || diagnostic.state=="AGV_ERROR");
                a.cargoReconcileSeen=cargoStopped ? Clock::now() : Clock::time_point{};
                a.cargoReconcileBoot=diagnostic.encoderBoot;a.cargoReconcileConnection=diagnostic.encoderConnection;
                const bool stopped=diagnostic.encoderBoot && a.encoder.boot==diagnostic.encoderBoot &&
                    a.encoder.connection==diagnostic.encoderConnection && diagnostic.node==a.currentNode &&
                    diagnostic.wifi && diagnostic.mqtt && diagnostic.motor=="STOP" &&
                    diagnostic.leftDuty==0 && diagnostic.rightDuty==0 && !diagnostic.taskActive &&
                    (diagnostic.state=="AGV_SAFE_STOP" || diagnostic.state=="AGV_ERROR");
                if (stopped) {
                    a.stoppedDiagnosticSeen=Clock::now();a.stoppedDiagnosticUptime=diagnostic.encoderUptime;
                    a.stoppedDiagnosticRfidAge=diagnostic.rfidAgeMs;
                } else a.stoppedDiagnosticSeen={};
                if ((a.replanNeedsNodeProof || a.cancelNeedsNodeProof) && a.state==AGVState::BLOCKED &&
                    networkAvailable_ && a.hasTelemetry && a.positionValid && Clock::now()-a.lastSeen<=offlineTimeout_ &&
                    diagnostic.encoderBoot && a.encoder.boot==diagnostic.encoderBoot &&
                    a.encoder.connection==diagnostic.encoderConnection &&
                    static_cast<int32_t>(diagnostic.encoderUptime-a.replanHoldUptime)>0 &&
                    (diagnostic.lastCommandId<0 || diagnostic.lastCommandId>=a.stopCommandId) &&
                    diagnostic.node==a.currentNode && diagnostic.rfidAgeMs<=250 && diagnostic.wifi && diagnostic.mqtt &&
                    diagnostic.motor=="STOP" && diagnostic.leftDuty==0 && diagnostic.rightDuty==0 &&
                    !diagnostic.taskActive && diagnostic.error=="NONE" &&
                    (diagnostic.state=="AGV_IDLE" || diagnostic.state=="AGV_WAITING")) {
                    a.replanNeedsNodeProof=false;a.uncertainPath.clear();
                    if(a.cancelNeedsNodeProof) {a.cancelNeedsNodeProof=false;a.state=AGVState::IDLE;}
                    a.nodeProofSince={};a.nodeProofLastWarning={};nodeProofAccepted=true;
                }
            }
        }
        if(nodeProofAccepted) {emit("AGV_"+id+" da xac minh dung tai RFID moi sau WAIT");notify();dispatchPending(true);}
        std::string sonarEvent;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if ((id == "01" || id == "02") && activeRobotNoLock(id) && it != fleet_.end() &&
                diagnostic.sonarPresent && diagnostic.wifi && diagnostic.mqtt &&
                diagnostic.node == it->second.currentNode && networkAvailable_) {
                auto& a = it->second;
                if (a.sonarObstacle != diagnostic.sonarObstacle ||
                    (diagnostic.sonarObstacle && a.sonarDirection != diagnostic.sonarDirection))
                    sonarEvent = diagnostic.sonarObstacle
                        ? "AGV_" + id + " VAT CAN PHIA " + std::string(diagnostic.sonarDirection == "rear" ? "SAU" : "TRUOC") +
                          " (" + std::to_string(static_cast<int>(std::lround(diagnostic.sonarDistanceCm))) + " cm); dang dung cho"
                        : "AGV_" + id + " DA HET VAT CAN; tiep tuc tuyen neu cac chot an toan hop le";
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
        observeMaintenance(id);
        if (PhysicalLayout::home(id)==diagnostic.node)
            recoverVerifiedNode(id,diagnostic.node,true,false);
        std::vector<std::pair<std::string,std::string>> newGrants;
        std::vector<std::string> clearedZones;
        bool changed = false, recovered = false, resetRequested = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end() || !diagnosticRecoveryNoLock() || !networkAvailable_) return;
            AGV& agv = it->second;
            const auto home = homeStations_.find(id);
            if(agv.maintenanceSupported&&!agv.maintenanceReady)return;
            if(agv.cancelNeedsNodeProof || agv.replanNeedsNodeProof) return;
            const bool expectedHome = home != homeStations_.end() && !PhysicalLayout::home(id).empty() &&
                home->second==PhysicalLayout::home(id);
            if (!expectedHome || !agv.hasTelemetry || !agv.positionValid ||
                Clock::now() - agv.lastSeen > offlineTimeout_ || agv.currentNode != home->second ||
                diagnostic.node != home->second || !diagnostic.wifi || !diagnostic.mqtt ||
                diagnostic.motor != "STOP" || diagnostic.leftDuty != 0 || diagnostic.rightDuty != 0 ||
                diagnostic.taskActive || !verifiedNodeStopNoLock(agv) ||
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

                const bool healthy = diagnostic.state == "AGV_IDLE" && diagnostic.error == "NONE" && !agv.operatorNodeReset;
                // STALE_COMMAND means Core and the MCU disagree about the
                // persistent command counter. A healthy motor diagnostic must
                // not immediately erase that protocol fault and dispatch more
                // low IDs. After the operator requests clear_error,
                // autoResetRequested allows the normal diagnostic gate again.
                const bool staleCounterLatched = agv.lastError == "STALE_COMMAND" && !agv.autoResetRequested;
                if (agv.state == AGVState::AGV_ERROR && healthy && !staleCounterLatched) {
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
        bool interrupted = false;
        std::string commandContext;
        const bool staleCommand = msg == "STALE_COMMAND";
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return;
            AGV& agv = it->second;
            if(msg=="INVALID_COMMAND" || msg=="STALE_COMMAND") {
                // Capture Core's last known context before interrupt/STOP clears it.
                // It is evidence for diagnosis, not proof of which MCU check failed.
                json context={{"node",agv.currentNode},{"target",agv.finalTarget},{"task_id",agv.taskId},
                    {"route_cmd_id",agv.routeCommandId},{"route_start",agv.routeStart},{"path",agv.commandPath},
                    {"heading",agv.heading},{"heading_source",agv.headingSource}};
                context["diagnostic_last_command_id"]=agv.nodeStopLastCommand;
                context["stop_command_id"]=agv.stopCommandId;
                const auto pending=pendingCmd_.find(id);
                if(pending!=pendingCmd_.end())context["pending_command"]=pending->second.payload;
                commandContext="; core_context="+context.dump();
            }
            interrupted = interruptWorkNoLock(agv, msg);
            agv.nodeStopSeen={};
            agv.lastError = msg;
            agv.state = AGVState::AGV_ERROR;
            if(PhysicalLayout::home(id)==agv.currentNode)agv.recoveryHeadingRequired=true;
            agv.autoResetEligible = msg == "NETWORK_UNAVAILABLE" || msg == "WIFI_TIMEOUT" || msg == "MQTT_TIMEOUT";
            agv.autoResetRequested = false;agv.autoResetCandidateAt = {};
            // Xe lỗi có thể nằm giữa RFID hiện tại và phần đường còn lại. Replan
            // mọi tuyến giao với toàn bộ footprint chưa xác minh, kể cả hành lang
            // gồm nhiều mutex nối trực tiếp, không chỉ đúng currentNode.
            toReplan = agvsAffectedByFailureNoLock(id);
            if (staleCommand) {
                // The rejected command did not enter the MCU queue. Retrying it
                // or issuing another still-low STOP creates an error storm.
                // Latch ERROR, discard the pending retry and wait for a fresh
                // diagnostic high-water plus explicit operator recovery.
                pendingCmd_.erase(id);
            } else if(!agv.liftWaitingRecovery)sendCommand(agv, "STOP");
        }
        arbiter_.cancelWaiting(id);
        appendMutexWaitersForFailedOwners({id},toReplan);
        emit("AGV_" + id + " BAO LOI: " + msg + (interrupted ? " -> task bi gian doan, can xac minh" : "")+commandContext);
        replanAffectedAgvs({id},toReplan);
        notify();
        dispatchPending();
    }

    void observeHeadingDiagnostic(const std::string& id, const MotionDiagnostic& d) {
        const auto zones=arbiter_.toJson();
        bool changed = false;
        bool finished = false;
        bool restoreBayHeading=false;std::string bayNode,bayHeading;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id); if (it == fleet_.end()) return;
            auto& a = it->second;
            const bool supported = d.headingSupported && d.wifi && d.mqtt && networkAvailable_;
            if(a.headingPendingId>0 && Clock::now()-a.headingRequestedAt>std::chrono::seconds(5)) {
                invalidatePendingHeadingNoLock(a,true);changed=true;
            }
            const bool valid = supported && a.positionValid && a.currentNode == d.node && d.headingNode == d.node &&
                (d.heading == "N" || d.heading == "E" || d.heading == "S" || d.heading == "W") &&
                Clock::now() - a.lastSeen <= offlineTimeout_;
            if(a.state==AGVState::AGV_ERROR && (!valid || d.headingSource=="bay_default") && !a.recoveryHeadingRequired) {
                a.recoveryHeadingRequired=true;changed=true;
            }
            a.headingConfirmationSupported=d.headingConfirmationSupported;
            bool ownsZone=false;
            for(const auto& zone:zones.items())if(zone.value().value("owner",std::string{})==id)ownsZone=true;
            const bool reconnectOnly=a.state==AGVState::AGV_ERROR &&
                a.lastError=="Ket noi lai: can xac minh robot va xoa loi thu cong";
            // A healthy parked robot uses the agreed pose of its own bay.
            // RFID anchor age and maintenance line/sonar checks are not a
            // manual heading confirmation. Keep full recovery proof for
            // faults, uncertain occupancy, active work and other nodes.
            const bool parkedBayStop = supported && a.positionValid && a.hasTelemetry &&
                Clock::now()-a.lastSeen<=offlineTimeout_ && a.currentNode==d.node && activeRobotNoLock(id) &&
                PhysicalLayout::home(id)==a.currentNode &&
                ((a.state==AGVState::IDLE && a.lastError.empty()) || reconnectOnly) &&
                d.state=="AGV_IDLE" && d.error=="NONE" &&
                d.motor=="STOP" && d.leftDuty==0 && d.rightDuty==0 && !d.taskActive && !d.liftBusy &&
                d.encoderBoot && d.encoderBoot==a.encoder.boot && d.encoderConnection==a.encoder.connection &&
                a.nodeStopSeen!=Clock::time_point{} && Clock::now()-a.nodeStopSeen<=std::chrono::milliseconds(1500) &&
                a.nodeStopUptime==d.encoderUptime && d.rfidAgeMs!=UINT32_MAX &&
                a.taskId.empty() && a.finalTarget.empty() && a.activePath.empty() && a.uncertainPath.empty() &&
                a.stoppedNodes.empty() && !ownsZone && !a.dropoffExitPending && !a.liftWaitingRecovery &&
                !a.cancelNeedsNodeProof && !a.replanNeedsNodeProof && !a.liftActive &&
                !a.operatorNodeReset && !a.autoResetRequested && a.headingPendingId==0;
            const bool parkedBayHeading=parkedBayStop && valid &&
                d.heading==PhysicalLayout::outwardHeading(a.currentNode) &&
                (d.headingSource=="bay_default" || (d.headingSource=="route" && d.stationReady));
            if(parkedBayStop && reconnectOnly){a.state=AGVState::IDLE;a.lastError.clear();a.autoResetEligible=false;changed=true;}
            if(parkedBayHeading && a.recoveryHeadingRequired) {
                if(reconnectOnly){a.state=AGVState::IDLE;a.lastError.clear();a.autoResetEligible=false;}
                a.recoveryHeadingRequired=false;a.headingConfirmedId=0;
                a.recoveryChosenHeading.clear();a.recoveryAutoResume=false;changed=true;
            }
            if(parkedBayStop && a.recoveryHeadingRequired && d.headingSource=="unknown" &&
                (d.heading.empty() || d.heading=="UNKNOWN") && d.rfidAgeMs<=250 && a.headingConfirmationSupported) {
                restoreBayHeading=true;bayNode=a.currentNode;bayHeading=PhysicalLayout::outwardHeading(a.currentNode);
            }
            const bool confirmation = valid && a.headingPendingId>0 && d.headingCommandId==a.headingPendingId &&
                d.headingSource=="operator" && d.heading==a.headingRequested && d.node==a.headingRequestedNode &&
                d.encoderBoot==a.headingRequestBoot && d.encoderConnection==a.headingRequestConnection &&
                static_cast<int32_t>(d.encoderUptime-a.headingRequestUptime)>0 && d.lastCommandId>=a.headingPendingId &&
                (verifiedNodeStopNoLock(a) || healthyParkedHomeNoLock(a)) && a.nodeStopHealthy && a.state==AGVState::IDLE &&
                !a.liftActive && a.taskId.empty() && a.activePath.empty() && a.finalTarget.empty();
            if(confirmation) {
                const auto pending=pendingCmd_.find(id);
                if(pending!=pendingCmd_.end() && pending->second.cmdId==a.headingPendingId)pendingCmd_.erase(pending);
                a.headingConfirmedId=a.headingPendingId;a.headingPendingId=0;a.recoveryHeadingRequired=false;changed=true;
            }
            const bool operatorHeading=d.headingSource=="operator" && a.headingConfirmedId>0 &&
                d.headingCommandId==a.headingConfirmedId && d.node==a.headingRequestedNode &&
                d.encoderBoot==a.headingRequestBoot && d.encoderConnection==a.headingRequestConnection;
            const std::string heading = valid && !a.recoveryHeadingRequired &&
                (d.headingSource == "bay_default" || d.headingSource == "route" || operatorHeading) ? d.heading : "";
            changed = changed || a.heading != heading || a.headingSupported != supported || a.headingNode != d.headingNode ||
                      a.headingSource != d.headingSource;
            a.heading = heading; a.headingNode = d.headingNode; a.headingSource = d.headingSource;
            a.headingSupported = supported; a.diagnosticSeen = Clock::now();
            const bool station = PhysicalLayout::isHome(a.currentNode);
            const bool capability = d.stationDockingSupported && supported;
            // Invalidate every command's old ready sample. Completion needs a
            // fresh stopped diagnostic at the confirmed RFID, facing the GS gate.
            // The front reader can leave the tag during the parking pivot;
            // RFID age is an anchor age, not the diagnostic packet's freshness.
            const bool ready = capability && valid && station && d.stationReady &&
                heading == PhysicalLayout::outwardHeading(a.currentNode) &&
                d.state == "AGV_IDLE" && d.error == "NONE" && !d.taskActive &&
                d.motor == "STOP" && d.leftDuty == 0 && d.rightDuty == 0 && d.rfidAgeMs != UINT32_MAX;
            changed = changed || a.stationDockingSupported != capability || a.stationReady != ready;
            a.stationDockingSupported = capability;
            a.stationReady = ready;
            if (capability && PhysicalLayout::isHome(a.finalTarget))
                a.stationArrivalRequired = true;
            finished = completeArrivalNoLock(a);
        }
        if(restoreBayHeading)confirmRecoveryHeading(id,bayNode,bayHeading);
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
            (d.stationTurnRequired && !PhysicalLayout::isHome(node))) return;
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
        if (d.reverseHomeSupported && d.stationTurnRequired && PhysicalLayout::isHome(node)) return;
        auto order = NodeManeuver::plan(map_.toJson(),node,next,a.heading,d.stationTurnRequired,
                                      d.reverseHomeSupported);
        if (order.is_null()) return;
        order["node"] = node; order["next_node"] = next;
        order["route_cmd_id"] = a.routeCommandId; order["path_index"] = step;
        const auto commandId = sendCommand(a,"NODE_ORDER",order);
        if (commandId > 0) {
            a.lastNodeOrderKey = key; a.lastNodeOrderAt = Clock::now();
            a.lastNodeOrderPayload = pendingCmd_.at(id).payload;
        }
    }

    // Explicit recovery for either robot may finish at its verified RFID.
    // ACK does not clear the fault; mutex ownership is never released here.
    void observeRobot01Reset(const std::string& id, const MotionDiagnostic& d) {
        bool cleared = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = fleet_.find(id);
            if (it == fleet_.end() || !networkAvailable_ || !activeRobotNoLock(id) ||
                (!it->second.operatorNodeReset && !(trialRobotId_ == "01" && id == "01"))) return;
            auto& a = it->second;
            if (!a.autoResetRequested || a.state != AGVState::AGV_ERROR ||
                !a.hasTelemetry || !a.positionValid || Clock::now()-a.lastSeen > offlineTimeout_ ||
                a.currentNode != d.node || d.rfidAgeMs == UINT32_MAX ||
                !d.wifi || !d.mqtt || d.taskActive || d.state != "AGV_IDLE" || d.error != "NONE" ||
                d.motor != "STOP" || d.leftDuty != 0 || d.rightDuty != 0 ||
                !a.taskId.empty() || !a.finalTarget.empty() || !a.activePath.empty()) return;
            if (a.operatorNodeReset && (a.currentNode!=a.recoveryExpectedNode || !verifiedNodeStopNoLock(a) || !a.nodeStopHealthy ||
                d.encoderBoot!=a.resetBoot || d.encoderConnection!=a.resetConnection ||
                static_cast<int32_t>(d.encoderUptime-a.replanHoldUptime)<=0 ||
                d.lastCommandId<a.stopCommandId)) return;
            a.uncertainPath.clear();
            a.state = AGVState::IDLE; a.lastError.clear();
            a.autoResetEligible = false; a.autoResetRequested = false; a.autoResetCandidateAt = {};
            a.operatorNodeReset=false;
            const auto pending=pendingCmd_.find(id);
            if(pending!=pendingCmd_.end() && pending->second.cmdId==a.stopCommandId)pendingCmd_.erase(pending);
            cleared = true;
        }
        if (cleared) {
            emit("AGV_"+id+" da reset tai RFID hien tai; co the tiep tuc nhiem vu cung ca");
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
            if(it->second.headingPendingId>0)return false;
            // An earlier Core could clear its own ERROR while the ESP32 kept
            // a latched fault. Permit an explicit repeat while safely IDLE.
            const bool latched = it->second.state == AGVState::AGV_ERROR;
            const bool idleRepair = diagnosticRecoveryNoLock() && it->second.state == AGVState::IDLE &&
                it->second.taskId.empty() && it->second.activePath.empty() &&
                it->second.uncertainPath.empty();
            if (!latched && !idleRepair) return false;
            if (!it->second.positionValid || Clock::now() - it->second.lastSeen > offlineTimeout_) return false;
            if (!networkAvailable_) return false;
            awaitingDiagnostic = diagnosticRecoveryNoLock();
            if (awaitingDiagnostic) {
                const auto home = homeStations_.find(id);
                const bool robot01Reset = trialRobotId_ == "01" && id == "01";
                if (!robot01Reset && (home == homeStations_.end() ||
                    PhysicalLayout::home(id).empty() || home->second != PhysicalLayout::home(id) ||
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
            it->second.replanHoldUptime=it->second.encoder.diagnosticUptime;
            it->second.stopCommandId=sendCommand(it->second,"RESET_FAULT");
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
    // A physical servo operation has its own cmd_id; receipt ACK never completes it.
    std::string startLift(const std::string& id,const std::string& node,bool up,long& commandId) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it=fleet_.find(id);
            if((id!="01" && id!="02") || !activeRobotNoLock(id) ||
                it==fleet_.end()) return "Firmware/cau hinh xe chua cho phep servo";
            auto& a=it->second;
            if(a.recoveryHeadingRequired || a.headingPendingId>0)return "Can hoan tat khoi phuc/huong truoc khi nang ha";
            if(a.dropoffExitPending)return "Cho lui ra gate va xac nhan ke da roi xe; khong lap servo";
            if(!up && !a.dropoffExitSupported)return "Can firmware ho tro ha hang roi lui ra gate";
            if(!acceptingTasks_ || !networkAvailable_ || !tasks_.empty() || !a.hasTelemetry || !a.positionValid ||
                Clock::now()-a.lastSeen>offlineTimeout_ || a.currentNode!=node || !PhysicalLayout::warehouse(node) ||
                a.state!=AGVState::IDLE || a.liftActive || !a.lastError.empty() ||
                !a.taskId.empty() || !a.finalTarget.empty() || !a.activePath.empty() || !a.uncertainPath.empty())
                return "Servo can xe dung yen dung kho, khong task hoac loi";
            if(!a.liftSupported || !a.liftReady || a.liftObservedNode!=node || Clock::now()-a.liftSeen>std::chrono::seconds(3)) return "Firmware servo chua san sang";
            if(!a.cargoMonitoringOnly)return "Can nap firmware cargo-monitor-v28 cho xe truoc khi nang/ha theo chu ky 5s";
            a.liftActive=true;a.liftWaitingRecovery=false;a.liftState="PENDING";a.liftNode=node;a.liftAction=up?"pickup":"dropoff";
            a.liftCargoUptime=a.liftDiagnosticUptime;
            a.liftStarted=Clock::now();a.liftBoot=a.encoder.boot;a.liftConnection=a.encoder.connection;
            a.state=up?AGVState::LIFTING:AGVState::LOWERING;
            commandId=sendCommand(a,up?"LIFT_UP":"LIFT_DOWN",{{"node",node}});
            a.liftCommandId=commandId;
            if(commandId<=0) {a.liftActive=false;a.liftState="INTERRUPTED";return "Khong phat duoc lenh servo";}
        }
        notify();return "";
    }

    // Operator reconciliation changes only cargo bookkeeping, never actuators,
    // position, faults or mutex. Validate the whole batch before changing either robot.
    std::string reconcileTestCargo(const std::vector<std::pair<std::string,long>>& robots,bool /*homeRollback*/=false) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for(const auto& entry:robots) {
                const auto& id=entry.first;
                const auto it=fleet_.find(id);
                if(it==fleet_.end() || !activeRobotNoLock(id))return "Xe khong thuoc pham vi van hanh";
                const auto& a=it->second;
                if(!a.taskId.empty() || !a.finalTarget.empty() || !a.activePath.empty() || a.liftActive ||
                    std::any_of(taskRecords_.begin(),taskRecords_.end(),[&](const auto& task){
                        return task.second.value("assigned_agv",std::string{})==id &&
                            (task.second.value("status",std::string{})=="pending" || task.second.value("status",std::string{})=="dispatched");}))
                    return "Xe dang co nhiem vu/nang ha; dung hoac huy nhiem vu truoc khi doi soat";
            }
            for(const auto& entry:robots) {
                auto& a=fleet_.at(entry.first);a.virtualCargoKnown=true;a.virtualCargo=false;
                a.dropoffExitPending=false;
            }
        }
        notify();return "";
    }

    bool cargoSensorReadyNoLock(const AGV& a,bool expected) const {
        return networkAvailable_ && a.hasTelemetry && a.positionValid &&
            Clock::now()-a.lastSeen<=std::chrono::milliseconds(1500) && a.cargoSensorSeen!=Clock::time_point{} &&
            Clock::now()-a.cargoSensorSeen<=std::chrono::milliseconds(1500) &&
            a.cargoSensorBoot==a.encoder.boot && a.cargoSensorConnection==a.encoder.connection &&
            a.cargoSensorSupported && a.cargoSensorStable && a.cargoSensorStopped &&
            a.cargoSensorNode==a.currentNode && a.cargoSensorPresent==expected;
    }
    void observeCargoSensor(const std::string& id,const MotionDiagnostic& d) {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it=fleet_.find(id);
        if(it==fleet_.end() || !activeRobotNoLock(id) || !d.encoderBoot ||
            d.encoderBoot!=it->second.encoder.boot || d.encoderConnection!=it->second.encoder.connection)return;
        auto& a=it->second;
        // Every accepted diagnostic invalidates a missing/malformed observation.
        if(!d.cargoSensorSupported || d.cargoSensorAge>500 ||
            (a.cargoSensorSeen!=Clock::time_point{} &&
             (static_cast<int32_t>(d.cargoSensorSeq-a.cargoSensorSeq)<=0 ||
              static_cast<int32_t>(d.encoderUptime-a.cargoSensorUptime)<=0))) {
            a.cargoSensorStable=false;return;
        }
        a.cargoSensorSupported=true;a.cargoSensorStable=d.cargoSensorStable && d.cargoSensorStableMs>=150;
        a.cargoSensorPresent=d.cargoSensorPresent;a.cargoSensorSeq=d.cargoSensorSeq;
        a.cargoSensorUptime=d.encoderUptime;a.cargoSensorBoot=d.encoderBoot;a.cargoSensorConnection=d.encoderConnection;
        a.cargoSensorNode=d.node;a.cargoSensorSeen=Clock::now();
        a.cargoSensorStopped=d.wifi && d.mqtt && d.error=="NONE" && d.motor=="STOP" &&
            d.leftDuty==0 && d.rightDuty==0 && !d.taskActive && d.state=="AGV_IDLE" && d.rfidAgeMs!=UINT32_MAX;
    }
    void observeLift(const std::string& id,const MotionDiagnostic& d) {
        bool changed=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it=fleet_.find(id);
            if((id!="01" && id!="02") || !activeRobotNoLock(id) ||
                it==fleet_.end() || !d.liftPresent || !networkAvailable_) return;
            auto& a=it->second;
            if(!d.encoderBoot || d.encoderBoot!=a.encoder.boot || d.encoderConnection!=a.encoder.connection)return;
            if(a.liftDiagnosticSeen && a.liftDiagnosticBoot==d.encoderBoot && a.liftDiagnosticConnection==d.encoderConnection &&
                static_cast<int32_t>(d.encoderUptime-a.liftDiagnosticUptime)<=0)return;
            a.liftDiagnosticSeen=true;a.liftDiagnosticUptime=d.encoderUptime;
            a.liftDiagnosticBoot=d.encoderBoot;a.liftDiagnosticConnection=d.encoderConnection;
            a.liftRecoverySupported=d.liftRecoverySupported;
            a.cargoMonitoringOnly=d.cargoMonitoringOnly;
            if(a.liftActive && a.liftRecoverySupported && d.liftCommandId==a.liftCommandId && d.node==a.liftNode &&
                (d.error=="NETWORK_UNAVAILABLE"||d.error=="WIFI_TIMEOUT"||d.error=="MQTT_TIMEOUT"||d.liftState=="WAITING_LIFT_RECOVERY")) {
                a.liftWaitingRecovery=true;a.liftState="WAITING_LIFT_RECOVERY";return;
            }
            const bool healthy=d.wifi && d.mqtt && d.error=="NONE" && d.node==a.currentNode && a.positionValid &&
                d.rfidAgeMs!=UINT32_MAX && d.motor=="STOP" && d.leftDuty==0 && d.rightDuty==0;
            const bool supported=d.liftSupported && healthy;
            changed=a.liftSupported!=supported;a.liftSupported=supported;a.liftSeen=Clock::now();
            changed=changed || a.dropoffExitSupported!=d.dropoffExitSupported;
            a.dropoffExitSupported=d.dropoffExitSupported;
            changed=changed || a.liftObservedNode!=d.node || a.liftReady!=(supported && !d.liftBusy && !d.taskActive && d.state=="AGV_IDLE");
            a.liftObservedNode=d.node;
            a.liftReady=supported && !d.liftBusy && !d.taskActive && d.state=="AGV_IDLE";
            if(!a.liftActive && d.liftBusy && healthy) {
                a.state=AGVState::AGV_ERROR;a.lastError="Servo dang chay ngoai lenh hien tai; can xac minh";
                a.virtualCargoKnown=false;sendCommand(a,"WAIT");changed=true;
            }
            if(a.liftActive) {
                if(a.liftRecoverySupported && a.liftWaitingRecovery && healthy && d.liftCommandId==a.liftCommandId &&
                    d.encoderBoot==a.encoder.boot && d.encoderConnection==a.encoder.connection &&
                    d.lastCommandId>=a.liftCommandId && d.node==a.liftNode) {
                    a.liftBoot=a.encoder.boot;a.liftConnection=a.encoder.connection;a.liftWaitingRecovery=false;
                    a.liftStarted=Clock::now()-std::chrono::milliseconds(std::min(d.liftElapsedMs,5000u));
                    a.lastError.clear();a.recoveryChosenHeading=recoveryExpectedHeadingNoLock(a.currentNode);a.recoveryChosenNode=a.currentNode;
                    a.state=a.liftAction=="pickup"?AGVState::LIFTING:AGVState::LOWERING;
                }
                const bool sameBoot=a.liftBoot==a.encoder.boot && a.liftConnection==a.encoder.connection;
                const bool matching=d.liftCommandId==a.liftCommandId;
                if(!healthy || !sameBoot || !d.liftSupported || (matching && (d.liftState=="INTERRUPTED"||d.node!=a.liftNode))) {
                    interruptWorkNoLock(a,"Servo interrupted");a.state=AGVState::AGV_ERROR;
                    a.lastError="Servo gian doan; can xac minh co cau";sendCommand(a,"WAIT");changed=true;
                } else if(matching) {
                    const bool up=a.liftAction=="pickup";
                    const auto busyState=up?"LIFTING":"LOWERING",doneState=up?"DONE_UP":"DONE_DOWN";
                    if(d.liftBusy && d.taskActive && d.liftState==busyState && d.state==(up?"AGV_LOADING":"AGV_UNLOADING")) {
                        const auto pending=pendingCmd_.find(id);
                        if(pending!=pendingCmd_.end() && pending->second.cmdId==a.liftCommandId)pendingCmd_.erase(pending);
                        changed=changed||a.liftState!=busyState;a.liftState=busyState;
                    } else if(!up && d.dropoffExitSupported && !d.liftBusy && !d.taskActive && d.state=="AGV_IDLE" &&
                        d.liftState=="LOWERED" && d.liftElapsedMs>=5000 && d.cargoKnown && d.virtualCargo &&
                        d.cargoMonitoringOnly && static_cast<int32_t>(d.encoderUptime-a.liftCargoUptime)>0 &&
                        Clock::now()-a.liftStarted>=std::chrono::milliseconds(5000)) {
                        a.liftActive=false;a.liftState="LOWERED";a.dropoffExitPending=true;
                        a.virtualCargoKnown=true;a.virtualCargo=true;a.liftCargoUptime=d.encoderUptime;
                        const auto pending=pendingCmd_.find(id);
                        if(pending!=pendingCmd_.end() && pending->second.cmdId==a.liftCommandId)pendingCmd_.erase(pending);
                        a.state=AGVState::IDLE;changed=true;
                    } else if(up && !d.liftBusy && !d.taskActive && d.state=="AGV_IDLE" && d.liftState==doneState &&
                        d.liftElapsedMs>=5000 && d.cargoKnown && d.virtualCargo==up &&
                        d.cargoMonitoringOnly && static_cast<int32_t>(d.encoderUptime-a.liftCargoUptime)>0 &&
                        Clock::now()-a.liftStarted>=std::chrono::milliseconds(5000)) {
                        a.liftActive=false;a.liftState=doneState;a.virtualCargoKnown=true;a.virtualCargo=up;
                        const auto pending=pendingCmd_.find(id);
                        if(pending!=pendingCmd_.end() && pending->second.cmdId==a.liftCommandId)pendingCmd_.erase(pending);
                        a.state=AGVState::IDLE;changed=true;
                    }
                }
            }
            if(a.dropoffExitPending && a.liftRecoverySupported && healthy && d.encoderBoot==a.encoder.boot &&
                d.encoderConnection==a.encoder.connection && d.liftCommandId==a.liftCommandId && d.lastCommandId>=a.liftCommandId &&
                !d.liftBusy && !d.taskActive && d.liftElapsedMs>=5000 &&
                d.cargoMonitoringOnly && ((d.node==a.liftNode && d.liftState=="LOWERED") ||
                 (d.node==PhysicalLayout::gate(a.liftNode) && d.liftState=="DONE_DOWN"))) {
                a.liftBoot=a.encoder.boot;a.liftConnection=a.encoder.connection;
            }
            if(a.dropoffExitPending && healthy && a.liftBoot==a.encoder.boot && a.liftConnection==a.encoder.connection &&
                d.dropoffExitSupported && d.liftCommandId==a.liftCommandId && !d.liftBusy && !d.taskActive &&
                d.state=="AGV_IDLE" && d.liftState=="DONE_DOWN" && d.liftElapsedMs>=5000 &&
                d.cargoKnown && !d.virtualCargo && d.node==PhysicalLayout::gate(a.liftNode) &&
                d.cargoMonitoringOnly && static_cast<int32_t>(d.encoderUptime-a.liftCargoUptime)>0) {
                a.dropoffExitPending=false;a.liftState="DONE_DOWN";
                a.virtualCargoKnown=true;a.virtualCargo=false;changed=true;
            }
        }
        if(changed) notify();
    }

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
            if(agv.recoveryHeadingRequired || agv.headingPendingId>0)return "Can xac nhan huong dau xe truoc khi chay";
            if (monitorOnlyRobots_.count(agvId)) return "Xe chi giam sat; khong nhan lenh di chuyen";
            if (!activeRobotNoLock(agvId)) return "Xe khong nam trong pham vi van hanh hien tai";
            if(agv.liftActive) return "Xe dang nang/ha hang; cho servo hoan tat";
            if(agv.dropoffExitPending && (agv.currentNode!=agv.liftNode || targetNode!=PhysicalLayout::gate(agv.liftNode) ||
                agv.liftBoot!=agv.encoder.boot || agv.liftConnection!=agv.encoder.connection))
                return "Sau ha chi duoc lui tu kho giao ra gate cung kho; khac phien can doi soat";
            if (!trialRobotId_.empty() && agvId != trialRobotId_) return "Che do thu chi cho phep xe " + trialRobotId_;
            if (twoRobotTrial_ && agvId != "01" && agvId != "02") return "Thu hai xe chi cho phep 01 va 02";
            if (!taskId.empty() && taskRecords_.count(taskId)) return "task_id da ton tai";
            if (!agv.positionValid || !agv.hasTelemetry || Clock::now() - agv.lastSeen > offlineTimeout_) return "Vi tri/telemetry cua xe chua hop le";
            if (!networkAvailable_) return "Mat ket noi MQTT broker";
            if (agv.cancelNeedsNodeProof || agv.replanNeedsNodeProof || !agv.uncertainPath.empty())
                return "Cho xac minh xe dung tai RFID moi sau WAIT";
            if (agv.stationArrivalRequired && agv.currentNode == agv.finalTarget && !agv.stationReady)
                return "Xe dang quay dau tai tram; cho xe dung va huong ra cua truoc khi giao tuyen moi";
            if (agv.state == AGVState::WORKING || agv.state == AGVState::BLOCKED ||
                !agv.taskId.empty() || !agv.finalTarget.empty() || !agv.activePath.empty())
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
            // if ((!agv.batteryMeasured || agv.battery < resumeBattery_) && !singleRobotTrial_ && !supervisedTrialNoLock())
            //     return "Pin chua do hoac khong du de nhan task";
            if (!batteryTestBypass_ && (!agv.batteryMeasured || agv.battery < resumeBattery_) && !singleRobotTrial_ && !supervisedTrialNoLock())
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
    // (hoặc BLOCKED, sắp làm lại) hướng tới đích đó thì gửi WAIT, giữ BLOCKED tới proof dừng -- KHÔNG trả
    // task đó về hàng đợi (khác với lỗi/mất kết nối, vì đây là người dùng CHỦ ĐỘNG huỷ).
    // Trả về tổng số mục bị ảnh hưởng (task trong hàng đợi + xe bị dừng); 0 nếu không có gì để huỷ.
    // Operator recovery is separate from clearError's legacy home policy.
    // A home may be reconciled automatically only when every old zone is >=50cm
    // away. A warehouse reset only clears a verified stationary fault; route
    // recovery elsewhere still needs a matching suspended task and heading.
    std::string recoverVerifiedNode(const std::string& id,const std::string& expectedNode,
                                   bool homeOnly,bool operatorConfirmed,bool repositioned=false,bool unified=false,bool forceReset=false) {
        const auto zones=arbiter_.toJson();
        bool changed=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);auto it=fleet_.find(id);
            if (it==fleet_.end() || !activeRobotNoLock(id)) return "Xe ngoai pham vi van hanh";
            auto& a=it->second;
            if(operatorConfirmed && a.headingPendingId>0)return "Dang cho xac nhan huong; khong chen lenh khoi phuc";
            const bool home=PhysicalLayout::home(id)==expectedNode;
            if (homeOnly && !home) return "Khong dung tram rieng cua xe";
            bool ownsZone=false;
            for(const auto& z:zones.items())if(z.value().value("owner",std::string{})==id)ownsZone=true;
            if(!operatorConfirmed && a.state!=AGVState::AGV_ERROR && !ownsZone &&
                a.uncertainPath.empty() && a.stoppedNodes.empty() && !a.cancelNeedsNodeProof && !a.replanNeedsNodeProof) return "";
            if (a.currentNode!=expectedNode || !verifiedNodeStopNoLock(a) || a.liftActive ||
                !a.taskId.empty() || !a.activePath.empty() || !a.finalTarget.empty())
                return "Can RFID moi va diagnostic dung dung phien, het task/nang ha";
            if(operatorConfirmed && a.operatorNodeReset && a.autoResetRequested)
                return "Dang cho diagnostic xac nhan reset; khong gui lap";
            if (!home) {
                if (!operatorConfirmed || !acceptingTasks_)
                    return "Chi phuc hoi tai RFID ngoai tram khi da xac nhan xe dung trong ca RUNNING";
                if (!unified && !PhysicalLayout::warehouse(expectedNode)) {
                    if (!unified && (!a.headingSupported || a.headingNode!=expectedNode || a.heading.empty() ||
                        Clock::now()-a.diagnosticSeen>std::chrono::milliseconds(1500)))
                        return "Chua xac dinh huong xe tai RFID giua tuyen; can diagnostic huong moi";
                    bool suspended=false;
                    for (const auto& t:taskRecords_) if (t.second.value("resume_available",false) &&
                        t.second.value("assigned_agv",std::string{})==id &&
                        t.second.value("resume_node",std::string{})==expectedNode) suspended=true;
                    if (!suspended) return "Khong co nhiem vu dang treo tai RFID nay";
                }
            }
            for (const auto& z:zones.items()) if (z.value().value("owner",std::string{})==id && !operatorConfirmed) {
                const auto distance=map_.undirectedDistance(expectedNode,z.key());
                const auto straight=map_.straightLineDistance(expectedNode,z.key());
                if (distance==LLONG_MAX || distance<50 || !std::isfinite(straight) || straight<50)
                    return "Can xac nhan than xe da roi het vung cu";
            }
            if(operatorConfirmed && (repositioned || !a.headingSupported || a.heading.empty() ||
                a.headingNode!=expectedNode || a.headingSource=="bay_default" ||
                Clock::now()-a.diagnosticSeen>std::chrono::milliseconds(1500))) {
                a.recoveryHeadingRequired=true;a.headingPendingId=0;a.headingConfirmedId=0;a.heading.clear();
            }
            changed=!a.uncertainPath.empty() || !a.stoppedNodes.empty() || a.cancelNeedsNodeProof || a.replanNeedsNodeProof;
            a.uncertainPath.clear();a.stoppedNodes.clear();a.stoppedLocalized=false;a.hasStopCandidate=false;
            a.estimatedPosition=nullptr;a.cancelNeedsNodeProof=a.replanNeedsNodeProof=false;
            a.nodeProofSince={};a.nodeProofLastWarning={};
            const bool resetProven=!a.operatorNodeReset || (a.currentNode==a.recoveryExpectedNode &&
                static_cast<int32_t>(a.nodeStopUptime-a.replanHoldUptime)>0 && a.nodeStopLastCommand>=a.stopCommandId);
            if (!forceReset && a.nodeStopHealthy && resetProven && (a.lastError!="STALE_COMMAND" || a.autoResetRequested)) {
                changed=changed || a.state!=AGVState::IDLE;
                a.state=AGVState::IDLE;a.lastError.clear();
            } else if (operatorConfirmed) {
                // RESET must be followed by a later healthy diagnostic with its
                // high-water cmd_id. Neither receipt ACK nor this click opens IDLE.
                a.state=AGVState::AGV_ERROR;a.operatorNodeReset=true;a.autoResetRequested=true;
                a.recoveryExpectedNode=expectedNode;
                a.resetRequestedAt=Clock::now();a.resetBoot=a.encoder.boot;a.resetConnection=a.encoder.connection;
                a.replanHoldUptime=a.nodeStopUptime;
                a.stopCommandId=sendCommand(a,"RESET_FAULT",{{"verified_recovery",unified}});changed=true;
            }
            releaseStationReservationNoLock(id);
        }
        refreshStoppedOccupancy();arbiter_.cancelWaiting(id);
        for (const auto& z:zones.items()) if (z.value().value("owner",std::string{})==id &&
            z.key()!=expectedNode && z.value().contains("grant_id") &&
            arbiter_.confirmClear(z.key(),id,z.value().at("grant_id").get<long>())) {
            changed=true;const auto owner=arbiter_.currentOwner(z.key());const auto grant=arbiter_.grant(z.key(),owner);
            if (!grant.is_null() && publish_) publish_("agv/"+owner+"/mutex_grant",grant.dump());
        }
        if (changed || operatorConfirmed) {
            emit("AGV_"+id+" doi soat dung tai "+expectedNode+"; giu quyen tai node, tra vung cu da xac minh");notify();
        }
        return "";
    }

    std::string recoverAndContinue(const std::string& id,const std::string& node,const std::string& heading) {
        {
            std::lock_guard<std::mutex> lk(mtx_);auto it=fleet_.find(id);
            if(it==fleet_.end() || NodeManeuver::heading(heading)<0)return "Huong phai la N/E/S/W";
            const auto expected=recoveryExpectedHeadingNoLock(node);
            if(!expected.empty() && expected!=heading)return "Huong khong dung quy uoc kho/tram";
            if(!it->second.headingConfirmationSupported)return "Firmware chua ho tro xac nhan huong";
        }
        const auto error=recoverVerifiedNode(id,node,false,true,true,true);
        if(!error.empty())return error;
        {std::lock_guard<std::mutex> lk(mtx_);auto& a=fleet_.at(id);
            a.recoveryChosenHeading=heading;a.recoveryChosenNode=node;a.recoveryAutoResume=true;
            for(auto& t:taskRecords_)if(t.second.value("assigned_agv",std::string{})==id &&
                t.second.value("resume_available",false))t.second["resume_node"]=node;
        }
        advanceRecovery();notify();return "";
    }

    std::string confirmRecoveryHeading(const std::string& id,const std::string& node,const std::string& heading) {
        {
            std::lock_guard<std::mutex> lk(mtx_);auto it=fleet_.find(id);
            if(it==fleet_.end() || !activeRobotNoLock(id))return "Xe ngoai pham vi van hanh";
            auto& a=it->second;
            if(NodeManeuver::heading(heading)<0)return "Huong phai la N/E/S/W";
            if(a.currentNode!=node || a.state!=AGVState::IDLE || !a.lastError.empty() ||
                !(verifiedNodeStopNoLock(a) || healthyParkedHomeNoLock(a)) || !a.nodeStopHealthy || a.liftActive || !a.taskId.empty() ||
                !a.activePath.empty() || !a.finalTarget.empty() || !a.uncertainPath.empty() ||
                a.cancelNeedsNodeProof || a.replanNeedsNodeProof)
                return "Can khoi phuc loi va RFID moi, xe dung yen, het task/nang ha truoc khi xac nhan huong";
            if(!a.headingConfirmationSupported)return "Can nap firmware ho tro xac nhan huong cho xe";
            const auto gate=PhysicalLayout::gate(node);
            const int expected=PhysicalLayout::isHome(node)?NodeManeuver::direction(map_.toJson(),node,gate):
                PhysicalLayout::warehouse(node)?NodeManeuver::direction(map_.toJson(),gate,node):-1;
            if(!gate.empty() && (expected<0 || NodeManeuver::heading(heading)!=expected))
                return "Dat dau xe dung huong quy uoc tai kho/tram truoc khi xac nhan";
            if(a.headingPendingId>0 && Clock::now()-a.headingRequestedAt<std::chrono::seconds(5))
                return "Dang cho xe xac nhan huong; khong gui lap";
            a.recoveryHeadingRequired=true;a.heading.clear();a.headingConfirmedId=0;
            a.headingRequested=heading;a.headingRequestedNode=node;
            a.headingRequestedAt=Clock::now();
            a.headingRequestUptime=a.nodeStopUptime;a.headingRequestBoot=a.encoder.boot;a.headingRequestConnection=a.encoder.connection;
            a.headingPendingId=sendCommand(a,"CONFIRM_HEADING",{{"node",node},{"heading",heading}});
            if(a.headingPendingId<=0)return "Khong gui duoc lenh xac nhan huong";
        }
        notify();return "";
    }

    bool mayRequestZone(const std::string& id,const std::string& zone) {
        std::lock_guard<std::mutex> lk(mtx_);const auto it=fleet_.find(id);
        if (it==fleet_.end() || !activeRobotNoLock(id)) return false;
        const auto& a=it->second;
        if (a.recoveryHeadingRequired || a.headingPendingId>0 || !networkAvailable_ || !a.positionValid || !a.hasTelemetry || Clock::now()-a.lastSeen>offlineTimeout_ ||
            (a.state!=AGVState::WORKING && a.state!=AGVState::CHARGING_EN_ROUTE) ||
            a.cancelNeedsNodeProof || a.replanNeedsNodeProof) return false;
        const auto zones=map_.mutexZones();
        if (a.currentNode==zone) return true;
        for (size_t i=0;i<a.activePath.size() && i<2;++i) {
            const auto& node=a.activePath[i];
            if (std::find(zones.begin(),zones.end(),node)!=zones.end()) {
                if(i==1 && (!a.straightThroughSupported || !NodeManeuver::straightThrough(
                    map_.toJson(),a.currentNode,a.activePath[0],node)))return false;
                return node==zone;
            }
        }
        return false;
    }

    // Finish the live queue, while retaining terminal task records and all
    // uncertain physical occupancy until the existing STOP/RFID proof passes.
    void endShiftWork() {
        std::vector<std::string> ids;
        {
            std::lock_guard<std::mutex> lk(mtx_);acceptingTasks_=false;
            while (!tasks_.empty()) {
                setTaskStatusNoLock(tasks_.top().taskId,"cancelled","","Ket thuc ca");tasks_.pop();
            }
            for (auto& kv:fleet_) {
                auto& a=kv.second;
                invalidatePendingHeadingNoLock(a,true);
                if (!a.taskId.empty() || !a.finalTarget.empty() || !a.activePath.empty() || a.liftActive) {
                    const auto task=a.taskId;
                    interruptWorkNoLock(a,"Ket thuc ca");
                    if (!task.empty()) setTaskStatusNoLock(task,"cancelled",a.id,"Ket thuc ca");
                    if (a.state!=AGVState::AGV_ERROR && a.state!=AGVState::OFFLINE) {
                        a.state=AGVState::BLOCKED;a.cancelNeedsNodeProof=true;
                        a.replanHoldUptime=a.encoder.diagnosticUptime;
                        a.nodeProofSince=Clock::now();a.nodeProofLastWarning={};
                    }
                    if (networkAvailable_) a.stopCommandId=sendCommand(a,"WAIT");
                }
                ids.push_back(a.id);
            }
            for (auto& kv:taskRecords_) if (kv.second.value("resume_available",false)) {
                kv.second["resume_available"]=false;
                setTaskStatusNoLock(kv.first,"cancelled",kv.second.value("assigned_agv",std::string{}),"Ket thuc ca");
            }
        }
        for (const auto& id:ids) arbiter_.cancelWaiting(id);
        emit("Ket thuc ca: da huy hang doi va nhiem vu dang treo; cho xac minh dung");notify();
    }

    std::string resumeTask(const std::string& taskId,const std::string& expectedNode) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            const auto t=taskRecords_.find(taskId);
            if (!acceptingTasks_ || !networkAvailable_) return "Chi tiep tuc nhiem vu trong ca RUNNING";
            if (t==taskRecords_.end() || !t->second.value("resume_available",false) || t->second["status"]!="interrupted")
                return "Nhiem vu khong con duoc phep tiep tuc";
            const auto id=t->second.value("assigned_agv",std::string{});auto it=fleet_.find(id);
            if (it==fleet_.end() || !activeRobotNoLock(id)) return "Xe chua san sang";
            auto& a=it->second;
            if(a.recoveryHeadingRequired || a.headingPendingId>0)return "Can xac nhan huong dau xe truoc khi tiep tuc";
            if(a.dropoffExitPending && (a.currentNode!=a.liftNode ||
                t->second.at("target").get<std::string>()!=PhysicalLayout::gate(a.liftNode) ||
                a.liftBoot!=a.encoder.boot || a.liftConnection!=a.encoder.connection))
                return "Chi tiep tuc chang lui ra gate cung kho sau ha";
            if (a.currentNode!=expectedNode || expectedNode!=t->second.value("resume_node",std::string{}) ||
                a.state!=AGVState::IDLE || !a.lastError.empty() || !verifiedNodeStopNoLock(a) || !a.nodeStopHealthy ||
                !a.taskId.empty() || !a.finalTarget.empty() || a.liftActive || a.cancelNeedsNodeProof || a.replanNeedsNodeProof ||
                !a.uncertainPath.empty()) return "Can reset loi va xac minh dung tai dung RFID cu";
            if (a.currentNode!=t->second.at("target").get<std::string>() &&
                (!a.headingSupported || a.headingNode!=expectedNode || a.heading.empty() ||
                 Clock::now()-a.diagnosticSeen>std::chrono::milliseconds(1500)))
                return "Xe da dung nhung chua xac dinh huong de tiep tuc; cho diagnostic huong moi";
            auto path=computePathNoLock(a,t->second.at("target").get<std::string>());
            if (path.empty()) return "Chua co duong an toan de tiep tuc";
            a.taskId=taskId;a.taskPriority=t->second.value("priority",0);
            t->second["resume_available"]=false;
            if(a.currentNode==t->second.at("target").get<std::string>()) {
                a.taskId.clear();setTaskStatusNoLock(taskId,"completed",id,"Da xac minh dung tai dich cu");
            } else {
                assignPath(a,path,t->second.at("target").get<std::string>(),AGVState::WORKING);
                if(a.state!=AGVState::WORKING || a.routeCommandId<=0)return "Khong cap duoc lenh moi de tiep tuc";
                setTaskStatusNoLock(taskId,"dispatched",id,"Tiep tuc sau xac minh RFID");
            }
        }
        notify();return "";
    }

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
                preservePathNoLock(a);
                a.taskId.clear();
                a.activePath.clear();
                a.finalTarget.clear();
                a.blockedResume = AGVState::IDLE;
                a.state = AGVState::BLOCKED;
                a.replanNeedsNodeProof=false;a.cancelNeedsNodeProof=true;
                a.replanHoldUptime=a.encoder.diagnosticUptime;
                a.nodeProofSince=Clock::now();a.nodeProofLastWarning={};
                a.stopCommandId=sendCommand(a, "WAIT");
                interruptedAgvs.push_back(a.id);
            }
        }
        for (auto& id : interruptedAgvs) {arbiter_.cancelWaiting(id);emit("Task bi huy -> AGV_" + id + " dang cho xac minh dung sau WAIT");}
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
        advanceRecovery();
        refreshStoppedOccupancy();
        checkLiftTimeouts();
        pruneTaskRecords();
        if (map_.expireObstacles()) notify();
        checkTimeouts();
        checkAcks();
        checkNodeProofWarnings();
        resumeBlocked();
        dispatchPending(false);
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
            if (a.sonarObstacle && now-a.sonarSeen <= std::chrono::seconds(3) &&
                encoderFresh && a.estimatedPosition.is_object() && sample.tracking &&
                a.sonarRouteCommandId == sample.route && a.sonarNextNode == sample.to &&
                a.sonarDirection == (sample.reverse ? "rear" : "front") &&
                std::isfinite(a.sonarDistanceCm) && a.sonarDistanceCm >= 0 && a.sonarDistanceCm <= 20) {
                const auto& p = a.estimatedPosition;
                const double length = p.value("edge_length_cm",0.0);
                if (length > 0 && sample.from == p.value("from",std::string{}) && sample.to == p.value("to",std::string{})) {
                    const double projected = std::clamp(sample.distance+a.sonarDistanceCm,0.0,length);
                    const auto layout = map_.edgeGeometry(sample.from,sample.to);
                    if(!layout.is_null()) for (const auto& e : layout["edges"]) {
                        if (!((e["from"] == sample.from && e["to"] == sample.to) ||
                            (e.value("two_way",false) && e["from"] == sample.to && e["to"] == sample.from))) continue;
                        json from, to;
                        for (const auto& n : layout["nodes"]) {
                            if (n["id"] == sample.from) from=n;
                            if (n["id"] == sample.to) to=n;
                        }
                        if (!from.is_null() && !to.is_null()) {
                            const auto point=EdgeGeometry::point(layout,e,sample.from,projected);
                            detected = {{"from",sample.from},{"to",sample.to},{"x",point.at("x")},
                                {"y",point.at("y")},
                                {"range_cm",a.sonarDistanceCm},{"direction",a.sonarDirection},{"source","sonar_encoder_estimate"}};
                        }
                        break;
                    }
                }
            }
            long long silentSec = std::chrono::duration_cast<std::chrono::seconds>(now - a.lastSeen).count();
            agvsJ.push_back({
                {"id", a.id}, {"node", a.currentNode}, {"battery", a.battery},
                {"battery_display_percent", a.batteryDisplayAvailable ? json(a.batteryDisplayPercent) : json(nullptr)},
                {"battery_display_available", a.batteryDisplayAvailable},
                {"battery_control_source", a.batteryFromDiagnostic ? "diagnostic" : a.batteryMeasured ? "telemetry" : "unavailable"},
                {"state", toString(a.state)}, {"path", a.activePath},
                {"target", a.finalTarget}, {"priority", a.isPriorityVehicle},
                {"lift",{{"supported",a.liftSupported && networkAvailable_ && now-a.liftSeen<=std::chrono::seconds(3)},
                    {"recovery_supported",a.liftRecoverySupported},{"waiting_recovery",a.liftWaitingRecovery},
                    {"ready",a.liftReady && networkAvailable_ && a.liftObservedNode==a.currentNode && now-a.liftSeen<=std::chrono::seconds(3)},
                    {"observed_node",a.liftObservedNode},
                    {"dropoff_exit_supported",a.dropoffExitSupported},{"dropoff_exit_pending",a.dropoffExitPending},
                    {"active",a.liftActive},{"state",a.liftState},{"command_id",a.liftCommandId},{"node",a.liftNode},
                    {"action",a.liftAction},{"fresh",networkAvailable_ && now-a.liftSeen<=std::chrono::seconds(3)}}},
                {"virtual_cargo",a.virtualCargo}, {"virtual_cargo_known",a.virtualCargoKnown},
                {"cargo_sensor",{{"required",false},{"monitoring_only",true},{"supported",a.cargoSensorSupported},
                    {"fresh",networkAvailable_ && a.hasTelemetry && now-a.lastSeen<=std::chrono::milliseconds(1500) &&
                        a.cargoSensorSeen!=Clock::time_point{} && now-a.cargoSensorSeen<=std::chrono::milliseconds(1500)},
                    {"stable",a.cargoSensorStable},{"present",a.cargoSensorStable?json(a.cargoSensorPresent):json(nullptr)},
                    {"observed_node",a.cargoSensorNode},{"stopped_verified",a.cargoSensorStopped},
                    {"sample_seq",a.cargoSensorSeq}}},
                {"recovery_node_verified",verifiedNodeStopNoLock(a) && a.nodeStopHealthy},
                {"recovery_stop_verified",verifiedNodeStopNoLock(a)},
                {"recovery_stop_reason",recoveryStopReasonNoLock(a)},
                {"heading_confirmation_required",a.recoveryHeadingRequired},
                {"heading_confirmation_pending",a.headingPendingId>0},
                {"heading_confirmation_supported",a.headingConfirmationSupported},
                {"recovery_expected_heading",recoveryExpectedHeadingNoLock(a.currentNode)},
                {"recovery_reset_pending",a.operatorNodeReset && a.autoResetRequested},
                {"recovery_auto_resume",a.recoveryAutoResume},
                {"maintenance_station",PhysicalLayout::home(a.id)==a.currentNode},
                {"last_seen_s", a.hasTelemetry ? json(silentSec) : json(nullptr)},
                {"task_id", a.taskId}, {"position_valid", a.positionValid},
                {"battery_measured", a.batteryMeasured}, {"error", a.lastError},
                {"needs_recharge", a.needsRecharge},
                {"participating", activeRobotNoLock(a.id)},
                {"encoder_supported", (a.id == "01" || a.id == "02") && activeRobotNoLock(a.id) && encoder.boot != 0},
                {"speed_cm_s", encoderFresh ? json(sample.speed) : json(nullptr)},
                {"odometry", encoder.received ? json{{"valid",encoderFresh},{"age_ms",encoderAge},
                    {"stale_after_ms",EncoderObservation::staleMs},{"seq",sample.seq},{"boot_id",sample.boot},
                    {"connection_session",sample.connection},{"tracking",sample.tracking},{"translating",sample.translating},
                    {"distance_cm",sample.distance},{"left_ticks",sample.leftTicks},{"right_ticks",sample.rightTicks},
                    {"pulses_per_100cm",sample.pulsesPer100Cm}} : json(nullptr)},
                {"estimated_position", encoderFresh ? a.estimatedPosition : json(nullptr)},
                {"obstacle_alert", a.sonarObstacle && networkAvailable_ &&
                    now-a.sonarSeen <= std::chrono::seconds(3) && a.hasTelemetry && now-a.lastSeen <= offlineTimeout_
                    ? json{{"direction",a.sonarDirection},{"range_cm",a.sonarDistanceCm}} : json(nullptr)},
                {"obstacle_detection", detected},
                {"uncertain_path", a.uncertainPath},
                {"stopped_nodes",a.stoppedNodes},{"stopped_occupancy_verified",localizedStopNoLock(a)},
                {"replan_waiting_for_node",a.replanNeedsNodeProof},
                {"cancel_waiting_for_node",a.cancelNeedsNodeProof},
                {"node_proof_wait_ms",(a.replanNeedsNodeProof || a.cancelNeedsNodeProof) ?
                    std::chrono::duration_cast<std::chrono::milliseconds>(now-a.nodeProofSince).count() : 0},
                {"encoder_session_limit_reached",a.encoderSessionLimitReported},
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
        j["battery_gate_enabled"] = !batteryTestBypass_;
        j["automatic_charging_enabled"] = automaticChargingEnabled_;
        j["home_stations"] = homeStations_;
        j["trial_robot_id"] = trialRobotId_;
        j["active_robot_ids"] = json::array();
        if (activeRobotIds_.empty()) {
            j["active_robot_ids"].push_back("01");j["active_robot_ids"].push_back("02");
        } else {
            std::vector<std::string> active(activeRobotIds_.begin(),activeRobotIds_.end());
            std::sort(active.begin(),active.end());
            for (const auto& id : active) j["active_robot_ids"].push_back(id);
        }
        j["queue_sequence"] = seqCounter_;
        j["planning"]={{"dispatch_scans",dispatchScans_}};
        return j;
    }

private:
    void observeMaintenance(const std::string& id) {
        std::string node;bool reset=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);const auto it=fleet_.find(id);if(it==fleet_.end())return;
            auto& a=it->second;const auto now=Clock::now();
            if(PhysicalLayout::home(id)!=a.currentNode || !verifiedNodeStopNoLock(a) ||
                (a.maintenanceSupported&&!a.maintenanceReady) ||
                a.liftActive || !a.taskId.empty() || !a.activePath.empty() || !a.finalTarget.empty()) {
                if(a.maintenanceSupported&&!a.maintenanceReady)a.maintenanceResetAttempted=false;
                a.maintenanceStableSince={};return;
            }
            if(a.maintenanceStableSince==Clock::time_point{} || now-a.maintenanceLastSample>std::chrono::milliseconds(1500))
                a.maintenanceStableSince=now;
            a.maintenanceLastSample=now;
            if(now-a.maintenanceStableSince<std::chrono::seconds(2))return;
            node=a.currentNode;
            if(!a.nodeStopHealthy || a.maintenanceNeedsReset) {
                if(a.maintenanceResetAttempted||a.operatorNodeReset)return;
                a.maintenanceResetAttempted=true;reset=true;
            } else if(!a.operatorNodeReset) {
                a.maintenanceResetAttempted=false;
                if(a.headingConfirmationSupported && a.recoveryHeadingRequired &&
                    !a.dropoffExitPending)
                    {a.recoveryChosenHeading=recoveryExpectedHeadingNoLock(node);a.recoveryChosenNode=node;}
            }
        }
        // Automatic clearance must first pass the measured home-distance policy.
        if(!recoverVerifiedNode(id,node,true,false).empty())return;
        if(reset)recoverVerifiedNode(id,node,true,true,false,true,true);
        advanceRecovery();
    }
    void advanceRecovery() {
        std::vector<std::tuple<std::string,std::string,std::string>> headings;
        bool changed=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);const auto now=Clock::now();
            for(auto& entry:fleet_) {
                auto& a=entry.second;
                if(!a.recoveryChosenHeading.empty() && a.recoveryChosenNode!=a.currentNode) {
                    invalidatePendingHeadingNoLock(a,true);a.recoveryChosenHeading.clear();a.recoveryAutoResume=false;changed=true;
                }
                if(a.headingPendingId>0 && now-a.headingRequestedAt>std::chrono::seconds(5)) {
                    invalidatePendingHeadingNoLock(a,true);a.recoveryChosenHeading.clear();changed=true;
                }
                if(a.operatorNodeReset && now-a.resetRequestedAt>std::chrono::seconds(15)) {
                    const auto p=pendingCmd_.find(a.id);
                    if(p!=pendingCmd_.end() && p->second.cmdId==a.stopCommandId)pendingCmd_.erase(p);
                    a.operatorNodeReset=false;a.autoResetRequested=false;a.recoveryChosenHeading.clear();
                    a.lastError="RESET_CONFIRMATION_TIMEOUT";changed=true;
                }
                if(!a.recoveryChosenHeading.empty() && a.state==AGVState::IDLE && a.lastError.empty() &&
                    verifiedNodeStopNoLock(a) && a.nodeStopHealthy && a.headingPendingId==0 && a.recoveryHeadingRequired)
                    headings.emplace_back(a.id,a.currentNode,a.recoveryChosenHeading);
            }
        }
        for(const auto& h:headings)confirmRecoveryHeading(std::get<0>(h),std::get<1>(h),std::get<2>(h));
        if(changed)notify();
    }
    bool localizedStopNoLock(const AGV& a) const {
        const auto now=Clock::now();
        return a.stoppedLocalized && !a.stoppedNodes.empty() && networkAvailable_ && a.hasTelemetry && a.positionValid &&
            a.stopCandidate.stopFrom==a.currentNode &&
            a.state==AGVState::AGV_ERROR && now-a.lastSeen<=std::chrono::milliseconds(1000) &&
            now-a.stoppedSeen<=std::chrono::milliseconds(1000) &&
            now-a.stoppedDiagnosticSeen<=std::chrono::milliseconds(1500) &&
            a.encoder.boot==a.stopCandidate.boot && a.encoder.connection==a.stopCandidate.connection;
    }
    bool healthyParkedHomeNoLock(const AGV& a) const {
        const auto now=Clock::now();
        return PhysicalLayout::home(a.id)==a.currentNode && activeRobotNoLock(a.id) &&
            networkAvailable_ && a.hasTelemetry && a.positionValid && a.state==AGVState::IDLE && a.lastError.empty() &&
            now-a.lastSeen<=std::chrono::milliseconds(1000) && a.nodeStopHealthy &&
            a.nodeStopSeen!=Clock::time_point{} && now-a.nodeStopSeen<=std::chrono::milliseconds(1500) &&
            a.nodeStopNode==a.currentNode && a.nodeStopRfidAge!=UINT32_MAX && a.nodeStopBoot &&
            a.nodeStopBoot==a.encoder.boot && a.nodeStopConnection==a.encoder.connection &&
            a.taskId.empty() && a.finalTarget.empty() && a.activePath.empty() && a.uncertainPath.empty() && a.stoppedNodes.empty() &&
            !a.cancelNeedsNodeProof && !a.replanNeedsNodeProof && !a.liftActive && !a.dropoffExitPending && !a.liftWaitingRecovery &&
            !a.operatorNodeReset && !a.autoResetRequested;
    }
    bool verifiedNodeStopNoLock(const AGV& a) const {
        const auto now=Clock::now();
        return networkAvailable_ && a.hasTelemetry && a.positionValid && a.state!=AGVState::OFFLINE &&
            a.nodeStopSeen!=Clock::time_point{} && now-a.nodeStopSeen<=std::chrono::milliseconds(1500) &&
            now-a.lastSeen<=std::chrono::milliseconds(1000) && a.nodeStopNode==a.currentNode &&
            a.nodeStopRfidAge<=250 && a.nodeStopBoot && a.nodeStopBoot==a.encoder.boot &&
            a.nodeStopConnection==a.encoder.connection;
    }
    std::string homeDepartureErrorNoLock(const AGV& a) const {
        if(a.recoveryHeadingRequired || a.headingPendingId>0 || a.operatorNodeReset || a.autoResetRequested)
            return "Cho hoan tat khoi phuc va xac nhan huong truoc khi ve tram";
        if(a.liftActive || a.liftWaitingRecovery || a.dropoffExitPending)
            return "Cho hoan tat nang/ha va chang ra gate truoc khi ve tram";
        // Legacy fixtures have no motion diagnostic. A real bound MCU must
        // prove physical STOP after its last route; RFID/ACK alone is not it.
        if(a.encoder.boot || a.nodeOrdersSupported || a.headingSupported) {
            if(!a.headingSupported || a.headingNode!=a.currentNode ||
                (a.heading!="N" && a.heading!="E" && a.heading!="S" && a.heading!="W") ||
                Clock::now()-a.diagnosticSeen>std::chrono::milliseconds(1500))
                return "Cho diagnostic huong dau xe hop le truoc khi ve tram";
            if(!verifiedNodeStopNoLock(a) || !a.nodeStopHealthy)
                return "Cho diagnostic xac nhan xe dung va ket thuc tuyen cu truoc khi ve tram";
            if(pendingCmd_.count(a.id))return "Cho ACK lenh truoc va diagnostic dung moi truoc khi ve tram";
        }
        return "";
    }
    std::string recoveryStopReasonNoLock(const AGV& a) const {
        if(verifiedNodeStopNoLock(a))return "";
        if(!networkAvailable_||a.state==AGVState::OFFLINE)return "Xe chua ket noi";
        if(!a.hasTelemetry||Clock::now()-a.lastSeen>std::chrono::milliseconds(1000))return "telemetry_stale";
        if(!a.positionValid)return "rfid_unknown";
        if(a.nodeStopSeen==Clock::time_point{})return "stop_unverified";
        if(Clock::now()-a.nodeStopSeen>std::chrono::milliseconds(1500))return "diagnostic_stale";
        if(a.nodeStopBoot!=a.encoder.boot||a.nodeStopConnection!=a.encoder.connection)return "session_changed";
        if(a.nodeStopNode!=a.currentNode)return "rfid_mismatch";
        return "rfid_stale";
    }
    std::string recoveryExpectedHeadingNoLock(const std::string& node) const {
        const auto gate=PhysicalLayout::gate(node);if(gate.empty())return "";
        const int direction=PhysicalLayout::isHome(node)?NodeManeuver::direction(map_.toJson(),node,gate):
            NodeManeuver::direction(map_.toJson(),gate,node);
        return direction>=0 && direction<4?std::string(1,"NESW"[direction]):"";
    }
    void replanAllForStoppedOccupancy() {
        std::vector<std::string> ids;
        {std::lock_guard<std::mutex> lk(mtx_);for(const auto& kv:fleet_)
            if(kv.second.state==AGVState::WORKING || kv.second.state==AGVState::CHARGING_EN_ROUTE || kv.second.state==AGVState::BLOCKED) ids.push_back(kv.first);}
        replanAffectedAgvs({},ids);
    }
    bool activeRobotNoLock(const std::string& id) const {
        return activeRobotIds_.empty() || activeRobotIds_.count(id) != 0;
    }
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
        agv.state = charging && automaticChargingEnabled_ ? AGVState::CHARGING : AGVState::IDLE;
        // if (charging) sendCommand(agv, "CHARGE_START"); // Luong goc.
        if (charging && automaticChargingEnabled_) sendCommand(agv, "CHARGE_START");
        if (charging && !automaticChargingEnabled_) releaseStationReservationNoLock(agv.id);
        return true;
    }
    void pruneTaskRecords() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::pair<long,std::string>> terminal;
        for (const auto& kv : taskRecords_) {
            const auto status = kv.second.value("status","");
            if ((status == "completed" || status == "cancelled" || status == "interrupted") &&
                !kv.second.value("resume_available",false)) terminal.push_back({kv.second.value("updated_sequence",0L),kv.first});
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
    void invalidatePendingHeadingNoLock(AGV& a,bool discardRetry=false) {
        if(a.headingPendingId<=0)return;
        if(discardRetry) {
            const auto pending=pendingCmd_.find(a.id);
            if(pending!=pendingCmd_.end() && pending->second.cmdId==a.headingPendingId)pendingCmd_.erase(pending);
        }
        a.headingPendingId=0;a.headingConfirmedId=0;a.recoveryHeadingRequired=true;a.heading.clear();
    }
    bool interruptWorkNoLock(AGV& a, const std::string& reason) {
        // Do not erase pendingCmd_ here: checkAcks may be iterating that map.
        invalidatePendingHeadingNoLock(a);
        a.recoveryAutoResume=false;a.recoveryChosenHeading.clear();
        a.operatorNodeReset=false;a.autoResetRequested=false;a.maintenanceStableSince={};
        if(a.liftActive) {
            const bool network=reason.find("MQTT")!=std::string::npos||reason.find("telemetry")!=std::string::npos||reason.find("NETWORK")!=std::string::npos||reason.find("WIFI")!=std::string::npos;
            if(a.liftRecoverySupported && network) {a.liftWaitingRecovery=true;a.liftState="WAITING_LIFT_RECOVERY";}
            else {a.liftActive=false;a.liftWaitingRecovery=false;a.liftState="INTERRUPTED";a.virtualCargoKnown=false;}
        }
        bool interrupted = !a.taskId.empty();
        if (interrupted) {
            setTaskStatusNoLock(a.taskId, "interrupted", a.id, reason);
            auto& task=taskRecords_.at(a.taskId);
            task["resume_available"]=acceptingTasks_ && a.positionValid;
            task["resume_node"]=a.currentNode;
        }
        preservePathNoLock(a);
        a.taskId.clear(); a.activePath.clear(); a.finalTarget.clear(); a.blockedResume = AGVState::IDLE;
        a.replanNeedsNodeProof=false;
        a.cancelNeedsNodeProof=false;a.nodeProofSince={};a.nodeProofLastWarning={};
        return interrupted;
    }

    void preservePathNoLock(AGV& a) {
        for(const auto& node:a.activePath)
            if(std::find(a.uncertainPath.begin(),a.uncertainPath.end(),node)==a.uncertainPath.end()) a.uncertainPath.push_back(node);
    }
    void checkNodeProofWarnings() {
        std::vector<std::string> waiting;
        {
            std::lock_guard<std::mutex> lk(mtx_);const auto now=Clock::now();
            for(auto& kv:fleet_) {auto& a=kv.second;
                if((a.cancelNeedsNodeProof || a.replanNeedsNodeProof) && now-a.nodeProofSince>=nodeProofWarning_ &&
                    (a.nodeProofLastWarning==Clock::time_point{} || now-a.nodeProofLastWarning>=std::chrono::seconds(30))) {
                    a.nodeProofLastWarning=now;waiting.push_back(a.id);
                }
            }
        }
        for(const auto& id:waiting) emit("[CANH BAO] AGV_"+id+" cho RFID moi sau WAIT; can dua xe ve the va xac minh dung, khong tu nha mutex");
    }

    void checkLiftTimeouts() {
        bool changed=false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for(auto& kv:fleet_) {
                auto& a=kv.second;
                if(a.liftActive && !a.liftWaitingRecovery && Clock::now()-a.liftStarted>std::chrono::seconds(12)) {
                    interruptWorkNoLock(a,"Servo completion timeout");a.state=AGVState::AGV_ERROR;
                    a.lastError="Khong nhan hoan tat servo; can xac minh";sendCommand(a,"WAIT");changed=true;
                }
            }
        }
        if(changed) notify();
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
            if (localizedStopNoLock(o)) {busy.insert(o.stoppedNodes.begin(),o.stoppedNodes.end());continue;}
            if (!o.currentNode.empty()) busy.insert(o.currentNode);
            for (auto& n : o.activePath) busy.insert(n);
            for (auto& n : o.uncertainPath) busy.insert(n);
        }
        return busy;
    }

    // Ưu tiên né cả route dự kiến; fallback vẫn giữ node/đích đang chiếm và vùng chưa xác minh.
    std::vector<std::string> computePathNoLock(const AGV& agv, const std::string& target) {
        if (buildOccupiedNodesNoLock(agv.id).count(agv.currentNode)) return {};
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
            if (localizedStopNoLock(a)) {blocked.insert(a.stoppedNodes.begin(),a.stoppedNodes.end());continue;}
            blocked.insert(a.stoppedNodes.begin(),a.stoppedNodes.end());
            if (!a.currentNode.empty()) blocked.insert(a.currentNode);
            if (!a.finalTarget.empty()) blocked.insert(a.finalTarget);
            blocked.insert(a.uncertainPath.begin(),a.uncertainPath.end());
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
            // Ordinary gates have no grant protocol: protect the next approach.
            if (localizedStopNoLock(a)) continue;
            // Hard reservation covers only the approach and exit of the next
            // connected mutex corridor. The rest of activePath is a soft avoid
            // preference in buildBusyNodesNoLock, not ownership of future nodes.
            std::vector<std::string> frontier;
            bool reachedMutex=false;
            for (const auto& node:a.activePath) {
                frontier.push_back(node);
                if (mutex.count(node)) reachedMutex=true;
                else if (reachedMutex) break;
            }
            for (const auto& node : frontier) if (!mutex.count(node)) blocked.insert(node);
            auto reserveCorridor = [&](const std::string& node) {
                if (!mutex.count(node)) return;
                const auto component = map_.mutexComponent(node);
                if (component.size() > 1) blocked.insert(component.begin(),component.end());
            };
            reserveCorridor(a.currentNode);
            for (const auto& node : frontier) reserveCorridor(node);
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

    // PHẢI gọi khi đang giữ mtx_. Một xe lỗi/offline có thể còn ở currentNode,
    // ở bất kỳ node nào của uncertainPath, hoặc quét/chiếm cả hành lang mutex.
    // Chỉ chọn xe đang có kế hoạch để replan; xe IDLE không cần nhận command mới.
    std::vector<std::string> agvsAffectedByFailureNoLock(const std::string& failedId) {
        std::vector<std::string> affected;
        const auto failed = fleet_.find(failedId);
        if (failed == fleet_.end()) return affected;

        std::unordered_set<std::string> footprint;
        auto addFootprintNode = [&](const std::string& node) {
            if (node.empty()) return;
            footprint.insert(node);
            const auto component = map_.mutexComponent(node);
            footprint.insert(component.begin(),component.end());
        };
        addFootprintNode(failed->second.currentNode);
        if (localizedStopNoLock(failed->second)) {
            footprint.clear();footprint.insert(failed->second.stoppedNodes.begin(),failed->second.stoppedNodes.end());
        } else {
            for (const auto& node : failed->second.activePath) addFootprintNode(node);
            for (const auto& node : failed->second.uncertainPath) addFootprintNode(node);
        }

        auto intersects = [&](const AGV& candidate) {
            if (footprint.count(candidate.currentNode) || footprint.count(candidate.finalTarget)) return true;
            return std::any_of(candidate.activePath.begin(),candidate.activePath.end(),
                [&](const std::string& node) {return footprint.count(node) > 0;});
        };
        for (const auto& kv : fleet_) {
            if (kv.first == failedId) continue;
            const auto state = kv.second.state;
            if (state != AGVState::WORKING && state != AGVState::CHARGING_EN_ROUTE && state != AGVState::BLOCKED)
                continue;
            if (intersects(kv.second)) affected.push_back(kv.first);
        }
        return affected;
    }

    // Gọi ngoài mtx_: lấy cả xe đang xếp hàng sau mutex do xe lỗi giữ. Snapshot
    // arbiter tách khỏi FleetManager lock để không tạo lock-order Fleet->Arbiter.
    void appendMutexWaitersForFailedOwners(const std::vector<std::string>& failedIds,
                                            std::vector<std::string>& affected) {
        const std::unordered_set<std::string> failed(failedIds.begin(),failedIds.end());
        const auto zones = arbiter_.toJson();
        for (const auto& zone : zones.items()) {
            if (!failed.count(zone.value().value("owner",std::string{}))) continue;
            for (const auto& waiter : zone.value().value("waiting",json::array()))
                affected.push_back(waiter.get<std::string>());
        }
    }

    // Gọi ngoài mtx_. Hủy lượt chờ cũ trước khi phát tuyến mới để xe không được
    // cấp grant muộn cho mutex mà tuyến mới đã né. Nếu không còn đường,
    // replanAGV chuyển xe sang BLOCKED/WAIT và tick sẽ thử lại an toàn.
    void replanAffectedAgvs(const std::vector<std::string>& failedIds,
                            std::vector<std::string> affected) {
        std::sort(affected.begin(),affected.end());
        affected.erase(std::unique(affected.begin(),affected.end()),affected.end());
        for (const auto& id : affected) {
            if (std::find(failedIds.begin(),failedIds.end(),id) != failedIds.end()) continue;
            arbiter_.cancelWaiting(id);
            replanAGV(id);
        }
    }

    // Dùng resumeBattery_ (không phải lowBattery_) để tránh xe pin 21% nhận task rồi
    // vài phút sau bị huỷ để đi sạc — xem hysteresis ở [6] trong ghi chú đầu file.
    bool dispatchableNoLock(const AGV& a) const {
        return !a.recoveryHeadingRequired && a.headingPendingId==0 && activeRobotNoLock(a.id) && !monitorOnlyRobots_.count(a.id) && (trialRobotId_.empty() || a.id == trialRobotId_) &&
               (!twoRobotTrial_ || a.id == "01" || a.id == "02") &&
               acceptingTasks_ && networkAvailable_ && !a.liftActive && !a.dropoffExitPending && a.state == AGVState::IDLE &&
               !a.cancelNeedsNodeProof && !a.replanNeedsNodeProof && a.uncertainPath.empty() &&
               // (supervisedTrialNoLock() || (a.batteryMeasured && a.battery >= resumeBattery_)) && a.hasTelemetry && a.positionValid &&
               (batteryTestBypass_ || supervisedTrialNoLock() || (a.batteryMeasured && a.battery >= resumeBattery_)) && a.hasTelemetry && a.positionValid &&
               Clock::now() - a.lastSeen <= offlineTimeout_ && !a.currentNode.empty();
    }

    // Tìm & giao AGV IDLE gần nhất (theo tổng trọng số cạnh) cho các task đang chờ,
    // ưu tiên xử lý task có priority cao trước (và tạo trước, nếu bằng priority).
    void dispatchPending(bool force = true) {
        bool assignedAny = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (singleRobotTrial_) return;
            if (tasks_.empty() || !networkAvailable_ || !acceptingTasks_) return;
            const auto now=Clock::now();
            if(!force && lastDispatchScan_!=Clock::time_point{} && now-lastDispatchScan_<std::chrono::milliseconds(250)) return;
            lastDispatchScan_=now;
            bool anyFree = false;
            for (auto& kv : fleet_) if (dispatchableNoLock(kv.second)) { anyFree = true; break; }
            if (!anyFree) return; // không có xe rảnh -> khỏi tốn Dijkstra
            ++dispatchScans_;

            std::priority_queue<Task, std::vector<Task>, TaskCmp> remaining;
            while (!tasks_.empty()) {
                Task t = tasks_.top(); tasks_.pop();

                std::string bestId; std::vector<std::string> bestPath; long long bestCost = LLONG_MAX;
                // pass 0: né node bận (xe khác đang đứng / sắp đi qua). pass 1: không né.
                for (int pass = 0; pass < 2 && bestId.empty(); ++pass) {
                    for (auto& kv : fleet_) {
                        const AGV& agv = kv.second;
                        if (!dispatchableNoLock(agv)) continue;
                        if (buildOccupiedNodesNoLock(agv.id).count(agv.currentNode)) continue;
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
        if(agv.recoveryHeadingRequired || agv.headingPendingId>0) {
            interruptWorkNoLock(agv,"Can xac nhan huong dau xe truoc khi chay");
            agv.state=AGVState::AGV_ERROR;agv.lastError="Can xac nhan huong dau xe truoc khi chay";return;
        }
        if (!path.empty() && path.front() == agv.currentNode) path.erase(path.begin());
        if(agv.dropoffExitPending && (agv.currentNode!=agv.liftNode || target!=PhysicalLayout::gate(agv.liftNode) ||
            path.size()!=1 || path.front()!=target)) {
            interruptWorkNoLock(agv,"Sau ha chi duoc lui ra gate cung kho");
            agv.state=AGVState::AGV_ERROR;agv.lastError="Sau ha chi duoc lui ra gate cung kho";return;
        }
        agv.activePath = path;
        agv.replanNeedsNodeProof=false;
        agv.commandPath = path; agv.routeStart = agv.currentNode;
        agv.lastNodeOrderKey.clear(); agv.lastNodeOrderPayload.clear();
        agv.finalTarget = target;
        agv.stationArrivalRequired = agv.stationDockingSupported && PhysicalLayout::isHome(target);
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
        if(agv.dropoffExitPending) {
            if(cmdName=="CHARGE_START")return 0;
            if((cmdName=="GOTO" || cmdName=="NODE_ORDER") &&
                (agv.liftBoot!=agv.encoder.boot || agv.liftConnection!=agv.encoder.connection))return 0;
            const auto gate=PhysicalLayout::gate(agv.liftNode);
            if(cmdName=="GOTO" && (agv.currentNode!=agv.liftNode || extra.value("target",std::string{})!=gate ||
                extra.value("path",json::array())!=json::array({gate})))return 0;
            if(cmdName=="NODE_ORDER" && (extra.value("node",std::string{})!=agv.liftNode ||
                extra.value("next_node",std::string{})!=gate))return 0;
        }
        if(cmdName=="STOP" || cmdName=="WAIT" || cmdName=="RESET_FAULT")invalidatePendingHeadingNoLock(agv);
        if((agv.recoveryHeadingRequired || agv.headingPendingId>0) &&
            (cmdName=="GOTO" || cmdName=="NODE_ORDER" || cmdName=="CHARGE_START"))return 0;
        if (!automaticChargingEnabled_ && cmdName == "CHARGE_START") return 0;
        // Chot cuoi cung: khong phat chuyen dong/sac cho xe khong duoc chon.
        if ((!activeRobotNoLock(agv.id) || (!trialRobotId_.empty() && agv.id != trialRobotId_)) &&
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
        std::vector<std::string> failedIds, toReplan;
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
                    failedIds.push_back(id);
                    const auto affected = agvsAffectedByFailureNoLock(id);
                    toReplan.insert(toReplan.end(),affected.begin(),affected.end());
                    it = pendingCmd_.erase(it);
                }
            }
        }
        if (!failedIds.empty()) {
            for (const auto& id : failedIds) arbiter_.cancelWaiting(id);
            appendMutexWaitersForFailedOwners(failedIds,toReplan);
            replanAffectedAgvs(failedIds,toReplan);
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
        auto remaining=path;
        if (!remaining.empty() && remaining.front()==agv.currentNode) remaining.erase(remaining.begin());
        if (!path.empty() && agv.state!=AGVState::BLOCKED && remaining==agv.activePath) return;
        if (agv.nodeOrdersSupported && agv.state!=AGVState::BLOCKED) {
            // MCU rejects GOTO over an active route; a stopped midpoint is not
            // a new route origin. Wait for a later bound STOP + fresh node tag.
            agv.replanNeedsNodeProof=true;agv.replanHoldUptime=agv.encoder.diagnosticUptime;
            preservePathNoLock(agv);
            agv.nodeProofSince=Clock::now();agv.nodeProofLastWarning={};
            agv.blockedResume=resume;agv.state=AGVState::BLOCKED;agv.activePath.clear();
            agv.stopCommandId=sendCommand(agv,"WAIT");return;
        }
        if (path.empty()) {
            if (agv.state != AGVState::BLOCKED) {
                agv.blockedResume = resume;
                agv.state = AGVState::BLOCKED; // hết đường -> chờ, tick() sẽ thử lại (resumeBlocked)
                agv.activePath.clear();
                sendCommand(agv, "WAIT");
            }
        } else if (!agv.replanNeedsNodeProof) {
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
            for (auto& id : lost) {
                const auto affected = agvsAffectedByFailureNoLock(id);
                toReplan.insert(toReplan.end(),affected.begin(),affected.end());
            }
            release = releaseZonesOnOffline_;
        }
        if (lost.empty()) return;

        for (auto& id : lost) {
            bool requeued = std::find(requeuedIds.begin(), requeuedIds.end(), id) != requeuedIds.end();
            arbiter_.cancelWaiting(id);
            emit("AGV_" + id + " MAT KET NOI (OFFLINE)" + (requeued ? " -> task bi gian doan, can xac minh" : ""));
        }
        appendMutexWaitersForFailedOwners(lost,toReplan);
        if (release) for (const auto& id : lost) releaseAgvZones(id);
        replanAffectedAgvs(lost,toReplan);
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
                if (!activeRobotNoLock(agv.id)) continue;
                if (!trialRobotId_.empty() && agv.id != trialRobotId_) continue;
                if (agv.state != AGVState::BLOCKED) continue;
                if (agv.replanNeedsNodeProof || agv.cancelNeedsNodeProof) continue;
                if (!agv.positionValid || Clock::now()-agv.lastSeen > offlineTimeout_) continue;
                if (agv.finalTarget.empty()) {
                    // Không còn việc dở. Nếu pin thấp thì để checkBatteryOne tìm đường tới trạm sạc.
                    // if (singleRobotTrial_ || supervisedTrialNoLock() || (!agv.needsRecharge && agv.battery >= resumeBattery_)) {
                    if (batteryTestBypass_ || singleRobotTrial_ || supervisedTrialNoLock() || (!agv.needsRecharge && agv.battery >= resumeBattery_)) {
                        agv.state = AGVState::IDLE;
                        agv.blockedResume = AGVState::IDLE;
                        changed = true;
                    }
                    continue;
                }
                if(agv.blockedResume==AGVState::CHARGING_EN_ROUTE && !homeDepartureErrorNoLock(agv).empty())continue;
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
            if (!automaticChargingEnabled_) return false; // Sac thu cong: khong doi tuyen/task theo pin.
            if (!activeRobotNoLock(id) || singleRobotTrial_ || supervisedTrialNoLock() || monitorOnlyRobots_.count(id)) return false; // che do thu khong tu ve sac
            auto it = fleet_.find(id);
            if (it == fleet_.end()) return false;
            AGV& agv = it->second;
            if (!networkAvailable_ || !agv.batteryMeasured || !agv.positionValid || !agv.hasTelemetry ||
                Clock::now()-agv.lastSeen > offlineTimeout_) return false;
            if(agv.cancelNeedsNodeProof || agv.replanNeedsNodeProof || !agv.uncertainPath.empty()) return false;
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
    bool batteryTestBypass_ = false;
    bool automaticChargingEnabled_ = true;
    Clock::time_point lastDispatchScan_{};
    std::uint64_t dispatchScans_=0;
    std::chrono::milliseconds nodeProofWarning_{10000};
    std::chrono::seconds diagnosticBatteryTimeout_{3};
    std::chrono::seconds offlineTimeout_{10};
    bool autoRegister_ = true;
    bool singleRobotTrial_ = false;
    bool twoRobotTrial_ = false;
    std::string trialRobotId_;
    bool supervisedTrialNoLock() const { return twoRobotTrial_ || !trialRobotId_.empty(); }
    std::unordered_set<std::string> activeRobotIds_;
    bool diagnosticRecoveryNoLock() const { return supervisedTrialNoLock() || !activeRobotIds_.empty(); }
    std::unordered_set<std::string> monitorOnlyRobots_;
    bool networkAvailable_ = true;
    bool acceptingTasks_ = true;
    std::unordered_map<std::string,std::string> homeStations_;
    bool releaseZonesOnOffline_ = false;
    std::unordered_map<std::string, std::string> chargeReservation_; // station -> agvId đang giữ/đặt chỗ
    std::unordered_map<std::string, PendingCmd> pendingCmd_;         // agvId -> lệnh đang chờ ACK
    long cmdSeq_ = 0;
    std::function<long()> commandIdAllocator_;
    std::function<void(long)> commandIdHighwaterCallback_;
    std::function<std::string()> taskIdAllocator_;
    int obstacleTtlSeconds_ = 30;
    std::chrono::milliseconds ackTimeout_{3000};
    int maxRetries_ = 3;
    std::function<void(const std::string&, const std::string&)> publish_;
    std::function<void()> onChanged_;
    std::function<void(const std::string&)> onEvent_;
    std::mutex mtx_;
};
