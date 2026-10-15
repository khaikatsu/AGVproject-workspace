#include "sensors/SensorModule.h"
#include "core/CoreNavigator.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(false)

int main() {
    try {
        SensorModule front;
        front.begin();
        const auto pulse = [&front](uint32_t start, uint32_t width) {
            SonarIo::timeUs = start; front.update();
            SonarIo::fire(13, HIGH, start + 100);
            SonarIo::fire(13, LOW, start + 100 + width);
            front.update(false);
        };
        CoreCommand command;
        command.type = CoreCommandType::Goto; command.cmdId = 101; command.pathLength = 2;
        AGVMath::copyText(command.path[0], sizeof(command.path[0]), "GS1");
        AGVMath::copyText(command.path[1], sizeof(command.path[1]), "M2");
        AGVMath::copyText(command.target, sizeof(command.target), "M2");
        RoutePlan route; CHECK(RoutePlanner::plan(nullptr, "S1", command, route));
        CHECK(route.departure[0] == RouteTurn::Straight && route.departure[1] == RouteTurn::Left);
        CoreNavigator nav; CHECK(nav.start("S1", command, route, 0));
        ControlInput in; in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = true;
        in.linePattern = 2;
        const auto step = [&]() {
            in.now = millis(); in.distanceValid = front.valid();
            in.obstacle = front.valid() && front.readDistanceCm() <=
                (nav.obstaclePaused() ? Config::OBSTACLE_CLEAR_CM : Config::OBSTACLE_DISTANCE_CM);
            return nav.update(in);
        };
        pulse(1000000, 1458); // About 25 cm; front clear.
        CHECK(step() == MotorAction::Forward);
        nav.reachedNode("GS1", 1003);
        CHECK(nav.grant("M2", 201));
        pulse(1990000, 1458);
        CHECK(step() == MotorAction::Stop); // RFID dwell not finished yet.

        SonarIo::timeUs = 2050000; front.update();
        SonarIo::fire(13, HIGH, 2050100);
        SonarIo::timeUs = 2081000; front.update(false); // Still HIGH after 30 ms.
        CHECK(front.valid()); // Do not discard the fresh preceding clear sample.
        CHECK(step() == MotorAction::PivotLeft);
        SonarIo::fire(13, LOW, 2088100); front.update(false); // Normal 38 ms no-target pulse.
        CHECK(front.valid() && std::isnan(front.readDistanceCm()));
        CHECK(step() == MotorAction::PivotLeft && nav.error() == ErrorCode::None);
        CHECK(nav.routeCommandId() == 101);

        pulse(2110000, 583); // Real obstacle at about 10 cm still stops immediately.
        CHECK(step() == MotorAction::Stop && nav.obstaclePaused());
        pulse(2170000, 38000); // Clear again, same route/turn continues.
        CHECK(front.valid() && step() == MotorAction::PivotLeft);
        CHECK(nav.routeCommandId() == 101 && !nav.obstaclePaused());

        // Real robot log: completed pulses around 47.5 ms, even on a clear path.
        SonarIo::timeUs = 2230000; front.update();
        SonarIo::fire(13, HIGH, 2230100);
        SonarIo::timeUs = 2276000; front.update(false); // Poll while still HIGH at 46 ms.
        CHECK(front.valid());
        SonarIo::fire(13, LOW, 2277587); front.update(false); // 47487 us from actual log.
        CHECK(front.valid() && std::isnan(front.readDistanceCm()));
        CHECK(step() == MotorAction::PivotLeft);
        CHECK(front.diagnostic().pulseUs == 47487);
        pulse(2290000, 47494);
        CHECK(front.valid() && step() == MotorAction::PivotLeft);

        SonarIo::timeUs = 2350000; front.update();
        SonarIo::fire(13, HIGH, 2350100);
        SonarIo::timeUs = 2350000 + Config::ULTRASONIC_ECHO_TIMEOUT_US + 1000;
        front.update(false); // Truly stuck beyond the configured deadline.
        CHECK(!front.valid());
        CHECK(step() == MotorAction::Stop && nav.error() == ErrorCode::SensorInvalid);
        std::cout << "PASS S1-GS1 sonar: 38ms/47.5ms no-target during pivot, real obstacle stop/resume, stuck HIGH fault\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
