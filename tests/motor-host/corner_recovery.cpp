#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>

#define CHECK(c) do { if (!(c)) { std::cerr << "CHECK FAILED: " << #c << " at line " << __LINE__ << std::endl; throw std::runtime_error(#c); } } while (false)

int main() {
    try {
        std::cout << "Testing Corner Lost-Line Recovery (Cua vang khoi line)..." << std::endl;
        static_assert(Config::CORNER_RECOVERY_ENABLED, "Corner recovery must be enabled");
        static_assert(Config::CORNER_RECOVERY_MS > 0, "Corner recovery duration must be > 0");
        static_assert(Config::CORNER_RECOVERY_PWM > 0, "Corner recovery PWM must be > 0");

        MotorModule motor;
        motor.begin();
        LineFollowModule line(motor);
        BuzzerModule buzzer;
        buzzer.begin();
        MovementModule move(motor, line, buzzer);

        CoreCommand c;
        c.type = CoreCommandType::Goto;
        c.cmdId = 1001;
        c.pathLength = 1;
        AGVMath::copyText(c.path[0], sizeof(c.path[0]), "M1");
        AGVMath::copyText(c.target, sizeof(c.target), "M1");

        RoutePlan route;
        CHECK(RoutePlanner::plan("M3", "G1", c, route));

        CoreNavigator nav;
        CHECK(nav.start("G1", c, route, 0));

        ControlInput in;
        in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;
        in.now = Config::RFID_STOP_DWELL_MS;
        in.linePattern = 2; // centered
        MotorAction early = MotorAction::Stop;

        // Waiting grant:
        CHECK(!nav.refreshFollowing(in, early));
        CHECK(nav.grant("M1", 50));
        CHECK(nav.update(in) == MotorAction::Forward);
        move.apply(MotorAction::Forward);

        // 1. Robot is following line forward:
        in.now += 20;
        in.linePattern = 2;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::Forward);
        CHECK(!nav.cornerRecovering());

        // 2. Robot enters curve, veering right (line on left: pattern 6 = 110, then pattern 4 = 100):
        in.now += 20;
        in.linePattern = 6;
        CHECK(nav.refreshFollowing(in, early));
        move.apply(early, nav.precisionPivot(), nav.precisionPivotPwm(), false,
                   {nav.forwardLineTracking(), in.linePattern, in.now});
        CHECK(!nav.cornerRecovering());

        in.now += 20;
        in.linePattern = 4;
        CHECK(nav.refreshFollowing(in, early));
        move.apply(early, nav.precisionPivot(), nav.precisionPivotPwm(), false,
                   {nav.forwardLineTracking(), in.linePattern, in.now});
        CHECK(!nav.cornerRecovering());

        // 3. High speed momentum causes robot to fly completely off the line (pattern 0 = 000):
        in.now += 10;
        in.linePattern = 0;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::PivotRight); // Should immediately steer/pivot right!
        CHECK(nav.cornerRecovering());
        CHECK(nav.precisionPivot());
        CHECK(nav.precisionPivotPwm() == Config::CORNER_RECOVERY_PWM);

        move.apply(early, nav.precisionPivot(), nav.precisionPivotPwm(), false,
                   {nav.forwardLineTracking(), in.linePattern, in.now});
        CHECK(move.action() == MotorAction::PivotRight);

        // Continue in corner recovery during blank (under Config::CORNER_RECOVERY_MS):
        in.now += 50;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::PivotRight);
        CHECK(nav.cornerRecovering());

        // 4. Line re-acquired at t + 100ms: pattern 2 (010) or 6 (110) appears!
        in.now += 50;
        in.linePattern = 2;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::Forward);
        CHECK(!nav.cornerRecovering()); // Must exit corner recovery immediately!
        CHECK(nav.forwardLineTracking()); // Line tracking active again!
        move.apply(early, nav.precisionPivot(), nav.precisionPivotPwm(), false,
                   {nav.forwardLineTracking(), in.linePattern, in.now});
        CHECK(move.action() == MotorAction::Forward);

        std::cout << "PASS: Left-drift (100/110) -> blank (000) -> PivotRight at "
                  << Config::CORNER_RECOVERY_PWM << " PWM -> line re-caught -> Forward PD resumed!" << std::endl;

        // 5. Test opposite side: curve drifting left (line on right: pattern 3 = 011 or 1 = 001):
        in.now += 50;
        in.linePattern = 3;
        CHECK(nav.refreshFollowing(in, early));
        move.apply(early, nav.precisionPivot(), nav.precisionPivotPwm(), false,
                   {nav.forwardLineTracking(), in.linePattern, in.now});

        in.now += 20;
        in.linePattern = 1;
        CHECK(nav.refreshFollowing(in, early));
        move.apply(early, nav.precisionPivot(), nav.precisionPivotPwm(), false,
                   {nav.forwardLineTracking(), in.linePattern, in.now});

        // Overshoot into 000:
        in.now += 10;
        in.linePattern = 0;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::PivotLeft); // Steers/pivots left!
        CHECK(nav.cornerRecovering());

        // Catches line again:
        in.now += 60;
        in.linePattern = 2;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::Forward);
        CHECK(!nav.cornerRecovering());

        std::cout << "PASS: Right-drift (001/011) -> blank (000) -> PivotLeft -> line re-caught -> Forward!" << std::endl;

        // 6. Test timeout fallback: if line is not found within Config::CORNER_RECOVERY_MS:
        in.now += 50;
        in.linePattern = 4;
        CHECK(nav.refreshFollowing(in, early));

        in.now += 10;
        in.linePattern = 0;
        const uint32_t lostAt = in.now;
        CHECK(nav.refreshFollowing(in, early));
        CHECK(early == MotorAction::PivotRight);
        CHECK(nav.cornerRecovering());

        // Advance past CORNER_RECOVERY_MS:
        in.now = lostAt + Config::CORNER_RECOVERY_MS + 10;
        const auto timeoutAction = nav.update(in);
        CHECK(!nav.cornerRecovering()); // Timeout clears corner recovering
        // Drops into standard line recovery / line loss:
        CHECK(nav.recoveringLine() || nav.error() == ErrorCode::LineLost || timeoutAction == MotorAction::Stop);

        std::cout << "PASS: Timeout after " << Config::CORNER_RECOVERY_MS << "ms safely falls back to standard line loss." << std::endl;
        std::cout << "ALL CORNER LOST-LINE RECOVERY TESTS PASSED FOR XE " << Config::AGV_ID << "!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
