// Core-only regression: no firmware compilation or robot/broker connection.
#include "commands/WebCommands.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(value) do {if(!(value))throw std::runtime_error("Failed: " #value);} while(0)

struct ReturnFixture {
    MapGraph map;MutexArbiter arbiter;FleetManager fleet;std::vector<json> sent;std::vector<std::string> logs;
    std::string id,home,gate;MotionDiagnostic d;
    std::unique_ptr<ShiftController> shift;
    ReturnFixture(std::string robot):fleet(map,arbiter),id(robot),
        home(id=="01"?"S1":"S2"),gate(id=="01"?"G2":"G5") {
        map.loadDefaultDemoMap();arbiter.resetZones(map.mutexZones());
        fleet.setActiveRobots({id});fleet.setHomeStations({{id,home}});
        fleet.setBatteryTestBypass(true);fleet.setAutomaticChargingEnabled(false);
        fleet.setPublishCallback([&](const std::string&,const std::string& payload){sent.push_back(json::parse(payload));});
        fleet.setEventCallback([&](const std::string& text){logs.push_back(text);});
        const auto dir=std::filesystem::path(__FILE__).parent_path().parent_path()/".qa"/
            ("shift-return-"+id+std::to_string(Clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir);
        shift=std::make_unique<ShiftController>(map,fleet,arbiter,dir/"shift.json",
            std::unordered_map<std::string,std::string>{{id,home}},false,false,"",std::unordered_set<std::string>{id});
        fleet.updateTelemetry(id,home,80,false);shift->tick();CHECK(shift->start().empty());
        fleet.reportAck(id,sent.back()["cmd_id"].get<long>());
        CHECK(fleet.directGoto(id,gate,"finished-job").empty());
        fleet.updateTelemetry(id,gate,80,false);CHECK(robotState()["state"]=="IDLE");
        CHECK(fleet.bindEncoderSession(id,7,1,100));
        d={gate,"AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
        d.encoderBoot=7;d.encoderConnection=1;d.encoderUptime=200;d.rfidAgeMs=0;
        d.headingSupported=d.nodeOrdersSupported=true;d.headingNode=gate;d.heading="E";d.headingSource="route";
        d.lastCommandId=sent.back()["cmd_id"].get<long>();
    }
    json robotState(){return fleet.toJson()["agvs"][0];}
    void observe(){d.encoderUptime+=100;fleet.updateTelemetry(id,gate,80,false);fleet.observeMotionDiagnostic(id,d);}
    size_t gotos(){return std::count_if(sent.begin(),sent.end(),[](const json& cmd){return cmd["cmd"]=="GOTO";});}
    void ack(){fleet.reportAck(id,sent.back()["cmd_id"].get<long>());}
};

int main(){try{
    for(const std::string id:{"01","02"})for(const std::string scenario:{"unknown_heading","active_task","motor_moving","old_rfid","pending_ack","wrong_boot"}) {
        ReturnFixture f(id);const auto before=f.gotos();
        if(scenario!="pending_ack")f.ack();
        if(scenario=="unknown_heading"){f.d.heading="UNKNOWN";f.d.headingSource="unknown";}
        if(scenario=="active_task"){f.d.taskActive=true;f.d.state="AGV_WAITING";}
        if(scenario=="motor_moving"){f.d.motor="FORWARD";f.d.leftDuty=f.d.rightDuty=100;}
        if(scenario=="old_rfid")f.d.rfidAgeMs=2000;
        if(scenario=="wrong_boot")f.d.encoderBoot=8;
        f.observe();CHECK(f.shift->end().empty());
        for(int i=0;i<5;++i)f.shift->tick();
        CHECK(f.gotos()==before);CHECK(f.robotState()["state"]=="IDLE");
        CHECK(f.shift->snapshot()["phase"]=="CLOSING");
        f.ack();f.d.heading="E";f.d.headingSource="route";f.d.taskActive=false;f.d.state="AGV_IDLE";
        f.d.motor="STOP";f.d.leftDuty=f.d.rightDuty=0;f.d.rfidAgeMs=0;f.d.encoderBoot=7;
        f.observe();f.shift->tick();CHECK(f.gotos()==before+1);
        CHECK(f.robotState()["target"]==f.home);CHECK(f.robotState()["state"]=="CHARGING_EN_ROUTE");
        const auto count=f.sent.size();for(int i=0;i<10;++i)f.shift->tick();CHECK(f.sent.size()==count);
    }
    for(const std::string id:{"01","02"}) {
        ReturnFixture f(id);f.ack();f.observe();
        CHECK(f.fleet.directGoto(id,id=="01"?"K2":"K5","unfinished-job").empty());
        const auto before=f.gotos();CHECK(f.shift->end().empty());
        CHECK(f.sent.back()["cmd"]=="WAIT");const auto stopId=f.sent.back()["cmd_id"].get<long>();
        CHECK(f.robotState()["cancel_waiting_for_node"]==true);
        f.ack();f.shift->tick();CHECK(f.gotos()==before); // Receipt ACK is not physical STOP.
        f.d.lastCommandId=stopId-1;f.observe();f.shift->tick();CHECK(f.gotos()==before);
        f.d.lastCommandId=stopId;f.observe();f.shift->tick();CHECK(f.gotos()==before+1);
        CHECK(f.robotState()["target"]==f.home);CHECK(f.shift->snapshot()["phase"]=="CLOSING");
        CHECK(f.fleet.toJson()["pending_tasks"].empty());
        const auto count=f.sent.size();f.fleet.reportError(id,"STALE_COMMAND");
        CHECK(f.sent.size()==count); // Preserve the no-low-ID-STOP rule.
        CHECK(std::any_of(f.logs.begin(),f.logs.end(),[](const std::string& text){
            return text.find("STALE_COMMAND; core_context=")!=std::string::npos &&
                text.find("diagnostic_last_command_id")!=std::string::npos && text.find("stop_command_id")!=std::string::npos;
        }));
    }
    std::cout<<"PASS shift return readiness: 01/02 x unknown heading, active MCU route, moving motor, stale RFID, missing ACK and wrong boot; fresh STOP starts one home route; active task cancellation waits for WAIT proof, no repeat GOTO/replay\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
