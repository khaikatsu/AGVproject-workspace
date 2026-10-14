#include "core/FLeetmanager.hpp"
#include <thread>
#include <iostream>
#define CHECK(c) do {if (!(c)) throw std::runtime_error(#c);} while(false)
json packet() {
    return {{"schema_version",1},{"robot_id","01"},{"core_session_id","core-test"},{"boot_id",1234},
        {"connection_session",1},{"seq",1},{"uptime_ms",1000},{"left_ticks",16},{"right_ticks",16},
        {"route_cmd_id",1},{"path_index",0},{"from_node","C4"},{"to_node","M4"},
        {"tracking",true},{"translating",true},{"reverse",false},{"distance_cm",10.0},{"speed_cm_s",10.0},{"pulses_per_100cm",161.0}};
}
int main() {
    try {
        MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager f(map,mutex);
        f.setTrialRobot("01");long command = 0;
        f.setPublishCallback([&](const std::string&,const std::string& wire){const auto j=json::parse(wire);if(j["cmd"]=="GOTO")command=j["cmd_id"];});
        f.updateTelemetry("01","C4",80,false);CHECK(f.directGoto("01","C3","encoder-task").empty());
        auto p = packet();p["route_cmd_id"]=command;
        CHECK(!f.observeOdometry("01",EncoderSample::parse(p))); // fresh control diagnostic must bind the MCU session first.
        f.bindEncoderSession("01",1234,1,1000);const auto before = f.durableSnapshot();const auto zones = mutex.toJson();
        CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
        auto row=f.toJson()["agvs"][0];CHECK(row["node"]=="C4");CHECK(row["estimated_position"]["progress"]==0.2);
        MotionDiagnostic sonar{"C4","AGV_WAITING","NONE","STOP",true,true,true,0,0,0};
        sonar.sonarPresent=true;sonar.sonarObstacle=true;sonar.sonarDistanceCm=12;sonar.sonarDirection="front";
        sonar.routeCommandId=command;sonar.nextNode="M4";
        f.observeMotionDiagnostic("01",sonar);
        row=f.toJson()["agvs"][0];
        CHECK(row["obstacle_alert"]["direction"]=="front");
        CHECK(row["obstacle_detection"]["from"]=="C4");
        CHECK(row["obstacle_detection"]["to"]=="M4");
        CHECK(row["obstacle_detection"]["range_cm"]==12);
        CHECK(row["obstacle_detection"]["source"]=="sonar_encoder_estimate");
        sonar.routeCommandId=command+1;f.observeMotionDiagnostic("01",sonar);
        CHECK(f.toJson()["agvs"][0]["obstacle_detection"].is_null());
        sonar.routeCommandId=command;sonar.sonarDirection="rear";f.observeMotionDiagnostic("01",sonar);
        CHECK(f.toJson()["agvs"][0]["obstacle_detection"].is_null());
        sonar.sonarDirection="front";
        sonar.sonarObstacle=false;sonar.sonarDistanceCm=25;f.observeMotionDiagnostic("01",sonar);
        CHECK(f.toJson()["agvs"][0]["obstacle_detection"].is_null());
        CHECK(f.toJson()["agvs"][0]["obstacle_alert"].is_null());
        CHECK(row["speed_cm_s"]==10);CHECK(f.durableSnapshot()==before);CHECK(mutex.toJson()==zones);
        CHECK(!f.observeOdometry("01",EncoderSample::parse(p))); // duplicate
        p["seq"]=2;p["uptime_ms"]=1200;p["distance_cm"]=60;
        CHECK(f.observeOdometry("01",EncoderSample::parse(p)));CHECK(f.toJson()["agvs"][0]["estimated_position"]["progress"]==0.98);
        CHECK(f.toJson()["tasks"][0]["status"]=="dispatched"); // saturation does not arrive/release.
        p["seq"]=3;p["distance_cm"]=20;CHECK(!f.observeOdometry("01",EncoderSample::parse(p))); // regressing distance
        p["distance_cm"]=60;p["connection_session"]=2;CHECK(!f.observeOdometry("01",EncoderSample::parse(p)));
        p["connection_session"]=1;p["route_cmd_id"]=command+99;CHECK(!f.observeOdometry("01",EncoderSample::parse(p)));
        p["route_cmd_id"]=command;p["path_index"]=1;CHECK(!f.observeOdometry("01",EncoderSample::parse(p)));
        p["path_index"]=0;f.updateTelemetry("01","M4",80,false);CHECK(f.toJson()["agvs"][0]["estimated_position"].is_null());
        CHECK(!f.observeOdometry("01",EncoderSample::parse(p))); // old anchor after RFID
        p["from_node"]="M4";p["to_node"]="GS2";p["path_index"]=1;p["distance_cm"]=5;
        CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
        f.bindEncoderSession("01",1234,2,1400);CHECK(f.toJson()["agvs"][0]["speed_cm_s"].is_null());
        CHECK(!f.observeOdometry("01",EncoderSample::parse(p))); // prior connection
        p["connection_session"]=2;p["seq"]=4;p["uptime_ms"]=1400;CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
        f.bindEncoderSession("01",1234,1,1200);p["seq"]=5;CHECK(f.observeOdometry("01",EncoderSample::parse(p))); // old binding cannot roll back
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));CHECK(f.toJson()["agvs"][0]["speed_cm_s"].is_null());
        f.reportError("01","LINE_LOST");p["seq"]=6;CHECK(!f.observeOdometry("01",EncoderSample::parse(p)));
        CHECK(mutex.toJson()==zones);
        for (const auto& [key,value] : std::vector<std::pair<std::string,json>>{{"seq",-1},{"seq",1.2},{"speed_cm_s","10"},
            {"speed_cm_s",-1},{"distance_cm",nullptr},{"translating",1},{"pulses_per_100cm",0},{"robot_id","02"}}) {
            auto bad=packet();bad[key]=value;bool rejected=false;try{EncoderSample::parse(bad);}catch(...){rejected=true;}CHECK(rejected);
        }
        FleetManager regular(map,mutex);regular.updateTelemetry("01","C4",80,false);regular.bindEncoderSession("01",1234,1,1000);
        CHECK(!regular.observeOdometry("01",EncoderSample::parse(packet())));CHECK(!regular.toJson()["agvs"][0]["encoder_supported"].get<bool>());
        EncoderObservation boot;CHECK(boot.bind(1,1,10));CHECK(boot.bind(2,1,10));CHECK(!boot.bind(1,2,20));
        FleetManager fault(map,mutex);fault.setTrialRobot("01");fault.updateTelemetry("01","S1",80,false);
        MotionDiagnostic stopped{"S1","AGV_SAFE_STOP","MOTION_LOCKED_PENDING_ZONE_CLEARANCE_CALIBRATION","STOP",false,true,true,0,0,0};
        fault.observeEncoderFirmwareFault("01",stopped);CHECK(fault.toJson()["agvs"][0]["state"]=="IDLE"); // unbound diagnostics ignored
        CHECK(fault.bindEncoderSession("01",1234,1,1000));fault.observeEncoderFirmwareFault("01",stopped);
        CHECK(fault.toJson()["agvs"][0]["state"]=="ERROR");CHECK(fault.toJson()["agvs"][0]["error"]==stopped.error);
        stopped.state="AGV_IDLE";stopped.error="NONE";fault.observeEncoderFirmwareFault("01",stopped);
        CHECK(fault.toJson()["agvs"][0]["state"]=="ERROR"); // observation cannot reset the MCU/Core
        std::cout << "PASS encoder core: session/schema/order, route binding, saturation, stale/error/mode isolation; no task/mutex/checkpoint mutation\n";
    } catch (const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
