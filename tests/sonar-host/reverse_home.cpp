#include "sensors/DirectionalSonar.h"
#include "core/CoreNavigator.h"
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(false)

int main() {
    try {
        SensorModule front, rear(Config::REAR_ULTRASONIC_TRIG_PIN,Config::REAR_ULTRASONIC_ECHO_PIN);
        front.begin(); rear.begin(); DirectionalSonar sonars(front,rear);
        sonars.update(false);
        const auto pulse=[&](bool reverse,uint32_t at,uint32_t width) {
            SonarIo::timeUs=at;sonars.update(reverse);
            CHECK(sonars.active().diagnostic().waiting);
            const int pin=reverse ? 15 : 13;
            SonarIo::fire(pin,HIGH,at+100);SonarIo::fire(pin,LOW,at+100+width);
            sonars.update(reverse);
        };
        pulse(false,100000,1458);
        CoreCommand command;command.type=CoreCommandType::Goto;command.cmdId=100;command.pathLength=1;
        AGVMath::copyText(command.path[0],sizeof(command.path[0]),"S1");
        AGVMath::copyText(command.target,sizeof(command.target),"S1");
        RoutePlan route;CHECK(RoutePlanner::plan("M1","GS1",command,route));
        CoreNavigator nav;CHECK(nav.start("GS1",command,route,100));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=true;in.linePattern=2;
        const auto step=[&]() {
            in.now=millis();in.distanceValid=sonars.active().valid();
            in.obstacle=sonars.active().isObstacleDetected();
            return sonars.acquiring() ? MotorAction::Stop : nav.update(in);
        };
        pulse(false,1100000,1458);CHECK(step()==MotorAction::PivotRight);
        CHECK(!nav.segmentReverse()&&!sonars.rearActive());
        SonarIo::timeUs=1203000;in.linePattern=0;CHECK(step()==MotorAction::PivotRight);
        SonarIo::timeUs=1204000;in.linePattern=2;CHECK(step()==MotorAction::Stop);
        CHECK(nav.following()&&nav.segmentReverse());
        SonarIo::timeUs=1205000;CHECK(sonars.update(nav.segmentReverse()));
        CHECK(sonars.rearActive()&&sonars.acquiring()&&!front.valid()&&!rear.valid());
        CHECK(step()==MotorAction::Stop); // No stale front reading may power reverse.
        pulse(true,1255000,583);CHECK(step()==MotorAction::Stop&&nav.obstaclePaused());
        pulse(true,1315000,1458);CHECK(step()==MotorAction::Backward&&!nav.obstaclePaused());
        CHECK(nav.active()&&!nav.stationOriented());
        nav.reachedNode("S1",millis());CHECK(!nav.active()&&nav.stationOriented());
        CHECK(!nav.stationTurnRequired()&&nav.update(in)==MotorAction::Stop);
        sonars.update(nav.segmentReverse());CHECK(!sonars.rearActive()&&!rear.valid());
        std::cout<<"PASS GS1 pivot/front -> STOP/fresh rear acquisition -> rear obstacle pause/resume -> RFID S1 stop\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
