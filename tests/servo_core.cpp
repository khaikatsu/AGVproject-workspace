#include "commands/WebCommands.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
int main(){try{
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager fleet(map,mutex);
    fleet.setTrialRobot("01");std::vector<json> commands;
    fleet.setPublishCallback([&](const std::string&,const std::string& p){commands.push_back(json::parse(p));});
    fleet.updateTelemetry("01","K5",80,false);fleet.bindEncoderSession("01",1,1,100);
    MotionDiagnostic d{"K5","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
    d.liftPresent=d.liftSupported=true;d.liftState="IDLE";fleet.observeMotionDiagnostic("01",d);
    auto robot=[&](){return fleet.toJson()["agvs"][0];};
    long command=0;CHECK(!fleet.startLift("02","K5",true,command).empty());
    CHECK(!fleet.startLift("01","S1",true,command).empty());
    CHECK(fleet.startLift("01","K5",true,command).empty());CHECK(command>0);
    CHECK(commands.back()["cmd"]=="LIFT_UP"&&commands.back()["node"]=="K5");
    CHECK(robot()["lift"]["active"]==true&&robot()["lift"]["state"]=="PENDING");
    fleet.reportAck("01",command);CHECK(robot()["virtual_cargo"]==false);
    CHECK(!fleet.directGoto("01","G5","must-not-move").empty());
    long another=0;CHECK(!fleet.startLift("01","K5",true,another).empty());
    d.liftCommandId=command+1;d.liftState="DONE_UP";d.liftElapsedMs=5000;d.cargoKnown=d.virtualCargo=true;
    fleet.observeMotionDiagnostic("01",d);CHECK(robot()["lift"]["active"]==true);
    d.liftCommandId=command;fleet.observeMotionDiagnostic("01",d);CHECK(robot()["lift"]["active"]==true); // Cannot complete early.
    d.liftBusy=d.taskActive=true;d.liftState="LIFTING";d.state="AGV_LOADING";d.liftElapsedMs=100;
    fleet.observeMotionDiagnostic("01",d);CHECK(robot()["lift"]["state"]=="LIFTING");CHECK(robot()["virtual_cargo"]==false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5010));
    d.liftBusy=d.taskActive=false;d.liftState="DONE_UP";d.state="AGV_IDLE";d.liftElapsedMs=4999;
    fleet.observeMotionDiagnostic("01",d);CHECK(robot()["virtual_cargo"]==false);
    d.liftElapsedMs=5000;fleet.observeMotionDiagnostic("01",d);
    CHECK(robot()["virtual_cargo"]==true&&robot()["virtual_cargo_known"]==true&&robot()["state"]=="IDLE");
    fleet.observeMotionDiagnostic("01",d);CHECK(commands.back()["cmd"]=="LIFT_UP");
    CHECK(fleet.startLift("01","K5",false,command).empty());fleet.reportAck("01",command);
    CHECK(robot()["virtual_cargo"]==true);
    d.liftCommandId=command;d.liftBusy=d.taskActive=true;d.liftState="LOWERING";d.state="AGV_UNLOADING";
    fleet.observeMotionDiagnostic("01",d);CHECK(robot()["state"]=="LOWERING");
    std::this_thread::sleep_for(std::chrono::milliseconds(5010));
    d.liftBusy=d.taskActive=false;d.liftState="DONE_DOWN";d.state="AGV_IDLE";d.virtualCargo=false;
    fleet.observeMotionDiagnostic("01",d);CHECK(robot()["virtual_cargo"]==false&&robot()["state"]=="IDLE");
    fleet.updateTelemetry("01","K5",80,false);
    CHECK(fleet.startLift("01","K5",true,command).empty());
    d.liftCommandId=command;d.liftState="INTERRUPTED";d.cargoKnown=false;fleet.observeMotionDiagnostic("01",d);
    CHECK(robot()["state"]=="ERROR"&&robot()["virtual_cargo_known"]==false&&commands.back()["cmd"]=="WAIT");
    for(unsigned hazard=0;hazard<3;++hazard){
        MutexArbiter zones(map.mutexZones());FleetManager isolated(map,zones);isolated.setTrialRobot("01");
        isolated.updateTelemetry("01","K5",80,false);isolated.bindEncoderSession("01",10,1,100);
        MotionDiagnostic fresh{"K5","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
        fresh.liftPresent=fresh.liftSupported=true;fresh.liftState="IDLE";isolated.observeMotionDiagnostic("01",fresh);
        long operation=0;
        if(hazard!=2)CHECK(isolated.startLift("01","K5",true,operation).empty());
        if(hazard==0)isolated.bindEncoderSession("01",11,1,1);
        if(hazard==1)fresh.node="G5";
        fresh.liftCommandId=operation;fresh.liftBusy=fresh.taskActive=true;fresh.state="AGV_LOADING";fresh.liftState="LIFTING";
        isolated.observeMotionDiagnostic("01",fresh);
        CHECK(isolated.toJson()["agvs"][0]["state"]=="ERROR"); // Boot change, wrong node, unsolicited servo.
        CHECK(isolated.toJson()["agvs"][0]["virtual_cargo_known"]==false);
    }
    std::cout<<"PASS Core servo: capability/ID/node, ACK not done, wrong/early result, motor lock, 5s up/down cargo, interruption\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
