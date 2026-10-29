#pragma once
#include "map/MapGraph.hpp"
#include "core/FLeetmanager.hpp"
#include "utils/Mutexarbiter.hpp"
#include "core/ShiftController.hpp"

// Chỉ gọi khi đã giữ khóa điều phối của server. Không thay hợp đồng MQTT MCU.
class WebCommands {
public:
    WebCommands(MapGraph& map, FleetManager& fleet, MutexArbiter& arbiter,
                std::string savePath = "data/map_layout.json")
        : map_(map), fleet_(fleet), arbiter_(arbiter), savePath_(std::move(savePath)) {}
    void setShiftController(ShiftController* shift) { shift_ = shift; }
    void setSimulationMode(bool enabled) { simulationMode_ = enabled; }

    json execute(const json& j) {
        json result = {{"type", "command_result"}, {"request_id", j.value("request_id", "")}, {"ok", false}};
        try {
            const auto cmd = j.at("cmd").get<std::string>();
            json data = json::object();
            if (cmd == "start_shift" || cmd == "end_shift") {
                if (!shift_) return failure(result,409,"Quan ly ca chua san sang");
                auto error = cmd == "start_shift" ? shift_->start() : shift_->end();
                if (!error.empty()) return failure(result,409,error);
                data["shift"] = shift_->snapshot();
            } else if (cmd == "add_task" || cmd == "direct_goto") {
                if (!fleet_.acceptingTasks()) return failure(result,409,"Ca chua bat dau hoac dang ket thuc; hang doi tam dung");
                const auto target = id(j, "target");
                if (!map_.nodeExists(target)) return failure(result, 404, "Node dich khong ton tai");
                if (j.contains("priority") && !j["priority"].is_number_integer()) return failure(result,400,"priority phai la so nguyen");
                if (j.contains("priority") && (j["priority"] < 0 || j["priority"] > 100))
                    return failure(result,400,"priority phai trong 0..100");
                int priority = j.value("priority", 0);
                if (priority < 0 || priority > 100) return failure(result, 400, "priority phai trong 0..100");
                const auto robotId = cmd == "direct_goto" ? id(j,"agv_id") : std::string{};
                const auto taskId = fleet_.createTaskId();
                if (cmd == "add_task") fleet_.addTask(target, priority, taskId);
                else {
                    auto error = fleet_.directGoto(robotId, target, taskId, priority);
                    if (!error.empty()) return failure(result, 409, error);
                }
                data["task_id"] = taskId;
            } else if (cmd == "warehouse_lift") {
                if(!fleet_.acceptingTasks()) return failure(result,409,"Ca chua RUNNING");
                const auto operation=j.at("action").get<std::string>();
                if(operation!="pickup" && operation!="dropoff") return failure(result,400,"action sai");
                long commandId=0;
                const auto error=fleet_.startLift(id(j,"agv_id"),id(j,"node"),operation=="pickup",commandId);
                if(!error.empty()) return failure(result,409,error);
                data["command_id"]=commandId;
            } else if (cmd == "reconcile_test_cargo") {
                if(!j.contains("physically_clear") || j["physically_clear"]!=true)
                    return failure(result,400,"Can xac nhan ke da ve kho lay va xe trong");
                const auto& entries=j.at("robots");
                if(!entries.is_array() || entries.empty() || entries.size()>2)return failure(result,400,"Danh sach xe sai");
                std::vector<std::pair<std::string,long>> robots;
                for(const auto& entry:entries) {
                    const auto robot=id(entry,"id");
                    if((robot!="01" && robot!="02") || !entry.at("lift_command_id").is_number_integer() ||
                        entry["lift_command_id"]<0 || entry["lift_command_id"]>INT_MAX ||
                        std::any_of(robots.begin(),robots.end(),[&](const auto& r){return r.first==robot;}))
                        return failure(result,400,"ID xe/lenh nang ha sai hoac trung");
                    robots.emplace_back(robot,entry["lift_command_id"].get<long>());
                }
                const auto error=fleet_.reconcileTestCargo(robots);
                if(!error.empty())return failure(result,409,error);
                data["reconciled"]=entries;
            } else if (cmd == "confirm_zone_clear") {
                if (!j.value("physically_clear",false)) return failure(result,400,"Can xac nhan da dung xe va vung trong");
                const auto zone = id(j,"zone"), owner = id(j,"owner");
                const auto robots = fleet_.toJson()["agvs"];
                const auto a = std::find_if(robots.begin(),robots.end(),[&](const json& robot){return robot["id"] == owner;});
                bool parkedAndStopped = false;
                if (a != robots.end() && shift_ && shift_->supported() &&
                    (*a).value("state","") == "IDLE" && (*a).value("online",false) &&
                    (*a).value("position_valid",false) && (*a).value("task_id","").empty() &&
                    (*a).value("target","").empty() && (*a)["path"].empty() &&
                    (*a)["uncertain_path"].empty()) {
                    const auto shift = shift_->snapshot();
                    const auto& homes = shift["home_stations"];
                    parkedAndStopped = (shift.value("two_robot_trial",false) || shift.value("supervised_trial",false) ||
                        shift.value("operation_profile",std::string{}) == "scoped") &&
                        homes.contains(owner) && (*a).value("node","") == homes[owner];
                }
                if (a != robots.end() && (*a)["state"] != "ERROR" &&
                    (*a)["state"] != "OFFLINE" && !parkedAndStopped)
                    return failure(result,409,"Chi xac minh quyen cua xe OFFLINE/ERROR hoac xe IDLE da xac minh tai tram");
                if (!j.at("grant_id").is_number_integer()) return failure(result,400,"grant_id sai");
                if (!arbiter_.confirmClear(zone,owner,j.at("grant_id").get<long>())) return failure(result,409,"Quyen vung da thay doi; tai lai trang thai");
                data = {{"zone",zone},{"new_owner",arbiter_.currentOwner(zone)}};
            } else if (cmd == "resume_task") {
                const auto error=fleet_.resumeTask(id(j,"task_id"),id(j,"expected_node"));
                if (!error.empty()) return failure(result,409,error);
                data["task_id"]=j.at("task_id");
            } else if (cmd == "confirm_recovery_heading") {
                if(!j.value("physically_confirmed",false))return failure(result,400,"Can xac nhan huong dau xe thuc te");
                const auto error=fleet_.confirmRecoveryHeading(id(j,"id"),id(j,"expected_node"),id(j,"heading"));
                if(!error.empty())return failure(result,409,error);
                data["pending"]=true;
            } else if (cmd == "recover_vehicle" || cmd == "recover_home" || cmd == "recover_node") {
                if (!j.value("physically_clear",false)) return failure(result,400,"Can xac nhan xe dung va cac vung cu da trong");
                if(j.contains("repositioned") && !j["repositioned"].is_boolean())return failure(result,400,"repositioned sai");
                const auto error=fleet_.recoverVerifiedNode(id(j,"id"),id(j,"expected_node"),cmd=="recover_home",true,
                    j.value("repositioned",false),cmd=="recover_vehicle");
                if (!error.empty()) return failure(result,409,error);
                data["node"]=j.at("expected_node");
            } else if (cmd == "cancel_task") {
                auto taskId = j.value("task_id", std::string{});
                auto target = j.value("target", std::string{});
                if (taskId.empty() && target.empty()) return failure(result, 400, "Thieu task_id/target");
                if ((!taskId.empty() && !validIdentifier(taskId)) || (!target.empty() && !validIdentifier(target))) return failure(result,400,"task_id/target sai");
                int count = fleet_.cancelTask(target, taskId);
                if (!count) return failure(result, 404, "Khong tim thay task dang cho/chay");
                data["cancelled"] = count;
            } else if (cmd == "clear_error") {
                const auto trial = fleet_.toJson().value("trial_robot_id",std::string{});
                const auto robot = id(j,"id");
                if (!fleet_.isActiveRobot(robot)) return failure(result,409,"Xe khong nam trong pham vi van hanh hien tai");
                const bool robot01Reset = trial == "01" && robot == "01";
                if (!robot01Reset && ((shift_ && shift_->snapshot().value("supervised_trial",false)) ||
                    !trial.empty())) {
                    const auto snapshot = fleet_.toJson();
                    const auto& homes = snapshot["home_stations"];
                    for (const auto& a : snapshot["agvs"]) if (a["id"] == robot &&
                        (!homes.contains(robot) || a["node"] != homes[robot]))
                        return failure(result,409,"Ca thu: dung xe va xac minh tai tram rieng S1/S2 truoc khi reset loi; RFID giua tuyen khong xac nhan huong sau reboot");
                }
                if (!fleet_.clearError(robot)) return failure(result, 409, "Xe khong o trang thai loi hoac telemetry/vi tri chua hop le");
            } else if (cmd == "remove_agv") {
                auto robot = id(j, "id");
                auto fleet = fleet_.toJson();
                auto found = std::find_if(fleet["agvs"].begin(), fleet["agvs"].end(), [&](const json& a){ return a["id"] == robot; });
                if (found == fleet["agvs"].end()) return failure(result, 404, "Khong tim thay robot");
                if ((*found)["state"] != "OFFLINE") return failure(result, 409, "Chi go robot OFFLINE sau khi da dua ra khoi kho");
                const auto zones = arbiter_.toJson();
                for (const auto& z : zones.items())
                    if (z.value().value("owner", "") == robot) return failure(result, 409, "Robot van giu mutex zone; can xac minh va tra quyen");
                fleet_.removeAGV(robot);
            } else if (cmd == "load_map_data" || cmd == "add_node" || cmd == "remove_node" ||
                       cmd == "add_edge" || cmd == "remove_edge") {
                // Chưa có firmware xác nhận dừng vật lý: chỉ sửa cấu trúc khi fleet và queue trống.
                auto fleet = fleet_.toJson();
                if (!fleet["agvs"].empty() || !fleet["pending_tasks"].empty())
                    return failure(result, 409, "Chi sua cau truc ban do khi chua co robot va nhiem vu trong core");
                const auto zones = arbiter_.toJson();
                for (const auto& z : zones.items()) if (!z.value().value("owner", "").empty())
                    return failure(result, 409, "Van co robot giu mutex zone");
                if (cmd == "load_map_data") map_.loadFromJson(j.at("map"));
                else if (cmd == "add_node") map_.addNode(id(j, "id"), j.at("x").get<double>(), j.at("y").get<double>(), j.value("type", "waypoint"));
                else if (cmd == "remove_node") {
                    auto node = id(j, "id");
                    if (!map_.nodeExists(node)) return failure(result, 404, "Node khong ton tai");
                    map_.removeNode(node);
                } else if (cmd == "add_edge") map_.addEdge(id(j, "from"), id(j, "to"), j.at("weight").get<int>(),
                                                          j.value("type", "main"), j.value("path_class", ""), j.value("two_way", false));
                else map_.removeEdge(id(j, "from"), id(j, "to"));
                arbiter_.resetZones(map_.mutexZones());
                data["map_revision"] = map_.toJson()["revision"];
            } else if (cmd == "set_obstacle") {
                auto node = id(j, "node");
                if (!map_.nodeExists(node)) return failure(result, 404, "Node khong ton tai");
                bool blocked = j.at("blocked").get<bool>();
                if (j.contains("ttl_s") && !j["ttl_s"].is_number_integer()) return failure(result,400,"TTL phai la so nguyen");
                int ttl = j.value("ttl_s",0);
                if (ttl < 0 || ttl > 86400) return failure(result,400,"TTL phai trong 0..86400");
                if (blocked) fleet_.reportObstacle("", node,ttl); else fleet_.clearObstacle(node);
                data = {{"node", node}, {"blocked", blocked}};
            } else if (cmd == "sim_edge_obstacle" || cmd == "sim_replan") {
                if (!simulationMode_) return failure(result,403,"Lenh chi dung trong mo phong");
                const auto deferred=j.value("deferred_agvs",std::vector<std::string>{});
                if (deferred.size()>2 || std::any_of(deferred.begin(),deferred.end(),[](const std::string& id){return id!="01" && id!="02";}))
                    return failure(result,400,"Danh sach xe mo phong khong hop le");
                if (cmd=="sim_edge_obstacle") {
                    const auto from=id(j,"from"),to=id(j,"to");
                    const bool blocked=j.at("blocked").get<bool>();
                    map_.setEdgeObstacle(from,to,blocked);
                    data={{"from",from},{"to",to},{"blocked",blocked}};
                }
                fleet_.simulationReplan(deferred);
            } else if (cmd == "save_map") {
                if (j.value("path", savePath_) != savePath_) return failure(result, 400, "Chi luu vao duong dan ban do cau hinh cua core");
                map_.saveToFile(savePath_);
                data["path"] = savePath_;
            } else return failure(result, 400, "Lenh khong duoc ho tro: " + cmd);
            result["ok"] = true; result["data"] = data;
        } catch (const std::invalid_argument& e) { return failure(result, 400, e.what()); }
          catch (const nlohmann::json::exception& e) { return failure(result, 400, e.what()); }
          catch (const std::exception& e) { return failure(result, 500, e.what()); }
        return result;
    }
private:
    static std::string id(const json& j, const std::string& key) {
        auto value = j.at(key).get<std::string>();
        if (!validIdentifier(value))
            throw std::invalid_argument("Dinh danh khong hop le: " + key);
        return value;
    }
    static json failure(json result, int status, const std::string& message) {
        result["status"] = status; result["error"] = message; return result;
    }
    MapGraph& map_;
    FleetManager& fleet_;
    MutexArbiter& arbiter_;
    std::string savePath_;
    ShiftController* shift_ = nullptr;
    bool simulationMode_ = false;
};
