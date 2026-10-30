#include "commands/WebCommands.hpp"
#include <iostream>
#include <stdexcept>

#define CHECK(value) do {if(!(value))throw std::runtime_error("Failed: " #value);} while(0)

int main() {
    try {
        const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
        for(const std::string id:{"01","02"}) for(const std::string saved:{"CLOSED","RUNNING","CLOSING"}) {
            const auto dir=root/".qa"/("shift-lifecycle-"+id+saved+std::to_string(Clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(dir);
            const auto file=dir/"shift.json";
            {std::ofstream out(file);out<<json{{"phase",saved}};}
            const std::string home=id=="01"?"S1":"S2";
            MapGraph map;map.loadDefaultDemoMap();MutexArbiter arbiter(map.mutexZones());FleetManager fleet(map,arbiter);
            std::vector<json> commands;
            fleet.setPublishCallback([&](const std::string&,const std::string& payload){commands.push_back(json::parse(payload));});
            ShiftController shift(map,fleet,arbiter,file,{{id,home}},false,false,"",{id});
            const std::string expected=saved=="CLOSED"?"CLOSED":"CLOSING";
            CHECK(shift.snapshot()["phase"]==expected);
            CHECK(!shift.snapshot()["can_start"].get<bool>());CHECK(!fleet.acceptingTasks());
            CHECK(fleet.toJson()["agvs"].empty());CHECK(!shift.start().empty());
            fleet.updateTelemetry(id,home,80,false);shift.tick();
            CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(shift.snapshot()["can_start"].get<bool>());
            CHECK(shift.start().empty());CHECK(fleet.acceptingTasks());
            CHECK(shift.end().empty());CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(!fleet.acceptingTasks());
            CHECK(arbiter.requestZone("M5",id));shift.tick();
            CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(!shift.snapshot()["can_start"].get<bool>());
            arbiter.releaseZone("M5",id);
            fleet.reportError(id,"NETWORK_UNAVAILABLE");shift.tick();
            CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(!shift.snapshot()["can_start"].get<bool>());
            CHECK(shift.end().empty());CHECK(!shift.start().empty());CHECK(!fleet.acceptingTasks());
            {std::ifstream input(file);json persisted;input>>persisted;CHECK(persisted["phase"]=="CLOSED");}
            MapGraph map2;map2.loadDefaultDemoMap();MutexArbiter arbiter2(map2.mutexZones());FleetManager fleet2(map2,arbiter2);
            ShiftController reloaded(map2,fleet2,arbiter2,file,{{id,home}},false,false,"",{id});
            CHECK(reloaded.snapshot()["phase"]=="CLOSED");CHECK(!reloaded.snapshot()["can_start"].get<bool>());
            CHECK(!fleet2.acceptingTasks());CHECK(fleet2.toJson()["agvs"].empty());
            CHECK(std::none_of(commands.begin(),commands.end(),[](const json& c){return c.value("cmd","")=="GOTO"||c.value("cmd","")=="CHARGE_START";}));
        }
        std::cout<<"PASS shift lifecycle: 01/02 x saved CLOSED/RUNNING/CLOSING; close/reopen guards, error, mutex, persistence and no automatic trips\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
