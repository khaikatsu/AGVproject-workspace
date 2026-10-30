// Core only; does not compile, test or flash firmware.
#define main recoveryConflictsMain
#include "recovery-conflicts.cpp"
#undef main

int main(){try{
    for(const std::string id:{"01","02"}) {
        Fixture home(id);home.d.headingSource="bay_default";
        home.d.maintenanceRecoverySupported=true;home.d.maintenanceReady=false;
        home.d.rfidAgeMs=6000;const auto before=home.sent.size();home.observe();
        CHECK(home.robot()["heading_confirmation_required"]==false);
        CHECK(home.robot()["heading"]==PhysicalLayout::outwardHeading(home.node));
        CHECK(home.robot()["state"]=="IDLE");CHECK(home.sent.size()==before);
        CHECK(home.robot()["cargo_sensor"]["present"]==true);
        CHECK(home.robot()["virtual_cargo"]==false);
        const auto file=std::filesystem::path(".qa")/("home-heading-"+id+std::to_string(Clock::now().time_since_epoch().count()))/"shift.json";
        ShiftController shift(home.map,home.fleet,home.mutex,file,{{id,home.node}},false,false,"",{id});
        home.observe();shift.tick();CHECK(shift.snapshot()["phase"]=="CLOSED");CHECK(shift.snapshot()["can_start"]==true);
        CHECK(home.sent.size()==before);CHECK(!home.fleet.acceptingTasks());
        // A fresh bound sample after reconnect must repair the bay pose again.
        CHECK(home.fleet.bindEncoderSession(id,7,2,home.d.encoderUptime+10));
        home.d.encoderConnection=2;home.d.encoderUptime+=20;home.fleet.updateTelemetry(id,home.node,80,false);
        home.observe();CHECK(home.robot()["state"]=="IDLE");
        CHECK(home.robot()["heading_confirmation_required"]==false);

        Fixture restored(id);const auto checkpoint=restored.fleet.durableSnapshot();
        restored.fleet.restore(checkpoint);restored.fleet.updateTelemetry(id,restored.node,80,false);
        CHECK(restored.fleet.bindEncoderSession(id,7,1,300));
        restored.d.encoderUptime=400;restored.d.headingSource="bay_default";restored.d.rfidAgeMs=6000;
        restored.d.maintenanceRecoverySupported=true;restored.d.maintenanceReady=false;
        restored.observe();CHECK(restored.robot()["state"]=="IDLE");
        CHECK(restored.robot()["heading_confirmation_required"]==false);

        for(const std::string scenario:{"other_bay","moving","fault","task","lift","wrong_boot","unknown_rfid","wrong_heading","other_node"}) {
            Fixture blocked(id,scenario!="other_node");blocked.d.headingSource="bay_default";
            CHECK(blocked.fleet.bindEncoderSession(id,7,2,500));
            blocked.d.encoderConnection=2;blocked.d.encoderUptime=600;
            blocked.d.maintenanceRecoverySupported=true;blocked.d.maintenanceReady=false;blocked.d.rfidAgeMs=6000;
            if(scenario=="other_bay"){blocked.node=id=="01"?"S2":"S1";blocked.fleet.updateTelemetry(id,blocked.node,80,false);blocked.d.node=blocked.d.headingNode=blocked.node;}
            if(scenario=="moving")blocked.d.leftDuty=80;
            if(scenario=="fault"){blocked.fleet.reportError(id,"LINE_LOST");blocked.d.error="LINE_LOST";blocked.d.state="AGV_SAFE_STOP";}
            if(scenario=="task")blocked.d.taskActive=true;
            if(scenario=="lift")blocked.d.liftBusy=true;
            if(scenario=="wrong_boot")blocked.d.encoderBoot=88;
            if(scenario=="unknown_rfid")blocked.d.rfidAgeMs=UINT32_MAX;
            if(scenario=="wrong_heading")blocked.d.heading=id=="01"?"N":"S";
            blocked.observe();
            if(blocked.robot()["heading_confirmation_required"]!=true)throw std::runtime_error(id+" unexpectedly unlocked: "+scenario);
        }
        Fixture docked(id);docked.d.headingSource="route";docked.d.stationReady=true;docked.observe();
        CHECK(docked.robot()["heading_confirmation_required"]==false);
        Fixture unknown(id);unknown.d.heading="UNKNOWN";unknown.d.headingSource="unknown";
        unknown.d.maintenanceRecoverySupported=true;unknown.d.maintenanceReady=false;unknown.d.rfidAgeMs=6000;
        const auto unknownBefore=unknown.sent.size();unknown.observe();CHECK(unknown.sent.size()==unknownBefore);
        unknown.d.rfidAgeMs=0;unknown.observe();CHECK(unknown.sent.back()["cmd"]=="CONFIRM_HEADING");
        CHECK(unknown.sent.back()["heading"]==PhysicalLayout::outwardHeading(unknown.node));
        const auto confirmation=unknown.sent.back()["cmd_id"].get<long>();
        CHECK(unknown.robot()["heading_confirmation_required"]==true);
        unknown.fleet.reportAck(id,confirmation);CHECK(unknown.robot()["heading_confirmation_required"]==true);
        unknown.d.rfidAgeMs=6000;unknown.d.heading=PhysicalLayout::outwardHeading(unknown.node);unknown.finish(confirmation);
        CHECK(unknown.robot()["heading_confirmation_required"]==false);
        CHECK(unknown.robot()["heading"]==PhysicalLayout::outwardHeading(unknown.node));
        CHECK(unknown.robot()["recovery_auto_resume"]==false);
    }
    std::cout<<"PASS healthy bay heading 01/02: no manual command, old RFID anchor, maintenance false, no cargo changes; unsafe/mid-route cases remain blocked\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
