#pragma once
#include "core/FLeetmanager.hpp"
#include "commands/CommandIds.hpp" // filesystem/atomic Windows replacement, không cấp command ID ở đây
#include <map>

// Chỉ gọi dưới g_stateMtx. Lưu chế độ ca, tuyệt đối không lưu/khôi phục vị trí robot.
class ShiftController {
public:
    ShiftController(MapGraph& map, FleetManager& fleet, MutexArbiter& arbiter,
                    std::filesystem::path file,
                    std::unordered_map<std::string,std::string> homes = {{"01","S1"},{"02","S2"}},
                    bool singleRobotTrial = false, bool twoRobotTrial = false,
                    std::string trialRobotId = "")
        : map_(map), fleet_(fleet), arbiter_(arbiter), file_(std::move(file)),
          homes_(std::move(homes)), singleRobotTrial_(singleRobotTrial), twoRobotTrial_(twoRobotTrial),
          trialRobotId_(std::move(trialRobotId)) {
        const auto stations = map_.chargingStations();
        supported_ = !homes_.empty();
        std::unordered_set<std::string> assigned;
        for (const auto& home : homes_) {
            if (home.first.empty() || home.first.find_first_of("/|\r\n") != std::string::npos ||
                std::find(stations.begin(),stations.end(),home.second) == stations.end() ||
                !assigned.insert(home.second).second) supported_ = false;
        }
        if (singleRobotTrial_ &&
            (homes_.size() != 1 || homes_.count("01") != 1 || homes_.at("01") != "S1")) supported_ = false;
        if (twoRobotTrial_ &&
            (homes_.size() != 2 || homes_.count("01") != 1 || homes_.count("02") != 1 ||
             homes_.at("01") != "S1" || homes_.at("02") != "S2")) supported_ = false;
        if (!trialRobotId_.empty() &&
            ((trialRobotId_ != "01" && trialRobotId_ != "02") || singleRobotTrial_ || twoRobotTrial_ ||
             homes_.size() != 1 || !homes_.count(trialRobotId_) ||
             homes_.at(trialRobotId_) != (trialRobotId_ == "01" ? "S1" : "S2"))) supported_ = false;
        if (!supported_) return; // đồ thị fixture/legacy không có cặp trạm: giữ chức năng cũ
        if (std::filesystem::exists(file_)) {
            std::ifstream input(file_); json data; input >> data;
            const auto saved = data.at("phase").get<std::string>();
            if (saved != "RUNNING" && saved != "CLOSING" && saved != "CLOSED")
                throw std::runtime_error("File trang thai ca khong hop le");
        }
        // robot01 test can start at its measured node after fresh telemetry/reset.
        fleet_.setHomeStations(homes_); fleet_.setAcceptingTasks(false);
        transition(trialRobotId_ == "01" ? "CLOSED" : "CLOSING");
    }

    bool supported() const { return supported_; }
    std::string end() {
        if (!supported_) return "Ban do chua co tram sac cau hinh";
        if (phase_ == "RUNNING") {
            transition("CLOSING"); // ghi được file trước khi thay trạng thái điều phối
            fleet_.setAcceptingTasks(false); parked_.clear();
        }
        tick(); return "";
    }
    std::string start() {
        if (!supported_) return "Ban do chua co tram sac cau hinh";
        if (phase_ == "RUNNING") return "";
        if (!trialRobotId_.empty() && !snapshot().value("can_start",false))
            return trialRobotId_ == "01" ? "Cho xe 01 online tai RFID hop le, het loi/task; xe khong chon phai dung tai tram"
                : "Cho xe " + trialRobotId_ + " tai tram rieng; xe khong chon phai dung/da xac minh tai tram, het task/vung";
        if (!snapshot().value("can_start",false)) return singleRobotTrial_
            ? "Cho xe 01 online o S1, RFID moi, khong loi/hanh trinh/vung chiem va khong co xe khac"
            : twoRobotTrial_
            ? "Cho ca hai xe 01/02 online o S1/S2, RFID moi, khong loi/hanh trinh/vung chiem"
            : "Cho it nhat mot xe online o tram rieng, RFID moi, khong loi/vung chiem va pin da do >=40%";
        const auto homesInShift = knownHomes(fleet_.toJson());
        transition("RUNNING");
        fleet_.beginWorkFromHome(homesInShift);
        return "";
    }
    void tick() {
        if (!supported_ || phase_ == "RUNNING") return;
        auto status = snapshot();
        if (trialRobotId_ == "01") {
            bool ready = status.value("inactive_robots_ready",true);
            for (const auto& robot : status["robots"]) if (robot.value("id","") == "01")
                ready = ready && robot.value("ready",false);
            if (phase_ == "CLOSING" && ready) transition("CLOSED");
            return; // A test stop does not force a trip back to S1.
        }
        if (phase_ == "CLOSED" && !status["all_home"].get<bool>()) transition("CLOSING");
        const auto fleet = fleet_.toJson();
        const auto homesInShift = knownHomes(fleet);
        const bool oneKnownRobot = homesInShift.size() == 1 && fleet["agvs"].size() == 1;
        const bool unmeasured = oneKnownRobot && !fleet["agvs"][0].value("battery_measured",false);
        const bool atHome = oneKnownRobot && fleet["agvs"][0].value("position_valid",false) &&
            fleet["agvs"][0].value("node","") == homesInShift.begin()->second;
        if (singleRobotTrial_ || twoRobotTrial_ || !trialRobotId_.empty() || (oneKnownRobot && (unmeasured || atHome))) {
            if (status["all_home"].get<bool>() && phase_ == "CLOSING") transition("CLOSED");
            return; // A lone robot already at home needs no charging command; an unmeasured one waits for RFID there.
        }
        // Thứ tự ID ổn định; mutex và đặt chỗ trạm vẫn do các module hiện có quản lý.
        std::map<std::string,std::string> ordered(homes_.begin(),homes_.end());
        for (const auto& home : ordered) {
            auto robots = fleet_.toJson()["agvs"];
            auto a = std::find_if(robots.begin(),robots.end(),[&](const json& r){return r["id"] == home.first;});
            if (a == robots.end()) continue;
            if (a->value("node","") != home.second || !a->value("position_valid",false) ||
                a->value("state","") == "ERROR" || a->value("state","") == "OFFLINE") parked_.erase(home.first);
            const bool chargingHome = a->value("node","") == home.second && a->value("state","") == "CHARGING";
            if (chargingHome) parked_.insert(home.first);
            if (!parked_.count(home.first)) {
                auto error = fleet_.returnToHome(home.first,home.second);
                if (error.empty() && a->value("node","") == home.second) parked_.insert(home.first);
            }
        }
        if (snapshot()["all_home"].get<bool>() && phase_ == "CLOSING") transition("CLOSED");
    }
    json snapshot() {
        json result = {{"supported",supported_},{"single_robot_trial",singleRobotTrial_},{"two_robot_trial",twoRobotTrial_},
            {"trial_robot_id",trialRobotId_},{"supervised_trial",twoRobotTrial_ || !trialRobotId_.empty()},
            {"test_mode",!trialRobotId_.empty() ? "robot" + trialRobotId_ : twoRobotTrial_ ? "both" : singleRobotTrial_ ? "legacy01" : "normal"},
            {"phase",supported_ ? phase_ : "UNAVAILABLE"},
            {"home_stations",homes_},{"robots",json::array()},{"all_home",false},{"can_start",false}};
        if (!supported_) return result;
        const auto fleet = fleet_.toJson(), zones = arbiter_.toJson();
        const auto homesInShift = knownHomes(fleet);
        bool allHome = !homesInShift.empty() && homesInShift.size() == fleet["agvs"].size();
        if (!trialRobotId_.empty()) allHome = homesInShift.size() == 1;
        if (twoRobotTrial_) allHome = allHome && homesInShift.size() == 2;
        bool batteriesReady = true, robot01Ready = false;
        std::map<std::string,std::string> ordered(homes_.begin(),homes_.end());
        for (const auto& home : ordered) {
            auto a = std::find_if(fleet["agvs"].begin(),fleet["agvs"].end(),[&](const json& r){return r["id"] == home.first;});
            bool verified = false, ready = false; std::string reason = "Chua tham gia ca", actual;
            if (a != fleet["agvs"].end()) {
                actual = a->value("node","");
                verified = a->value("online",false) && a->value("position_valid",false) && actual == home.second;
                bool ownsZone = false;
                for (const auto& z : zones.items()) if (z.value().value("owner","") == home.first) ownsZone = true;
                const auto state = a->value("state","");
                ready = verified && (state == "IDLE" || state == "CHARGING") &&
                    (*a)["path"].empty() && (*a)["uncertain_path"].empty() && a->value("target","").empty() && !ownsZone;
                const bool homeReady = ready;
                if (trialRobotId_ == "01" && home.first == "01") {
                    ready = a->value("online",false) && a->value("position_valid",false) && state == "IDLE" &&
                        (*a)["path"].empty() && (*a)["uncertain_path"].empty() &&
                        a->value("target","").empty() && a->value("task_id","").empty();
                    robot01Ready = ready;
                }
                if (!a->value("online",false)) reason = "Mat telemetry";
                else if (!a->value("position_valid",false)) reason = "Cho RFID hop le";
                else if (state == "ERROR") reason = "Can xac minh xe / xoa loi";
                else if (!a->value("task_id","").empty()) reason = "Cho hoan thanh task hien tai";
                else if (state == "BLOCKED") reason = "Cho duong ve tram sac";
                else if (state == "LIFTING" || state == "LOWERING") reason = a->contains("lift") && (*a)["lift"].value("state","")=="PENDING"
                    ? "Cho nhung nhan lenh servo" : state=="LIFTING" ? "Dang nang hang" : "Dang ha hang";
                else if (!verified) reason = "Dang ve tram " + home.second;
                else if (ownsZone) reason = "Cho xac nhan roi mutex zone";
                else if (ready) reason = "Da xac minh tai tram";
                if (trialRobotId_ == "01" && ready) reason = "San sang thu tai RFID hien tai";
                const bool unknownSingleBattery = homesInShift.size() == 1 &&
                    !a->value("battery_measured",false) && a->value("battery",0) == -1;
                batteriesReady = batteriesReady && (singleRobotTrial_ || twoRobotTrial_ || !trialRobotId_.empty() || unknownSingleBattery ||
                    (a->value("battery_measured",false) && a->value("battery",0) >= 40));
                allHome = allHome && homeReady;
            }
            result["robots"].push_back({{"id",home.first},{"home_station",home.second},{"actual_node",actual},
                {"participating",a != fleet["agvs"].end()},
                {"at_home",verified},{"ready",ready},{"reason",reason}});
        }
        if (!trialRobotId_.empty()) {
            bool inactiveReady = true;
            for (const auto& robot : fleet["agvs"]) {
                const auto id = robot.value("id",std::string{});
                if (id == trialRobotId_) continue;
                const std::string home = id == "01" ? "S1" : id == "02" ? "S2" : "";
                bool ownsZone = false;
                for (const auto& zone : zones.items()) if (zone.value().value("owner",std::string{}) == id) ownsZone = true;
                const bool safe = !home.empty() && robot.value("online",false) && robot.value("position_valid",false) &&
                    robot.value("node",std::string{}) == home && robot.value("state",std::string{}) == "IDLE" &&
                    robot.value("task_id",std::string{}).empty() && robot.value("target",std::string{}).empty() &&
                    robot["path"].empty() && robot["uncertain_path"].empty() && !ownsZone;
                inactiveReady = inactiveReady && safe;
                result["robots"].push_back({{"id",id},{"home_station",home},{"actual_node",robot.value("node",std::string{})},
                    {"participating",false},{"at_home",safe},{"ready",safe},
                    {"reason",safe ? "Xe khong chon: dung tai tram" : "Xe khong chon: can xac minh tai tram"}});
            }
            result["inactive_robots_ready"] = inactiveReady;
            allHome = allHome && inactiveReady;
        }
        result["all_home"] = allHome;
        result["can_start"] = phase_ == "CLOSED" && allHome && batteriesReady && fleet_.networkAvailable();
        if (trialRobotId_ == "01") result["can_start"] = phase_ == "CLOSED" && robot01Ready &&
            result.value("inactive_robots_ready",true) && fleet_.networkAvailable();
        return result;
    }
private:
    std::unordered_map<std::string,std::string> knownHomes(const json& fleet) const {
        std::unordered_map<std::string,std::string> result;
        for (const auto& robot : fleet["agvs"]) {
            const auto home = homes_.find(robot.value("id",std::string{}));
            if (home == homes_.end()) { if (!trialRobotId_.empty()) continue; return {}; }
            result.insert(*home);
        }
        return result;
    }
    void transition(const std::string& phase) {
        if (!file_.parent_path().empty()) std::filesystem::create_directories(file_.parent_path());
        auto temporary = file_; temporary += ".tmp";
        std::ofstream output(temporary);
        output << json{{"phase",phase},{"home_stations",homes_}}.dump(2); output.flush();
        if (!output) throw std::runtime_error("Khong luu duoc trang thai ca");
        output.close();
        durableReplaceFile(temporary,file_);
        phase_ = phase;
    }
    MapGraph& map_; FleetManager& fleet_; MutexArbiter& arbiter_;
    std::filesystem::path file_;
    std::unordered_map<std::string,std::string> homes_;
    std::unordered_set<std::string> parked_;
    bool supported_ = false;
    bool singleRobotTrial_ = false;
    bool twoRobotTrial_ = false;
    std::string trialRobotId_;
    std::string phase_ = "CLOSING";
};
