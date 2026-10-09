#include <unity.h>
#include "core/CoreProtocol.h"
#include "core/RoutePlanner.h"
#include "core/CoreNavigator.h"
#include "core/MutexClearance.h"
#include <cstring>

void setUp() {}
void tearDown() {}

static bool decode(const char* json, CoreCommand& command) {
    return CoreProtocol::decodeCommand(reinterpret_cast<const uint8_t*>(json), std::strlen(json), command);
}

void test_core_commands() {
    CoreCommand command;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"})", command));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CoreCommandType::Goto), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_INT(42, command.cmdId);
    TEST_ASSERT_EQUAL_STRING("M1", command.path[0]);
    TEST_ASSERT_EQUAL_STRING("GS1", command.target);
    CoreCommand original = command, replay;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"})", replay));
    TEST_ASSERT_TRUE(CoreProtocol::sameCommand(original, replay));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["G1","GS1"],"target":"GS1"})", replay));
    TEST_ASSERT_FALSE(CoreProtocol::sameCommand(original, replay));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"STOP","cmd_id":43})", command));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"WAIT","cmd_id":44})", command));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"CHARGE_START","cmd_id":45})", command));
}

void test_bad_commands_stay_rejected() {
    CoreCommand command;
    TEST_ASSERT_FALSE(decode(R"({"command":"TASK","cmd_id":46})", command));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"GOTO","cmd_id":0,"path":["M1"],"target":"M1"})", command));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"GOTO","cmd_id":47,"path":["M1"],"target":"G1"})", command));
    TEST_ASSERT_FALSE(decode("{\"cmd\":\"STOP\",\"cmd_id\":49} x", command));
}

void test_core_wire_formats() {
    char wire[64];
    TEST_ASSERT_TRUE(CoreProtocol::telemetry("UNKNOWN", 85, false, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN|85|0", wire);
    TEST_ASSERT_TRUE(CoreProtocol::telemetry("S1", -1, false, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_STRING("S1|?|0", wire);
    TEST_ASSERT_TRUE(CoreProtocol::ack(42, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_STRING("42", wire);
}

void test_bench_voltage_interlock() {
    if (Config::TWO_ROBOT_BENCH_TRIAL) {
        TEST_ASSERT_FALSE(Config::benchVoltageReady(0.71f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(5.23f));
        TEST_ASSERT_TRUE(Config::benchVoltageReady(7.4f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(9.0f));
    }
}

void test_mutex_clearance_requires_measured_next_tag() {
    MutexClearance clearance;
    MutexClearance::Release release;
    clearance.grant("M1", 91, "C1", true);
    clearance.reached("C1", "M1");
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("M1", "G2"); // 35 cm; earliest read leaves only 7 cm.
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("G2", "M5"); // Verified straight 35 + 35 cm.
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M1", release.zone);
    TEST_ASSERT_EQUAL_INT(91, release.grantId);
    clearance.queued(release);
    TEST_ASSERT_FALSE(clearance.pending(release));
    TEST_ASSERT_FALSE(clearance.canAcceptGrant("M1", 91));
    clearance.reset();
    TEST_ASSERT_FALSE(clearance.canAcceptGrant("M1", 91));
    TEST_ASSERT_TRUE(clearance.canAcceptGrant("M1", 94));

    clearance.grant("M4", 92, "M4", true);
    clearance.reached("M4", "GS2"); // 50 cm measured exit.
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M4", release.zone);
    TEST_ASSERT_EQUAL_INT(92, release.grantId);
    clearance.reset(); // Disconnect keeps Core ownership; no local replay.
    TEST_ASSERT_FALSE(clearance.pending(release));
}

void test_mutex_clearance_rejects_unvisited_or_wrong_route() {
    MutexClearance clearance;
    MutexClearance::Release release;
    clearance.grant("M6", 93, "G5", true);
    clearance.reached("M6", "G6");
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("G5", "M6");
    clearance.reached("M6", "G4");
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("G4", "K4"); // A turn is not the measured straight chain.
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("M6", "G6");
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M6", release.zone);
    TEST_ASSERT_EQUAL_INT(93, release.grantId);
}

void test_directed_route_turns() {
    TEST_ASSERT_TRUE(RoutePlanner::directedEdge("M1", "G2"));
    TEST_ASSERT_FALSE(RoutePlanner::directedEdge("M5", "G2"));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Right), static_cast<int>(RoutePlanner::turn("C1", "M1", "G2")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(RoutePlanner::turn("C1", "M1", "GS1")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(RoutePlanner::turn("G2", "M5", "M6")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Right), static_cast<int>(RoutePlanner::turn("G5", "M6", "G6")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Reverse), static_cast<int>(RoutePlanner::turn("G2", "K2", "G2")));

    CoreCommand command;
    RoutePlan plan;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":50,"path":["M5","M6","G6"],"target":"G6"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("M1", "G2", command, plan));
    TEST_ASSERT_EQUAL_INT(3, plan.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(plan.departure[1]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[2]));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "G2", command, plan));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Unknown), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_FALSE(RoutePlanner::plan("M5", "G2", command, plan));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":51,"path":["G2"],"target":"G2"})", command));
    TEST_ASSERT_FALSE(RoutePlanner::plan(nullptr, "M5", command, plan));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":52,"path":["GS1","M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, plan));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(plan.departure[1]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":58,"path":["GS2"],"target":"GS2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S2", command, plan));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":53,"path":["M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("S1", "GS1", command, plan, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Right), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":55,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("C1", "M1", command, plan, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Reverse), static_cast<int>(plan.departure[0]));
}

void test_solo_route_crosses_mutex_without_grant() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":56,"path":["GS1","M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    TEST_ASSERT_TRUE(RoutePlanner::oneRobotTrialRoute("01", "S1", command, route));
    TEST_ASSERT_FALSE(RoutePlanner::oneRobotTrialRoute("02", "S1", command, route));
    CoreNavigator normal;
    TEST_ASSERT_TRUE(normal.start("S1", command, route, 0));
    normal.reachedNode("GS1", 202);
    TEST_ASSERT_EQUAL_STRING("M2", normal.missingZone());
    CoreNavigator solo;
    TEST_ASSERT_TRUE(solo.start("S1", command, route, 0, true));
    solo.reachedNode("GS1", 202);
    TEST_ASSERT_NULL(solo.missingZone());
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.linePattern = 2;
    input.now = 202 + Config::RFID_STOP_DWELL_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(solo.update(input)));
    solo.stop();
    TEST_ASSERT_NULL(solo.missingZone());
}

void test_core_navigator_stops_at_rfid() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":54,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S1", command, route, 0));
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.linePattern = 2;
    input.now = Config::RFID_STOP_DWELL_MS - 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(input)));
    input.now = Config::RFID_STOP_DWELL_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(input)));
    nav.reachedNode("GS1", 202);
    TEST_ASSERT_EQUAL_INT(AGV_IDLE, nav.state());
}

void test_two_robot_navigation_requires_and_revokes_grant() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":62,"path":["M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("S1", "GS1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("GS1", command, route, 0));
    TEST_ASSERT_EQUAL_STRING("M2", nav.missingZone());
    TEST_ASSERT_TRUE(nav.grant("M2", 124));
    TEST_ASSERT_NULL(nav.missingZone());
    nav.revokeGrant("M2", 123); // Old token cannot revoke the current grant.
    TEST_ASSERT_NULL(nav.missingZone());
    nav.revokeGrant("M2", 124);
    TEST_ASSERT_EQUAL_STRING("M2", nav.missingZone());
}

void test_turn_does_not_reacquire_junction_before_blank() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":55,"path":["M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("S1", "GS1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("GS1", command, route, 0, true));
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.linePattern = 3;
    input.now = Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 7;
    input.now += Config::TURN_MIN_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 0;
    ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 2;
    ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(input)));
}

void test_trial_gently_recovers_from_small_line_offset() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":57,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S1", command, route, 0, true));
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.now = Config::RFID_STOP_DWELL_MS + 1;
    input.linePattern = 6;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::GentleCorrectLeft), static_cast<int>(nav.update(input)));
    TEST_ASSERT_EQUAL_STRING("GENTLE_CORRECT_LEFT", motorActionName(AGVMath::lineAction(6)));
    input.linePattern = 2; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(input)));
    input.linePattern = 3; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::GentleCorrectRight), static_cast<int>(nav.update(input)));
    input.linePattern = 4; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::CorrectLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 1; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::CorrectRight), static_cast<int>(nav.update(input)));
    input.linePattern = 0; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(input)));
}

int main() {
    static_assert(Config::ENABLE_CORE_MOTION ==
                  (Config::ONE_ROBOT_TRIAL || Config::TWO_ROBOT_BENCH_TRIAL),
                  "Only explicit trial builds may enable motion");
    static_assert(Config::RFID_STOP_DWELL_MS == (Config::ENABLE_CORE_MOTION ? 1000u : 200u),
                  "RFID pause must match the selected firmware mode");
    static_assert(!Config::ONE_ROBOT_TRIAL || (Config::GENTLE_CORRECTION_SPEED > 0 &&
                  Config::GENTLE_CORRECTION_SPEED < Config::CORRECTION_SPEED),
                  "Trial gentle correction must keep both wheels moving");
    UNITY_BEGIN();
    RUN_TEST(test_core_commands);
    RUN_TEST(test_bad_commands_stay_rejected);
    RUN_TEST(test_core_wire_formats);
    RUN_TEST(test_bench_voltage_interlock);
    RUN_TEST(test_mutex_clearance_requires_measured_next_tag);
    RUN_TEST(test_mutex_clearance_rejects_unvisited_or_wrong_route);
    RUN_TEST(test_directed_route_turns);
    RUN_TEST(test_solo_route_crosses_mutex_without_grant);
    RUN_TEST(test_core_navigator_stops_at_rfid);
    RUN_TEST(test_two_robot_navigation_requires_and_revokes_grant);
    RUN_TEST(test_turn_does_not_reacquire_junction_before_blank);
    if (Config::ONE_ROBOT_TRIAL) RUN_TEST(test_trial_gently_recovers_from_small_line_offset);
    return UNITY_END();
}
