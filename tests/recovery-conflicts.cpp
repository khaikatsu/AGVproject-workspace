// Server-only adversarial cases. No firmware, broker or operational data.
#include "commands/WebCommands.hpp"
#include <iostream>
#include <thread>
#include <functional>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
struct Fixture {
    MapGraph map;MutexArbiter mutex;FleetManager fleet;std::vector<json> sent;
    std::string id,node;MotionDiagnostic d;
    Fixture(std::string robot="01",bool atHome=true):mutex(zones()),fleet(map,mutex),id(robot),
        node(atHome?(robot=="01"?"S1":"S2"):"K5") {
        fleet.setActiveRobots({"01","02"});fleet.setAutomaticChargingEnabled(false);
        fleet.setHomeStations(PhysicalLayout::homes());
        fleet.setPublishCallback([&](const std::string&,const std::string& wire){sent.push_back(json::parse(wire));});
        fleet.updateTelemetry(id,node,80,false);CHECK(fleet.bindEncoderSession(id,7,1,100));
        d.node=node;d.state="AGV_IDLE";d.error="NONE";d.motor="STOP";d.taskActive=false;
        d.wifi=d.mqtt=true;d.leftDuty=d.rightDuty=0;d.rfidAgeMs=0;
        d.encoderBoot=7;d.encoderConnection=1;d.encoderUptime=200;d.lastCommandId=1000;
        d.headingSupported=d.headingConfirmationSupported=true;d.headingSource="route";d.headingNode=node;
        d.heading=atHome?(id=="01"?"S":"N"):"S";
        d.liftPresent=d.liftSupported=true;d.liftState="IDLE";
        d.cargoSensorSupported=d.cargoSensorStable=d.cargoSensorPresent=true;
        d.cargoSensorAge=0;d.cargoSensorStableMs=150;observe();
        d.heading=this->robot()["recovery_expected_heading"].get<std::string>();observe();
    }
    std::vector<std::string> zones(){map.loadDefaultDemoMap();return map.mutexZones();}
    void observe(){++d.encoderUptime;++d.cargoSensorSeq;fleet.observeMotionDiagnostic(id,d);}
    json robot(){const auto state=fleet.toJson();for(const auto& r:state["agvs"])if(r["id"]==id)return r;throw std::runtime_error("missing");}
    long request(){CHECK(fleet.confirmRecoveryHeading(id,node,d.heading).empty());return sent.back()["cmd_id"].get<long>();}
    void finish(long cmd){d.headingSource="operator";d.headingCommandId=cmd;d.lastCommandId=cmd;observe();}
};
int main(){int failed=0,passed=0;
    auto run=[&](const std::string& name,const std::function<void()>& test){try{test();++passed;std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    for(const std::string id:{"01","02"}){
        run(id+" completed diagnostic with lost ACK",[&]{Fixture f(id);const auto cmd=f.request();f.finish(cmd);
            CHECK(f.robot()["heading_confirmation_required"]==false);f.fleet.setAckTimeout(1);f.fleet.setMaxRetries(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));f.fleet.tick();CHECK(f.robot()["state"]=="IDLE");});
        run(id+" reset cannot replace pending heading",[&]{Fixture f(id);f.request();const auto n=f.sent.size();
            CHECK(!f.fleet.clearError(id));CHECK(f.sent.size()==n);});
        run(id+" completed reset with lost ACK",[&]{Fixture f(id,false);f.fleet.reportError(id,"LINE_LOST");
            f.d.state="AGV_SAFE_STOP";f.d.error="LINE_LOST";f.observe();
            CHECK(f.fleet.recoverVerifiedNode(id,f.node,false,true,false,true).empty());
            const auto cmd=f.sent.back()["cmd_id"].get<long>();CHECK(f.sent.back()["cmd"]=="RESET_FAULT");
            f.d.state="AGV_IDLE";f.d.error="NONE";f.d.lastCommandId=cmd;f.observe();
            CHECK(f.robot()["state"]=="IDLE");f.fleet.setAckTimeout(1);f.fleet.setMaxRetries(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));f.fleet.tick();CHECK(f.robot()["state"]=="IDLE");});
        run(id+" recover cannot replace pending heading",[&]{Fixture f(id);f.request();const auto n=f.sent.size();
            CHECK(!f.fleet.recoverVerifiedNode(id,f.node,true,true,true,true).empty());CHECK(f.sent.size()==n);});
        run(id+" new fault invalidates old heading result",[&]{Fixture f(id,false);const auto cmd=f.request();
            f.fleet.reportError(id,"NEW_FAULT");CHECK(f.robot()["heading_confirmation_pending"]==false);
            f.observe();CHECK(f.fleet.recoverVerifiedNode(id,f.node,false,true,false,true).empty());
            f.finish(cmd);CHECK(f.robot()["heading_confirmation_required"]==true);});
        run(id+" end shift invalidates pending heading",[&]{Fixture f(id);const auto cmd=f.request();
            f.fleet.endShiftWork();f.finish(cmd);CHECK(f.robot()["heading_confirmation_required"]==true);
            CHECK(f.robot()["heading_confirmation_pending"]==false);});
        run(id+" lift cannot replace pending heading",[&]{Fixture f(id,false);f.request();long cmd=0;const auto n=f.sent.size();
            CHECK(!f.fleet.startLift(id,f.node,true,cmd).empty());CHECK(f.sent.size()==n);});
        run(id+" offline retains owned mutex",[&]{Fixture f(id);CHECK(f.mutex.requestZone("M2",id));
            const auto token=f.mutex.grant("M2",id)["grant_id"].get<long>();
            f.fleet.reportOffline(id);CHECK(f.mutex.currentOwner("M2")==id);
            CHECK(!f.mutex.confirmClear("M2",id,token+1));CHECK(f.mutex.currentOwner("M2")==id);});
        run(id+" reconnect removes stale mutex waiter without releasing owner",[&]{Fixture f(id);
            const std::string other=id=="01"?"02":"01";
            CHECK(f.mutex.requestZone("M2",other));CHECK(!f.mutex.requestZone("M2",id));
            const auto token=f.mutex.grant("M2",other)["grant_id"].get<long>();
            CHECK(f.fleet.bindEncoderSession(id,7,2,500));CHECK(f.mutex.currentOwner("M2")==other);
            CHECK(f.mutex.confirmClear("M2",other,token));CHECK(f.mutex.currentOwner("M2").empty());});
        run(id+" timeout then retry rejects late old heading",[&]{Fixture f(id);const auto old=f.request();
            std::this_thread::sleep_for(std::chrono::milliseconds(5100));f.fleet.updateTelemetry(id,f.node,80,false);f.observe();
            CHECK(f.robot()["heading_confirmation_pending"]==false);CHECK(f.robot()["heading_confirmation_required"]==true);
            const auto fresh=f.request();CHECK(fresh>old);f.finish(old);
            CHECK(f.robot()["heading_confirmation_required"]==true);CHECK(f.robot()["heading_confirmation_pending"]==true);
            f.finish(fresh);CHECK(f.robot()["heading_confirmation_required"]==false);});
    }
    run("two robots compete for one mutex and stale token cannot release new owner",[]{
        Fixture f;std::thread a([&]{for(int i=0;i<100;++i)f.mutex.requestZone("M2","01");});
        std::thread b([&]{for(int i=0;i<100;++i)f.mutex.requestZone("M2","02");});a.join();b.join();
        const auto owner=f.mutex.currentOwner("M2");const std::string other=owner=="01"?"02":"01";
        const auto token=f.mutex.grant("M2",owner)["grant_id"].get<long>();
        CHECK(f.mutex.confirmClear("M2",owner,token));CHECK(f.mutex.currentOwner("M2")==other);
        CHECK(!f.mutex.confirmClear("M2",owner,token));CHECK(f.mutex.currentOwner("M2")==other);
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
