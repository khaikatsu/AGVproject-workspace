#include "core/CoreNavigator.h"
#include "motion/MovementModule.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

#define CHECK(c) do { if (!(c)) throw std::runtime_error(std::string(#c)+" at line "+std::to_string(__LINE__)); } while(false)

int main() {
    try {
        static_assert(Config::REVERSE_ALIGN_FORWARD_MS > 0, "REVERSE_ALIGN_FORWARD_MS must be > 0");
        MotorModule motor; motor.begin();
        LineFollowModule line{motor};
        BuzzerModule buzzer; buzzer.begin();
        MovementModule move{motor, line, buzzer};
        CoreNavigator nav;
        ControlInput in;
        in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;

        const char* prevNode = (std::strcmp(Config::AGV_ID, "01") == 0) ? "M1" : "M4";
        const char* gsNode   = (std::strcmp(Config::AGV_ID, "01") == 0) ? "GS1" : "GS2";
        const char* sNode    = (std::strcmp(Config::AGV_ID, "01") == 0) ? "S1" : "S2";

        CoreCommand c;
        c.type = CoreCommandType::Goto;
        c.cmdId = 2001;
        c.pathLength = 1;
        std::strcpy(c.path[0], sNode);
        std::strcpy(c.target, sNode);

        RoutePlan route;
        CHECK(RoutePlanner::plan(prevNode, gsNode, c, route, false, RobotHeading::Unknown));
        CHECK(route.reverseAfterTurn[0] == true);
        CHECK(route.departure[0] == RouteTurn::Right);

        uint32_t now = 1000;
        CHECK(nav.start(gsNode, c, route, now));
        now += Config::RFID_STOP_DWELL_MS;

        // 1. Pivot right at GS
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::PivotRight);

        // 2. Line departures during pivot
        now += 100;
        in.now = now; in.linePattern = 0;
        CHECK(nav.update(in) == MotorAction::PivotRight);

        // 3. Line center 010 returns on the extension line -> enters TurnCapture
        now += 150;
        in.now = now; in.linePattern = 2;
        CHECK(nav.observeTurnLine(in));
        CHECK(nav.capturingTurnLine());

        // 4. In TurnCapture: align 010 for GS_CONFIRM_MS
        const uint32_t lockStart = now;
        while (now - lockStart < Config::GS_CONFIRM_MS) {
            in.now = now; in.linePattern = 2;
            CHECK(nav.update(in) == MotorAction::Stop);
            now += Config::LEFT_CAPTURE_SAMPLE_MS;
        }

        // Lock confirmed -> transitions to Following
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::Stop);
        CHECK(nav.following());

        // 5. In Following: forward PD alignment must be active
        CHECK(!nav.reverseAligned());
        CHECK(!nav.segmentReverse());
        CHECK(nav.forwardLineTracking());

        const uint32_t alignStart = now;

        // Halfway through forward alignment: still moving forward with PD
        now = alignStart + (Config::REVERSE_ALIGN_FORWARD_MS / 2);
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::Forward);
        CHECK(!nav.reverseAligned());
        CHECK(!nav.segmentReverse());
        CHECK(nav.forwardLineTracking());

        // Just before forward alignment ends: still forward
        now = alignStart + Config::REVERSE_ALIGN_FORWARD_MS - 10;
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::Forward);
        CHECK(!nav.reverseAligned());
        CHECK(!nav.segmentReverse());
        CHECK(nav.forwardLineTracking());

        // 6. In Settle pause: motor must STOP to kill inertia
        now = alignStart + Config::REVERSE_ALIGN_FORWARD_MS + 20;
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::Stop);
        CHECK(!nav.reverseAligned());
        CHECK(!nav.segmentReverse());

        // 7. Settle pause completes -> reverseAligned becomes true!
        now = alignStart + Config::REVERSE_ALIGN_FORWARD_MS + Config::REVERSE_ALIGN_SETTLE_MS + 10;
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::Stop); // One STOP cycle before reverse PWM
        CHECK(nav.reverseAligned());
        CHECK(nav.segmentReverse());
        CHECK(!nav.forwardLineTracking());

        // 8. Reversing backwards into S: reverse line action active
        now += 20;
        in.now = now; in.linePattern = 2;
        CHECK(nav.update(in) == MotorAction::Backward);
        CHECK(nav.reverseAligned());
        CHECK(nav.segmentReverse());

        // 9. RFID at S node reached
        nav.reachedNode(sNode, now);
        CHECK(!nav.active()); // Route complete!

        std::cout << "PASS " << Config::AGV_ID << ": GS docking forward PD align ("
                  << Config::REVERSE_ALIGN_FORWARD_MS << "ms), settle stop ("
                  << Config::REVERSE_ALIGN_SETTLE_MS << "ms), clean reverse to " << sNode << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
