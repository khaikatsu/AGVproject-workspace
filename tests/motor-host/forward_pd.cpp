#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (false)
static ControlInput healthy(uint32_t now, uint8_t pattern=2) {
    ControlInput in; in.now=now; in.linePattern=pattern;
    in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    return in;
}
static void checkLineAssist(MotorModule& motor, LineFollowModule& line, MovementModule& move) {
    const auto duty=[&](int l,int r){CHECK(motor.leftDuty()==l);CHECK(motor.rightDuty()==r);};
    const auto capped=[](int pwm){return std::min(pwm,Config::FORWARD_PD_BASE_SPEED);};
    const int outer=capped(Config::LINE_ASSIST_OUTER_PWM), near=capped(Config::LINE_ASSIST_NEAR_INNER_PWM);
    const int far=capped(Config::LINE_ASSIST_FAR_INNER_PWM), wide=capped(Config::LINE_ASSIST_WIDE_PWM);
    const int lt=Config::FORWARD_PD_LEFT_TRIM,rt=Config::FORWARD_PD_RIGHT_TRIM;
    const auto expected=[&](uint8_t pattern) {
        switch(pattern) {
            case 4:duty(outer+lt,far+rt);break;case 6:duty(outer+lt,near+rt);break;
            case 1:duty(far+lt,outer+rt);break;case 3:duty(near+lt,outer+rt);break;
            case 5:case 7:duty(wide+lt,wide+rt);break;
            case 2:duty(Config::FORWARD_PD_BASE_SPEED+lt,Config::FORWARD_PD_BASE_SPEED+rt);break;
            default:duty(0,0);break;
        }
    };
    // Exercise the GPIO->normalized pattern->guarded forward driver path.
    auto in=healthy(100);
    for(uint8_t pattern:{2,6,4,6,2,3,1,3,7,5,2}) {
        MotorIo::pins[39]=(pattern&4)?1:0;MotorIo::pins[34]=(pattern&2)?1:0;MotorIo::pins[35]=(pattern&1)?1:0;
        in.linePattern=line.readSensors().pattern;CHECK(in.linePattern==pattern);
        // Same timestamp intentionally checks an immediate change of side.
        move.applyFollowing(AGVMath::lineAction(pattern),in,701,0);expected(pattern);
        CHECK(motor.leftDuty()>0 && motor.rightDuty()>0); // Never reverse a wheel to follow a curve.
    }
    // Noise and center/111 transitions cannot leave a derivative or side bias.
    for(int sample=0;sample<50;++sample) {
        in.now+=1;in.linePattern=sample%2?1:4;
        move.applyFollowing(AGVMath::lineAction(in.linePattern),in,701,0);expected(in.linePattern);
    }
    for(uint8_t pattern:{7,2,1,2,4,7,2}) {
        ++in.now;in.linePattern=pattern;
        move.applyFollowing(AGVMath::lineAction(pattern),in,701,0);expected(pattern);
    }
    // Safety wins before any assist PWM, including within one millisecond.
    for(int hazard=0;hazard<9;++hazard) {
        in=healthy(300,4);move.applyFollowing(AGVMath::lineAction(4),in,701,0);expected(4);
        ++in.now;MotorAction action=AGVMath::lineAction(4);
        if(hazard==0)in.wifi=false;if(hazard==1)in.mqtt=false;if(hazard==2)in.obstacle=true;
        if(hazard==3)in.distanceValid=false;if(hazard==4)in.motionConfigured=false;
        if(hazard==5)in.lineConfigured=false;if(hazard==6)in.linePattern=0;
        if(hazard==7)action=MotorAction::Stop;if(hazard==8)in.servoBusy=true;
        move.applyFollowing(action,in,701,0);duty(0,0);
    }
    // Node pivot, precision search/capture, settling and reverse outputs are not assist outputs.
    for(const auto action:{MotorAction::PivotLeft,MotorAction::PivotRight,MotorAction::Backward,
            MotorAction::ReverseCorrectLeft,MotorAction::ReverseCorrectRight}) {
        move.apply(action);const int left=motor.leftDuty(),right=motor.rightDuty();
        move.applyFollowing(AGVMath::lineAction(4),healthy(400,4),701,0);expected(4);
        move.apply(action);duty(left,right);
    }
    move.apply(MotorAction::PivotLeft,true,Config::LINE_RECOVERY_PWM);
    duty(Config::LINE_RECOVERY_PWM,-Config::LINE_RECOVERY_PWM);
    move.apply(MotorAction::PivotRight,true,Config::LEFT_TURN_CAPTURE_PWM);
    duty(-Config::LEFT_TURN_CAPTURE_PWM,Config::LEFT_TURN_CAPTURE_PWM);
    move.stop();
    // Actual rounded M2->G6 segment still requires grant and original route.
    CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=701;c.pathLength=1;
    AGVMath::copyText(c.path[0],sizeof(c.path[0]),"G6");AGVMath::copyText(c.target,sizeof(c.target),"G6");
    RoutePlan route;CHECK(RoutePlanner::plan("GS1","M2",c,route));
    CoreNavigator nav;CHECK(nav.start("M2",c,route,0));in=healthy(Config::RFID_STOP_DWELL_MS,4);
    auto action=nav.update(in);CHECK(action==MotorAction::Stop);move.applyFollowing(action,in,701,0);duty(0,0);
    CHECK(nav.grant("M2",71));
    for(uint8_t pattern:{4,6,2,3,1,7,5,2,1}) {
        in.now+=10;in.linePattern=pattern;action=nav.update(in);
        CHECK(nav.following() && nav.error()==ErrorCode::None);
        move.applyFollowing(action,in,701,0);expected(pattern);
        CHECK(nav.routeCommandId()==701 && nav.pathIndex()==0);
    }
    in.linePattern=0;++in.now;const auto lostAt=in.now;
    action=nav.update(in);CHECK(action==MotorAction::Stop);move.applyFollowing(action,in,701,0);duty(0,0);
    in.now=lostAt+600;action=nav.update(in);CHECK(action==MotorAction::Stop);
    move.apply(action,nav.precisionPivot(),nav.precisionPivotPwm());duty(0,0);CHECK(nav.active());
    in.linePattern=1;
    for(int sample=0;sample<6;++sample) {
        in.now=lostAt+1200+sample*20;action=nav.update(in);
        move.applyFollowing(action,in,701,0);
        if(sample<5)duty(0,0);else expected(1);
    }
    CHECK(nav.following() && nav.error()==ErrorCode::None);
    nav.revokeGrant("M2",71);++in.now;MotorAction early;
    CHECK(nav.refreshFollowing(in,early));CHECK(early==MotorAction::Stop);
    move.applyFollowing(early,in,701,0);duty(0,0);
    nav.stop();move.stop();
    std::cout<<"PASS "<<Config::AGV_ID<<": forward curve assist GPIO/side/111/guards/route; node pivots and reverse preserved\n";
}
int main() {
    try {
        MotorModule motor; motor.begin(); LineFollowModule line(motor); BuzzerModule buzzer; buzzer.begin();
        MovementModule move(motor,line,buzzer);
        const auto duty=[&](int l,int r){CHECK(motor.leftDuty()==l);CHECK(motor.rightDuty()==r);};
        const auto clamp=[](int v){return std::max(0,std::min(255,v));};
        const auto expectedLeft=[&](int c){return clamp(Config::FORWARD_PD_BASE_SPEED+Config::FORWARD_PD_LEFT_TRIM+c);};
        const auto expectedRight=[&](int c){return clamp(Config::FORWARD_PD_BASE_SPEED+Config::FORWARD_PD_RIGHT_TRIM-c);};
        const auto corrected=[&](int c){duty(expectedLeft(c),expectedRight(c));};
        if (!Config::FORWARD_PD_ENABLED) {
            for (const auto pattern : {2,6,4,3,1,5,7}) {
                const auto action=AGVMath::lineAction(pattern);move.apply(action);
                const int l=motor.leftDuty(),r=motor.rightDuty();
                move.applyFollowing(action,healthy(100,pattern),1,0);duty(l,r);
            }
            std::cout<<"PASS "<<Config::AGV_ID<<": PD disabled restores original driver table\n";return 0;
        }
        if (Config::FORWARD_LINE_ASSIST_ENABLED) {checkLineAssist(motor,line,move);return 0;}
        // Equivalent GPIO stimulus to the proven sketch (which calls GPIO35
        // LEFT). Core uses GPIO35 as RIGHT: mirror patterns, retain ENA/ENB.
        MotorIo::pins[35]=1;MotorIo::pins[34]=1;MotorIo::pins[39]=0;
        CHECK(line.readSensors().pattern==3);
        auto in=healthy(0);move.applyFollowing(MotorAction::Forward,in,1,0);corrected(0);
        in.now=10;in.linePattern=line.readSensors().pattern;
        move.applyFollowing(AGVMath::lineAction(in.linePattern),in,1,0);corrected(-80);
        in.now=20;move.applyFollowing(AGVMath::lineAction(3),in,1,0);corrected(-50);
        move.stop();duty(0,0);
        move.applyFollowing(MotorAction::Forward,healthy(30),1,0);
        in=healthy(40,6);move.applyFollowing(AGVMath::lineAction(6),in,1,0);corrected(80);
        in.now=50;move.applyFollowing(AGVMath::lineAction(6),in,1,0);corrected(50);
        // Same and sub-10ms samples retain output, not an artificial D spike.
        in.linePattern=3;in.now=55;move.applyFollowing(AGVMath::lineAction(3),in,1,0);corrected(50);
        in.now=60;move.applyFollowing(AGVMath::lineAction(3),in,1,0);corrected(-110);
        in.now=80;in.linePattern=6;move.applyFollowing(AGVMath::lineAction(6),in,1,0);corrected(80); // D normalized to20ms.
        in.now=140;in.linePattern=3;move.applyFollowing(AGVMath::lineAction(3),in,1,0);corrected(-50); // stale history dropped.
        // Route/step changes discard D history without a STOP/PWM-zero notch.
        MotorIo::writes.clear();in.now=141;in.linePattern=6;
        move.applyFollowing(AGVMath::lineAction(6),in,2,0);corrected(50);
        for(const auto& w:MotorIo::writes)CHECK(w.second>0);
        in.linePattern=3;move.applyFollowing(AGVMath::lineAction(3),in,2,1);corrected(-50);
        move.resetForwardPD();in.linePattern=6;move.applyFollowing(AGVMath::lineAction(6),in,2,1);corrected(50);
        // A wide marker stays straight, without the previous correction's D.
        for(const auto pattern:{5,7}) {in.now+=10;in.linePattern=pattern;move.applyFollowing(MotorAction::Forward,in,2,1);corrected(0);}
        // Real-time hazards and denied actions win even one ms after PD output.
        for(int hazard=0;hazard<9;++hazard) {
            move.stop();move.applyFollowing(MotorAction::Forward,healthy(200),3,0);
            in=healthy(201);MotorAction action=MotorAction::Forward;
            if(hazard==0)in.wifi=false;if(hazard==1)in.mqtt=false;if(hazard==2)in.obstacle=true;
            if(hazard==3)in.distanceValid=false;if(hazard==4)in.motionConfigured=false;
            if(hazard==5)in.lineConfigured=false;if(hazard==6)in.linePattern=0;
            if(hazard==7)action=MotorAction::Stop;
            if(hazard==8)in.servoBusy=true;
            move.applyFollowing(action,in,3,0);duty(0,0);
            move.applyFollowing(AGVMath::lineAction(6),healthy(202,6),3,0);corrected(50); // No stale D.
        }
        // Changing away from authorized forward following preserves old output.
        for(const auto action:{MotorAction::Backward,MotorAction::ReverseCorrectLeft,
                MotorAction::ReverseCorrectRight,MotorAction::PivotLeft,MotorAction::PivotRight}) {
            move.apply(action);const int l=motor.leftDuty(),r=motor.rightDuty();
            move.applyFollowing(MotorAction::Forward,healthy(300),4,0);
            move.apply(action);duty(l,r);
        }
        ForwardLinePD pd;auto d=pd.update(2,UINT32_MAX-5);CHECK(d.left==expectedLeft(0) && d.right==expectedRight(0));
        d=pd.update(6,4);CHECK(d.left==expectedLeft(80) && d.right==expectedRight(80)); // millis rollover, dt10.
        d=pd.update(0,5);CHECK(d.left==0 && d.right==0);
        d=pd.update(4,6);CHECK(d.left==expectedLeft(100) && d.right==expectedRight(100)); // restart suppresses D kick.
        pd.reset();ForwardLinePD other;other.update(2,0);other.update(1,10);
        d=pd.update(6,10);CHECK(d.left==expectedLeft(50) && d.right==expectedRight(50)); // Controllers do not share state.

        CoreCommand c;c.type=CoreCommandType::Goto;c.cmdId=700;c.pathLength=1;
        AGVMath::copyText(c.path[0],sizeof(c.path[0]),"G6");AGVMath::copyText(c.target,sizeof(c.target),"G6");
        RoutePlan route;CHECK(RoutePlanner::plan("GS1","M2",c,route));
        // The actual rounded segment cannot start without M2's grant.
        CoreNavigator nav;CHECK(nav.start("M2",c,route,0));in=healthy(Config::RFID_STOP_DWELL_MS);
        auto action=nav.update(in);CHECK(action==MotorAction::Stop);
        move.applyFollowing(action,in,700,0);duty(0,0);
        CHECK(nav.grant("M2",70));action=nav.update(in);CHECK(action==MotorAction::Forward);
        move.applyFollowing(action,in,700,0);corrected(0);
        in.now+=10;in.linePattern=3;MotorAction early=MotorAction::Stop;
        CHECK(nav.refreshFollowing(in,early));move.applyFollowing(early,in,700,0);corrected(-80);
        nav.revokeGrant("M2",70);++in.now;CHECK(nav.refreshFollowing(in,early));
        move.applyFollowing(early,in,700,0);duty(0,0);
        CHECK(nav.grant("M2",71));in.linePattern=2;in.now+=10;
        move.applyFollowing(nav.update(in),in,700,0);corrected(0);
        in.linePattern=0;++in.now;action=nav.update(in);CHECK(action==MotorAction::Stop);
        move.applyFollowing(action,in,700,0);duty(0,0);CHECK(nav.recoveringLine());
        // No physical PWM is allowed past the 600ms search budget while blank.
        const auto lostAt=in.now;
        for(uint32_t elapsed:{600u,1000u}) {
            in.now=lostAt+elapsed;action=nav.update(in);
            CHECK(action==MotorAction::Stop);move.applyFollowing(action,in,700,0);duty(0,0);
            CHECK(nav.active() && nav.error()==ErrorCode::None);
        }
        // Reacquiring 001 late must feed PD again once stable,
        // without a new task/node order or an operator fault reset.
        in.linePattern=1;in.now=lostAt+1200u;
        for(int sample=0;sample<6;++sample) {
            action=nav.update(in);
            move.applyFollowing(action,in,700,0);
            if(sample<5) {CHECK(action==MotorAction::Stop);duty(0,0);}
            in.now+=20;
        }
        CHECK(nav.following());CHECK(nav.error()==ErrorCode::None);
        CHECK(action==AGVMath::lineAction(1));corrected(-100);
        CHECK(nav.routeCommandId()==700 && nav.pathIndex()==0);
        // Remaining phase/task/timeout behavior belongs to the navigator.
        nav.stop();in.linePattern=2;++in.now;move.applyFollowing(nav.update(in),in,700,0);duty(0,0);
        std::cout<<"PASS "<<Config::AGV_ID<<": PD GPIO/dt/reset/limits, reverse/pivot preserved, M2-G6 grant/000/STOP guards\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
