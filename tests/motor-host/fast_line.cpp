#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>
#define CHECK(c) do {if (!(c)) throw std::runtime_error(#c);} while (false)
int main() {
    try {
        static_assert(Config::FAST_LINE_RESPONSE,"Run with profile 01 or 02");
        MotorModule motor;motor.begin();LineFollowModule line(motor);BuzzerModule buzzer;buzzer.begin();
        MovementModule move(motor,line,buzzer);
        move.apply(MotorAction::Forward);const int left=motor.leftDuty(),right=motor.rightDuty();
        MotorIo::writes.clear();move.apply(MotorAction::Forward);CHECK(MotorIo::writes.empty());
        motor.setLeftSpeed(left-5);motor.setRightSpeed(right-5);motor.motorForward();
        CHECK(MotorIo::writes.size()==2);
        for(const auto& w:MotorIo::writes)CHECK(w.second>0); // Same-direction PWM never drops to zero.
        MotorIo::writes.clear();move.apply(MotorAction::Backward);
        for(const auto channel:{Config::MOTOR_PWM_CHANNELS[0],Config::MOTOR_PWM_CHANNELS[1]}) {
            std::vector<int> values;for(const auto& w:MotorIo::writes)if(w.first==channel)values.push_back(w.second);
            CHECK(values.size()==2 && values[0]==0 && values[1]>0); // Direction change still disables bridge first.
        }
        MotorIo::writes.clear();move.apply(MotorAction::Backward);CHECK(MotorIo::writes.empty());
        move.stop();CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        CHECK(MotorIo::pwm[0]==0 && MotorIo::pwm[1]==0);

        CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=9701;c.pathLength=1;
        AGVMath::copyText(c.path[0],sizeof(c.path[0]),"M1");AGVMath::copyText(c.target,sizeof(c.target),"M1");
        RoutePlan route;CHECK(RoutePlanner::plan("M3","G1",c,route));
        CoreNavigator nav;CHECK(nav.start("G1",c,route,0));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;MotorAction early=MotorAction::Stop;
        CHECK(!nav.refreshFollowing(in,early));CHECK(nav.update(in)==MotorAction::Stop); // Waiting grant cannot start early.
        CHECK(nav.grant("M1",71));CHECK(nav.update(in)==MotorAction::Forward);move.apply(MotorAction::Forward);
        for(const auto pattern:{4,1,6,3,2}) {
            in.linePattern=pattern;++in.now;
            CHECK(nav.refreshFollowing(in,early));move.apply(early);
            CHECK(early==AGVMath::lineAction(pattern));
            // A slow RFID operation happens only after the latest pattern has
            // reached actual driver output. No simulated node arrival involved.
            if(pattern==4 || pattern==6)CHECK(motor.leftDuty()>0 && motor.rightDuty()==0);
            if(pattern==1 || pattern==3)CHECK(motor.leftDuty()==0 && motor.rightDuty()>0);
            in.now+=25;CHECK(nav.pathIndex()==0 && nav.routeCommandId()==9701);
        }
        nav.revokeGrant("M1",71);CHECK(nav.refreshFollowing(in,early));CHECK(early==MotorAction::Stop);
        CHECK(nav.update(in)==MotorAction::Stop);CHECK(nav.grant("M1",72));
        in.linePattern=0;++in.now;CHECK(nav.refreshFollowing(in,early));CHECK(early==MotorAction::Stop);
        move.apply(early);CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        CHECK(nav.recoveringLine());CHECK(!nav.refreshFollowing(in,early)); // Recovery retains its own timing/state machine.
        nav.stop();CHECK(!nav.refreshFollowing(in,early));
        for(int hazard=0;hazard<5;++hazard) {
            CoreNavigator guarded;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;in.obstacle=false;
            in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
            CHECK(guarded.start("G1",c,route,0));CHECK(guarded.grant("M1",80+hazard));CHECK(guarded.update(in)==MotorAction::Forward);
            if(hazard==0)in.mqtt=false;if(hazard==1)in.wifi=false;if(hazard==2)in.obstacle=true;
            if(hazard==3)in.distanceValid=false;if(hazard==4)in.motionConfigured=false;
            CHECK(guarded.refreshFollowing(in,early));CHECK(early==MotorAction::Stop);
            move.apply(early);CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        }
        std::cout<<"PASS "<<Config::AGV_ID<<": line reaches driver before RFID; PWM continuous when same direction; grant/STOP/blank/network/sonar guards\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
