#include "commands/WebCommands.hpp"
#include "core/RuntimeStore.hpp"
#include <iostream>
#include <thread>
#define CHECK(x) do {if(!(x)) throw std::runtime_error("Failed: " #x);} while(false)

json robot(FleetManager& fleet,const std::string& id) {
    const auto state=fleet.toJson();
    for(const auto& a:state.at("agvs")) if(a.at("id")==id) return a;
    throw std::runtime_error("Missing robot");
}
json smallMap() {
    return {{"nodes",json::array({{{"id","A"},{"x",0},{"y",0}},{{"id","B"},{"x",100},{"y",0}},
        {{"id","C"},{"x",200},{"y",0}},{{"id","D"},{"x",100},{"y",100}},{{"id","X"},{"x",300},{"y",0}}})},
        {"edges",json::array({{{"from","A"},{"to","B"},{"weight",100}},{{"from","B"},{"to","C"},{"weight",100}},
        {{"from","D"},{"to","B"},{"weight",100}},{{"from","D"},{"to","X"},{"weight",100}}})}};
}
MotionDiagnostic stopped(const std::string& node,uint32_t uptime=200) {
    MotionDiagnostic d;d.node=node;d.state="AGV_IDLE";d.error="NONE";d.motor="STOP";
    d.taskActive=false;d.wifi=d.mqtt=true;d.rfidAgeMs=0;d.leftDuty=d.rightDuty=0;
    d.encoderBoot=10;d.encoderConnection=1;d.encoderUptime=uptime;return d;
}
int main() {
    try {
        const auto folder=std::filesystem::path(".qa")/("hardening-unit-"+std::to_string(Clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder);
        for(const std::string id:{"01","02"}) {
            const auto other=id=="01"?"02":"01";
            MapGraph map;map.loadFromJson(smallMap());MutexArbiter arbiter({"B"});FleetManager fleet(map,arbiter);
            fleet.setAutomaticChargingEnabled(false);fleet.setAckTimeout(60000);
            std::vector<json> commands;std::vector<std::string> logs;
            fleet.setPublishCallback([&](const auto&,const auto& p){commands.push_back(json::parse(p));});
            fleet.setEventCallback([&](const auto& s){logs.push_back(s);});
            fleet.updateTelemetry(id,"A",80,false);CHECK(fleet.bindEncoderSession(id,10,1,100));
            CHECK(fleet.directGoto(id,"C","running").empty());CHECK(arbiter.requestZone("B",id));
            fleet.addTask("C",1,"next");CHECK(fleet.cancelTask("","running")==1);
            CHECK(commands.back().at("cmd")=="WAIT");
            CHECK(robot(fleet,id).at("state")=="BLOCKED");CHECK(robot(fleet,id).at("cancel_waiting_for_node")==true);
            CHECK(robot(fleet,id).at("uncertain_path")==json::array({"B","C"}));
            const auto waitCommand=commands.back().at("cmd_id").get<long>();
            fleet.reportAck(id,waitCommand);fleet.tick();CHECK(commands.back().at("cmd")=="WAIT");
            CHECK(!fleet.directGoto(id,"C","must-not-overwrite").empty());
            fleet.updateTelemetry(other,"D",80,false);CHECK(!fleet.directGoto(other,"C","cross-cancelled-path").empty());
            auto proof=stopped("A",100);fleet.observeMotionDiagnostic(id,proof); // Older than WAIT.
            CHECK(robot(fleet,id).at("cancel_waiting_for_node")==true);
            proof=stopped("A");proof.rfidAgeMs=251;fleet.observeMotionDiagnostic(id,proof);
            proof.rfidAgeMs=0;proof.taskActive=true;fleet.observeMotionDiagnostic(id,proof);
            proof.taskActive=false;proof.motor="FORWARD";fleet.observeMotionDiagnostic(id,proof);
            proof.motor="STOP";proof.lastCommandId=waitCommand-1;fleet.observeMotionDiagnostic(id,proof);
            CHECK(robot(fleet,id).at("cancel_waiting_for_node")==true);
            RuntimeStore store(folder/(id+"-runtime.json"));store.save({{"version",1},{"fleet",fleet.durableSnapshot()},{"mutex",arbiter.toJson()}});
            FleetManager restored(map,arbiter);restored.restore(store.load().value().at("fleet"));
            CHECK(!robot(restored,id).at("uncertain_path").empty());CHECK(robot(restored,id).at("state")=="OFFLINE");
            proof.lastCommandId=waitCommand;fleet.observeMotionDiagnostic(id,proof);
            CHECK(robot(fleet,id).at("cancel_waiting_for_node")==false);
            CHECK(robot(fleet,id).at("uncertain_path").empty());
            CHECK(commands.back().at("cmd")=="GOTO");CHECK(commands.back().at("target")=="C");
            CHECK(arbiter.currentOwner("B")==id); // Node proof never invents mutex clearance.

            const auto durable=fleet.durableSnapshot();
            auto heading=stopped("A",300);heading.headingSupported=true;heading.heading="E";heading.headingNode="A";
            heading.headingSource="route";fleet.observeMotionDiagnostic(id,heading);
            CHECK(fleet.durableSnapshot()==durable); // Live heading/capability cannot dirty disk.
            for(uint32_t boot=11;boot<=74;++boot) CHECK(fleet.bindEncoderSession(id,boot,1,100));
            CHECK(!fleet.bindEncoderSession(id,75,1,100));CHECK(!fleet.bindEncoderSession(id,76,1,100));
            CHECK(robot(fleet,id).at("encoder_session_limit_reached")==true);
            CHECK(std::count_if(logs.begin(),logs.end(),[](const auto& s){return s.find("64 lan")!=std::string::npos;})==1);
        }
        {
            MapGraph map;map.loadDefaultDemoMap();const auto revision=map.toJson().at("revision").get<unsigned long>();
            map.loadDefaultDemoMap();CHECK(map.toJson().at("revision").get<unsigned long>()>revision);
            PhysicalLayout::validate(map.toJson());auto renamed=map.toJson();
            for(auto& n:renamed["nodes"]) if(n["id"]=="S1") n["id"]="Renamed";
            bool rejected=false;try{PhysicalLayout::validate(renamed);}catch(const std::exception&){rejected=true;}CHECK(rejected);
            const auto full=map.toJson();const auto edge=map.edgeGeometry("G1","M1");CHECK(!edge.is_null());
            CHECK(edge.at("nodes").size()==2);CHECK(edge.at("edges").size()==1);
            CHECK(EdgeGeometry::point(full,edge.at("edges").at(0),"G1",100)==
                EdgeGeometry::point(edge,edge.at("edges").at(0),"G1",100));
            CHECK(map.edgeGeometry("M1","G1").is_null());
            map.setObstacle("G1",true,30);map.setObstacle("G2",true,0);
            const auto live=map.toJson();map.saveToFile((folder/"map.json").string());CHECK(map.toJson()==live);
            MapGraph saved;saved.loadFromFile((folder/"map.json").string());CHECK(!saved.hasObstacle("G1"));CHECK(saved.hasObstacle("G2"));
            CHECK(!saved.toJson().contains("obstacle_deadlines"));
        }
        {
            MapGraph map;map.loadFromJson(smallMap());MutexArbiter arbiter;FleetManager fleet(map,arbiter);WebCommands web(map,fleet,arbiter);
            int ids=0;fleet.setTaskIdAllocator([&]{return "allocated-"+std::to_string(++ids);});
            for(const auto& input:{json{{"cmd","add_task"},{"target","C"},{"priority","bad"}},
                json{{"cmd","add_task"},{"target","C"},{"priority",4294967296ULL}},
                json{{"cmd","add_task"},{"target","missing"}},json{{"cmd","direct_goto"},{"target","C"},{"agv_id","bad/id"}}}) {
                CHECK(web.execute(input).at("ok")==false);CHECK(ids==0);
            }
        }
        {
            MutexArbiter arbiter({"B"});arbiter.setBlockedZones({"B"});CHECK(!arbiter.requestZone("B","01"));
            CHECK(arbiter.toJson().at("B").at("waiting").empty());arbiter.setBlockedZones({});
            CHECK(arbiter.requestZone("B","01")); // Mirrors firmware's periodic retry after unblock.
        }
        {
            MapGraph map;map.loadFromJson(smallMap());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            for(int i=0;i<1000;++i) fleet.addTask("X",0,"unreachable-"+std::to_string(i));
            fleet.updateTelemetry("01","A",80,false);
            const auto scans=fleet.toJson().at("planning").at("dispatch_scans").get<std::uint64_t>();
            CHECK(scans==1);
            for(int i=0;i<20;++i) fleet.updateTelemetry("01","A",80,false);
            CHECK(fleet.toJson().at("planning").at("dispatch_scans").get<std::uint64_t>()<=scans+1);
            CHECK(fleet.toJson().at("pending_tasks").size()==1000);
            fleet.updateTelemetry("01","D",80,false); // A changed origin forces a prompt rescan.
            CHECK(robot(fleet,"01").at("state")=="WORKING");
        }
        {
            MapGraph map;map.loadFromJson(smallMap());MutexArbiter arbiter;FleetManager fleet(map,arbiter);
            std::vector<std::string> logs;fleet.setEventCallback([&](const auto& s){logs.push_back(s);});
            fleet.setAckTimeout(60000);fleet.updateTelemetry("01","A",80,false);fleet.updateTelemetry("02","D",80,false);
            fleet.bindEncoderSession("01",10,1,100);fleet.bindEncoderSession("02",10,1,100);
            for(const std::string id:{"01","02"}) {
                CHECK(fleet.directGoto(id,id=="01"?"C":"X","wait-"+id).empty());
                CHECK(fleet.cancelTask("","wait-"+id)==1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10100));
            fleet.updateTelemetry("01","A",80,false);fleet.updateTelemetry("02","D",80,false);
            fleet.tick();fleet.tick();
            CHECK(std::count_if(logs.begin(),logs.end(),[](const auto& s){return s.find("[CANH BAO]")!=std::string::npos;})==2);
        }
#if defined(_WIN32)
        {
            const auto path=folder/"locked.json",temporary=folder/"locked.tmp";
            {std::ofstream out(path);out<<"old";}{std::ofstream out(temporary);out<<"new";}
            const auto handle=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            CHECK(handle!=INVALID_HANDLE_VALUE);
            const auto slow=AtomicCommitMetrics::slowCount.load();
            std::thread unlock([handle]{std::this_thread::sleep_for(std::chrono::milliseconds(650));CloseHandle(handle);});
            try{durableReplaceFile(temporary,path);}catch(...){unlock.join();throw;}
            unlock.join();CHECK(AtomicCommitMetrics::slowCount.load()>slow);CHECK(AtomicCommitMetrics::maxMs.load()>500);
            std::ifstream in(path);std::string value;in>>value;CHECK(value=="new");
        }
#endif
        std::cout<<"PASS Core hardening: cancel/ACK/STOP proof/footprint/restart 01+02, durable fields, geometry, TTL, validation, boot limit, blocked retry, wait warnings, slow commit\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
