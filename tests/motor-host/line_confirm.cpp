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

static void following() {
    for (uint8_t pattern : {2,3,6}) {
        Rig r; r.start(nullptr,"S1","GS1"); CHECK(r.step(2,r.in.now)==MotorAction::Forward);
        const auto lost=r.in.now+1; CHECK(r.step(0,lost)==MotorAction::Stop); r.stopped();
        const auto confirm=lost+700; // Past the motor-search budget, still inside task grace.
        for (uint32_t dt=0;dt<100;dt+=20) { CHECK(r.step(pattern,confirm+dt)==MotorAction::Stop); r.stopped(); }
        CHECK(r.step(pattern,confirm+100)!=MotorAction::Stop); CHECK(r.nav.following());
        CHECK(r.motor.leftDuty()+r.motor.rightDuty()>0);
        CHECK(r.nav.routeCommandId()==900 && r.nav.pathIndex()==0 && std::strcmp(r.nav.nextNode(),"GS1")==0);
    }
    for (uint8_t pattern : {0,1,4,5,7}) {
        Rig r; r.start(nullptr,"S1","GS1"); r.step(2,r.in.now); const auto lost=r.in.now+1; r.step(0,lost);
        for (uint32_t dt=700;dt<=900;dt+=20) { r.step(pattern,lost+dt); CHECK(!r.nav.following()); r.stopped(); }
        r.step(pattern,lost+Config::LINE_LOSS_GRACE_MS); CHECK(r.nav.error()==ErrorCode::LineLost);
        r.step(2,r.in.now+1); r.stopped(); CHECK(!r.nav.active());
    }
    for (uint8_t pattern : {1,3,4,6}) {
        Rig r; r.start(nullptr,"K5","G5"); CHECK(r.step(2,r.in.now)==MotorAction::Backward);
        r.step(0,r.in.now+1); const auto at=r.in.now;
        for(uint32_t dt=20;dt<=200;dt+=20) { r.step(pattern,at+dt); CHECK(r.nav.recoveringLine()); }
        for(uint32_t dt=220;dt<320;dt+=20) CHECK(r.step(2,at+dt)==MotorAction::Stop);
        CHECK(r.step(2,at+320)==MotorAction::Backward); // Reverse still requires010.
    }
}

static void turnConfirmation() {
    for(bool right : {false,true}) for(uint8_t edge : {1,4}) {
        Rig r; r.start("M3","G1",right?"M5":"K1");
        if(right)CHECK(r.nav.grant("M5",20));
        CHECK(r.step(2,r.in.now)==(right?MotorAction::PivotRight:MotorAction::PivotLeft));
        const auto at=r.in.now;
        for(uint32_t dt=1;dt<=201;dt+=20) {
            r.step(edge,at+dt);
            CHECK(!r.nav.capturingTurnLine() && !r.nav.following());
        }
        // An edge may prove departure, but only a center-bearing return starts capture.
        CHECK(r.step(3,at+221)==MotorAction::Stop);
        CHECK(r.nav.capturingTurnLine()); r.stopped();
    }
    for(bool right : {false,true}) for(uint8_t pattern : {3,6}) for(bool wrap : {false,true}) {
        Rig r; const auto at=r.capture(right,pattern,wrap?UINT32_MAX-1050u:0u);
        for(uint32_t dt=0;dt<100;dt+=20) { CHECK(r.step(pattern,at+dt)==MotorAction::Stop); r.stopped(); CHECK(r.nav.capturingTurnLine()); }
        CHECK(r.step(pattern,at+100)==MotorAction::Stop); CHECK(r.nav.following()); r.stopped();
        CHECK(r.nav.settlingTurnLine() && r.nav.routeCommandId()==900 && r.nav.pathIndex()==0);
        CHECK(r.step(pattern,at+101)!=MotorAction::Stop);
        CHECK(r.motor.leftDuty()+r.motor.rightDuty()>0);
        CHECK(r.motor.leftDuty()<=r.nav.turnCapturePwm() && r.motor.rightDuty()<=r.nav.turnCapturePwm());
    }
    for(bool right : {false,true}) for(uint8_t edge : {1,4}) for(bool wrap : {false,true}) {
        Rig r; const auto at=r.capture(right,3,wrap?UINT32_MAX-1050u:0u);
        for(uint32_t dt=20;dt<=800;dt+=20) {
            const auto action=r.step(edge,at+dt);
            CHECK(r.nav.capturingTurnLine() && !r.nav.following());
            CHECK(action==MotorAction::Stop || action==MotorAction::PivotLeft || action==MotorAction::PivotRight);
            if(dt>=Config::LINE_RECOVERY_TIMEOUT_MS)r.stopped();
        }
        // Center-bearing confirmation must start afresh after edge-only evidence.
        for(uint32_t dt=820;dt<920;dt+=20) {
            CHECK(r.step(6,at+dt)==MotorAction::Stop); CHECK(r.nav.capturingTurnLine()); r.stopped();
        }
        CHECK(r.step(6,at+920)==MotorAction::Stop); CHECK(r.nav.following()); r.stopped();
    }
    Rig late; const auto at=late.capture();
    for(uint32_t dt=1;dt<=600;dt+=1) {
        late.step(0,at+dt); CHECK(late.nav.error()==ErrorCode::None);
        if(dt>=600)late.stopped();
    }
    for(uint32_t dt=650;dt<=730;dt+=20) { CHECK(late.step(6,at+dt)==MotorAction::Stop); late.stopped(); }
    CHECK(late.step(6,at+750)==MotorAction::Stop); CHECK(late.nav.following());

    Rig blank; const auto blankAt=blank.capture(); bool sawPivot=false;
    for(uint32_t dt=10;dt<Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS;dt+=10) {
        const auto action=blank.step(0,blankAt+dt);
        CHECK(blank.nav.error()==ErrorCode::None);
        if(dt>=Config::LINE_RECOVERY_TIMEOUT_MS) { CHECK(action==MotorAction::Stop); blank.stopped(); }
        else if(action!=MotorAction::Stop)sawPivot=true;
    }
    CHECK(sawPivot); blank.step(0,blankAt+Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS);
    CHECK(blank.nav.error()==ErrorCode::LineLost && !blank.nav.active());
    blank.step(2,blank.in.now+1); blank.stopped(); CHECK(blank.nav.error()==ErrorCode::LineLost);
}

static void noiseAndHazards() {
    {
        // Neither alternating nor sustained edge-only samples can resume the route.
        Rig r; r.start(nullptr,"S1","GS1"); r.step(2,r.in.now); r.step(0,r.in.now+1);
        const auto at=r.in.now;
        for(uint32_t dt=20;dt<=180;dt+=20) {
            CHECK(r.step(((dt/20)&1u)?4:1,at+dt)==MotorAction::Stop);
            CHECK(r.nav.recoveringLine()); r.stopped();
        }
        for(uint32_t dt=200;dt<300;dt+=20) {
            CHECK(r.step(1,at+dt)==MotorAction::Stop); CHECK(r.nav.recoveringLine()); r.stopped();
        }
        CHECK(r.step(1,at+300)==MotorAction::Stop); CHECK(r.nav.recoveringLine()); r.stopped();
        for(uint32_t dt=320;dt<420;dt+=20) {
            CHECK(r.step(3,at+dt)==MotorAction::Stop); CHECK(r.nav.recoveringLine()); r.stopped();
        }
        CHECK(r.step(3,at+420)!=MotorAction::Stop); CHECK(r.nav.following());
    }
    // Following recovery must discard stale/pre-hazard line evidence as well.
    for(bool obstacle : {false,true}) {
        Rig r; r.start("K1","G1","M5"); CHECK(r.nav.grant("M5",21));
        CHECK(r.step(2,r.in.now)==MotorAction::Forward);
        r.step(0,r.in.now+1); const auto at=r.in.now;
        for(uint32_t dt=20;dt<=80;dt+=20) { r.step(3,at+dt); r.stopped(); }
        if(obstacle)r.in.obstacle=true; else r.nav.revokeGrant("M5",21);
        r.step(3,at+100); r.stopped(); CHECK(r.nav.recoveringLine());
        if(obstacle)r.in.obstacle=false; else CHECK(r.nav.grant("M5",22));
        for(uint32_t dt=120;dt<220;dt+=20) { r.step(6,at+dt); CHECK(r.nav.recoveringLine()); r.stopped(); }
        CHECK(r.step(3,at+220)!=MotorAction::Stop); CHECK(r.nav.following());
    }
    {
        Rig r; r.start(nullptr,"S1","GS1"); r.step(2,r.in.now); r.step(0,r.in.now+1);
        const auto at=r.in.now;
        for(uint32_t dt : {100u,160u,220u}) { r.step(3,at+dt); CHECK(r.nav.recoveringLine()); r.stopped(); }
        r.step(7,at+240); r.step(6,at+260);
        for(int i=0;i<10;++i)r.step(6,at+260);
        CHECK(r.nav.recoveringLine());
        for(uint32_t dt=280;dt<360;dt+=20) { r.step(6,at+dt); CHECK(r.nav.recoveringLine()); }
        CHECK(r.step(6,at+360)!=MotorAction::Stop); CHECK(r.nav.following());
    }
    for(uint8_t invalid : {0,1,4,5,7}) {
        Rig r; const auto at=r.capture();
        for(uint32_t dt=0;dt<=60;dt+=20)r.step(3,at+dt);
        r.step(invalid,at+80); r.step(3,at+100); CHECK(r.nav.capturingTurnLine());
        for(int repeat=0;repeat<10;++repeat)r.step(3,at+100);
        CHECK(r.nav.capturingTurnLine());
        r.step(3,at+160); r.step(3,at+220); CHECK(r.nav.capturingTurnLine()); // Stale gaps reset evidence.
        for(uint32_t dt=240;dt<=300;dt+=20) { r.step(3,at+dt); CHECK(r.nav.capturingTurnLine()); }
        r.step(3,at+320); CHECK(r.nav.following());
    }
    for(int hazard=0;hazard<7;++hazard) {
        Rig r; const auto at=r.capture(); for(uint32_t dt=0;dt<=60;dt+=20)r.step(3,at+dt);
        if(hazard==0)r.in.wifi=false; if(hazard==1)r.in.mqtt=false;
        if(hazard==2)r.in.distanceValid=false; if(hazard==3)r.in.motionConfigured=false;
        if(hazard==4)r.in.lineConfigured=false; if(hazard==5)r.nav.stop(); if(hazard==6)r.in.obstacle=true;
        CHECK(r.step(3,at+80)==MotorAction::Stop); r.stopped();
        if(hazard<6) { CHECK(!r.nav.active()); r.step(2,at+100); r.stopped(); }
        else {
            CHECK(r.nav.obstaclePaused()); r.in.obstacle=false;
            for(uint32_t dt=100;dt<200;dt+=20) { CHECK(r.step(3,at+dt)==MotorAction::Stop); CHECK(r.nav.capturingTurnLine()); }
            r.step(3,at+200); CHECK(r.nav.following());
        }
    }
    Rig zone; zone.start("G2","M5","M6"); CHECK(zone.nav.grant("M5",10)); CHECK(zone.nav.grant("M6",11));
    CHECK(zone.step(2,zone.in.now)==MotorAction::PivotLeft); zone.step(0,zone.in.now+1); zone.step(6,zone.in.now+1);
    const auto at=zone.in.now; for(uint32_t dt=0;dt<=60;dt+=20)zone.step(6,at+dt);
    zone.nav.revokeGrant("M6",11); CHECK(zone.step(6,at+80)==MotorAction::Stop); zone.stopped();
    CHECK(zone.nav.grant("M6",12));
    for(uint32_t dt=100;dt<200;dt+=20) { zone.step(6,at+dt); CHECK(zone.nav.capturingTurnLine()); zone.stopped(); }
    zone.step(6,at+200); CHECK(zone.nav.following());
    zone.nav.revokeGrant("M6",12); zone.step(6,at+201); zone.stopped();
}

int main() {
    try {
        static_assert(Config::LINE_RECOVERY_TRACK_CONFIRM,"Profiles must opt into tracked-line confirmation");
        following(); turnConfirmation(); noiseAndHazards();
        std::cout<<"PASS "<<Config::AGV_ID<<": 010/011/110 fresh stable recovery; 001/100 cannot start/finish capture or resume recovery; LEFT/RIGHT and wrap; "
                 <<"600ms motor budget/1500ms confirmation; PWM cap; reverse stays centered; "
                 <<"noise/stale/wide/blank/STOP/network/sonar/grant/obstacle; no fault replay\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
