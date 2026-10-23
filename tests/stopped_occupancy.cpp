#include "core/FLeetmanager.hpp"
#include <iostream>
#include <thread>
#define CHECK(c) do {if (!(c)) throw std::runtime_error(#c);} while(false)
json layout() {
    json j={{"nodes",json::array()},{"edges",json::array()}};
    for (auto id : {"A","B","Q","M1","M2","T","U"})
        j["nodes"].push_back({{"id",id},{"x",0},{"y",0},{"type",std::string(id)=="B" || id[0]=='M' ? "mutex":"waypoint"}});
    for (auto e : {std::make_pair("A","B"),std::make_pair("B","M1"),std::make_pair("M1","M2"),
                   std::make_pair("M2","U"),std::make_pair("Q","M1"),std::make_pair("M2","T")})
        j["edges"].push_back({{"from",e.first},{"to",e.second},{"weight",50},{"two_way",true}});
    return j;
}
json robot(FleetManager& f,const std::string& id) {
    const auto state=f.toJson();for(const auto& a:state["agvs"])if(a["id"]==id)return a;
    throw std::runtime_error("missing robot");
}
json proof(long route,uint32_t seq,uint32_t uptime,const std::string& from="A",const std::string& to="B") {
    return {{"schema_version",1},{"robot_id","01"},{"core_session_id","fixture"},{"boot_id",10},
        {"connection_session",1},{"seq",seq},{"uptime_ms",uptime},{"left_ticks",30},{"right_ticks",30},
        {"route_cmd_id",0},{"path_index",0},{"from_node",from},{"to_node",""},{"tracking",false},
        {"translating",false},{"reverse",false},{"distance_cm",0},{"speed_cm_s",0},{"pulses_per_100cm",161},
        {"stopped_position",{{"version",1},{"from_node",from},{"to_node",to},{"route_cmd_id",to.empty()?0:route},
            {"path_index",0},{"stationary_ms",600+200*seq},{"distance_cm",to.empty()?0:10}}}};
}
void diagnostic(FleetManager& f,const std::string& node,uint32_t uptime,uint32_t rfidAge=UINT32_MAX) {
    MotionDiagnostic d;
    d.node=node;d.state="AGV_SAFE_STOP";d.error="MOTOR_FAULT";d.motor="STOP";
    d.taskActive=false;d.wifi=d.mqtt=true;d.rfidAgeMs=rfidAge;d.leftDuty=d.rightDuty=0;
    d.encoderBoot=10;d.encoderConnection=1;d.encoderUptime=uptime;
    f.observeMotionDiagnostic("01",d);
}
int main() {try {
    MapGraph map;map.loadFromJson(layout());MutexArbiter arbiter(map.mutexZones());FleetManager f(map,arbiter);
    f.setActiveRobots({"01","02"});f.setAutomaticChargingEnabled(false);
    long route=0,grant=100;
    arbiter.setGrantIdAllocator([&]{return ++grant;});
    f.setPublishCallback([&](const std::string& topic,const std::string& wire){
        const auto j=json::parse(wire);if(topic=="agv/01/command" && j.value("cmd","")=="GOTO")route=j.at("cmd_id");
    });
    f.updateTelemetry("01","A",80,false);f.updateTelemetry("02","Q",80,false);
    CHECK(f.directGoto("01","U","failed-task").empty());
    CHECK(f.directGoto("02","T","continue-task").empty());
    CHECK(robot(f,"02")["state"]=="BLOCKED");
    CHECK(arbiter.requestZone("B","01"));CHECK(arbiter.requestZone("M1","01"));CHECK(arbiter.requestZone("M2","01"));
    const long oldGrant=arbiter.grant("M1","01").at("grant_id");
    CHECK(!arbiter.requestZone("M1","02"));
    f.reportError("01","MOTOR_FAULT");
    CHECK(f.bindEncoderSession("01",10,1,1000));diagnostic(f,"A",1000);
    auto p=proof(route,1,1000);
    auto wrong=p;wrong["stopped_position"]["route_cmd_id"]=route+100;
    CHECK(!f.observeOdometry("01",EncoderSample::parse(wrong)));
    wrong=p;wrong["stopped_position"]["distance_cm"]=51;
    CHECK(!f.observeOdometry("01",EncoderSample::parse(wrong))); // missed target RFID: do not narrow.
    CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(arbiter.currentOwner("M1")=="01"); // one packet never releases.
    CHECK(!f.observeOdometry("01",EncoderSample::parse(p))); // duplicate
    p=proof(route,2,1200);CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(robot(f,"01")["state"]=="ERROR");CHECK(robot(f,"01")["stopped_nodes"]==json::array({"A","B"}));
    CHECK(robot(f,"01")["stopped_occupancy_verified"]==true);
    CHECK(arbiter.currentOwner("B")=="01");CHECK(arbiter.currentOwner("M1").empty());CHECK(arbiter.currentOwner("M2").empty());
    CHECK(robot(f,"02")["state"]=="WORKING");CHECK(robot(f,"02")["path"]==json::array({"M1","M2","T"}));
    CHECK(f.bindEncoderSession("02",20,1,1000));
    MotionDiagnostic moving;
    moving.node="Q";moving.state="AGV_MOVING";moving.error="NONE";moving.motor="FORWARD";
    moving.taskActive=true;moving.wifi=moving.mqtt=true;moving.nodeOrdersSupported=true;
    moving.encoderBoot=20;moving.encoderConnection=1;moving.encoderUptime=1000;
    f.observeMotionDiagnostic("02",moving);
    CHECK(!arbiter.requestZone("B","02")); // planner AND grants protect the endpoint.
    arbiter.releaseZone("M1","01",oldGrant);CHECK(arbiter.currentOwner("M1").empty());
    CHECK(!f.directGoto("02","A","cannot-enter-endpoint").empty());
    auto checkpoint=f.durableSnapshot();FleetManager restored(map,arbiter);restored.restore(checkpoint);restored.refreshStoppedOccupancy();
    CHECK(robot(restored,"01")["stopped_occupancy_verified"]==false);
    CHECK(!arbiter.requestZone("M1","02")); // restore never trusts an old narrow footprint.
    f.refreshStoppedOccupancy();
    // Valid normal packet with the proof absent invalidates narrowing immediately.
    p=proof(route,3,1400);p.erase("stopped_position");CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(robot(f,"01")["stopped_occupancy_verified"]==false);CHECK(robot(f,"02")["state"]=="BLOCKED");
    CHECK(!arbiter.requestZone("M1","02"));
    diagnostic(f,"A",1600);p=proof(route,4,1600);CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    p=proof(route,5,1800);CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(robot(f,"02")["state"]=="BLOCKED");CHECK(robot(f,"02")["replan_waiting_for_node"]==true);
    moving.state="AGV_IDLE";moving.motor="STOP";moving.taskActive=false;moving.leftDuty=moving.rightDuty=0;
    moving.rfidAgeMs=251;moving.encoderUptime=2000;f.observeMotionDiagnostic("02",moving);f.tick();
    CHECK(robot(f,"02")["replan_waiting_for_node"]==true); // old tag cannot start a replacement route.
    moving.rfidAgeMs=0;moving.encoderUptime=2100;f.observeMotionDiagnostic("02",moving);f.tick();
    CHECK(robot(f,"02")["state"]=="WORKING");
    // Actual RFID plus stopped diagnostic narrows to one node, never inferred from distance.
    f.updateTelemetry("01","B",80,false);diagnostic(f,"B",2000,0);
    p=proof(0,6,2000,"B","");CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    p=proof(0,7,2200,"B","");CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(robot(f,"01")["stopped_nodes"]==json::array({"B"}));CHECK(arbiter.currentOwner("B")=="01");
    // Stale proof expands the blocked region even before telemetry's 10s timeout.
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));f.refreshStoppedOccupancy();
    CHECK(robot(f,"01")["stopped_occupancy_verified"]==false);CHECK(robot(f,"02")["state"]=="BLOCKED");
    CHECK(!arbiter.requestZone("M1","02"));
    f.updateTelemetry("01","B",80,false);diagnostic(f,"B",2600,0);
    p=proof(0,8,2600,"B","");CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    p=proof(0,9,2800,"B","");CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(robot(f,"01")["stopped_occupancy_verified"]==true);
    p=proof(0,10,3000,"B","");p["left_ticks"]=31;
    CHECK(f.observeOdometry("01",EncoderSample::parse(p)));
    CHECK(robot(f,"01")["stopped_occupancy_verified"]==false); // changed raw ticks revoke narrowing now.
    // Wrong type/version/short stationarity is rejected, not silently downgraded.
    for (const json& bad : {json(599),json(-1),json(1.5),json("600")}) {
        auto invalid=proof(route,8,2400);invalid["stopped_position"]["stationary_ms"]=bad;
        bool rejected=false;try{EncoderSample::parse(invalid);}catch(...){rejected=true;}CHECK(rejected);
    }
    std::cout<<"PASS stopped occupancy: stable proof, A-B/node, outside mutex release, reroute, stale fallback, restart and replay\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
