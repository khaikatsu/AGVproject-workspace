// Core-only fixture: no firmware code, broker, operational counter or robot.
#include "core/FLeetmanager.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
int main(){try{for(const std::string id:{"01","02"}){
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager fleet(map,mutex);
    fleet.setTrialRobot(id);fleet.setNetworkAvailable(true);fleet.updateTelemetry(id,"K5",80,false);
    fleet.bindEncoderSession(id,7,1,100);
    std::vector<json> sent;fleet.setPublishCallback([&](const std::string&,const std::string& s){sent.push_back(json::parse(s));});
    MotionDiagnostic d{"K5","AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
    d.encoderBoot=7;d.encoderConnection=1;d.encoderUptime=100;d.liftPresent=d.liftSupported=true;d.liftState="IDLE";
    auto robot=[&](){return fleet.toJson()["agvs"][0];};
    auto observe=[&](){++d.encoderUptime;++d.cargoSensorSeq;fleet.observeMotionDiagnostic(id,d);};
    long cmd=0;observe();CHECK(!fleet.startLift(id,"K5",true,cmd).empty());CHECK(sent.empty());
    d.cargoSensorSupported=true;d.cargoSensorStable=true;d.cargoSensorAge=0;d.cargoSensorStableMs=150;
    observe();CHECK(!fleet.startLift(id,"K5",true,cmd).empty());CHECK(robot()["cargo_sensor"]["present"]==false);
    d.cargoSensorPresent=true;d.cargoSensorAge=501;observe();CHECK(!fleet.startLift(id,"K5",true,cmd).empty());
    d.cargoSensorAge=0;d.cargoSensorStable=false;observe();CHECK(robot()["cargo_sensor"]["present"].is_null());
    d.cargoSensorStable=true;d.leftDuty=1;observe();CHECK(!fleet.startLift(id,"K5",true,cmd).empty());
    d.leftDuty=0;d.node="G5";observe();CHECK(!fleet.startLift(id,"K5",true,cmd).empty());
    d.node="K5";d.encoderBoot=99;observe();CHECK(!fleet.startLift(id,"K5",true,cmd).empty());
    d.encoderBoot=7;observe();CHECK(robot()["cargo_sensor"]["stopped_verified"]==true);
    fleet.observeMotionDiagnostic(id,d);CHECK(!fleet.startLift(id,"K5",true,cmd).empty()); // Replay invalidates proof.
    observe();CHECK(fleet.startLift(id,"K5",true,cmd).empty());CHECK(sent.size()==1&&sent[0]["cmd"]=="LIFT_UP");
    fleet.reportError(id,"CARGO_LOST");CHECK(robot()["state"]=="ERROR");
    CHECK(robot()["lift"]["active"]==false);CHECK(!fleet.directGoto(id,"G5","no-auto-resume").empty());
    CHECK(mutex.toJson()==MutexArbiter(map.mutexZones()).toJson());
    std::cout<<"PASS cargo proof Core "<<id<<"\n";
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
