#include "commands/WebCommands.hpp"
#include <iostream>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("Failed: " #condition); } while (false)

int main() {
    try {
        for(const std::string id:{"01","02"})for(const bool nodeOrders:{false,true}) {
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            fleet.setTrialRobot(id);fleet.setBatteryTestBypass(true);fleet.setAutomaticChargingEnabled(false);
            std::vector<json> sent;std::vector<std::string> logs;
            fleet.setPublishCallback([&](const std::string&,const std::string& p){sent.push_back(json::parse(p));});
            fleet.setEventCallback([&](const std::string& text){logs.push_back(text);});
            const std::string home=id=="01"?"S1":"S2",gate=id=="01"?"GS1":"GS2";
            fleet.updateTelemetry(id,home,80,false);
            MotionDiagnostic d{home,"AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
            d.nodeOrdersSupported=nodeOrders;fleet.observeNodeOrderRequest(id,d);
            CHECK(fleet.directGoto(id,gate,"first-route").empty());
            const auto before=fleet.toJson();const auto count=sent.size();
            WebCommands web(map,fleet,arbiter);
            const auto rejected=web.execute({{"cmd","direct_goto"},{"agv_id",id},{"target","K1"}});
            CHECK(rejected["ok"]==false);CHECK(rejected["status"]==409);
            CHECK(sent.size()==count);
            const auto after=fleet.toJson();
            for(const auto* field:{"task_id","target","path","state"})
                CHECK(after["agvs"][0][field]==before["agvs"][0][field]);
            CHECK(after["tasks"]==before["tasks"]);CHECK(after["pending_tasks"]==before["pending_tasks"]);
            d.nodeOrdersSupported=false;fleet.observeNodeOrderRequest(id,d);
            CHECK(!fleet.directGoto(id,gate,"repeat-after-capability-loss").empty());
            CHECK(sent.size()==count);
            CHECK(fleet.toJson()["pending_tasks"].empty());
            // INVALID_COMMAND must keep the original task/route in the audit
            // even though handling the error clears motion and issues STOP.
            fleet.reportError(id,"INVALID_COMMAND");
            CHECK(std::any_of(logs.begin(),logs.end(),[&](const std::string& line){
                return line.find("core_context=")!=std::string::npos && line.find("first-route")!=std::string::npos &&
                    line.find("route_cmd_id")!=std::string::npos && line.find(gate)!=std::string::npos;
            }));
            CHECK(fleet.toJson()["agvs"][0]["state"]=="ERROR");
            CHECK(sent.back()["cmd"]=="STOP");
            CHECK(!fleet.directGoto(id,gate,"unrecovered-fault").empty());
        }
        std::cout<<"PASS direct GOTO guard: 01/02, capability unknown/present/lost, HTTP result409, no overlapping command/requeue, error audit and fault latch\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
