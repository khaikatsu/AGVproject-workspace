#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (false)

int main() {
    try {
        static_assert(Config::LINE_LOSS_GRACE_MS==7000,"Robot02 grace must be 7s");
        MotorModule motor;motor.begin();LineFollowModule line(motor);BuzzerModule buzzer;buzzer.begin();
        MovementModule move(motor,line,buzzer);
        for(bool reverse : {false,true}) for(bool search : {false,true}) {
            CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=7901;c.pathLength=1;
            const char* target=reverse ? "G5" : "GS1";
            AGVMath::copyText(c.path[0],sizeof(c.path[0]),target);
            AGVMath::copyText(c.target,sizeof(c.target),target);
            RoutePlan route;CHECK(RoutePlanner::plan(nullptr,reverse?"K5":"S1",c,route));
            CoreNavigator nav;CHECK(nav.start(reverse?"K5":"S1",c,route,0));
            ControlInput in;in.wifi=in.mqtt=in.distanceValid=in.motionConfigured=in.lineConfigured=true;
            in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;
            auto step=[&]() {auto a=nav.update(in);move.apply(a,nav.precisionPivot(),nav.precisionPivotPwm(),nav.settlingTurnLine());return a;};
            const auto translation=reverse?MotorAction::Backward:MotorAction::Forward;
            CHECK(step()==translation);
            if(search){in.linePattern=1;++in.now;step();}
            in.linePattern=0;++in.now;const auto lost=in.now;CHECK(step()==MotorAction::Stop);
            in.now=lost+Config::LINE_RECOVERY_STOP_MS;
            CHECK(step()==(search?MotorAction::PivotRight:MotorAction::Stop));
            for(uint32_t elapsed : {600u,1000u,5000u,6500u}) {
                in.now=lost+elapsed;CHECK(step()==MotorAction::Stop);
                CHECK(nav.active() && nav.error()==ErrorCode::None && nav.recoveringLine());
                CHECK(nav.routeCommandId()==7901 && nav.pathIndex()==0 && !nav.following());
                CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
            }
            in.linePattern=2;in.now=lost+6600;CHECK(step()==MotorAction::Stop);
            in.now+=50;CHECK(step()==MotorAction::Stop);
            in.now+=50;CHECK(step()==translation && nav.following());
            CHECK(nav.routeCommandId()==7901 && nav.pathIndex()==0);
            in.linePattern=0;++in.now;const auto second=in.now;CHECK(step()==MotorAction::Stop);
            in.now=second+6999;CHECK(step()==MotorAction::Stop && nav.active());
            in.now=second+7000;CHECK(step()==MotorAction::Stop);
            CHECK(nav.error()==ErrorCode::LineLost && !nav.active());
        }
        // Lost network must not wait for line grace, and caller STOP wins.
        for(bool stop : {false,true}) {
            CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=7902;c.pathLength=1;
            AGVMath::copyText(c.path[0],sizeof(c.path[0]),"GS1");
            AGVMath::copyText(c.target,sizeof(c.target),"GS1");
            RoutePlan route;CHECK(RoutePlanner::plan(nullptr,"S1",c,route));
            CoreNavigator nav;CHECK(nav.start("S1",c,route,0));
            ControlInput in;in.wifi=in.mqtt=in.distanceValid=in.motionConfigured=in.lineConfigured=true;
            in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;nav.update(in);
            in.linePattern=0;++in.now;nav.update(in);
            if(stop)nav.stop();else in.mqtt=false;
            ++in.now;CHECK(nav.update(in)==MotorAction::Stop && !nav.active());
        }
        std::cout<<"PASS robot02 grace7s: forward/reverse, search<=600ms, stopped wait, same task resume, deadline, network and STOP\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
