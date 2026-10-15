#include "sensors/DirectionalSonar.h"
#include "core/CoreNavigator.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(false)

int main() {
    try {
        SensorModule front, rear(Config::REAR_ULTRASONIC_TRIG_PIN, Config::REAR_ULTRASONIC_ECHO_PIN);
        front.begin(); rear.begin();
        DirectionalSonar sonars(front, rear);
        CHECK(sonars.update(false));
        CHECK(!sonars.rearActive() && sonars.acquiring());
        CHECK(SonarIo::triggered.empty());
        SonarIo::timeUs = 50000; sonars.update(false);
        CHECK(SonarIo::triggered.size() == 1 && SonarIo::triggered.back() == 12);
        SonarIo::fire(13, HIGH, 50100); SonarIo::fire(13, LOW, 51558);
        sonars.update(false);
        CHECK(front.valid() && !rear.valid() && !sonars.acquiring());

        // A reverse route selects the rear BEFORE powering the motors. It must
        // keep scanning while paused, otherwise it could never see clearance.
        CoreNavigator nav;
        CoreCommand command; command.type = CoreCommandType::Goto; command.cmdId = 42;
        command.pathLength = 1;
        AGVMath::copyText(command.path[0], sizeof(command.path[0]), "G5");
        AGVMath::copyText(command.target, sizeof(command.target), "G5");
        RoutePlan route; route.count = 1; route.departure[0] = RouteTurn::Reverse;
        CHECK(nav.start("K5", command, route, 52, false, false));
        ControlInput in; in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = true;
        in.linePattern = 2;
        SonarIo::timeUs = 52000; CHECK(sonars.update(nav.segmentReverse()));
        CHECK(sonars.rearActive() && sonars.acquiring() && !front.valid() && !rear.valid());
        // Echoes from the old direction must not restore its cleared reading.
        SonarIo::fire(13, HIGH, 53000); SonarIo::fire(13, LOW, 54000);
        CHECK(!front.valid());
        SonarIo::timeUs = 101000; sonars.update(true);
        CHECK(SonarIo::triggered.size() == 1);
        SonarIo::timeUs = 102000; sonars.update(true);
        CHECK(SonarIo::triggered.size() == 2 && SonarIo::triggered.back() == 0);
        SonarIo::fire(15, HIGH, 102100); SonarIo::fire(15, LOW, 102974);
        sonars.update(true);
        CHECK(!sonars.acquiring() && rear.isObstacleDetected() && !front.valid());
        in.now = 1100; in.distanceValid = sonars.active().valid(); in.obstacle = true;
        CHECK(nav.update(in) == MotorAction::Stop && nav.obstaclePaused());
        const auto id = nav.routeCommandId();
        SonarIo::timeUs = 162000; sonars.update(nav.segmentReverse());
        CHECK(SonarIo::triggered.back() == 0 && SonarIo::triggered.size() == 3);
        SonarIo::fire(15, HIGH, 162100); SonarIo::fire(15, LOW, 163558);
        sonars.update(true);
        CHECK(!rear.isObstacleDetected() && rear.readDistanceCm() > Config::OBSTACLE_CLEAR_CM);
        in.now = 1200; in.obstacle = false;
        CHECK(nav.update(in) == MotorAction::Backward);
        CHECK(nav.routeCommandId() == id && !nav.obstaclePaused());

        // STOP/forward cancels the pending rear echo and rear triggering.
        SonarIo::timeUs = 222000; sonars.update(true);
        CHECK(SonarIo::triggered.back() == 0);
        nav.stop(); SonarIo::timeUs = 223000; sonars.update(nav.segmentReverse());
        CHECK(!sonars.rearActive() && !rear.valid() && sonars.acquiring());
        SonarIo::fire(15, HIGH, 224000); SonarIo::fire(15, LOW, 225000);
        const auto stoppedCount = SonarIo::triggered.size();
        SonarIo::timeUs = 273000; sonars.update(false);
        CHECK(SonarIo::triggered.size() == stoppedCount + 1 && SonarIo::triggered.back() == 12);
        CHECK(!rear.valid());
        SonarIo::timeUs = 273000 + Config::ULTRASONIC_ECHO_TIMEOUT_US;
        sonars.update(false); // LOW timeout: established outside-range policy.
        CHECK(front.valid() && !sonars.acquiring());

        // Stuck HIGH on a newly selected sonar may delay at most 200 ms.
        SonarIo::timeUs = 400000; sonars.update(true);
        SonarIo::timeUs = 450000; sonars.update(true);
        SonarIo::fire(15, HIGH, 450100);
        SonarIo::timeUs = 481000; sonars.update(true);
        CHECK(sonars.acquiring() && !rear.valid());
        SonarIo::timeUs = 599000; sonars.update(true); CHECK(sonars.acquiring());
        SonarIo::timeUs = 600000; sonars.update(true);
        CHECK(!sonars.acquiring() && !rear.valid());
        CHECK(nav.start("K5", command, route, 400, false, false));
        in.now = 600; in.distanceValid = false;
        CHECK(nav.update(in) == MotorAction::Stop && nav.error() == ErrorCode::SensorInvalid);

        // Once acquired, a stale sample gets no new acquisition grace period.
        SonarIo::levels[15] = LOW;
        SonarIo::timeUs = 650000; sonars.update(false);
        SonarIo::timeUs = 700000; sonars.update(false);
        SonarIo::fire(13, HIGH, 700100); SonarIo::fire(13, LOW, 701558); sonars.update(false);
        CHECK(front.valid() && !sonars.acquiring());
        SonarIo::timeUs = 1100000;
        CHECK(!front.valid() && !sonars.acquiring());
        std::cout << "PASS directional sonar: trigger gating, switch quiet/freshness, reverse pause/resume, STOP, timeout/stale faults\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
