#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>
#define CHECK(c) do {if (!(c)) throw std::runtime_error(#c);} while (false)
int main() {
    try {
        static_assert(Config::FAST_LINE_RESPONSE,"Run with profile 01 or 02");
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
        static_assert(Config::GENTLE_FORWARD_CORRECTION,"Robot01 rounded-line correction must be enabled");
        static_assert(!Config::LEFT_TURN_CAPTURE_ENABLED,"Robot02 left capture leaked into Robot01");
        static_assert(Config::LINE_LOSS_GRACE_MS==0,"Robot02 seven-second grace leaked into Robot01");
        static_assert(!Config::REVERSE_GENTLE_CORRECTION,"Robot02 reverse correction leaked into Robot01");
#elif defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
        static_assert(!Config::GENTLE_FORWARD_CORRECTION,"Robot01 rounded-line correction leaked into Robot02");
        static_assert(Config::LEFT_TURN_CAPTURE_ENABLED,"Robot02 left capture must remain enabled");
        static_assert(Config::LINE_LOSS_GRACE_MS==7000,"Robot02 seven-second grace must remain enabled");
        static_assert(Config::REVERSE_GENTLE_CORRECTION,"Robot02 reverse correction must remain enabled");
#endif
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
            if(pattern==4)CHECK(motor.leftDuty()>0 && motor.rightDuty()==0);
            if(pattern==1)CHECK(motor.leftDuty()==0 && motor.rightDuty()>0);
            if(pattern==6 || pattern==3) {
                if(Config::GENTLE_FORWARD_CORRECTION) {
                    CHECK(motor.leftDuty()>0 && motor.rightDuty()>0);
                    // Curves must slow the inner wheel without speeding up
                    // either channel above its straight-line duty. Enough
                    // differential must remain after asymmetric wheel trims.
                    CHECK(motor.leftDuty()<=left && motor.rightDuty()<=right);
                    CHECK(motor.leftDuty()+motor.rightDuty()<left+right);
                    if(pattern==6) CHECK(motor.leftDuty()-motor.rightDuty()>=40);
                    else CHECK(motor.rightDuty()-motor.leftDuty()>=40);
                    std::cout<<"curve pattern="<<pattern<<" duty="<<motor.leftDuty()<<"/"<<motor.rightDuty()<<'\n';
                }
                else if(pattern==6)CHECK(motor.leftDuty()>0 && motor.rightDuty()==0);
                else CHECK(motor.leftDuty()==0 && motor.rightDuty()>0);
            }
            in.now+=25;CHECK(nav.pathIndex()==0 && nav.routeCommandId()==9701);
        }
        nav.revokeGrant("M1",71);CHECK(nav.refreshFollowing(in,early));CHECK(early==MotorAction::Stop);
        CHECK(nav.update(in)==MotorAction::Stop);CHECK(nav.grant("M1",72));
        in.linePattern=0;++in.now;CHECK(nav.refreshFollowing(in,early));CHECK(early==MotorAction::Stop);
        move.apply(early);CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        CHECK(nav.recoveringLine());CHECK(!nav.refreshFollowing(in,early)); // Recovery retains its own timing/state machine.
        nav.stop();CHECK(!nav.refreshFollowing(in,early));
        if (Config::LINE_RECOVERY_SIDE_CONFIRM) {
            for (uint8_t pattern : {1,3,4,6}) {
                CoreNavigator recovered;in.linePattern=pattern;in.now=Config::RFID_STOP_DWELL_MS;
                CHECK(recovered.start("G1",c,route,0));CHECK(recovered.grant("M1",90));
                move.apply(recovered.update(in));
                in.linePattern=0;++in.now;const auto lostAt=in.now;
                move.apply(recovered.update(in));CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
                for (unsigned sample=0;sample<=5;++sample) {
                    in.now=lostAt+100+sample*20;in.linePattern=pattern;
                    CHECK(!recovered.refreshFollowing(in,early));
                    const auto action=recovered.update(in);move.apply(action);
                    if(sample<5) {
                        CHECK(action==MotorAction::Stop);CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
                    } else {
                        CHECK(action==AGVMath::lineAction(pattern));CHECK(recovered.following());
                        if(pattern==6 || pattern==3) CHECK(motor.leftDuty()>0 && motor.rightDuty()>0);
                        else if(pattern==4) CHECK(motor.leftDuty()>0 && motor.rightDuty()==0);
                        else CHECK(motor.leftDuty()==0 && motor.rightDuty()>0);
                    }
                }
                CHECK(recovered.routeCommandId()==9701 && recovered.pathIndex()==0);
                recovered.stop();move.stop();
            }
            std::cout<<"PASS 01: 000 -> stable side -> actual correction PWM; STOP during validation\n";
        }
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
