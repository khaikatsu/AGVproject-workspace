#include "commands/WebCommands.hpp"
#include "commands/CommandIds.hpp"
#include "core/RuntimeStore.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("Failed: " #condition); } while (false)
json graph() {
    json j = {{"nodes",json::array()}, {"edges",json::array()}, {"map_name","Test"}};
    for (auto id : {"A","B","C","D","M","Q","T"}) j["nodes"].push_back({{"id",id},{"x",0},{"y",0},{"type",std::string(id)=="M" ? "mutex" : "waypoint"}});
    for (auto e : {std::make_tuple("A","T",300), std::make_tuple("B","C",1), std::make_tuple("C","D",1),
                   std::make_tuple("D","T",1),std::make_tuple("M","Q",1),std::make_tuple("Q","T",1)})
        j["edges"].push_back({{"from",std::get<0>(e)},{"to",std::get<1>(e)},{"weight",std::get<2>(e)}});
    return j;
}
json record(FleetManager& f,const std::string& id) {
    const auto state = f.toJson();
    for (auto& t : state["tasks"]) if (t["task_id"] == id) return t;
    throw std::runtime_error("Missing task " + id);
}
json comparableMap(MapGraph& map) {
    auto j = map.toJson();j.erase("revision");
    for (const auto* key : {"nodes","edges","mutex_zones","charging_stations","obstacles"})
        std::sort(j[key].begin(),j[key].end(),[](const json& a,const json& b) {return a.dump() < b.dump();});
    return j;
}
void confirmCancelledStop(FleetManager& fleet,const std::string& id,const std::string& node) {
    fleet.bindEncoderSession(id,777,1,100);
    MotionDiagnostic stop{node,"AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
    stop.encoderBoot=777;stop.encoderConnection=1;stop.encoderUptime=1000;
    fleet.observeMotionDiagnostic(id,stop);
}
int main() {
    try {
        {
            // Same temporary policy as Core main; original default policy remains tested below.
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            for (const std::string id : {"01", "02"}) {
                auto dir = root/".qa"/("battery-bypass-" + id + "-" + std::to_string(Clock::now().time_since_epoch().count()));
                MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
                fleet.setBatteryTestBypass(true);fleet.setAutomaticChargingEnabled(false);
                fleet.setActiveRobots({id});fleet.setNetworkAvailable(true);
                std::vector<json> sent;
                fleet.setPublishCallback([&](const std::string&,const std::string& p){sent.push_back(json::parse(p));});
                const std::string home=id=="01"?"S1":"S2",gate=id=="01"?"GS1":"GS2";
                ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{id,home}},false,false,"",{id});
                fleet.updateTelemetry(id,home,-1,false);shift.tick();
                CHECK(shift.snapshot()["can_start"]==true);CHECK(shift.start().empty());
                CHECK(!fleet.batteryGateEnabled());CHECK(fleet.toJson()["automatic_charging_enabled"]==false);
                CHECK(fleet.directGoto(id,gate,"unknown-pin").empty());
                fleet.updateTelemetry(id,gate,0,false);CHECK(record(fleet,"unknown-pin")["status"]=="completed");
                for (int percent : {0, 29, 30, 39}) {
                    fleet.updateTelemetry(id,gate,percent,false);
                    CHECK(fleet.directGoto(id,home,"low-pin-"+std::to_string(percent)).empty());
                    fleet.updateTelemetry(id,home,percent,false);fleet.tick();
                    CHECK(record(fleet,"low-pin-"+std::to_string(percent))["status"]=="completed");
                    CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
                }
                // Queued assignment also bypasses pin; a later drop must not steal the route.
                fleet.addTask(gate,1,"low-pin-auto");
                CHECK(record(fleet,"low-pin-auto")["status"]=="dispatched");
                fleet.updateTelemetry(id,home,0,false);fleet.tick();
                CHECK(record(fleet,"low-pin-auto")["status"]=="dispatched");
                CHECK(fleet.toJson()["agvs"][0]["target"]==gate);
                fleet.updateTelemetry(id,gate,0,false);
                CHECK(record(fleet,"low-pin-auto")["status"]=="completed");
                CHECK(shift.end().empty());CHECK(fleet.toJson()["agvs"][0]["target"]==home);
                fleet.updateTelemetry(id,home,0,false);shift.tick();
                CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(shift.snapshot()["can_start"]==true);
                CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
                CHECK(std::none_of(sent.begin(),sent.end(),[](const json& c){return c.value("cmd","")=="CHARGE_START";}));
                CHECK(shift.start().empty());fleet.setNetworkAvailable(false);
                CHECK(!fleet.directGoto(id,gate,"no-network").empty());fleet.setNetworkAvailable(true);
                fleet.setBatteryTestBypass(false);CHECK(fleet.batteryGateEnabled());
                CHECK(!fleet.directGoto(id,gate,"restored-pin-gate").empty());
            }
        }
        {
            MapGraph map;map.loadDefaultDemoMap();const auto layout=map.toJson();
            CHECK(NodeManeuver::straightThrough(layout,"G6","M4","GS2"));
            CHECK(NodeManeuver::straightThrough(layout,"M4","GS2","M3"));
            CHECK(NodeManeuver::straightThrough(layout,"GS2","M3","G1"));
            CHECK(!NodeManeuver::straightThrough(layout,"G6","M4","G5"));
            CHECK(!NodeManeuver::straightThrough(layout,"K1","G1","M5")); // Backing out requires a gate pivot.
            CHECK(!NodeManeuver::straightThrough(layout,"GS1","S1","GS1"));
            MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("01");std::vector<json> sent;
            fleet.setPublishCallback([&](const std::string&,const std::string& p){sent.push_back(json::parse(p));});
            CHECK(fleet.updateTelemetry("01","G6",80,false));
            CHECK(!fleet.updateTelemetry("01","G6",80,false)); // Heartbeat is not a new RFID observation.
            CHECK(fleet.directGoto("01","G1","legacy-through").empty());
            CHECK(!sent.back().contains("pass_through"));fleet.cancelTask("","legacy-through");
            confirmCancelledStop(fleet,"01","G6");
            MotionDiagnostic d{"G6","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
            d.headingSupported=true;d.heading="S";d.headingNode="G6";d.headingSource="route";
            d.nodeOrdersSupported=d.straightThroughSupported=true;fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.directGoto("01","G1","continuous-trip").empty());
            CHECK(sent.back()["path"]==json::array({"M4","GS2","M3","G1"}));
            CHECK(sent.back()["pass_through"]==json::array({0,1,2}));
            fleet.reportAck("01",sent.back()["cmd_id"].get<long>());
            for (const auto* node : {"M4","GS2","M3"}) {
                fleet.updateTelemetry("01",node,80,false);
                CHECK(record(fleet,"continuous-trip")["status"]=="dispatched");
                CHECK(fleet.toJson()["agvs"][0]["node"]==node);
            }
            fleet.updateTelemetry("01","G1",80,false);CHECK(record(fleet,"continuous-trip")["status"]=="completed");
            fleet.updateTelemetry("01","G6",80,false);fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.directGoto("01","K5","turn-trip").empty());
            const auto through=sent.back()["pass_through"];
            CHECK(std::find(through.begin(),through.end(),json(0))==through.end());
            fleet.cancelTask("","turn-trip");confirmCancelledStop(fleet,"01","G6");
            d.straightThroughSupported=false;fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.directGoto("01","G1","old-firmware").empty());CHECK(!sent.back().contains("pass_through"));
            fleet.cancelTask("","old-firmware");confirmCancelledStop(fleet,"01","G6");fleet.updateTelemetry("01","K1",80,false);
            d.node=d.headingNode="K1";d.heading="W";d.headingSource="bay_default";d.straightThroughSupported=true;
            fleet.observeMotionDiagnostic("01",d);CHECK(fleet.directGoto("01","M5","warehouse-exit-through").empty());
            CHECK(sent.back()["pass_through"].empty()); // Do not reject the valid reverse-exit route at the MCU.
        }
        {
            long next = 100;
            MutexArbiter arbiter({"M"});arbiter.setGrantIdAllocator([&]{return ++next;});arbiter.setRetryTimeout(1);
            CHECK(!arbiter.requestZone("invalid","01"));CHECK(arbiter.requestZone("M","01"));
            const auto granted = arbiter.grant("M","01");CHECK(granted["grant_id"] == 101);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));CHECK(arbiter.retryGrants().size() == 1);
            CHECK(!arbiter.acknowledge("M","02",101));CHECK(!arbiter.acknowledge("M","01",100));
            CHECK(arbiter.acknowledge("M","01",101));CHECK(arbiter.retryGrants().empty());
            CHECK(!arbiter.requestZone("M","02"));const auto snapshot = arbiter.toJson();
            MutexArbiter restored({"M"});restored.setGrantIdAllocator([&]{return ++next;});restored.restore(snapshot);
            CHECK(restored.toJson()["M"]["recovery_required"] == true);CHECK(restored.retryGrants().empty());
            CHECK(restored.releaseZone("M","01",101).empty());CHECK(restored.currentOwner("M") == "01");
            CHECK(restored.requestZone("M","01"));CHECK(restored.grant("M","01")["grant_id"] == 102);
            restored.releaseZone("M","01",101);CHECK(restored.currentOwner("M") == "01");
            CHECK(restored.releaseZone("M","01",102) == "02");
        }
        {
            MapGraph map;map.loadFromJson(graph());map.setObstacle("T",true,1);
            MapGraph roundtrip;roundtrip.loadFromJson(map.toJson());CHECK(roundtrip.hasObstacle("T"));
            CHECK(roundtrip.expireObstacles(MapGraph::epochSeconds()+2));CHECK(!roundtrip.hasObstacle("T"));
            CHECK(map.hasObstacle("T"));CHECK(map.expireObstacles(MapGraph::epochSeconds()+2));CHECK(!map.hasObstacle("T"));
            map.setObstacle("T",true);map.setObstacle("T",true,1);map.expireObstacles(MapGraph::epochSeconds()+2);CHECK(map.hasObstacle("T"));
            map.setObstacle("T",false);MutexArbiter arbiter({"M"});FleetManager fleet(map,arbiter);
            fleet.setNetworkAvailable(false);fleet.addTask("T",1,"queue-low");fleet.addTask("T",2,"queue-high");
            fleet.updateTelemetry("01","A",80,false);const auto saved = fleet.durableSnapshot();
            FleetManager restored(map,arbiter);restored.restore(saved);
            CHECK(restored.toJson()["pending_tasks"][0]["task_id"] == "queue-high");
            CHECK(restored.toJson()["agvs"][0]["online"] == false);CHECK(restored.toJson()["agvs"][0]["position_valid"] == false);
            restored.updateTelemetry("01","A",80,false);CHECK(restored.toJson()["agvs"][0]["state"] == "ERROR");
            auto folder = std::filesystem::temp_directory_path()/("agv-runtime-test-"+std::to_string(Clock::now().time_since_epoch().count()));
            RuntimeStore store(folder/"runtime.json");const json data = {{"version",1},{"map",map.toJson()},{"fleet",saved},{"mutex",arbiter.toJson()}};
            store.save(data);CHECK(store.load().value() == data);
            auto sameGraph = map.toJson();
            std::reverse(sameGraph["nodes"].begin(),sameGraph["nodes"].end());
            sameGraph["revision"] = 999;
            sameGraph["obstacles"] = json::array({"A"});
            CHECK(RuntimeStore::mapsCompatible(map.toJson(),sameGraph));
            sameGraph["edges"][0]["weight"] = 999;
            CHECK(!RuntimeStore::mapsCompatible(map.toJson(),sameGraph));
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("01");fleet.setHomeStations({{"01","S1"}});WebCommands web(map,fleet,arbiter);
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& p){commands.push_back(json::parse(p));});
            fleet.updateTelemetry("01","GS1",-1,false);fleet.reportError("01","LINE_LOST");
            auto result=web.execute({{"cmd","clear_error"},{"id","01"}});
            CHECK(result["ok"].get<bool>());CHECK(commands.back()["cmd"]=="RESET_FAULT");
            CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            fleet.reportAck("01",commands.back()["cmd_id"].get<long>());
            MotionDiagnostic d{"GS1","AGV_SAFE_STOP","LINE_LOST","STOP",false,true,true,0,0,0};
            fleet.observeMotionDiagnostic("01",d);CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            d.state="AGV_IDLE";d.error="NONE";d.node="M2";
            fleet.observeMotionDiagnostic("01",d);CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            d.node="GS1";d.motor="FORWARD";d.leftDuty=d.rightDuty=245;
            fleet.observeMotionDiagnostic("01",d);CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            d.motor="STOP";d.leftDuty=d.rightDuty=0;d.rfidAgeMs=5000;
            fleet.observeMotionDiagnostic("01",d);CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
            CHECK(fleet.directGoto("01","M2","after-mid-route-reset").empty());
            fleet.updateTelemetry("01","M2",-1,false);CHECK(arbiter.requestZone("M2","01"));
            const auto grant=arbiter.grant("M2","01");fleet.reportError("01","LINE_LOST");
            CHECK(web.execute({{"cmd","clear_error"},{"id","01"}})["ok"].get<bool>());
            d.node="M2";fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
            CHECK(arbiter.currentOwner("M2")=="01");CHECK(arbiter.grant("M2","01")["grant_id"]==grant["grant_id"]);
            const auto shiftDir=std::filesystem::path(".qa")/("mid-reset-shift-"+std::to_string(Clock::now().time_since_epoch().count()));
            ShiftController shift(map,fleet,arbiter,shiftDir/"shift.json",{{"01","S1"}},false,false,"01");
            CHECK(shift.snapshot()["can_start"].get<bool>());CHECK(!shift.snapshot()["all_home"].get<bool>());
            CHECK(shift.start().empty());CHECK(shift.snapshot()["phase"]=="RUNNING");
            CHECK(shift.end().empty());CHECK(shift.snapshot()["phase"]=="CLOSED");
            CHECK(arbiter.currentOwner("M2")=="01");
            fleet.updateTelemetry("01","UNKNOWN",-1,false);
            CHECK(!web.execute({{"cmd","clear_error"},{"id","01"}})["ok"].get<bool>());
        }
#if defined(_WIN32)
            const auto runtimePath = folder/"runtime.json";
            HANDLE reader = CreateFileW(runtimePath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            CHECK(reader != INVALID_HANDLE_VALUE);
            std::thread unlock([reader] {std::this_thread::sleep_for(std::chrono::milliseconds(60));CloseHandle(reader);});
            auto changed = data;changed["test_sequence"] = 1;
            try {store.save(changed);} catch (...) {unlock.join();throw;}
            unlock.join();CHECK(store.load().value() == changed);
#endif
            std::ofstream damaged(folder/"runtime.json");damaged << "broken";damaged.close();
            bool rejected = false;try {store.load();} catch (...) {rejected = true;}CHECK(rejected);
            CHECK(!validIdentifier("bad|id"));CHECK(!validIdentifier("bad+id"));CHECK(!validIdentifier("<tag>"));
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",-1,false);
            CHECK(fleet.toJson()["agvs"][0]["position_valid"] == true);
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == false);
            fleet.updateTelemetry("01","B",19,false);CHECK(fleet.toJson()["agvs"][0]["needs_recharge"] == true);
            fleet.updateTelemetry("01","B",25,false);CHECK(fleet.toJson()["agvs"][0]["needs_recharge"] == true);
            fleet.updateTelemetry("01","B",40,false);CHECK(fleet.toJson()["agvs"][0]["needs_recharge"] == false);
            bool rejected = false;try {fleet.setChargeThresholds(40,20,90);} catch (...) {rejected = true;}CHECK(rejected);
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.setNetworkAvailable(true);fleet.setAcceptingTasks(true);
            fleet.updateTelemetry("01","A",10,false);
            CHECK(fleet.directGoto("01","T","normal-low-battery") == "Pin chua do hoac khong du de nhan task");
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            std::vector<std::string> commands;
            fleet.setPublishCallback([&](const std::string&, const std::string& payload) {
                commands.push_back(json::parse(payload).at("cmd").get<std::string>());
            });
            fleet.setMonitorOnlyRobot("02");
            fleet.updateTelemetry("02","S2",10,false);
            CHECK(fleet.toJson()["agvs"][0]["state"] == "IDLE");
            commands.clear(); // Lần thấy telemetry đầu có thể gửi WAIT để đồng bộ trạng thái.
            CHECK(fleet.returnToHome("02","S2").empty());
            CHECK(commands.empty());
            fleet.updateTelemetry("02","S2",80,false);
            CHECK(fleet.directGoto("02","K1","monitor-reject") == "Xe chi giam sat; khong nhan lenh di chuyen");
            fleet.addTask("K1",1,"monitor-queue");
            CHECK(record(fleet,"monitor-queue")["status"] == "pending");
            CHECK(commands.empty());
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","S1",-1,false);
            CHECK(!fleet.directGoto("01","GS1","trial-locked").empty());
            fleet.setSingleRobotTrial(true);
            bool automaticRejected = false;
            try {fleet.addTask("M2");} catch (const std::invalid_argument&) {automaticRejected = true;}
            CHECK(automaticRejected);
            CHECK(fleet.directGoto("01","M2","trial-s1-m2").empty());
            CHECK(!fleet.directGoto("01","G1","trial-busy").empty());
            fleet.updateTelemetry("01","GS1",-1,false);
            fleet.updateTelemetry("01","M2",-1,false);
            CHECK(record(fleet,"trial-s1-m2")["status"] == "completed");
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == false);
            fleet.updateBatteryDiagnostic("01",67);
            CHECK(fleet.toJson()["agvs"][0]["battery_display_percent"] == 67);
            CHECK(fleet.toJson()["agvs"][0]["battery"] == 67);
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == true);
            CHECK(fleet.toJson()["agvs"][0]["battery_control_source"] == "diagnostic");
            fleet.updateTelemetry("01","M2",-1,false);
            CHECK(fleet.toJson()["agvs"][0]["battery"] == 67);
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == true);
            fleet.updateTelemetry("01","M2",10,false);
            CHECK(fleet.toJson()["agvs"][0]["battery"] == 10);
            CHECK(fleet.toJson()["agvs"][0]["battery_control_source"] == "telemetry");
            CHECK(fleet.directGoto("01","G1","trial-m2-c3").empty());
            fleet.updateTelemetry("02","S2",80,false);
            CHECK(record(fleet,"trial-m2-c3")["status"] == "interrupted");
            CHECK(fleet.toJson()["agvs"].size() == 2);
            CHECK(!fleet.directGoto("01","G1","trial-other-present").empty());
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.setNetworkAvailable(true);fleet.setAcceptingTasks(true);
            fleet.updateTelemetry("01","A",-1,false);
            fleet.updateBatteryDiagnostic("01",67);
            fleet.updateBatteryDiagnostic("01",-1);
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == false);
            CHECK(fleet.toJson()["agvs"][0]["battery_control_source"] == "unavailable");
            CHECK(!fleet.directGoto("01","T","normal-invalid-diagnostic-battery").empty());
            fleet.updateBatteryDiagnostic("01",67);
            CHECK(fleet.directGoto("01","T","normal-diagnostic-battery").empty());
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",55,false);
            fleet.updateBatteryDiagnostic("01",-1);
            CHECK(fleet.toJson()["agvs"][0]["battery"] == 55);
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == true);
            CHECK(fleet.toJson()["agvs"][0]["battery_control_source"] == "telemetry");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            CHECK(fleet.encoderEnabled("01")); // capability theo firmware/ID, khong theo ten mode test
            fleet.setActiveRobots({"01"});fleet.setAcceptingTasks(true);
            fleet.updateTelemetry("01","A",67,false);fleet.updateTelemetry("02","B",80,false);
            CHECK(fleet.encoderEnabled("01"));CHECK(!fleet.encoderEnabled("02"));
            CHECK(fleet.directGoto("02","T","inactive-robot") == "Xe khong nam trong pham vi van hanh hien tai");
            fleet.addTask("T",1,"scoped-auto");
            CHECK(record(fleet,"scoped-auto")["assigned_agv"] == "01");
            const auto scoped=fleet.toJson();
            CHECK(scoped["active_robot_ids"]==json::array({"01"}));
            for(const auto& robot:scoped["agvs"])
                CHECK(robot["participating"].get<bool>()==(robot["id"]=="01"));
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setActiveRobots({"01"});
            fleet.updateTelemetry("01","S1",67,false);fleet.updateTelemetry("02","S2",80,false);
            const auto dir=std::filesystem::path(".qa")/("scoped-shift-"+std::to_string(Clock::now().time_since_epoch().count()));
            ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{"01","S1"}},false,false,"",{"01"});
            CHECK(shift.snapshot()["operation_profile"]=="scoped");
            CHECK(shift.snapshot()["active_robot_ids"]==json::array({"01"}));
            shift.tick();
            CHECK(shift.snapshot()["can_start"].get<bool>());
            fleet.updateTelemetry("02","UNKNOWN",80,false);
            CHECK(!shift.snapshot()["can_start"].get<bool>());
        }
        {
            MapGraph map;map.loadDefaultDemoMap();
            CHECK(map.toJson()["nodes"].size() == 22);CHECK(map.mutexZones().size() == 6);
            for (auto removed : {"C1","C2","C3","C4"}) CHECK(!map.nodeExists(removed));
            auto horizontal = map.findShortestPath("G1","G6");
            CHECK((horizontal == std::vector<std::string>{"G1","M5","M6","G6"}));
            CHECK(map.pathCost(horizontal) == 200);
            CHECK((map.findShortestPath("M1","M3") == std::vector<std::string>{"M1","G2","M5","G3","M3"}));
            CHECK((map.findShortestPath("M4","M2") == std::vector<std::string>{"M4","G5","M6","G4","M2"}));
            CHECK(map.findShortestPath("M1","K3",{"M5"}).empty());
            CHECK(!map.findShortestPath("M5","G1").empty()); // vòng ngoài nay đã khép
            CHECK((map.findShortestPath("M3","M1") == std::vector<std::string>{"M3","G1","M1"}));
            CHECK(map.pathCost({"M3","G1","M1"})==270);
            for (auto home : {"S1","S2"}) for (auto warehouse : {"K1","K2","K3","K4","K5","K6"}) {
                CHECK(!map.findShortestPath(home,warehouse).empty());
                CHECK(!map.findShortestPath(warehouse,home).empty());
            }
            const auto expected = comparableMap(map);
            std::unordered_map<std::string,json> nodes;
            for (auto& n : expected["nodes"]) nodes[n["id"].get<std::string>()] = n;
            for (auto& e : expected["edges"]) {
                auto a = nodes.at(e["from"].get<std::string>()),b = nodes.at(e["to"].get<std::string>());
                if (e.contains("geometry")) CHECK(e["weight"]==135);
                else CHECK(e["weight"].get<int>() == std::lround(std::hypot(a["x"].get<double>()-b["x"].get<double>(),a["y"].get<double>()-b["y"].get<double>())));
            }
            for (auto station : {"K2","K4"}) CHECK(nodes[station]["y"] == 35);
            for (auto station : {"K3","K5"}) CHECK(nodes[station]["y"] == 105);
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            for (const auto* file : {"data/map_layout.json"}) {
                MapGraph saved;saved.loadFromFile((root/file).string());CHECK(comparableMap(saved) == expected);
            }
            MutexArbiter arbiter(map.mutexZones());
            for (auto zone : {"M5","M6"}) {
                CHECK(arbiter.requestZone(zone,"01"));CHECK(!arbiter.requestZone(zone,"02"));
                CHECK(arbiter.currentOwner(zone) == "01");CHECK(arbiter.releaseZone(zone,"02").empty());
                CHECK(arbiter.currentOwner(zone) == "01");CHECK(arbiter.releaseZone(zone,"01") == "02");
                CHECK(arbiter.currentOwner(zone) == "02");arbiter.releaseZone(zone,"02");
            }
        }
        {
            auto folder = std::filesystem::temp_directory_path() / ("agv-counter-test-" + std::to_string(Clock::now().time_since_epoch().count()));
            auto file = folder / "counter.json";
            long first;
            {CommandIds ids(file);first = ids.next();CHECK(ids.next() > first);}
            {CommandIds ids(file);CHECK(ids.next() > first + 1);}
            {CommandIds ids(file);ids.ensureAbove(5000);CHECK(ids.next() == 5001);}
            {CommandIds ids(file);CHECK(ids.next() > 5001);}
            std::ofstream out(file);out << "invalid";out.close();
            bool rejected = false;try {CommandIds ids(file);} catch (...) {rejected = true;}CHECK(rejected);
        }
        {
            // A stale ID must stop Core's resend loop without ACKing or
            // replacing it with another low STOP. Healthy diagnostic alone
            // cannot erase the counter mismatch.
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("01");fleet.setHomeStations({{"01","S1"}});
            long nextId=10,observed=-1;
            fleet.setCommandIdAllocator([&]{return ++nextId;});
            fleet.setCommandIdHighwaterCallback([&](long value){observed=value;if(nextId<value) nextId=value;});
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            fleet.updateTelemetry("01","S1",80,false);
            const auto beforeGoto=commands.size();
            CHECK(fleet.directGoto("01","GS1","stale-command").empty());
            CHECK(commands.size()==beforeGoto+1 && commands.back()["cmd"]=="GOTO");
            fleet.reportError("01","STALE_COMMAND");
            CHECK(commands.size()==beforeGoto+1); // no unsafe low-ID STOP
            fleet.setAckTimeout(1);fleet.setMaxRetries(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));fleet.tick();
            CHECK(commands.size()==beforeGoto+1); // pending retry was discarded
            MotionDiagnostic healthy{"S1","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
            healthy.lastCommandId=5000;fleet.observeMotionDiagnostic("01",healthy);
            CHECK(observed==5000);CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            CHECK(fleet.clearError("01"));
            CHECK(commands.back()["cmd"]=="RESET_FAULT" && commands.back()["cmd_id"].get<long>()>5000);
            fleet.reportAck("01",commands.back()["cmd_id"].get<long>());
            fleet.observeMotionDiagnostic("01",healthy);
            CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
        }
        {
            MapGraph map;map.loadFromJson(graph());auto old = map.toJson();
            auto bad = graph();bad["edges"][0]["to"] = "missing";
            try {map.loadFromJson(bad);CHECK(false);} catch (const std::invalid_argument&) {}
            CHECK(map.toJson() == old);
            bad = graph();bad["edges"][0]["weight"] = -1;
            try {map.loadFromJson(bad);CHECK(false);} catch (const std::invalid_argument&) {}
            bad["edges"][0]["weight"] = 1.5;
            try {map.loadFromJson(bad);CHECK(false);} catch (const std::invalid_argument&) {}
            CHECK(map.pathCost(map.findShortestPath("B","T")) == 3);
            CHECK(map.findShortestPath("T","A").empty());
            map.addEdge("A","B",2,"branch","",true);
            CHECK(map.findShortestPath("B","A").size() == 2);
            map.addEdge("A","B",2,"branch","",false);
            CHECK(map.findShortestPath("B","A").empty());
            map.addEdge("A","B",2,"branch","",true);map.removeEdge("A","B");
            CHECK(map.findShortestPath("B","A").empty());
            map.removeNode("M");CHECK(map.mutexZones().empty());
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",80,false);fleet.updateTelemetry("02","B",80,false);
            fleet.addTask("T",0,"weighted");CHECK(record(fleet,"weighted")["assigned_agv"] == "02");
            fleet.updateTelemetry("02","T",79,false);CHECK(record(fleet,"weighted")["status"] == "completed");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.setTwoRobotTrial(true);
            fleet.updateTelemetry("01","A",-1,false);fleet.updateTelemetry("02","B",-1,false);
            fleet.addTask("T",0,"nearest-no-battery");
            CHECK(record(fleet,"nearest-no-battery")["assigned_agv"] == "02");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.setTwoRobotTrial(true);
            fleet.updateTelemetry("01","A",95,false);fleet.updateTelemetry("02","B",10,false);
            fleet.addTask("T",0,"nearest-despite-low-battery");
            CHECK(record(fleet,"nearest-despite-low-battery")["assigned_agv"] == "02");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.setTwoRobotTrial(true);
            fleet.updateTelemetry("01","A",-1,false);fleet.updateTelemetry("02","B",-1,false);
            fleet.reportError("02","offline trial");
            fleet.addTask("T",0,"nearest-available");
            CHECK(record(fleet,"nearest-available")["assigned_agv"] == "01");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.addTask("T",0,"same-1");fleet.addTask("T",0,"same-2");
            CHECK(fleet.cancelTask("","same-1") == 1);
            CHECK(record(fleet,"same-1")["status"] == "cancelled");
            CHECK(record(fleet,"same-2")["status"] == "pending");
            CHECK(fleet.toJson()["pending_tasks"].size() == 1);
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            WebCommands commands(map,fleet,arbiter);
            CHECK(commands.execute({{"cmd","load_map_data"},{"map",graph()},{"request_id","map"}})["ok"] == true);
            fleet.updateTelemetry("01","M",80,false);
            CHECK(commands.execute({{"cmd","load_map_data"},{"map",graph()}})["status"] == 409);
            CHECK(commands.execute({{"cmd","direct_goto"},{"agv_id","missing"},{"target","T"},{"task_id","bad"}})["ok"] == false);
            CHECK(commands.execute({{"cmd","add_task"},{"target","missing"},{"task_id","bad"}})["status"] == 404);
            fleet.addTask("T",0,"error");fleet.reportError("01","Motor fault");
            CHECK(record(fleet,"error")["status"] == "interrupted");
            CHECK(fleet.toJson()["pending_tasks"].empty());
            CHECK(fleet.toJson()["agvs"][0]["error"] == "Motor fault");
        }
        {
            // Lỗi phải chặn toàn bộ uncertainPath, không chỉ currentNode. Xe đang
            // chờ mutex cũ bị rút khỏi FIFO và chuyển BLOCKED nếu không có lối né.
            json j={{"map_name","Failure footprint"},{"nodes",json::array()}, {"edges",json::array()}};
            for (const auto* id : {"A","B","M","T","U"})
                j["nodes"].push_back({{"id",id},{"x",0},{"y",0},{"type",std::string(id)=="M"?"mutex":"waypoint"}});
            for (const auto& edge : {std::make_tuple("A","M",1),std::make_tuple("M","U",1),
                                     std::make_tuple("B","M",1),std::make_tuple("M","T",1)})
                j["edges"].push_back({{"from",std::get<0>(edge)},{"to",std::get<1>(edge)},{"weight",std::get<2>(edge)}});
            MapGraph map;map.loadFromJson(j);MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",80,false);fleet.updateTelemetry("02","B",80,false);
            CHECK(fleet.directGoto("01","U","failed-route").empty());
            CHECK(fleet.directGoto("02","T","affected-route").empty());
            CHECK(arbiter.requestZone("M","01"));CHECK(!arbiter.requestZone("M","02"));
            fleet.reportError("01","MOTOR_FAULT");
            auto robot=[&](const std::string& id){const auto snapshot=fleet.toJson();for(const auto& a:snapshot["agvs"])if(a["id"]==id)return a;throw std::runtime_error("missing footprint robot "+id);};
            CHECK(robot("01")["uncertain_path"]==json::array({"M","U"}));
            CHECK(robot("02")["state"]=="BLOCKED");CHECK(robot("02")["path"].empty());
            CHECK(arbiter.currentOwner("M")=="01");CHECK(arbiter.toJson()["M"]["waiting"].empty());
        }
        {
            // MQTT LWT cũng phải replan ngay. Trước sửa, reportOffline chỉ giữ
            // mutex/state và xe 02 tiếp tục GOTO cũ qua node X của xe mất mạng.
            json j={{"map_name","LWT reroute"},{"nodes",json::array()}, {"edges",json::array()}};
            for (const auto* id : {"A","B","X","Y","T"})
                j["nodes"].push_back({{"id",id},{"x",0},{"y",0},{"type","waypoint"}});
            for (const auto& edge : {std::make_tuple("B","X",1),std::make_tuple("X","T",1),
                                     std::make_tuple("B","Y",5),std::make_tuple("Y","T",5)})
                j["edges"].push_back({{"from",std::get<0>(edge)},{"to",std::get<1>(edge)},{"weight",std::get<2>(edge)}});
            MapGraph map;map.loadFromJson(j);MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",80,false);fleet.updateTelemetry("02","B",80,false);
            CHECK(fleet.directGoto("02","T","lwt-reroute").empty());
            fleet.updateTelemetry("01","X",80,false);fleet.reportOffline("01");
            auto robot=[&](const std::string& id){const auto snapshot=fleet.toJson();for(const auto& a:snapshot["agvs"])if(a["id"]==id)return a;throw std::runtime_error("missing LWT robot "+id);};
            CHECK(robot("02")["state"]=="WORKING");CHECK(robot("02")["path"]==json::array({"Y","T"}));
        }
        {
            // Một node trong cùng mutex component cũng là footprint vật lý cần né.
            json j={{"map_name","Mutex corridor reroute"},{"nodes",json::array()}, {"edges",json::array()}};
            for (const auto* id : {"A","B","M1","M2","Y","T"})
                j["nodes"].push_back({{"id",id},{"x",0},{"y",0},{"type",id[0]=='M'?"mutex":"waypoint"}});
            for (const auto& edge : {std::make_tuple("B","M2",1),std::make_tuple("M2","T",1),
                                     std::make_tuple("B","Y",5),std::make_tuple("Y","T",5),
                                     std::make_tuple("M1","M2",1)})
                j["edges"].push_back({{"from",std::get<0>(edge)},{"to",std::get<1>(edge)},{"weight",std::get<2>(edge)}});
            MapGraph map;map.loadFromJson(j);MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",80,false);fleet.updateTelemetry("02","B",80,false);
            CHECK(fleet.directGoto("02","T","corridor-reroute").empty());
            fleet.updateTelemetry("01","M1",80,false);fleet.reportError("01","LINE_LOST");
            auto robot=[&](const std::string& id){const auto snapshot=fleet.toJson();for(const auto& a:snapshot["agvs"])if(a["id"]==id)return a;throw std::runtime_error("missing corridor robot "+id);};
            CHECK(robot("02")["state"]=="WORKING");CHECK(robot("02")["path"]==json::array({"Y","T"}));
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;arbiter.registerZone("M");FleetManager fleet(map,arbiter);
            fleet.setOfflineTimeout(1);fleet.updateTelemetry("01","M",80,false);fleet.addTask("T",0,"lost");
            CHECK(arbiter.requestZone("M","01"));CHECK(!arbiter.requestZone("M","02"));
            std::this_thread::sleep_for(std::chrono::milliseconds(1100));fleet.tick();
            CHECK(arbiter.currentOwner("M") == "01");CHECK(record(fleet,"lost")["status"] == "interrupted");
            fleet.updateTelemetry("02","B",80,false);fleet.addTask("T",0,"blocked");
            CHECK(record(fleet,"blocked")["status"] == "pending"); // T remains physically uncertain.
            fleet.updateTelemetry("01","M",80,false);
            auto state = fleet.toJson();
            for (auto& a : state["agvs"]) if (a["id"] == "01") CHECK(a["state"] == "ERROR");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.setAckTimeout(1);fleet.setMaxRetries(0);fleet.updateTelemetry("01","M",80,false);fleet.addTask("T",0,"ack");
            std::this_thread::sleep_for(std::chrono::milliseconds(3));fleet.tick();
            CHECK(record(fleet,"ack")["status"] == "interrupted");CHECK(fleet.toJson()["pending_tasks"].empty());
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",80,false);fleet.setNetworkAvailable(false);fleet.addTask("T",0,"network");
            fleet.tick();CHECK(record(fleet,"network")["status"] == "pending");
            fleet.setNetworkAvailable(true);fleet.tick();CHECK(record(fleet,"network")["status"] == "dispatched");
        }
        {
            MapGraph map;map.loadFromJson(graph());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            fleet.updateTelemetry("01","A",80,false);
            fleet.setCommandIdAllocator([]()->long {throw std::runtime_error("Disk failure");});
            CHECK(!fleet.directGoto("01","T","disk").empty());CHECK(record(fleet,"disk")["status"] == "interrupted");
            fleet.reportCharged("01",90);CHECK(fleet.toJson()["agvs"][0]["state"] == "ERROR");
        }
        {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            auto dir = root/".qa"/("shift-unit-" + std::to_string(Clock::now().time_since_epoch().count()));
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            ShiftController shift(map,fleet,arbiter,dir/"shift.json");WebCommands web(map,fleet,arbiter);web.setShiftController(&shift);
            CHECK(fleet.toJson()["agvs"].empty());CHECK(!shift.snapshot()["can_start"].get<bool>());
            CHECK(!web.execute({{"cmd","start_shift"}})["ok"].get<bool>());
            fleet.updateTelemetry("01","G1",85,false);fleet.updateTelemetry("02","S2",80,false);shift.tick();
            auto robot = [&](const std::string& id) {const auto state=fleet.toJson();for (auto a : state["agvs"]) if (a["id"]==id) return a;throw std::runtime_error("Missing robot");};
            CHECK(robot("01")["node"]=="G1");CHECK(robot("01")["target"]=="S1");
            CHECK(shift.snapshot()["phase"]=="CLOSING");
            CHECK(web.execute({{"cmd","add_task"},{"task_id","closed"},{"target","K1"}})["status"]==409);
            fleet.updateTelemetry("01","S1",85,false);shift.tick();CHECK(shift.snapshot()["phase"]=="CLOSED");
            CHECK(web.execute({{"cmd","start_shift"}})["ok"].get<bool>());
            CHECK(fleet.directGoto("01","K3","last").empty());
            CHECK(fleet.directGoto("02","K4","other").empty());
            fleet.addTask("K6",0,"queued");CHECK(record(fleet,"queued")["status"]=="pending");
            CHECK(web.execute({{"cmd","end_shift"}})["ok"].get<bool>());
            CHECK(robot("01")["target"]=="K3");CHECK(record(fleet,"last")["status"]=="dispatched");
            CHECK(!fleet.directGoto("01","K1","late").empty());
            fleet.updateTelemetry("01","K3",85,false);shift.tick();
            CHECK(record(fleet,"last")["status"]=="completed");CHECK(record(fleet,"queued")["status"]=="pending");
            CHECK(robot("01")["target"]=="S1");CHECK(robot("01")["node"]=="K3");
            fleet.updateTelemetry("01","S1",85,false);
            fleet.tick(); // route reservations hold 02 until 01 finishes the shared approach home
            CHECK(robot("02")["state"]=="WORKING");
            fleet.updateTelemetry("02","K4",80,false);shift.tick();CHECK(robot("02")["target"]=="S2");
            arbiter.requestZone("M5","01");fleet.updateTelemetry("01","S1",85,false);fleet.updateTelemetry("02","S2",80,false);shift.tick();
            CHECK(shift.snapshot()["phase"]=="CLOSING");CHECK(!shift.snapshot()["can_start"].get<bool>());
            arbiter.releaseZone("M5","01");shift.tick();CHECK(shift.snapshot()["phase"]=="CLOSED");
            fleet.setNetworkAvailable(false);CHECK(!shift.start().empty());fleet.setNetworkAvailable(true);
            fleet.reportCharged("01",90);shift.tick();CHECK(robot("01")["state"]=="IDLE"); // không lặp CHARGE_START sau charging_done
            CHECK(web.execute({{"cmd","start_shift"}})["ok"].get<bool>());
            CHECK(record(fleet,"queued")["status"]=="dispatched");
            MapGraph rebootMap;rebootMap.loadDefaultDemoMap();MutexArbiter rebootArbiter(rebootMap.mutexZones());FleetManager rebootFleet(rebootMap,rebootArbiter);
            ShiftController reboot(rebootMap,rebootFleet,rebootArbiter,dir/"shift.json");
            CHECK(reboot.snapshot()["phase"]=="CLOSING");CHECK(rebootFleet.toJson()["agvs"].empty());CHECK(!rebootFleet.acceptingTasks());
        }
        {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            for (const std::string id : {"01", "02"}) {
                auto dir = root/".qa"/("one-known-shift-" + id + "-" + std::to_string(Clock::now().time_since_epoch().count()));
                MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
                fleet.setNetworkAvailable(true);
                std::vector<json> commands;
                fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
                ShiftController shift(map,fleet,arbiter,dir/"shift.json");
                const std::string station = id == "01" ? "S1" : "S2";
                const std::string other = id == "01" ? "02" : "01";
                const std::string otherStation = id == "01" ? "S2" : "S1";
                CHECK(!shift.snapshot()["can_start"].get<bool>());
                fleet.updateTelemetry(id,station,-1,false);shift.tick();
                auto status = shift.snapshot();
                CHECK(status["phase"]=="CLOSED");CHECK(status["can_start"].get<bool>());
                CHECK(status["home_stations"].size()==2);
                CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="CHARGE_START" || c.value("cmd","")=="GOTO";}));
                CHECK(shift.start().empty());
                CHECK(fleet.acceptingTasks());CHECK(fleet.toJson()["agvs"].size()==1);
                CHECK(!fleet.directGoto(id,"K1","unmeasured").empty()); // task pin gate remains unchanged
                CHECK(shift.end().empty());shift.tick();
                CHECK(shift.snapshot()["can_start"].get<bool>());
                fleet.updateTelemetry(id,station,85,false);shift.tick();
                CHECK(shift.snapshot()["can_start"].get<bool>());
                CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="CHARGE_START";}));
                fleet.updateTelemetry(id,station,25,false);shift.tick();
                CHECK(!shift.snapshot()["can_start"].get<bool>());
                fleet.updateTelemetry(id,station,-1,false);shift.tick();
                CHECK(shift.snapshot()["can_start"].get<bool>());
                fleet.updateTelemetry(other,otherStation,-1,false);shift.tick();
                CHECK(!shift.snapshot()["can_start"].get<bool>()); // two known robots need measured battery
            }
        }
        {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            auto dir = root/".qa"/("solo-shift-unit-" + std::to_string(Clock::now().time_since_epoch().count()));
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setSingleRobotTrial(true);fleet.setNetworkAvailable(true);
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{"01","S1"}},true);
            CHECK(!shift.snapshot()["can_start"].get<bool>());
            fleet.updateTelemetry("01","S1",-1,false);shift.tick();
            CHECK(shift.snapshot()["phase"]=="CLOSED");
            CHECK(shift.snapshot()["can_start"].get<bool>());
            CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="CHARGE_START";}));
            CHECK(shift.start().empty());
            CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
            CHECK(shift.end().empty());
            fleet.updateTelemetry("01","S1",10,false);shift.tick();
            CHECK(shift.snapshot()["can_start"].get<bool>()); // trial does not gate on battery
            fleet.updateTelemetry("01","S1",-1,false);shift.tick();
            CHECK(shift.snapshot()["can_start"].get<bool>());
            fleet.reportError("01","test");shift.tick();
            CHECK(!shift.snapshot()["can_start"].get<bool>());
            CHECK(shift.snapshot()["phase"]=="CLOSING");
            fleet.updateTelemetry("01","S1",-1,false);
            CHECK(fleet.clearError("01"));shift.tick();
            CHECK(shift.snapshot()["can_start"].get<bool>());
            fleet.updateTelemetry("02","S2",85,false);shift.tick();
            CHECK(!shift.snapshot()["can_start"].get<bool>());
            CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="CHARGE_START";}));
        }
        for (const auto* selected : {"01","02"}) {
            const std::string id = selected, other = id == "01" ? "02" : "01";
            const std::string home = id == "01" ? "S1" : "S2", otherHome = id == "01" ? "S2" : "S1";
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            auto dir = root/".qa"/("selected-trial-unit-" + id + std::to_string(Clock::now().time_since_epoch().count()));
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot(id);fleet.setNetworkAvailable(true);
            std::vector<std::pair<std::string,json>> commands;
            fleet.setPublishCallback([&](const std::string& topic,const std::string& payload){commands.push_back({topic,json::parse(payload)});});
            ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{id,home}},false,false,id);
            fleet.updateTelemetry(id,home,-1,false);shift.tick();
            CHECK(shift.snapshot()["can_start"] == true);
            CHECK(shift.snapshot()["trial_robot_id"] == id);CHECK(shift.snapshot()["supervised_trial"] == true);
            CHECK(shift.snapshot()["two_robot_trial"] == false);CHECK(shift.snapshot()["single_robot_trial"] == false);
            fleet.updateTelemetry(other,"UNKNOWN",-1,false);shift.tick();
            CHECK(shift.snapshot()["can_start"] == false);
            fleet.updateTelemetry(other,otherHome,10,false);shift.tick();
            CHECK(shift.snapshot()["can_start"] == true);CHECK(shift.start().empty());
            CHECK(!fleet.directGoto(other,"K3","wrong-robot").empty());
            fleet.addTask(id == "01" ? "GS1" : "M3",0,"selected-auto");
            CHECK(record(fleet,"selected-auto")["assigned_agv"] == id);
            CHECK(record(fleet,"selected-auto")["status"] == "dispatched");
            CHECK(std::none_of(commands.begin(),commands.end(),[&](const auto& entry){return
                entry.first == "agv/"+other+"/command" && (entry.second.value("cmd","") == "GOTO" || entry.second.value("cmd","") == "CHARGE_START");}));
            CHECK(arbiter.requestZone("M2",id));CHECK(!arbiter.requestZone("M2",other));
            fleet.reportOffline(id);CHECK(arbiter.currentOwner("M2") == id);
        }
        {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            auto dir = root/".qa"/("two-robot-trial-unit-" + std::to_string(Clock::now().time_since_epoch().count()));
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTwoRobotTrial(true);fleet.setNetworkAvailable(true);
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{"01","S1"},{"02","S2"}},false,true);
            fleet.updateTelemetry("01","S1",-1,false);shift.tick();
            CHECK(!shift.snapshot()["can_start"].get<bool>()); // both robots must be present
            fleet.updateTelemetry("02","S2",-1,false);shift.tick();
            CHECK(shift.snapshot()["two_robot_trial"] == true);
            CHECK(shift.snapshot()["can_start"].get<bool>());
            CHECK(shift.start().empty());
            CHECK(fleet.directGoto("01","K3","dual-pickup").empty());
            CHECK(fleet.directGoto("02","K4","dual-dropoff").empty());
            CHECK(record(fleet,"dual-pickup")["status"] == "dispatched");
            CHECK(record(fleet,"dual-dropoff")["status"] == "dispatched");
            CHECK(arbiter.requestZone("M1","01"));
            CHECK(!arbiter.requestZone("M1","02"));
            fleet.updateTelemetry("01","GS1",-1,false); // leaving a tag read is not zone clearance
            CHECK(arbiter.currentOwner("M1") == "01");
            fleet.reportNetworkLoss();
            CHECK(arbiter.currentOwner("M1") == "01");
            CHECK(record(fleet,"dual-pickup")["status"] == "interrupted");
            CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="CHARGE_START";}));
        }
        {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            auto dir = root/".qa"/("shift-blocked-" + std::to_string(Clock::now().time_since_epoch().count()));
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{"01","S1"}});
            map.setObstacle("GS1",true);fleet.updateTelemetry("01","K3",80,false);shift.tick();
            CHECK(fleet.toJson()["agvs"][0]["state"]=="BLOCKED");CHECK(!shift.snapshot()["can_start"].get<bool>());
            map.setObstacle("GS1",false);fleet.tick();CHECK(fleet.toJson()["agvs"][0]["target"]=="S1");
            fleet.reportError("01","test");shift.tick();CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            CHECK(!shift.start().empty());
            fleet.updateTelemetry("01","S1",80,false);CHECK(fleet.clearError("01"));shift.tick();CHECK(shift.start().empty());
            fleet.updateTelemetry("01","K3",10,false);CHECK(fleet.toJson()["agvs"][0]["target"]=="S1"); // trạm riêng, không chọn S2 gần hơn
        }
        {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            auto dir = root/".qa"/("shift-offline-" + std::to_string(Clock::now().time_since_epoch().count()));
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            ShiftController shift(map,fleet,arbiter,dir/"shift.json",{{"01","S1"}});
            fleet.updateTelemetry("01","INVALID",80,false);shift.tick();CHECK(!shift.snapshot()["all_home"].get<bool>());
            CHECK(fleet.toJson()["agvs"][0]["target"]==""); // không suy vị trí từ trạm mặc định
            fleet.updateTelemetry("01","S1",80,false);shift.tick();CHECK(shift.snapshot()["phase"]=="CLOSED");
            fleet.setOfflineTimeout(1);std::this_thread::sleep_for(std::chrono::milliseconds(1100));fleet.tick();shift.tick();
            CHECK(!shift.snapshot()["can_start"].get<bool>());CHECK(shift.snapshot()["phase"]=="CLOSING");
            fleet.updateTelemetry("01","S1",80,false);shift.tick();CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            CHECK(!shift.start().empty());
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            std::vector<json> commands;fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            fleet.updateTelemetry("01","UNKNOWN",85,false);
            CHECK(fleet.toJson()["agvs"][0]["state"]=="UNLOCALIZED");CHECK(fleet.toJson()["agvs"][0]["node"]=="");
            CHECK(!fleet.toJson()["agvs"][0]["position_valid"].get<bool>());CHECK(commands.size()==1);
            fleet.updateTelemetry("01","UNKNOWN",85,false);CHECK(commands.size()==1); // heartbeat không spam command
            fleet.updateTelemetry("01","G1",85,false);CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
            CHECK(fleet.directGoto("01","K4","rfid-lost").empty());arbiter.requestZone("M5","01");
            fleet.updateTelemetry("01","UNKNOWN",85,false);
            CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");CHECK(fleet.toJson()["agvs"][0]["node"]=="G1");
            CHECK(record(fleet,"rfid-lost")["status"]=="interrupted");CHECK(!fleet.toJson()["agvs"][0]["uncertain_path"].empty());
            CHECK(arbiter.currentOwner("M5")=="01");CHECK(!fleet.clearError("01"));
            fleet.updateTelemetry("01","M5",85,false);CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            CHECK(map.mutexComponent("M5")==std::unordered_set<std::string>({"M5","M6"}));
            CHECK(map.mutexComponent("M1")==std::unordered_set<std::string>({"M1"}));
            fleet.updateTelemetry("01","S1",85,false);fleet.updateTelemetry("02","S2",85,false);
            CHECK(fleet.directGoto("01","K3","reserve-one").empty());
            CHECK(fleet.directGoto("02","K4","reserve-two").empty());
            auto robot = [&](const std::string& id) {const auto snapshot=fleet.toJson();for (auto a : snapshot["agvs"]) if (a["id"]==id) return a;throw std::runtime_error("Missing robot");};
            CHECK(robot("02")["state"]=="BLOCKED");CHECK(robot("02")["path"].empty());
            fleet.updateTelemetry("01","K3",85,false);fleet.tick();CHECK(robot("02")["state"]=="WORKING");
            fleet.reportNetworkLoss();CHECK(record(fleet,"reserve-two")["status"]=="interrupted");
            CHECK(robot("02")["state"]=="ERROR");CHECK(!robot("02")["uncertain_path"].empty());
            fleet.setNetworkAvailable(true);fleet.updateTelemetry("02","S2",85,false);fleet.tick();
            CHECK(robot("02")["state"]=="ERROR"); // reconnect is not permission to move
            CHECK(fleet.clearError("02"));
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            long grantId = 100;arbiter.setGrantIdAllocator([&]{return ++grantId;});
            fleet.setTwoRobotTrial(true);fleet.setHomeStations({{"01","S1"},{"02","S2"}});
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string& topic,const std::string& payload) {
                if (topic.find("/command") != std::string::npos) commands.push_back(json::parse(payload));
            });
            CHECK(map.undirectedDistance("S1","M2")==70);
            CHECK(map.undirectedDistance("S2","M3")==70);
            CHECK(map.straightLineDistance("S1","M2") >= 50.0);
            CHECK(map.straightLineDistance("S1","GS1") < 50.0);
            fleet.updateTelemetry("01","S1",-1,false);fleet.updateTelemetry("02","S2",-1,false);
            CHECK(arbiter.requestZone("M2","01"));
            CHECK(!arbiter.requestZone("M2","02"));
            fleet.reportError("02","LINE_LOST");
            CHECK(arbiter.toJson()["M2"]["waiting"].empty()); // loi bo hang doi, khong chiem zone
            fleet.reportOffline("01");
            CHECK(arbiter.currentOwner("M2")=="01"); // LWT khong chung minh da roi M2
            fleet.updateTelemetry("01","M2",-1,false);
            MotionDiagnostic fault{"M2","AGV_SAFE_STOP","NETWORK_UNAVAILABLE","STOP",false,true,true,0,0,0};
            fleet.observeMotionDiagnostic("01",fault);
            CHECK(arbiter.currentOwner("M2")=="01");
            fleet.updateTelemetry("01","S1",-1,false);
            CHECK(arbiter.currentOwner("M2")=="01"); // telemetry don le khong nhả quyen
            fault.node="S1";fault.leftDuty=1;fleet.observeMotionDiagnostic("01",fault);
            CHECK(arbiter.currentOwner("M2")=="01"); // chi lenh STOP nhung PWM chua ve 0
            fault.leftDuty=0;fleet.observeMotionDiagnostic("01",fault);
            CHECK(arbiter.currentOwner("M2").empty());
            auto robot = [&](const std::string& id) {
                const auto snapshot = fleet.toJson();
                for (const auto& a : snapshot["agvs"]) if (a["id"]==id) return a;
                throw std::runtime_error("missing " + id);
            };
            CHECK(robot("01")["state"]=="ERROR");
            const auto resetCount = [&]{return std::count_if(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="RESET_FAULT";});};
            CHECK(resetCount()==0);
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
            fleet.observeMotionDiagnostic("01",fault);CHECK(resetCount()==1);
            fleet.observeMotionDiagnostic("01",fault);CHECK(resetCount()==1);
            MotionDiagnostic healthy{"S1","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
            fleet.observeMotionDiagnostic("01",healthy);
            CHECK(robot("01")["state"]=="IDLE");CHECK(robot("01")["error"]=="");

            CHECK(arbiter.requestZone("M3","02"));
            fleet.updateTelemetry("02","S2",-1,false);
            MotionDiagnostic physicalFault{"S2","AGV_SAFE_STOP","LINE_LOST","STOP",false,true,true,0,0,0};
            fleet.observeMotionDiagnostic("02",physicalFault);
            CHECK(arbiter.currentOwner("M3").empty()); // da o tram, nhung loi MCU van giu
            CHECK(resetCount()==1);
            CHECK(robot("02")["state"]=="ERROR");
            MotionDiagnostic running{"S2","AGV_MOVING","NONE","FORWARD",true,true,true,0,0,0};
            fleet.observeMotionDiagnostic("02",running);CHECK(robot("02")["state"]=="ERROR");
            fleet.observeMotionDiagnostic("02",MotionDiagnostic{"S2","AGV_IDLE","NONE","STOP",false,true,true,500,0,0});
            CHECK(robot("02")["state"]=="ERROR"); // RFID cu khong mo khoa
            fleet.observeMotionDiagnostic("02",MotionDiagnostic{"S2","AGV_IDLE","NONE","STOP",false,true,true,0,1,0});
            CHECK(robot("02")["state"]=="ERROR"); // duty con khac 0
            fleet.observeMotionDiagnostic("02",MotionDiagnostic{"S2","AGV_IDLE","NONE","STOP",false,true,true,0,0,0});
            CHECK(robot("02")["state"]=="IDLE");

            arbiter.registerZone("GS1"); // zone gia lap chi 20 cm cach S1
            CHECK(arbiter.requestZone("GS1","01"));
            fleet.reportError("01","LINE_LOST");
            fleet.updateTelemetry("01","S1",-1,false);
            fleet.observeMotionDiagnostic("01",healthy);
            CHECK(arbiter.currentOwner("GS1")=="01");
            CHECK(robot("01")["state"]=="ERROR");
        }
        {
            MapGraph map;map.loadDefaultDemoMap();
            auto layout = map.toJson();
            for (auto& node : layout["nodes"]) if (node["id"] == "S1") {
                node["x"] = 149;node["y"] = 0;
            }
            map.loadFromJson(layout);
            CHECK(map.undirectedDistance("S1","M2") == 70);
            CHECK(map.straightLineDistance("S1","M2") < 50.0);
            MutexArbiter arbiter(map.mutexZones());arbiter.setGrantIdAllocator([] {return 1200L;});
            FleetManager fleet(map,arbiter);fleet.setTwoRobotTrial(true);
            fleet.setHomeStations({{"01","S1"},{"02","S2"}});
            fleet.updateTelemetry("01","S1",-1,false);
            CHECK(arbiter.requestZone("M2","01"));
            fleet.reportError("01","LINE_LOST");
            fleet.observeMotionDiagnostic("01",MotionDiagnostic{"S1","AGV_IDLE","NONE","STOP",false,true,true,0,0,0});
            CHECK(arbiter.currentOwner("M2") == "01"); // map co duong dai nhung tram sat zone
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("02");fleet.setHomeStations({{"02","S2"}});
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            fleet.updateTelemetry("02","GS2",-1,false);fleet.reportError("02","NETWORK_UNAVAILABLE");
            fleet.addTask("M3",0,"after-reset");commands.clear();
            CHECK(!fleet.clearError("02")); // RFID GS2 after reboot does not establish heading.
            CHECK(commands.empty());CHECK(record(fleet,"after-reset")["status"]=="pending");
            fleet.updateTelemetry("02","S2",-1,false);CHECK(fleet.clearError("02"));
            CHECK(commands.size()==1);CHECK(commands[0]["cmd"]=="RESET_FAULT");
            CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            fleet.reportAck("02",commands[0]["cmd_id"].get<long>());fleet.tick();
            CHECK(commands.size()==1); // Receipt ACK does not prove RESET_FAULT succeeded.
            fleet.observeMotionDiagnostic("02",MotionDiagnostic{"S2","AGV_SAFE_STOP","NETWORK_UNAVAILABLE","STOP",false,true,true,0,0,0});
            CHECK(commands.size()==1);
            fleet.observeMotionDiagnostic("02",MotionDiagnostic{"S2","AGV_IDLE","NONE","STOP",false,true,true,0,0,0});
            CHECK(record(fleet,"after-reset")["status"]=="dispatched");
            CHECK(commands.back()["cmd"]=="GOTO");CHECK(commands.back()["path"]==json::array({"GS2","M3"}));
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("01");fleet.setHomeStations({{"01","S1"}});
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            fleet.updateTelemetry("01","S1",-1,false);
            MotionDiagnostic d{"S1","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
            d.headingSupported=true;d.headingNode="S1";d.heading="S";d.headingSource="bay_default";
            fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.toJson()["agvs"][0]["heading"]=="S");
            CHECK(fleet.toJson()["agvs"][0]["heading_source"]=="bay_default");
            CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c["cmd"]=="SET_HEADING";}));
            d.headingSource="operator";fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.toJson()["agvs"][0]["heading"]=="UNKNOWN");
            d.headingSource="route";d.headingNode="GS1";fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.toJson()["agvs"][0]["heading"]=="UNKNOWN");
            d.headingNode="S1";fleet.observeMotionDiagnostic("01",d);
            CHECK(fleet.toJson()["agvs"][0]["heading"]=="S");
            fleet.reportOffline("01");CHECK(fleet.toJson()["agvs"][0]["heading"]=="UNKNOWN");
        }
        {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("01");fleet.setHomeStations({{"01","S1"}});
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            fleet.updateTelemetry("01","GS1",-1,false);
            MotionDiagnostic d{"GS1","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
            d.headingSupported=true;d.stationDockingSupported=true;d.headingNode="GS1";d.heading="N";d.headingSource="route";
            fleet.observeMotionDiagnostic("01",d);
            fleet.addTask("S1",0,"station-return");
            CHECK(record(fleet,"station-return")["status"]=="dispatched");
            fleet.reportAck("01",commands.back()["cmd_id"].get<long>());
            fleet.updateTelemetry("01","S1",-1,false);
            CHECK(record(fleet,"station-return")["status"]=="dispatched"); // RFID/ACK cannot finish the parking turn.
            const auto commandCount=commands.size();
            CHECK(!fleet.directGoto("01","GS1","interrupt-parking").empty());
            CHECK(commands.size()==commandCount);
            d.node=d.headingNode="S1";d.heading="S";d.headingSource="bay_default";d.stationReady=true;
            d.taskActive=true;d.state="AGV_MOVING";d.motor="PIVOT_RIGHT";d.leftDuty=d.rightDuty=245;
            fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return")["status"]=="dispatched");
            d.taskActive=false;d.state="AGV_IDLE";d.motor="STOP";d.leftDuty=d.rightDuty=0;
            d.rfidAgeMs=UINT32_MAX;fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return")["status"]=="dispatched");
            d.rfidAgeMs=0;d.heading="N";fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return")["status"]=="dispatched");
            d.heading="S";d.error="TURN_TIMEOUT";fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return")["status"]=="dispatched");
            d.error="NONE";d.rfidAgeMs=175278;fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return")["status"]=="completed");
            CHECK(fleet.toJson()["agvs"][0]["state"]=="IDLE");
            // A stopped ready reading from the previous visit must not complete a new return.
            fleet.addTask("GS1",0,"depart-again");fleet.updateTelemetry("01","GS1",-1,false);
            fleet.addTask("S1",0,"station-return-again");fleet.updateTelemetry("01","S1",-1,false);
            CHECK(record(fleet,"station-return-again")["status"]=="dispatched");
            d.stationDockingSupported=false;fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return-again")["status"]=="dispatched");
            d.stationDockingSupported=true;fleet.observeMotionDiagnostic("01",d);
            CHECK(record(fleet,"station-return-again")["status"]=="completed");
        }
        {
            MapGraph map;map.loadFromFile("data/map_layout.json");MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot("01");std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& p){commands.push_back(json::parse(p));});
            fleet.updateTelemetry("01","S1",-1,false);fleet.addTask("M2",0,"node-orders");
            const long routeId=commands.back()["cmd_id"].get<long>();
            MotionDiagnostic d{"S1","AGV_WAITING","NONE","STOP",true,true,true,0,0,0};
            d.headingSupported=true;d.heading="S";d.headingNode="S1";d.headingSource="bay_default";
            d.nodeOrdersSupported=true;d.awaitingNodeOrder=true;d.routeCommandId=routeId;d.pathIndex=0;d.nextNode="GS1";
            d.rfidAgeMs=5000; // A stopped confirmed start remains usable after the operator waits to click GOTO.
            const auto count=commands.size();d.routeCommandId=routeId+1;fleet.observeMotionDiagnostic("01",d);
            CHECK(commands.size()==count);d.routeCommandId=routeId;
            fleet.observeMotionDiagnostic("01",d);CHECK(commands.back()["cmd"]=="NODE_ORDER");
            CHECK(commands.back()["maneuver"]=="STRAIGHT");const auto first=commands.back();
            fleet.observeMotionDiagnostic("01",d);CHECK(commands.size()==count+1); // No new ID/action on a duplicate request.
            fleet.reportAck("01",first["cmd_id"].get<long>());
            fleet.updateTelemetry("01","GS1",-1,false);d.node=d.headingNode="GS1";d.pathIndex=1;d.nextNode="M2";d.headingSource="route";
            d.rfidAgeMs=UINT32_MAX;fleet.observeMotionDiagnostic("01",d);CHECK(commands.back()==first);
            d.rfidAgeMs=0;d.motor="FORWARD";fleet.observeMotionDiagnostic("01",d);CHECK(commands.back()==first);
            d.motor="STOP";fleet.observeMotionDiagnostic("01",d);
            CHECK(commands.back()["maneuver"]=="LEFT");CHECK(commands.back()["node"]=="GS1");
            CHECK(commands.back()["path_index"]==1);CHECK(commands.back()["route_cmd_id"]==routeId);
            const auto turnCommandCount=commands.size();
            CHECK(!fleet.directGoto("01","G1","override-turn").empty());CHECK(commands.size()==turnCommandCount);
            fleet.reportAck("01",commands.back()["cmd_id"].get<long>());
            CHECK(record(fleet,"node-orders")["status"]=="dispatched"); // Receipt does not prove the turn or arrival.
            fleet.updateTelemetry("01","M2",-1,false);CHECK(record(fleet,"node-orders")["status"]=="completed");
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","S1","W")["maneuver"]=="RIGHT");
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","S1","E",false,true)["maneuver"]=="RIGHT");
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","S1","W",false,true)["maneuver"]=="LEFT");
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","S1","N",false,true)["turn_crossings"]==2);
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","S1","S",false,true)["maneuver"]=="STRAIGHT");
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","S1","S",false,true)["reverse_after_turn"]==true);
            CHECK(NodeManeuver::plan(map.toJson(),"GS2","S2","E",false,true)["reverse_after_turn"]==true);
            CHECK(NodeManeuver::plan(map.toJson(),"GS2","S2","E",false,true)["maneuver"]=="LEFT");
            CHECK(NodeManeuver::plan(map.toJson(),"GS2","S2","W",false,true)["maneuver"]=="RIGHT");
            CHECK(NodeManeuver::plan(map.toJson(),"S1","S1","N",true)["maneuver"]=="STATION_TURN_RIGHT");
            CHECK(NodeManeuver::plan(map.toJson(),"GS1","M4","S").is_null()); // No axis-only shortcut to an unconnected node.
        }
#if defined(_WIN32)
        {
            const auto dir=std::filesystem::path(".qa")/("atomic-lock-"+std::to_string(Clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(dir);const auto file=dir/"runtime.json";
            RuntimeStore store(file);json sample={{"version",1},{"fleet",json::object()},{"mutex",json::object()},{"test",1}};
            store.save(sample);
            HANDLE handle=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            CHECK(handle!=INVALID_HANDLE_VALUE);
            std::thread unlock([handle]{std::this_thread::sleep_for(std::chrono::milliseconds(650));CloseHandle(handle);});
            sample["test"]=2;
            try {store.save(sample);} catch (...) {unlock.join();throw;}
            unlock.join();CHECK(store.load()->at("test")==2);
            handle=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            CHECK(handle!=INVALID_HANDLE_VALUE);sample["test"]=3;bool failed=false;
            try {store.save(sample);} catch(const std::exception&){failed=true;}
            CloseHandle(handle);CHECK(failed);CHECK(store.load()->at("test")==2);
            store.save(sample);CHECK(store.load()->at("test")==3); // Failure did not poison the last saved value or delete the old checkpoint.
        }
#endif
        {
            MapGraph map;
            map.loadFromJson({{"nodes",{{{"id","A"}},{{"id","B"}},{{"id","C"}},{{"id","D"}}}},
                {"edges",{{{"from","A"},{"to","B"},{"weight",1},{"two_way",true}},
                {{"from","B"},{"to","D"},{"weight",1}},{{"from","A"},{"to","C"},{"weight",3}},
                {{"from","C"},{"to","D"},{"weight",3}}}}});
            CHECK(map.findShortestPath("A","D")==std::vector<std::string>({"A","B","D"}));
            map.setEdgeObstacle("B","A",true);
            CHECK(map.findShortestPath("A","D")==std::vector<std::string>({"A","C","D"}));
            CHECK(map.findShortestPath("B","A").empty());
            CHECK(map.findShortestPath("B","D")==std::vector<std::string>({"B","D"}));
            MapGraph copy;copy.loadFromJson(map.toJson());CHECK(copy.findShortestPath("A","D")==map.findShortestPath("A","D"));
            auto invalid=map.toJson();invalid["blocked_edges"].push_back({{"from","A"},{"to","D"}});
            bool rejected=false;try{map.loadFromJson(invalid);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
            CHECK(map.findShortestPath("A","D")==copy.findShortestPath("A","D"));
            map.setEdgeObstacle("A","B",false);CHECK(map.findShortestPath("A","D")==std::vector<std::string>({"A","B","D"}));
            MutexArbiter arbiter;FleetManager fleet(map,arbiter);WebCommands commands(map,fleet,arbiter);
            CHECK(commands.execute({{"cmd","sim_edge_obstacle"},{"from","A"},{"to","B"},{"blocked",true}})["status"]==403);
            commands.setSimulationMode(true);CHECK(commands.execute({{"cmd","sim_edge_obstacle"},{"from","A"},{"to","B"},{"blocked",true}})["ok"]==true);
        }
        std::cout << "PASS core: routes/mutex/task/ACK/offline/shift/node orders; checkpoint recovery; edge obstacles reroute, roundtrip and simulation gate\n";
    } catch (const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
