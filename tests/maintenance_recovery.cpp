// Core regression only. Does not compile or run firmware.
#define main previousRecoveryMain
#include "recovery-conflicts.cpp"
#undef main
int main(){try {
    for(const std::string id:{"01","02"}) {
        Fixture f(id,false);f.fleet.reportError(id,"LINE_LOST");
        f.d.state="AGV_SAFE_STOP";f.d.error="LINE_LOST";f.observe();
        CHECK(!f.fleet.recoverAndContinue(id,f.node,"?").empty());
        CHECK(f.fleet.recoverAndContinue(id,f.node,f.d.heading).empty());
        CHECK(f.sent.back()["cmd"]=="RESET_FAULT");
        const auto reset=f.sent.back()["cmd_id"].get<long>();f.fleet.reportAck(id,reset);
        CHECK(f.robot()["state"]=="ERROR");
        f.d.state="AGV_IDLE";f.d.error="NONE";f.d.lastCommandId=reset;f.observe();f.fleet.tick();
        CHECK(f.sent.back()["cmd"]=="CONFIRM_HEADING");
        const auto heading=f.sent.back()["cmd_id"].get<long>();f.finish(heading);
        CHECK(f.robot()["heading_confirmation_required"]==false);
        CHECK(f.robot()["recovery_auto_resume"]==true);
        f.fleet.reportError(id,"NEW_FAULT");CHECK(f.robot()["recovery_auto_resume"]==false);
        Fixture home(id);home.d.maintenanceRecoverySupported=true;home.d.maintenanceReady=true;home.d.cargoSensorPresent=false;home.observe();
        CHECK(home.robot()["heading_confirmation_required"]==true);
        for(int i=0;i<5;++i){std::this_thread::sleep_for(std::chrono::milliseconds(450));
            home.fleet.updateTelemetry(id,home.node,80,false);home.observe();}
        CHECK(home.sent.back()["cmd"]=="CONFIRM_HEADING");
        home.finish(home.sent.back()["cmd_id"].get<long>());
        CHECK(home.robot()["heading_confirmation_required"]==false);
        CHECK(home.robot()["recovery_auto_resume"]==false);
        home.d.liftCommandId=900;home.d.liftState="WAITING_LIFT_RECOVERY";home.observe();
        CHECK(home.sent.back()["cmd"]=="RESET_FAULT");
        CHECK(home.robot()["recovery_reset_pending"]==true);
        home.d.lastCommandId=home.sent.back()["cmd_id"].get<long>();home.d.liftState="IDLE";home.observe();
        CHECK(home.robot()["recovery_reset_pending"]==false);
        CHECK(home.robot()["virtual_cargo_known"]==true);
        Fixture lift(id,false);lift.d.liftRecoverySupported=true;lift.observe();long operation=0;
        CHECK(lift.fleet.startLift(id,lift.node,true,operation).empty());
        lift.fleet.reportOffline(id);CHECK(lift.robot()["lift"]["waiting_recovery"]==true);
        CHECK(lift.fleet.bindEncoderSession(id,7,2,500));lift.fleet.updateTelemetry(id,lift.node,80,false);
        CHECK(lift.sent.back()["cmd"]=="WAIT");CHECK(lift.sent.back()["preserve_lift"]==true);
        lift.d.encoderConnection=2;lift.d.encoderUptime=600;lift.d.liftCommandId=operation;
        lift.d.liftState="DONE_UP";lift.d.liftElapsedMs=5000;lift.d.cargoKnown=true;lift.d.virtualCargo=true;
        lift.d.lastCommandId=operation;lift.observe();
        CHECK(lift.robot()["lift"]["active"]==false);CHECK(lift.robot()["lift"]["state"]=="DONE_UP");
        CHECK(lift.robot()["virtual_cargo"]==true);
        lift.fleet.observeMotionDiagnostic("not-registered",lift.d);
        Fixture moved(id,false);moved.fleet.reportError(id,"LINE_LOST");moved.observe();
        CHECK(moved.fleet.recoverAndContinue(id,moved.node,moved.d.heading).empty());
        moved.fleet.updateTelemetry(id,"G1",80,false);moved.fleet.tick();
        CHECK(moved.robot()["recovery_auto_resume"]==false);
    }
    std::cout<<"PASS maintenance and one-submit recovery for 01/02\n";return previousRecoveryMain();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
