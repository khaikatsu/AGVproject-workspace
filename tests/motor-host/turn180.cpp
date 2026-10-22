#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(std::string(#c)+" at line "+std::to_string(__LINE__)); } while(false)

struct Rig {
    MotorModule motor;
    LineFollowModule line{motor};
    BuzzerModule buzzer;
    MovementModule move{motor,line,buzzer};
    CoreNavigator nav;
    ControlInput in;
    uint32_t began=0;
    Rig() {
        motor.begin(); buzzer.begin();
        in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    }
    MotorAction step(uint8_t pattern,uint32_t at) {
        in.now=at;in.linePattern=pattern;
        const auto action=nav.update(in);
        move.apply(action,nav.precisionPivot(),nav.precisionPivotPwm(),nav.settlingTurnLine(),
                   {nav.forwardLineTracking(),pattern,at});
        return action;
    }
    void stopped() { CHECK(motor.leftDuty()==0 && motor.rightDuty()==0); }
    void start(bool two=false,uint32_t at=0) {
        CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=1300;c.pathLength=1;
        std::strcpy(c.path[0],two?"M5":"GS1");std::strcpy(c.target,c.path[0]);
        RoutePlan route;
        CHECK(RoutePlanner::plan(two?"K1":"G1",two?"G1":"M1",c,route,true));
        CHECK(route.departure[0]==RouteTurn::TurnAround && route.turnCrossings[0]==(two?2:1));
        CHECK(nav.start(two?"G1":"M1",c,route,at));
        CHECK(nav.grant(two?"M5":"M1",31));
        began=at+Config::RFID_STOP_DWELL_MS;
        CHECK(step(2,began)==MotorAction::PivotRight);CHECK(nav.fastTurnSampling());
    }
    uint32_t candidate(uint8_t pattern=3) {
        const auto blank=began+Config::TURN_MIN_MS+2;
        CHECK(step(0,blank)==MotorAction::PivotRight);
        const auto at=blank+Config::TURN_LINE_SAMPLE_MS;
        in.now=at;in.linePattern=pattern;
        CHECK(nav.observeTurnLine(in)); // Same early observation called BEFORE RFID in the controller.
        move.stop();stopped();CHECK(nav.capturingTurnAroundLine());
        CHECK(nav.state()==AGV_MOVING && !nav.following());
        return at;
    }
};

static void fastStopAndConfirm() {
    for(uint8_t pattern:{2,3,6}) for(bool wrap:{false,true}) {
        Rig r;r.start(false,wrap?UINT32_MAX-1100u:0u);
        const auto at=r.candidate(pattern);
        CHECK(r.nav.fastTurnSampling()); // Controller defers RFID until STOP confirmation finishes.
        for(uint32_t dt=2;dt<100;dt+=2) {
            CHECK(r.step(pattern,at+dt)==MotorAction::Stop);r.stopped();CHECK(!r.nav.following());
        }
        CHECK(r.step(pattern,at+100)==MotorAction::Stop);r.stopped();
        CHECK(r.nav.following() && !r.nav.fastTurnSampling() && r.nav.settlingTurnLine());
        CHECK(r.nav.routeCommandId()==1300 && r.nav.pathIndex()==0);
        CHECK(r.step(pattern,at+102)!=MotorAction::Stop);
        CHECK(r.motor.leftDuty()>=0 && r.motor.rightDuty()>=0);
        CHECK(r.motor.leftDuty()<=Config::LINE_RECOVERY_PWM && r.motor.rightDuty()<=Config::LINE_RECOVERY_PWM);
    }
}

static void countBranchesAndRejectEdges() {
    Rig r;r.start(true);const auto at=r.began;
    r.step(0,at+102);
    CHECK(r.step(3,at+104)==MotorAction::PivotRight); // Intermediate 90-degree branch.
    CHECK(!r.nav.capturingTurnAroundLine() && !r.nav.following());
    for(uint32_t dt=106;dt<=224;dt+=2) {
        CHECK(r.step(6,at+dt)==MotorAction::PivotRight);CHECK(!r.nav.following());
    }
    r.step(0,at+226);CHECK(r.step(6,at+228)==MotorAction::Stop);r.stopped();
    CHECK(r.nav.capturingTurnAroundLine());
    for(uint32_t dt=230;dt<328;dt+=2) {r.step(3,at+dt);r.stopped();}
    CHECK(r.step(3,at+328)==MotorAction::Stop && r.nav.following());r.stopped();

    for(uint8_t invalid:{1,4,5,7}) {
        Rig edge;edge.start();edge.step(0,edge.began+102);
        for(uint32_t dt=104;dt<400;dt+=2) {
            CHECK(edge.step(invalid,edge.began+dt)==MotorAction::PivotRight);
            CHECK(!edge.nav.capturingTurnAroundLine() && !edge.nav.following());
        }
        CHECK(edge.step(6,edge.began+400)==MotorAction::Stop);edge.stopped();
    }
}

static void noiseAndDeadline() {
    for(uint8_t invalid:{0,1,4,5,7}) {
        Rig r;r.start();const auto at=r.candidate();
        for(uint32_t dt=2;dt<=60;dt+=2)r.step(3,at+dt);
        r.step(invalid,at+62);r.stopped();
        for(uint32_t dt=64;dt<164;dt+=2) {r.step(6,at+dt);r.stopped();CHECK(!r.nav.following());}
        r.step(6,at+164);CHECK(r.nav.following());r.stopped();
    }
    Rig stale;stale.start();const auto at=stale.candidate();
    for(int i=0;i<100;++i)stale.step(3,at);CHECK(!stale.nav.following());
    stale.step(3,at+80);stale.step(3,at+160);CHECK(!stale.nav.following());
    CHECK(stale.nav.turnMaxSampleGapMs()>=80);
    for(uint32_t dt=162;dt<260;dt+=2) {stale.step(3,at+dt);CHECK(!stale.nav.following());}
    stale.step(3,at+260);CHECK(stale.nav.following());

    Rig timeout;timeout.start();const auto captured=timeout.candidate();
    timeout.step(1,captured+2);timeout.stopped();
    CHECK(timeout.step(1,timeout.began+Config::TURN_TIMEOUT_MS)==MotorAction::Stop);
    CHECK(timeout.nav.error()==ErrorCode::TurnTimeout && !timeout.nav.active());
    timeout.step(2,timeout.in.now+2);timeout.stopped();CHECK(!timeout.nav.active());
}

static void safety() {
    for(int hazard=0;hazard<7;++hazard) {
        Rig r;r.start();const auto at=r.candidate();
        for(uint32_t dt=2;dt<=60;dt+=2)r.step(3,at+dt);
        if(hazard==0)r.in.wifi=false;if(hazard==1)r.in.mqtt=false;
        if(hazard==2)r.in.distanceValid=false;if(hazard==3)r.in.motionConfigured=false;
        if(hazard==4)r.in.lineConfigured=false;if(hazard==5)r.nav.stop();
        if(hazard==6)r.in.obstacle=true;
        CHECK(r.step(3,at+62)==MotorAction::Stop);r.stopped();CHECK(!r.nav.following());
        if(hazard<6)CHECK(!r.nav.active());
        else {
            r.in.obstacle=false;
            for(uint32_t dt=64;dt<164;dt+=2) {r.step(6,at+dt);CHECK(!r.nav.following());r.stopped();}
            r.step(6,at+164);CHECK(r.nav.following());r.stopped();
        }
    }
    Rig grant;grant.start();const auto at=grant.candidate();
    grant.nav.revokeGrant("M1",31);grant.step(3,at+100);grant.stopped();CHECK(!grant.nav.following());
    CHECK(grant.nav.grant("M1",32));
    for(uint32_t dt=102;dt<202;dt+=2) {grant.step(3,at+dt);CHECK(!grant.nav.following());}
    grant.step(3,at+202);CHECK(grant.nav.following());grant.stopped();
    Rig pivot;pivot.start();pivot.nav.revokeGrant("M1",31);
    CHECK(pivot.step(0,pivot.began+102)==MotorAction::Stop);pivot.stopped();
}

static void stationStop() {
    // Exercise the compatible front-in station turn even with reverse-dock profiles.
    for(const char* station:{"S1","S2"}) {
        Rig r;CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=1301;c.pathLength=1;
        std::strcpy(c.path[0],station);std::strcpy(c.target,station);
        RoutePlan route;route.count=1;route.departure[0]=RouteTurn::Straight;
        CHECK(r.nav.start(std::strcmp(station,"S1")==0?"GS1":"GS2",c,route,0));
        r.step(2,Config::RFID_STOP_DWELL_MS);r.nav.reachedNode(station,r.in.now+2);
        const auto began=r.in.now+2+Config::RFID_STOP_DWELL_MS;
        CHECK(r.step(2,began)==MotorAction::PivotRight);
        r.step(0,began+102);CHECK(r.step(6,began+104)==MotorAction::Stop);r.stopped();
        CHECK(!r.nav.stationOriented() && r.nav.active());
        for(uint32_t dt=106;dt<204;dt+=2) {r.step(6,began+dt);CHECK(!r.nav.stationOriented());r.stopped();}
        r.step(6,began+204);CHECK(r.nav.stationOriented() && !r.nav.active());r.stopped();
    }
}

int main() {
    try {
        static_assert(Config::TURN_AROUND_CAPTURE_ENABLED && Config::TURN_LINE_SAMPLE_MS==2,"Run with two-robot profiles");
        fastStopAndConfirm();countBranchesAndRejectEdges();noiseAndDeadline();safety();stationStop();
        std::cout<<"PASS "<<Config::AGV_ID<<": 180 early STOP, 2ms fresh samples/100ms at rest, narrow only, branch count, "
                 <<"forward cap/station ready, stale/noise/wrap, original deadline, grant/STOP/network/sonar\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
