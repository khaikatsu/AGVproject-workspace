// Server-only regression; no MCU firmware, broker or physical robot is used.
#include "commands/WebCommands.hpp"
#include <iostream>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
json robot(FleetManager& f,const std::string& id){const auto s=f.toJson();for(const auto& a:s["agvs"])if(a["id"]==id)return a;throw std::runtime_error("missing robot");}
json task(FleetManager& f,const std::string& id){const auto s=f.toJson();for(const auto& t:s["tasks"])if(t["task_id"]==id)return t;throw std::runtime_error("missing task");}
MotionDiagnostic stopped(const std::string& node,uint32_t uptime,bool healthy=true){
    MotionDiagnostic d{node,healthy?"AGV_IDLE":"AGV_SAFE_STOP",healthy?"NONE":"LINE_LOST","STOP",false,true,true,0,0,0};
    d.encoderBoot=10;d.encoderConnection=1;d.encoderUptime=uptime;
    d.headingSupported=true;d.heading=node=="S2"?"N":"S";d.headingNode=node;d.headingSource="bay_default";
    d.lastCommandId=1000;return d;
}
void exercise(const std::string& id){
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager f(map,mutex);
    f.setActiveRobots({id});f.setAutomaticChargingEnabled(false);f.setNetworkAvailable(true);
    std::vector<json> commands;f.setPublishCallback([&](const std::string& topic,const std::string& wire){
        if(topic=="agv/"+id+"/command")commands.push_back(json::parse(wire));
    });
    f.updateTelemetry(id,"G1",80,false);CHECK(f.bindEncoderSession(id,10,1,100));
    CHECK(f.directGoto(id,"K1","resume-me").empty());const auto first=commands.back()["cmd_id"].get<long>();
    CHECK(f.mayRequestZone(id,"M1")==false);
    f.reportError(id,"LINE_LOST");CHECK(task(f,"resume-me")["resume_available"]==true);
    CHECK(mutex.requestZone("M2",id));WebCommands web(map,f,mutex);
    json recover={{"cmd","recover_node"},{"id",id},{"expected_node","G1"},{"physically_clear",true}};
    auto noConfirm=recover;noConfirm["physically_clear"]=false;CHECK(web.execute(noConfirm)["ok"]==false);
    auto d=stopped("G1",200,false);d.rfidAgeMs=251;f.observeMotionDiagnostic(id,d);
    CHECK(web.execute(recover)["ok"]==false);CHECK(mutex.currentOwner("M2")==id);
    d.rfidAgeMs=0;d.encoderUptime=300;f.observeMotionDiagnostic(id,d);
    d.heading="UNKNOWN";d.headingSource="unknown";d.encoderUptime=310;f.observeMotionDiagnostic(id,d);
    CHECK(web.execute(recover)["ok"]==false);CHECK(mutex.currentOwner("M2")==id);
    d.heading="S";d.headingSource="route";d.encoderUptime=320;f.observeMotionDiagnostic(id,d);
    recover["cmd"]="recover_vehicle";
    CHECK(web.execute(recover)["ok"]==true);CHECK(mutex.currentOwner("M2").empty());
    const auto reset=commands.back()["cmd_id"].get<long>();CHECK(commands.back()["cmd"]=="RESET_FAULT");
    f.reportAck(id,reset);CHECK(robot(f,id)["state"]=="ERROR");
    d=stopped("G1",400);d.lastCommandId=reset-1;f.observeMotionDiagnostic(id,d);CHECK(robot(f,id)["state"]=="ERROR");
    d.lastCommandId=reset;d.encoderUptime=500;f.observeMotionDiagnostic(id,d);CHECK(robot(f,id)["state"]=="IDLE");
    d.heading="UNKNOWN";d.headingSource="unknown";d.encoderUptime=510;f.observeMotionDiagnostic(id,d);
    const auto beforeResume=commands.size();
    CHECK(!f.resumeTask("resume-me","G1").empty());CHECK(commands.size()==beforeResume);
    CHECK(task(f,"resume-me")["resume_available"]==true);
    d.heading="S";d.headingSource="route";d.encoderUptime=520;f.observeMotionDiagnostic(id,d);
    CHECK(!f.resumeTask("resume-me","G1").empty());
    d.headingConfirmationSupported=true;d.encoderUptime=530;f.observeMotionDiagnostic(id,d);
    CHECK(f.confirmRecoveryHeading(id,"G1","S").empty());
    const auto headingId=commands.back()["cmd_id"].get<long>();CHECK(commands.back()["cmd"]=="CONFIRM_HEADING");
    f.reportAck(id,headingId);CHECK(robot(f,id)["heading_confirmation_required"]==true);
    d.headingSource="operator";d.headingCommandId=headingId;d.lastCommandId=headingId;d.encoderUptime=540;
    f.observeMotionDiagnostic(id,d);CHECK(robot(f,id)["heading_confirmation_required"]==false);
    CHECK(!f.resumeTask("resume-me","G2").empty());CHECK(f.resumeTask("resume-me","G1").empty());
    CHECK(commands.back()["cmd"]=="GOTO");CHECK(commands.back()["cmd_id"].get<long>()>first);
    CHECK(task(f,"resume-me")["status"]=="dispatched");
    f.reportError(id,"LINE_LOST");f.endShiftWork();CHECK(task(f,"resume-me")["status"]=="cancelled");
    f.setAcceptingTasks(true);CHECK(!f.resumeTask("resume-me","G1").empty());
    CHECK(mutex.requestZone("M2",id));const std::string home=id=="01"?"S1":"S2";
    f.updateTelemetry(id,home,80,false);d=stopped(home,600,false);d.motor="FORWARD";f.observeMotionDiagnostic(id,d);
    CHECK(mutex.currentOwner("M2")==id);CHECK(robot(f,id)["node"]==home);
    d.motor="STOP";d.encoderUptime=700;f.observeMotionDiagnostic(id,d);CHECK(mutex.currentOwner("M2").empty());
    CHECK(robot(f,id)["state"]=="ERROR");CHECK(robot(f,id)["uncertain_path"].empty());
    MapGraph m;m.loadDefaultDemoMap();MutexArbiter a(m.mutexZones());FleetManager fleet(m,a);
    fleet.setActiveRobots({id});fleet.setAutomaticChargingEnabled(false);
    const auto dir=std::filesystem::path(".qa/shift-recovery-20261028")/id;
    ShiftController shift(m,fleet,a,dir/"shift.json",{{id,home}},false,false,"",{id});
    fleet.updateTelemetry(id,home,80,false);shift.tick();CHECK(shift.start().empty());
    CHECK(fleet.directGoto(id,"K1","running").empty());fleet.addTask("K6",0,"pending");
    CHECK(!fleet.toJson()["pending_tasks"].empty());CHECK(shift.end().empty());
    CHECK(fleet.toJson()["pending_tasks"].empty());CHECK(task(fleet,"running")["status"]=="cancelled");
    CHECK(task(fleet,"pending")["status"]=="cancelled");CHECK(robot(fleet,id)["cancel_waiting_for_node"]==true);
    CHECK(!shift.snapshot()["can_start"].get<bool>());
    CHECK(fleet.bindEncoderSession(id,10,1,100));d=stopped(home,900);fleet.observeMotionDiagnostic(id,d);shift.tick();
    CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(shift.start().empty());CHECK(fleet.toJson()["pending_tasks"].empty());
    FleetManager restored(m,a);restored.restore(fleet.durableSnapshot());CHECK(task(restored,"running")["resume_available"]==false);
}
void warehouseRecovery(const std::string& id,const std::string& node){
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager f(map,mutex);
    f.setActiveRobots({id});f.setAutomaticChargingEnabled(false);f.setNetworkAvailable(true);
    std::vector<json> commands;f.setPublishCallback([&](const std::string& topic,const std::string& wire){
        if(topic=="agv/"+id+"/command")commands.push_back(json::parse(wire));
    });
    f.updateTelemetry(id,node,80,false);CHECK(f.bindEncoderSession(id,10,1,100));
    f.reportError(id,"Can xac minh RFID va duong robot truoc khi xoa loi");
    CHECK(f.toJson()["tasks"].empty());CHECK(mutex.requestZone("M2",id));
    auto d=stopped(node,200,false);d.heading="UNKNOWN";d.headingSource="unknown";
    d.headingSupported=false;d.rfidAgeMs=0;f.observeMotionDiagnostic(id,d);
    CHECK(!f.recoverVerifiedNode(id,node,true,true).empty());
    CHECK(!f.recoverVerifiedNode(id,node,false,false).empty());
    f.setAcceptingTasks(false);CHECK(!f.recoverVerifiedNode(id,node,false,true).empty());f.setAcceptingTasks(true);
    CHECK(!f.recoverVerifiedNode(id,node=="K1"?"K2":"K1",false,true).empty());
    d.rfidAgeMs=251;d.encoderUptime=210;f.observeMotionDiagnostic(id,d);
    CHECK(!f.recoverVerifiedNode(id,node,false,true).empty());
    d.rfidAgeMs=0;d.leftDuty=1;d.encoderUptime=220;f.observeMotionDiagnostic(id,d);
    CHECK(!f.recoverVerifiedNode(id,node,false,true).empty());CHECK(mutex.currentOwner("M2")==id);
    d.leftDuty=0;d.liftBusy=true;d.encoderUptime=230;f.observeMotionDiagnostic(id,d);
    CHECK(!f.recoverVerifiedNode(id,node,false,true).empty());
    d.liftBusy=false;d.encoderConnection=2;d.encoderUptime=240;f.observeMotionDiagnostic(id,d);
    CHECK(!f.recoverVerifiedNode(id,node,false,true).empty());
    d.encoderConnection=1;d.encoderUptime=250;f.observeMotionDiagnostic(id,d);
    const auto beforeReset=commands.size();
    CHECK(f.recoverVerifiedNode(id,node,false,true).empty());CHECK(commands.size()==beforeReset+1);
    CHECK(commands.back()["cmd"]=="RESET_FAULT");const auto reset=commands.back()["cmd_id"].get<long>();
    CHECK(mutex.currentOwner("M2").empty());CHECK(robot(f,id)["state"]=="ERROR");
    f.reportAck(id,reset);CHECK(robot(f,id)["state"]=="ERROR");
    d.state="AGV_IDLE";d.error="NONE";d.lastCommandId=reset-1;d.encoderUptime=300;f.observeMotionDiagnostic(id,d);
    CHECK(robot(f,id)["state"]=="ERROR");
    d.lastCommandId=reset;d.encoderUptime=400;f.observeMotionDiagnostic(id,d);
    CHECK(robot(f,id)["state"]=="IDLE");CHECK(robot(f,id)["heading"]=="UNKNOWN");
    CHECK(commands.size()==beforeReset+1);CHECK(f.toJson()["tasks"].empty());
    CHECK(!f.resumeTask("nonexistent",node).empty());
}
void confirmHeadingRecovery(const std::string& id){
    MapGraph map;map.loadDefaultDemoMap();MutexArbiter mutex(map.mutexZones());FleetManager f(map,mutex);
    f.setActiveRobots({id});f.setAutomaticChargingEnabled(false);f.setNetworkAvailable(true);
    std::vector<json> commands;f.setPublishCallback([&](const std::string& topic,const std::string& wire){
        if(topic=="agv/"+id+"/command")commands.push_back(json::parse(wire));
    });
    const std::string home=id=="01"?"S1":"S2",heading=id=="01"?"S":"N";
    f.updateTelemetry(id,home,80,false);CHECK(f.bindEncoderSession(id,10,1,100));
    auto d=stopped(home,200);f.observeMotionDiagnostic(id,d);
    CHECK(f.recoverVerifiedNode(id,home,true,true,true,true).empty());
    CHECK(robot(f,id)["heading_confirmation_required"]==true);
    CHECK(!f.directGoto(id,"K1").empty());
    CHECK(!f.confirmRecoveryHeading(id,home,heading).empty()); // unsupported firmware
    d.headingConfirmationSupported=true;d.encoderUptime=300;f.observeMotionDiagnostic(id,d);
    CHECK(!f.confirmRecoveryHeading(id,home,heading=="S"?"N":"S").empty());
    CHECK(!f.confirmRecoveryHeading(id,"G1",heading).empty());
    CHECK(f.confirmRecoveryHeading(id,home,heading).empty());
    const auto command=commands.back();const auto cmd=command["cmd_id"].get<long>();
    CHECK(command["cmd"]=="CONFIRM_HEADING");CHECK(command["node"]==home);CHECK(command["heading"]==heading);
    const auto count=commands.size();CHECK(!f.confirmRecoveryHeading(id,home,heading).empty());CHECK(commands.size()==count);
    f.reportAck(id,cmd);CHECK(robot(f,id)["heading_confirmation_pending"]==true);
    d.headingSource="operator";d.headingCommandId=cmd;d.lastCommandId=cmd;
    d.encoderUptime=300;f.observeMotionDiagnostic(id,d);CHECK(robot(f,id)["heading_confirmation_required"]==true);
    d.encoderUptime=400;d.headingCommandId=cmd+1;f.observeMotionDiagnostic(id,d);
    CHECK(robot(f,id)["heading_confirmation_required"]==true);
    d.headingCommandId=cmd;d.encoderConnection=2;d.encoderUptime=410;f.observeMotionDiagnostic(id,d);
    CHECK(robot(f,id)["heading_confirmation_required"]==true);
    d.encoderConnection=1;d.leftDuty=1;d.encoderUptime=420;f.observeMotionDiagnostic(id,d);
    CHECK(robot(f,id)["heading_confirmation_required"]==true);
    d.leftDuty=0;d.encoderUptime=500;f.observeMotionDiagnostic(id,d);
    CHECK(robot(f,id)["heading_confirmation_required"]==false);CHECK(robot(f,id)["heading"]==heading);
    CHECK(commands.size()==count); // confirmation itself never sends GOTO or servo
    CHECK(f.bindEncoderSession(id,10,2,600));
    CHECK(robot(f,id)["heading_confirmation_required"]==true);CHECK(!f.directGoto(id,"K1").empty());
}
int main(){try{for(const auto* id:{"01","02"}){exercise(id);confirmHeadingRecovery(id);for(int k=1;k<=6;++k)warehouseRecovery(id,"K"+std::to_string(k));}
    std::cout<<"Shift recovery 01/02 + K1-K6 fault-only recovery PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
