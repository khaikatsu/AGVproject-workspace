#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(std::string(#c)+" at line "+std::to_string(__LINE__)); } while (false)

struct Rig {
    MotorModule motor;
    LineFollowModule line{motor};
    BuzzerModule buzzer;
    MovementModule move{motor,line,buzzer};
    CoreNavigator nav;
    ControlInput in;
    Rig() {
        motor.begin(); line.begin(); buzzer.begin();
        in.wifi=in.mqtt=in.distanceValid=in.motionConfigured=in.lineConfigured=true;
    }
    void start(const char* previous,const char* current,const char* target,uint32_t at=0) {
        CoreCommand c; c.type=CoreCommandType::Goto; c.cmdId=900; c.pathLength=1;
        std::strcpy(c.path[0],target); std::strcpy(c.target,target);
        RoutePlan route; CHECK(RoutePlanner::plan(previous,current,c,route));
        CHECK(nav.start(current,c,route,at)); in.now=at+Config::RFID_STOP_DWELL_MS; in.linePattern=2;
    }
    MotorAction step(uint8_t pattern,uint32_t at) {
        in.now=at; in.linePattern=pattern;
        const auto action=nav.update(in);
        move.apply(action,nav.precisionPivot(),nav.precisionPivotPwm(),nav.settlingTurnLine(),
                   {nav.forwardLineTracking(),pattern,at});
        return action;
    }
    void stopped() { CHECK(motor.leftDuty()==0 && motor.rightDuty()==0); }
    uint32_t capture(bool right=false,uint8_t pattern=3,uint32_t at=0) {
        start("M3","G1",right?"M5":"K1",at);
        if(right)CHECK(nav.grant("M5",20));
        CHECK(step(2,in.now)==(right?MotorAction::PivotRight:MotorAction::PivotLeft));
        step(0,in.now+1); step(pattern,in.now+1);
        CHECK(nav.capturingTurnLine()); stopped(); return in.now;
    }
};

constexpr uint32_t ConfirmMs=Config::LINE_RECOVERY_CENTER_MS;

static void following() {
    for(uint8_t pattern:{2,3,6}) {
        Rig r;r.start(nullptr,"S1","GS1");CHECK(r.step(2,r.in.now)==MotorAction::Forward);
        const auto lost=r.in.now+1;r.step(0,lost);
        const auto first=lost+700;
        for(uint32_t dt=0;dt<ConfirmMs;dt+=5) {
            CHECK(r.step(pattern,first+dt)==MotorAction::Stop);r.stopped();CHECK(!r.nav.following());
        }
        CHECK(r.step(pattern,first+ConfirmMs)!=MotorAction::Stop);CHECK(r.nav.following());
        CHECK(r.nav.routeCommandId()==900 && r.nav.pathIndex()==0);
    }
    for(uint8_t pattern:{0,1,4,5,7}) {
        Rig r;r.start(nullptr,"S1","GS1");r.step(2,r.in.now);
        const auto lost=r.in.now+1;r.step(0,lost);
        for(uint32_t dt=700;dt<=900;dt+=20) {r.step(pattern,lost+dt);CHECK(!r.nav.following());r.stopped();}
        r.step(pattern,lost+Config::LINE_LOSS_GRACE_MS);CHECK(r.nav.error()==ErrorCode::LineLost);
        r.step(2,r.in.now+1);r.stopped();CHECK(!r.nav.active());
    }
    for(uint8_t pattern:{1,3,4,6}) {
        Rig r;r.start(nullptr,"K5","G5");CHECK(r.step(2,r.in.now)==MotorAction::Backward);
        r.step(0,r.in.now+1);const auto at=r.in.now;
        for(uint32_t dt=5;dt<=40;dt+=5) {r.step(pattern,at+dt);CHECK(r.nav.recoveringLine());}
        const auto first=at+700;
        for(uint32_t dt=0;dt<ConfirmMs;dt+=5)CHECK(r.step(2,first+dt)==MotorAction::Stop);
        CHECK(r.step(2,first+ConfirmMs)==MotorAction::Backward);
    }
}

static void turnConfirmation() {
    for(bool right:{false,true}) for(uint8_t edge:{1,4}) {
        Rig r;r.start("M3","G1",right?"M5":"K1");if(right)CHECK(r.nav.grant("M5",20));
        r.step(2,r.in.now);const auto began=r.in.now;
        for(uint32_t dt=1;dt<=201;dt+=20) {
            r.step(edge,began+dt);CHECK(!r.nav.capturingTurnLine() && !r.nav.following());
        }
        CHECK(r.step(3,began+221)==MotorAction::Stop);CHECK(r.nav.capturingTurnLine());r.stopped();
    }
    for(bool right:{false,true}) for(uint8_t pattern:{3,6}) for(bool wrap:{false,true}) {
        Rig r;const auto at=r.capture(right,pattern,wrap?UINT32_MAX-1050u:0u);
        for(uint32_t dt=0;dt<ConfirmMs;dt+=5) {
            CHECK(r.step(pattern,at+dt)==MotorAction::Stop);CHECK(r.nav.capturingTurnLine());r.stopped();
        }
        CHECK(r.step(pattern,at+ConfirmMs)==MotorAction::Stop);CHECK(r.nav.following());r.stopped();
        CHECK(r.nav.settlingTurnLine());CHECK(r.step(pattern,at+ConfirmMs+1)!=MotorAction::Stop);
        CHECK(r.motor.leftDuty()<=r.nav.turnCapturePwm() && r.motor.rightDuty()<=r.nav.turnCapturePwm());
    }
    for(bool right:{false,true}) for(uint8_t edge:{1,4}) {
        Rig r;const auto at=r.capture(right);
        for(uint32_t dt=5;dt<=800;dt+=5) {
            r.step(edge,at+dt);CHECK(r.nav.capturingTurnLine());
            if(dt>=Config::LINE_RECOVERY_TIMEOUT_MS)r.stopped();
        }
        for(uint32_t dt=0;dt<ConfirmMs;dt+=5) {r.step(6,at+805+dt);CHECK(!r.nav.following());r.stopped();}
        r.step(6,at+805+ConfirmMs);CHECK(r.nav.following());
    }
    Rig blank;const auto at=blank.capture();bool pivoted=false;
    for(uint32_t dt=5;dt<Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS;dt+=5) {
        const auto action=blank.step(0,at+dt);
        if(dt>=Config::LINE_RECOVERY_TIMEOUT_MS) {CHECK(action==MotorAction::Stop);blank.stopped();}
        else if(action!=MotorAction::Stop)pivoted=true;
    }
    CHECK(pivoted);blank.step(0,at+Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS);
    CHECK(blank.nav.error()==ErrorCode::LineLost);blank.step(2,blank.in.now+1);blank.stopped();
}

static void noiseAndHazards() {
    for(uint8_t invalid:{0,1,4,5,7}) {
        Rig r;const auto at=r.capture();
        for(uint32_t dt=0;dt<=30;dt+=5)r.step(3,at+dt);
        r.step(invalid,at+35);r.step(3,at+40);
        for(int i=0;i<20;++i)r.step(3,at+40);
        CHECK(!r.nav.following());r.step(3,at+100);CHECK(!r.nav.following());
        for(uint32_t dt=5;dt<ConfirmMs;dt+=5) {r.step(3,at+100+dt);CHECK(!r.nav.following());r.stopped();}
        r.step(3,at+100+ConfirmMs);CHECK(r.nav.following());
    }
    for(int hazard=0;hazard<7;++hazard) {
        Rig r;const auto at=r.capture();for(uint32_t dt=0;dt<=30;dt+=5)r.step(3,at+dt);
        if(hazard==0)r.in.wifi=false;if(hazard==1)r.in.mqtt=false;
        if(hazard==2)r.in.distanceValid=false;if(hazard==3)r.in.motionConfigured=false;
        if(hazard==4)r.in.lineConfigured=false;if(hazard==5)r.nav.stop();if(hazard==6)r.in.obstacle=true;
        CHECK(r.step(3,at+35)==MotorAction::Stop);r.stopped();CHECK(!r.nav.following());
        if(hazard<2 || hazard==6) {
            if(hazard<2)CHECK(r.nav.networkPaused());else CHECK(r.nav.obstaclePaused());
            r.in.wifi=r.in.mqtt=true;r.in.obstacle=false;
            for(uint32_t dt=0;dt<ConfirmMs;dt+=5) {r.step(6,at+40+dt);CHECK(!r.nav.following());r.stopped();}
            r.step(6,at+40+ConfirmMs);CHECK(r.nav.following());
        } else {CHECK(!r.nav.active());r.step(2,at+100);r.stopped();}
    }
    Rig zone;zone.start("G2","M5","M6");CHECK(zone.nav.grant("M5",10));CHECK(zone.nav.grant("M6",11));
    zone.step(2,zone.in.now);zone.step(0,zone.in.now+1);zone.step(6,zone.in.now+1);
    const auto at=zone.in.now;for(uint32_t dt=0;dt<=30;dt+=5)zone.step(6,at+dt);
    zone.nav.revokeGrant("M6",11);zone.step(6,at+35);zone.stopped();CHECK(!zone.nav.following());
    CHECK(zone.nav.grant("M6",12));
    for(uint32_t dt=0;dt<ConfirmMs;dt+=5) {zone.step(6,at+40+dt);CHECK(!zone.nav.following());zone.stopped();}
    zone.step(6,at+40+ConfirmMs);CHECK(zone.nav.following());
    zone.nav.revokeGrant("M6",12);zone.step(6,zone.in.now+1);zone.stopped();

    // Following must also discard pre-obstacle/grant evidence after blank line.
    for(bool obstacle:{false,true}) {
        Rig r;r.start("K1","G1","M5");CHECK(r.nav.grant("M5",21));r.step(2,r.in.now);
        r.step(0,r.in.now+1);const auto at=r.in.now;
        for(uint32_t dt=5;dt<=30;dt+=5)r.step(3,at+dt);
        if(obstacle)r.in.obstacle=true;else r.nav.revokeGrant("M5",21);
        r.step(3,at+35);r.stopped();CHECK(r.nav.recoveringLine());
        if(obstacle)r.in.obstacle=false;else CHECK(r.nav.grant("M5",22));
        for(uint32_t dt=0;dt<ConfirmMs;dt+=5) {r.step(6,at+40+dt);CHECK(r.nav.recoveringLine());r.stopped();}
        r.step(6,at+40+ConfirmMs);CHECK(r.nav.following());
    }
}

int main() {
    try {
        static_assert(Config::LINE_RECOVERY_TRACK_CONFIRM,"Run with two-robot profiles");
        following();turnConfirmation();noiseAndHazards();
        std::cout<<"PASS "<<Config::AGV_ID<<": ordinary line window "<<ConfirmMs
                 <<"ms; narrow/edge/noise/stale/LEFT/RIGHT/wrap, reverse010, 600/1500ms, "
                   "PWM cap/STOP/network pause/sonar/grant/obstacle/fault\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
