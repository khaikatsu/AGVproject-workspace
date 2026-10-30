#include "commands/WebCommands.hpp"
#include <iostream>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
int main(){try{for(const std::string id:{"01","02"}){
    const std::string home=PhysicalLayout::home(id);
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager fleet(map,mutex);
    fleet.setActiveRobots({id});fleet.updateTelemetry(id,home,80,false);
    auto saved=fleet.durableSnapshot();saved["agvs"][0]["dropoff_exit_pending"]=true;
    saved["agvs"][0]["dropoff_lift_node"]="K2";saved["agvs"][0]["dropoff_lift_command_id"]=55;
    fleet.restore(saved);fleet.updateTelemetry(id,home,80,false);CHECK(fleet.bindEncoderSession(id,7,1,100));
    MotionDiagnostic d{home,"AGV_IDLE","NONE","STOP",false,true,true,0,0,0};
    d.encoderBoot=7;d.encoderConnection=1;d.encoderUptime=200;d.liftPresent=d.liftSupported=true;
    d.liftCommandId=55;d.liftState="LOWERED";d.cargoSensorSupported=d.cargoSensorStable=d.cargoSensorPresent=true;
    d.cargoSensorAge=0;d.cargoSensorStableMs=150;d.cargoSensorSeq=1;
    fleet.observeMotionDiagnostic(id,d);
    auto robot=[&](){return fleet.toJson()["agvs"][0];};
    CHECK(robot()["lift"]["dropoff_exit_pending"]==true);CHECK(robot()["virtual_cargo"]==true);
    std::vector<json> commands;fleet.setPublishCallback([&](const std::string&,const std::string& p){commands.push_back(json::parse(p));});
    CHECK(!fleet.reconcileTestCargo({{id,55}}).empty());
    fleet.setAcceptingTasks(true);CHECK(!fleet.reconcileTestCargo({{id,55}},true).empty());
    fleet.setAcceptingTasks(false);CHECK(!fleet.reconcileTestCargo({{id,56}},true).empty());
    CHECK(robot()["virtual_cargo"]==true);
    d.leftDuty=20;++d.encoderUptime;++d.cargoSensorSeq;fleet.observeMotionDiagnostic(id,d);
    CHECK(!fleet.reconcileTestCargo({{id,55}},true).empty());
    d.leftDuty=0;++d.encoderUptime;++d.cargoSensorSeq;fleet.observeMotionDiagnostic(id,d);
    WebCommands web(map,fleet,mutex);
    CHECK(web.execute({{"cmd","reconcile_test_cargo"},{"home_rollback",true},{"robots",json::array({{{"id",id},{"lift_command_id",55}}})}})["ok"]==false);
    const auto stateBefore=robot()["state"],zones=mutex.toJson();
    const auto result=web.execute({{"cmd","reconcile_test_cargo"},{"home_rollback",true},{"physically_clear",true},
        {"robots",json::array({{{"id",id},{"lift_command_id",55}}})}});
    CHECK(result["ok"]==true);CHECK(result["data"]["mode"]=="operator_home_rollback");
    CHECK(result["data"]["cargo_evidence"][0]["cargo_sensor"]["present"]==true);
    CHECK(robot()["virtual_cargo"]==false);CHECK(robot()["virtual_cargo_known"]==true);
    CHECK(robot()["lift"]["dropoff_exit_pending"]==false);CHECK(robot()["cargo_sensor"]["present"]==true);
    CHECK(robot()["state"]==stateBefore);CHECK(mutex.toJson()==zones);CHECK(commands.empty());
    fleet.updateTelemetry(id,"K2",80,false);d.node="K2";++d.encoderUptime;++d.cargoSensorSeq;fleet.observeMotionDiagnostic(id,d);
    CHECK(!fleet.reconcileTestCargo({{id,55}},true).empty());
}std::cout<<"PASS operator home cargo rollback 01/02: paused/home/STOP/lift-ID/confirmation guards; sensor and fault/mutex preserved; no actuator commands\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
