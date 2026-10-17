#include "core/CoreNavigator.h"
#include "core/OdometryTracker.h"
#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>

#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (false)
int main() {
    static_assert(Config::LEFT_TURN_CAPTURE_ENABLED, "Run with profile02");
    static_assert(Config::TURN_SPEED == 230 && Config::LEFT_TURN_CAPTURE_PWM == 215,
                  "Robot02 pivot torque settings changed");
    try {
        MotorModule motor; motor.begin();
        LineFollowModule line(motor); BuzzerModule buzzer; buzzer.begin();
        MovementModule movement(motor,line,buzzer);
        CoreCommand c; c.type=CoreCommandType::Goto; c.cmdId=601; c.pathLength=1;
        AGVMath::copyText(c.path[0],sizeof(c.path[0]),"K1");
        AGVMath::copyText(c.target,sizeof(c.target),"K1");
        RoutePlan route; CHECK(RoutePlanner::plan("C3","G1",c,route));
        CoreNavigator nav; CHECK(nav.start("G1",c,route,0));
        ControlInput in; in.wifi=in.mqtt=in.distanceValid=in.motionConfigured=in.lineConfigured=true;
        in.now=Config::RFID_STOP_DWELL_MS; in.linePattern=2;
        auto step=[&]() { const auto a=nav.update(in); movement.apply(a,nav.precisionPivot(),nav.precisionPivotPwm(),nav.settlingTurnLine()); return a; };
        CHECK(step()==MotorAction::PivotLeft);
        CHECK(motor.leftDuty()==Config::TURN_SPEED && motor.rightDuty()==-Config::TURN_SPEED);
        in.linePattern=1; in.now+=10; CHECK(step()==MotorAction::PivotLeft);
        CHECK(motor.leftDuty()==215 && motor.rightDuty()==-215);
        // Same ordering as AGVController: latch, STOP, then RFID work.
        in.linePattern=3; in.now+=10; CHECK(nav.observeTurnLine(in)); movement.stop();
        CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        CHECK(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]]==0);
        CHECK(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]]==0);
        CHECK(step()==MotorAction::Stop && nav.capturingTurnLine());
        // Pivot ticks must not advance the estimated translation distance.
        OdometryTracker odom; odom.begin(0,0,in.now);
        odom.context("G1","K1",601,0,1,nav.active(),nav.following(),false);
        in.linePattern=0; in.now+=Config::LINE_RECOVERY_STOP_MS;
        CHECK(step()==MotorAction::PivotRight);
        CHECK(motor.leftDuty()==-215 && motor.rightDuty()==215);
        odom.update(20,20,in.now); CHECK(odom.snapshot(1).distanceCm==0);
        in.linePattern=2; in.now+=Config::LEFT_CAPTURE_SAMPLE_MS; CHECK(step()==MotorAction::Stop);
        in.now+=Config::LEFT_CAPTURE_SAMPLE_MS; CHECK(step()==MotorAction::Stop && nav.following());
        CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        ++in.now; CHECK(step()==MotorAction::Forward);
        CHECK(nav.settlingTurnLine());
        CHECK(motor.leftDuty()==215 && motor.rightDuty()==215);
        CHECK(nav.routeCommandId()==601 && nav.pathIndex()==0 && nav.active());
        CHECK(nav.precisionPivotPwm()==Config::LINE_RECOVERY_PWM);
        odom.context("G1","K1",601,0,1,nav.active(),nav.following(),false);
        odom.update(30,30,in.now+100); CHECK(odom.snapshot(2).distanceCm>0);
        in.now+=Config::LEFT_CAPTURE_SETTLE_MS;CHECK(step()==MotorAction::Forward);
        CHECK(!nav.settlingTurnLine());
        CHECK(motor.leftDuty()==Config::BASE_SPEED+Config::LEFT_SPEED_TRIM);
        CHECK(motor.rightDuty()==Config::BASE_SPEED+Config::RIGHT_SPEED_TRIM);
        // Side centering must match each eye, and never translate before 010.
        for (uint8_t side : {3,6}) {
            nav.stop();CHECK(nav.start("G1",c,route,0));
            in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;step();
            in.linePattern=0;++in.now;step();
            in.linePattern=side;++in.now;CHECK(nav.observeTurnLine(in));movement.stop();
            CHECK(step()==MotorAction::Stop);
            in.now+=Config::LINE_RECOVERY_REST_MS;
            const auto expected=side==3 ? MotorAction::PivotRight : MotorAction::PivotLeft;
            CHECK(step()==expected);
            CHECK(motor.leftDuty()==(side==3 ? -215 : 215));
            CHECK(motor.rightDuty()==-motor.leftDuty());
            CHECK(!nav.following());
            in.linePattern=2;in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;
            CHECK(step()==MotorAction::Stop && nav.settlingTurnLine());
            ++in.now;CHECK(step()==MotorAction::Forward);
            CHECK(motor.leftDuty()==215 && motor.rightDuty()==215);
            in.linePattern=0;++in.now;CHECK(step()==MotorAction::Stop);
            CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        }
        movement.apply(MotorAction::ReverseCorrectLeft,false,215,true);
        CHECK(motor.leftDuty()==0 && motor.rightDuty()==-205);
        CHECK(buzzer.pinLevel()==LOW);
        movement.apply(MotorAction::ReverseCorrectRight,false,215,true);
        CHECK(motor.leftDuty()==-215 && motor.rightDuty()==0);
        movement.stop(); CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        std::cout << "PASS G1 left align: GPIO STOP, side-aware pivot, sampled center, capped departure, blank STOP, reverse buzzer, route and odometry\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
