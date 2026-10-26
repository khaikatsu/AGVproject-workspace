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
    MotorAction pivot=MotorAction::Stop;
    Rig() {
        motor.begin(); buzzer.begin();
        in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    }
    MotorAction step(uint8_t pattern,uint32_t at) {
        in.now=at; in.linePattern=pattern;
        const auto action=nav.update(in);
        move.apply(action,nav.precisionPivot(),nav.precisionPivotPwm(),nav.settlingTurnLine());
        return action;
    }
    void stopped() const { CHECK(motor.leftDuty()==0 && motor.rightDuty()==0); }
    void start(const char* station,RobotHeading heading,uint32_t at=0) {
        const char* gate=RoutePlanner::bayGate(station);
        CoreCommand c; c.type=CoreCommandType::Goto; c.cmdId=1700; c.pathLength=1;
        std::strcpy(c.path[0],station); std::strcpy(c.target,station);
        RoutePlan plan; CHECK(RoutePlanner::plan(nullptr,gate,c,plan,false,heading));
        CHECK(plan.reverseAfterTurn[0]);
        CHECK(nav.start(gate,c,plan,at,false,true));
        CoreCommand order; order.type=CoreCommandType::NodeOrder; order.cmdId=1701; order.routeCmdId=1700;
        std::strcpy(order.node,gate); std::strcpy(order.nextNode,station);
        std::strcpy(order.maneuver,RoutePlanner::turnName(plan.departure[0]));
        order.turnCrossings=plan.departure[0]==RouteTurn::TurnAround ? plan.turnCrossings[0] : 1;
        order.reverseAfterTurn=true;
        CHECK(nav.authorizeNodeOrder(order));
        began=at+Config::RFID_STOP_DWELL_MS;
        pivot=plan.departure[0]==RouteTurn::Left ? MotorAction::PivotLeft : MotorAction::PivotRight;
        CHECK(step(2,began)==pivot);
        CHECK(nav.precisionPivotPwm()==Config::GS_TURN_PWM);
        CHECK(std::abs(motor.leftDuty())==Config::GS_TURN_PWM+Config::TURN_LEFT_SPEED_TRIM);
        CHECK(std::abs(motor.rightDuty())==Config::GS_TURN_PWM+Config::TURN_RIGHT_SPEED_TRIM);
    }
    uint32_t capture(uint8_t pattern=3) {
        CHECK(step(0,began+100)==pivot);
        CHECK(step(pattern,began+102)==pivot);
        CHECK(!nav.capturingTurnLine());
        CHECK(step(2,began+104)==pivot); // 010 must not stop a GS docking turn.
        CHECK(step(Config::GS_DOCK_CONFIRM_PATTERN,began+106)==MotorAction::Stop);
        CHECK(nav.capturingTurnLine()); stopped(); return began+106;
    }
    void wide(uint32_t at,uint32_t window=Config::GS_DOCK_CONFIRM_MS) {
        for(uint32_t dt=0;dt<window;dt+=5) {
            CHECK(step(Config::GS_DOCK_CONFIRM_PATTERN,at+dt)==MotorAction::Stop); stopped(); CHECK(!nav.following());
        }
        CHECK(step(Config::GS_DOCK_CONFIRM_PATTERN,at+window)==MotorAction::Stop); stopped();
        CHECK(nav.following() && nav.segmentReverse());
        CHECK(nav.routeCommandId()==1700 && nav.pathIndex()==0 && !nav.stationOriented());
    }
};

static void ordinaryDocking() {
    for(const char* station:{"S1","S2"}) for(auto heading:{RobotHeading::East,RobotHeading::West})
        for(bool wrap:{false,true}) {
            Rig r; r.start(station,heading,wrap?UINT32_MAX-1100u:0u); const auto at=r.capture();
            // Narrow line and repeated timestamps cannot authorize GS reverse.
            r.step(3,at); r.step(2,at+5);
            for(int i=0;i<20;++i)r.step(2,at+5);
            CHECK(!r.nav.following()); r.stopped();
            r.step(6,at+10); CHECK(!r.nav.following());
            r.wide(at+15);
            r.in.distanceValid=false;
            CHECK(r.step(2,r.in.now+1)==MotorAction::Stop); r.stopped();
        }
}

static void noiseBudgetAndTimeout() {
    for(const char* station:{"S1","S2"}) for(uint8_t invalid:{0,1,2,3,4,5,6}) {
        Rig r; r.start(station,RobotHeading::East); const auto at=r.capture();
        for(uint32_t dt=0;dt<=60;dt+=5)r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+dt);
        r.step(invalid,at+65); r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+70);
        r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+130);
        r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+190); // stale gaps restart confirmation
        CHECK(!r.nav.following()); r.stopped();
        r.wide(at+245); // first fresh sample after another >50ms gap
    }
    for(const char* station:{"S1","S2"}) {
        Rig r; r.start(station,RobotHeading::West); const auto at=r.capture(); bool pivoted=false;
        for(uint32_t dt=5;dt<Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS;dt+=5) {
            const auto action=r.step(6,at+dt);
            CHECK(!r.nav.following());
            if(dt>=Config::LINE_RECOVERY_TIMEOUT_MS) { CHECK(action==MotorAction::Stop);r.stopped(); }
            else if(action==MotorAction::PivotLeft || action==MotorAction::PivotRight)pivoted=true;
        }
        CHECK(pivoted);
        CHECK(r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS)==MotorAction::Stop);
        CHECK(r.nav.error()==ErrorCode::LineLost && !r.nav.active());
        r.step(2,r.in.now+500); r.stopped(); CHECK(!r.nav.active());
        Rig late;late.start(station,RobotHeading::East);const auto lateAt=late.capture();
        for(uint32_t dt=5;dt<=650;dt+=5)late.step(0,lateAt+dt);
        late.wide(lateAt+655); // late evidence allowed, late motor search forbidden
    }
}

static void hazardsAndArrival() {
    for(const char* station:{"S1","S2"}) for(int hazard=0;hazard<7;++hazard) {
        Rig r; r.start(station,RobotHeading::West);const auto at=r.capture();
        for(uint32_t dt=0;dt<=60;dt+=5)r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+dt);
        if(hazard==0)r.in.wifi=false;if(hazard==1)r.in.mqtt=false;
        if(hazard==2)r.in.distanceValid=false;if(hazard==3)r.in.motionConfigured=false;
        if(hazard==4)r.in.lineConfigured=false;if(hazard==5)r.nav.stop();
        if(hazard==6)r.in.obstacle=true;
        CHECK(r.step(Config::GS_DOCK_CONFIRM_PATTERN,at+65)==MotorAction::Stop);r.stopped();CHECK(!r.nav.following());
        if(hazard==0 || hazard==1 || hazard==6) {
            r.in.wifi=r.in.mqtt=true;r.in.obstacle=false;r.wide(at+70);
            CHECK(r.step(2,r.in.now+1)==MotorAction::Backward);
            r.nav.reachedNode(station,r.in.now+1);
            r.step(2,r.in.now+2);r.stopped();CHECK(!r.nav.active() && r.nav.stationOriented());
        } else {CHECK(!r.nav.active());r.step(2,at+500);r.stopped();}
    }
    Rig offline;offline.start("S1",RobotHeading::East);const auto at=offline.capture();offline.in.mqtt=false;
    offline.step(Config::GS_DOCK_CONFIRM_PATTERN,at);
    offline.step(Config::GS_DOCK_CONFIRM_PATTERN,at+Config::MQTT_TIMEOUT_MS);
    CHECK(offline.nav.error()==ErrorCode::NotConnected);offline.stopped();
    Rig premature;premature.start("S2",RobotHeading::West);premature.capture();
    premature.nav.reachedNode("S2",premature.in.now);CHECK(premature.nav.error()==ErrorCode::UnexpectedNode);
}

static void turnAroundDocking() {
    for(const char* station:{"S1","S2"}) {
        Rig r;r.start(station,std::strcmp(station,"S1")==0?RobotHeading::North:RobotHeading::South);
        CHECK(r.step(0,r.began+100)==MotorAction::PivotRight);
        CHECK(r.step(3,r.began+102)==MotorAction::PivotRight);CHECK(!r.nav.capturingTurnAroundLine());
        r.step(0,r.began+202);CHECK(r.step(6,r.began+204)==MotorAction::PivotRight);
        CHECK(r.step(Config::GS_DOCK_CONFIRM_PATTERN,r.began+206)==MotorAction::Stop);
        CHECK(r.nav.capturingTurnAroundLine());r.stopped();
        const auto at=r.began+206;
        for(uint32_t dt=2;dt<=600;dt+=2) {r.step(3,at+dt);CHECK(!r.nav.following());r.stopped();}
        r.wide(at+605,Config::TURN_AROUND_CONFIRM_MS);
        CHECK(r.step(2,r.in.now+1)==MotorAction::Backward);
        r.nav.reachedNode(station,r.in.now+1);CHECK(r.nav.stationOriented() && !r.nav.active());
    }
}

int main() {
    try {
        ordinaryDocking();noiseBudgetAndTimeout();hazardsAndArrival();turnAroundDocking();
        std::cout<<"PASS "<<Config::AGV_ID<<": GS1/S1+GS2/S2, LEFT/RIGHT/180, profile GS PWM+trims, stable111, "
                    "noise/stale/wrap/hazards, 600ms search/1500ms timeout, rear handoff, RFID arrival\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
