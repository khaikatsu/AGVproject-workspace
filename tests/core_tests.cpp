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
int main() {
    try {
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
            CHECK(!fleet.directGoto("01","C3","trial-busy").empty());
            fleet.updateTelemetry("01","GS1",-1,false);
            fleet.updateTelemetry("01","M2",-1,false);
            CHECK(record(fleet,"trial-s1-m2")["status"] == "completed");
            CHECK(fleet.toJson()["agvs"][0]["battery_measured"] == false);
            fleet.updateBatteryDisplay("01",67);
            CHECK(fleet.toJson()["agvs"][0]["battery_display_percent"] == 67);
            fleet.updateTelemetry("01","M2",10,false);
            CHECK(fleet.toJson()["agvs"][0]["battery"] == 10);
            CHECK(fleet.directGoto("01","C3","trial-m2-c3").empty());
            fleet.updateTelemetry("02","S2",80,false);
            CHECK(record(fleet,"trial-m2-c3")["status"] == "interrupted");
            CHECK(fleet.toJson()["agvs"].size() == 2);
            CHECK(!fleet.directGoto("01","C3","trial-other-present").empty());
        }
        {
            MapGraph map;map.loadDefaultDemoMap();
            CHECK(map.toJson()["nodes"].size() == 26);CHECK(map.mutexZones().size() == 6);
            auto horizontal = map.findShortestPath("G1","G6");
            CHECK((horizontal == std::vector<std::string>{"G1","M5","M6","G6"}));
            CHECK(map.pathCost(horizontal) == 200);
            CHECK((map.findShortestPath("M1","M3") == std::vector<std::string>{"M1","G2","M5","G3","M3"}));
            CHECK((map.findShortestPath("M4","M2") == std::vector<std::string>{"M4","G5","M6","G4","M2"}));
            CHECK(map.findShortestPath("M1","K3",{"M5"}).empty());
            CHECK(!map.findShortestPath("M5","G1").empty()); // vòng ngoài nay đã khép
            CHECK((map.findShortestPath("C3","C1") == std::vector<std::string>{"C3","G1","C1"}));
            for (auto home : {"S1","S2"}) for (auto warehouse : {"K1","K2","K3","K4","K5","K6"}) {
                CHECK(!map.findShortestPath(home,warehouse).empty());
                CHECK(!map.findShortestPath(warehouse,home).empty());
            }
            const auto expected = comparableMap(map);
            std::unordered_map<std::string,json> nodes;
            for (auto& n : expected["nodes"]) nodes[n["id"].get<std::string>()] = n;
            for (auto& e : expected["edges"]) {
                auto a = nodes.at(e["from"].get<std::string>()),b = nodes.at(e["to"].get<std::string>());
                CHECK(e["weight"].get<int>() == std::lround(std::hypot(a["x"].get<double>()-b["x"].get<double>(),a["y"].get<double>()-b["y"].get<double>())));
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
            std::ofstream out(file);out << "invalid";out.close();
            bool rejected = false;try {CommandIds ids(file);} catch (...) {rejected = true;}CHECK(rejected);
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
        std::cout << "PASS core: maps/routes/mutex/task lifecycle/ACK/offline/shift; route reservations, broker-loss interruption, UNKNOWN localization and safe home recovery\n";
    } catch (const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
