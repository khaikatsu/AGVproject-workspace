#include "commands/WebCommands.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
void exercise(const std::string& id){
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager fleet(map,mutex);
    fleet.setTrialRobot(id);std::vector<json> commands;
    fleet.setPublishCallback([&](const std::string&,const std::string& p){commands.push_back(json::parse(p));});
    fleet.updateTelemetry(id,"K5",80,false);fleet.bindEncoderSession(id,1,1,100);
    MotionDiagnostic d{"K5","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
    d.liftPresent=d.liftSupported=true;d.liftState="IDLE";fleet.observeMotionDiagnostic(id,d);
    auto robot=[&](){return fleet.toJson()["agvs"][0];};
    long command=0;CHECK(!fleet.startLift("03","K5",true,command).empty());
    CHECK(!fleet.startLift(id,"S1",true,command).empty());
    CHECK(fleet.startLift(id,"K5",true,command).empty());CHECK(command>0);
    CHECK(commands.back()["cmd"]=="LIFT_UP"&&commands.back()["node"]=="K5");
    CHECK(robot()["lift"]["active"]==true&&robot()["lift"]["state"]=="PENDING");
    fleet.reportAck(id,command);CHECK(robot()["virtual_cargo"]==false);
    CHECK(!fleet.directGoto(id,"G5","must-not-move").empty());
    long another=0;CHECK(!fleet.startLift(id,"K5",true,another).empty());
    d.liftCommandId=command+1;d.liftState="DONE_UP";d.liftElapsedMs=5000;d.cargoKnown=d.virtualCargo=true;
    fleet.observeMotionDiagnostic(id,d);CHECK(robot()["lift"]["active"]==true);
    d.liftCommandId=command;fleet.observeMotionDiagnostic(id,d);CHECK(robot()["lift"]["active"]==true); // Cannot complete early.
    d.liftBusy=d.taskActive=true;d.liftState="LIFTING";d.state="AGV_LOADING";d.liftElapsedMs=100;
    fleet.observeMotionDiagnostic(id,d);CHECK(robot()["lift"]["state"]=="LIFTING");CHECK(robot()["virtual_cargo"]==false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5010));
    d.liftBusy=d.taskActive=false;d.liftState="DONE_UP";d.state="AGV_IDLE";d.liftElapsedMs=4999;
    fleet.observeMotionDiagnostic(id,d);CHECK(robot()["virtual_cargo"]==false);
    d.liftElapsedMs=5000;fleet.observeMotionDiagnostic(id,d);
    CHECK(robot()["virtual_cargo"]==true&&robot()["virtual_cargo_known"]==true&&robot()["state"]=="IDLE");
    fleet.observeMotionDiagnostic(id,d);CHECK(commands.back()["cmd"]=="LIFT_UP");
    WebCommands web(map,fleet,mutex);
    json reset={{"cmd","reconcile_test_cargo"},{"physically_clear",true},{"robots",json::array({{{"id",id},{"lift_command_id",command}}})}};
    CHECK(!web.execute(reset)["ok"].get<bool>()); // No stopped proof bound to encoder boot yet.
    d.encoderBoot=1;d.encoderConnection=1;fleet.observeMotionDiagnostic(id,d);
    auto wrong=reset;wrong["robots"][0]["lift_command_id"]=command+1;
    CHECK(!web.execute(wrong)["ok"].get<bool>()&&robot()["virtual_cargo"]==true);
    auto invalid=reset;invalid["robots"].push_back({{"id",id=="01"?"02":"01"},{"lift_command_id",0}});
    CHECK(!web.execute(invalid)["ok"].get<bool>()&&robot()["virtual_cargo"]==true);
    const auto writes=commands.size();const auto zonesBefore=mutex.toJson();
    CHECK(web.execute(reset)["ok"]==true);CHECK(robot()["virtual_cargo"]==false&&robot()["virtual_cargo_known"]==true);
    CHECK(web.execute(reset)["ok"]==true);CHECK(commands.size()==writes&&mutex.toJson()==zonesBefore);
    fleet.observeMotionDiagnostic(id,d);CHECK(robot()["virtual_cargo"]==false); // Old MCU DONE_UP cannot undo operator reconciliation.
    CHECK(fleet.startLift(id,"K5",false,command).empty());fleet.reportAck(id,command);
    CHECK(robot()["virtual_cargo"]==false);
    CHECK(!web.execute(reset)["ok"].get<bool>()); // A new servo cycle invalidates the old confirmation.
    d.liftCommandId=command;d.liftBusy=d.taskActive=true;d.liftState="LOWERING";d.state="AGV_UNLOADING";
    fleet.observeMotionDiagnostic(id,d);CHECK(robot()["state"]=="LOWERING");
    std::this_thread::sleep_for(std::chrono::milliseconds(5010));
    d.liftBusy=d.taskActive=false;d.liftState="DONE_DOWN";d.state="AGV_IDLE";d.virtualCargo=false;
    fleet.observeMotionDiagnostic(id,d);CHECK(robot()["virtual_cargo"]==false&&robot()["state"]=="IDLE");
    fleet.updateTelemetry(id,"K5",80,false);
    CHECK(fleet.startLift(id,"K5",true,command).empty());
    d.liftCommandId=command;d.liftState="INTERRUPTED";d.cargoKnown=false;fleet.observeMotionDiagnostic(id,d);
    CHECK(robot()["state"]=="ERROR"&&robot()["virtual_cargo_known"]==false&&commands.back()["cmd"]=="WAIT");
    d.state="AGV_SAFE_STOP";d.error="SERVO_INTERRUPTED";d.encoderConnection=2;fleet.observeMotionDiagnostic(id,d);
    reset["robots"][0]["lift_command_id"]=command;
    CHECK(!web.execute(reset)["ok"].get<bool>());
    d.encoderConnection=1;fleet.observeMotionDiagnostic(id,d);
    const auto fault=robot()["error"];const auto stoppedWrites=commands.size();
    CHECK(web.execute(reset)["ok"]==true&&robot()["state"]=="ERROR"&&robot()["error"]==fault);
    CHECK(robot()["virtual_cargo"]==false&&robot()["virtual_cargo_known"]==true&&commands.size()==stoppedWrites);
    for(unsigned hazard=0;hazard<3;++hazard){
        MutexArbiter zones(map.mutexZones());FleetManager isolated(map,zones);isolated.setTrialRobot(id);
        isolated.updateTelemetry(id,"K5",80,false);isolated.bindEncoderSession(id,10,1,100);
        MotionDiagnostic fresh{"K5","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
        fresh.liftPresent=fresh.liftSupported=true;fresh.liftState="IDLE";isolated.observeMotionDiagnostic(id,fresh);
        long operation=0;
        if(hazard!=2)CHECK(isolated.startLift(id,"K5",true,operation).empty());
        if(hazard==0)isolated.bindEncoderSession(id,11,1,1);
        if(hazard==1)fresh.node="G5";
        fresh.liftCommandId=operation;fresh.liftBusy=fresh.taskActive=true;fresh.state="AGV_LOADING";fresh.liftState="LIFTING";
        isolated.observeMotionDiagnostic(id,fresh);
        CHECK(isolated.toJson()["agvs"][0]["state"]=="ERROR"); // Boot change, wrong node, unsolicited servo.
        CHECK(isolated.toJson()["agvs"][0]["virtual_cargo_known"]==false);
    }
    std::cout<<"PASS Core servo/reconciliation "<<id<<": matched lift, 5s up/down, batch CAS/stop proof, no MQTT or mutex/fault reset, old DONE ignored\n";
}
int main(){try{exercise("01");exercise("02");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
