#include <unity.h>
#include "core/CoreProtocol.h"
#include "core/RoutePlanner.h"
#include "core/CoreNavigator.h"
#include "core/MutexClearance.h"
#include <cstring>
#include <string>

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

void test_grant_owns_payload_before_ack_publish() {
    char wire[128]=R"({"zone":"M2","grant_id":57360})";
    CoreGrant grant;
    TEST_ASSERT_TRUE(CoreProtocol::decodeGrant(reinterpret_cast<uint8_t*>(wire),std::strlen(wire),grant));
    std::memset(wire,'X',sizeof(wire)); // PubSubClient ACK reuses the incoming packet storage.
    TEST_ASSERT_EQUAL_STRING("M2",grant.zone);TEST_ASSERT_EQUAL_INT(57360,grant.grantId);
    char ack[96];TEST_ASSERT_TRUE(CoreProtocol::grantAck(grant,ack,sizeof(ack)));
    TEST_ASSERT_EQUAL_STRING(R"({"zone":"M2","grant_id":57360})",ack);
    TEST_ASSERT_FALSE(CoreProtocol::grantAck(grant,ack,8));
    const char* invalid[]={R"({"zone":"M2XXXXXX","grant_id":1})",R"({"zone":"M7","grant_id":1})",
        R"({"zone":"M2\u0000X","grant_id":1})",R"({"zone":"M2","grant_id":0})",
        R"({"zone":"M2","grant_id":2147483648})",R"({"zone":"M2","grant_id":1}x)"};
    for (const auto* payload:invalid)
        TEST_ASSERT_FALSE(CoreProtocol::decodeGrant(reinterpret_cast<const uint8_t*>(payload),std::strlen(payload),grant));
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
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::TurnAround), static_cast<int>(plan.departure[0]));
}

void test_fixed_bay_heading_and_reverse_exit() {
    CoreCommand command;
    TEST_ASSERT_FALSE(decode(R"({"cmd":"SET_HEADING","cmd_id":69,"node":"S1","heading":"E"})",command));
    const char* bays[]={"S1","S2","K1","K2","K3","K4","K5","K6"};
    const RobotHeading heads[]={RobotHeading::South,RobotHeading::North,
        RobotHeading::West,RobotHeading::West,RobotHeading::West,
        RobotHeading::East,RobotHeading::East,RobotHeading::East};
    for (unsigned i=0;i<8;++i) {
        const char* gate=RoutePlanner::bayGate(bays[i]);
        const bool station=RoutePlanner::station(bays[i]);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(heads[i]),static_cast<int>(RoutePlanner::bayHeading(bays[i])));
        command=CoreCommand{};command.type=CoreCommandType::Goto;command.pathLength=1;
        std::strcpy(command.path[0],gate);std::strcpy(command.target,gate);
        RoutePlan route;
        // Fixed default applies even if a stale incoming/explicit heading says otherwise.
        TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,bays[i],command,route));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(station ? RouteTurn::Straight : RouteTurn::Reverse),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed(bays[i],command,route,false,true));
        TEST_ASSERT_EQUAL(station,RoutePlanner::reverseAllowed(bays[i],command,route,false,false));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(heads[i]),static_cast<int>(RoutePlanner::edgeHeading(bays[i],gate,!station)));
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start(bays[i],command,route,0));
        ControlInput input;input.wifi=input.mqtt=input.motionConfigured=input.lineConfigured=input.distanceValid=true;
        input.linePattern=2;input.now=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(station ? MotorAction::Forward : MotorAction::Backward),static_cast<int>(nav.update(input)));
        nav.reachedNode(gate,input.now+1);TEST_ASSERT_FALSE(nav.active());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(input)));
        std::strcpy(command.path[0],bays[i]);std::strcpy(command.target,bays[i]);
        TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,gate,command,route,false,RoutePlanner::edgeHeading(gate,bays[i])));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(nav.start(gate,command,route,input.now));input.now+=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(input)));
    }
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RobotHeading::Unknown),static_cast<int>(RoutePlanner::bayHeading("GS1")));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":70,"path":["GS1","M2"],"target":"M2"})",command));
    RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"S1",command,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[1]));
    route.departure[1]=RouteTurn::Reverse;
    TEST_ASSERT_FALSE(RoutePlanner::reverseAllowed("S1",command,route,false,true));
}

void test_station_arrival_turns_at_station_before_ready() {
    const char* stations[]={"S1","S2"};const char* gates[]={"GS1","GS2"};const char* previous[]={"M1","M4"};
    for (unsigned i=0;i<2;++i) {
        CoreCommand command;command.type=CoreCommandType::Goto;command.cmdId=73+i;command.pathLength=1;
        std::strcpy(command.target,stations[i]);std::strcpy(command.path[0],stations[i]);
        RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(previous[i],gates[i],command,route));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed(gates[i],command,route,false,true));
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed(gates[i],command,route,false,false));
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start(gates[i],command,route,0));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
        in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);
        in.linePattern=2;++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.following());TEST_ASSERT_FALSE(nav.segmentReverse());
        nav.reachedNode(stations[i],in.now);TEST_ASSERT_TRUE(nav.active());
        TEST_ASSERT_FALSE(nav.stationOriented());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        // A fresh GOTO cannot interrupt parking while the front still faces inward.
        TEST_ASSERT_FALSE(nav.start(stations[i],command,route,in.now));
        in.now+=Config::RFID_STOP_DWELL_MS;in.linePattern=2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
        in.now+=Config::TURN_MIN_MS+1; // Old line is not sufficient for readiness.
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
        in.linePattern=0;++in.now;nav.update(in);in.linePattern=2;++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_TRUE(nav.stationOriented());
        std::strcpy(command.target,gates[i]);std::strcpy(command.path[0],gates[i]);
        TEST_ASSERT_TRUE(RoutePlanner::plan(gates[i],stations[i],command,route,true));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(nav.start(stations[i],command,route,in.now));in.now+=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    }
    CoreCommand command;TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":76,"path":["S1"],"target":"S1"})",command));
    RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS1",command,route,false,RobotHeading::North));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
    CoreNavigator timeout;TEST_ASSERT_TRUE(timeout.start("GS1",command,route,0));
    timeout.reachedNode("S1",1);
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=1+Config::RFID_STOP_DWELL_MS;timeout.update(in);
    in.now+=Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::TurnTimeout),static_cast<int>(timeout.error()));
    TEST_ASSERT_FALSE(timeout.stationOriented());
}

void test_bay_exit_turnaround_counts_map_branches() {
    CoreCommand command;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":71,"path":["G1","M5"],"target":"M5"})",command));
    RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"K1",command,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Reverse),static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::TurnAround),static_cast<int>(route.departure[1]));
    TEST_ASSERT_EQUAL_INT(2,route.turnCrossings[1]); // G1 has a north branch before the desired east line.
    TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed("K1",command,route,false,true));
    CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("K1",command,route,0));nav.reachedNode("G1",100);
    TEST_ASSERT_TRUE(nav.grant("M5",126));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.now=100+Config::RFID_STOP_DWELL_MS;in.linePattern=2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);
    in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.now+=Config::TURN_MIN_MS+1; // One continuous centered patch must not count twice.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;++in.now;nav.update(in);in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":72,"path":["GS1"],"target":"GS1"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("C1","M1",command,route,true));
    TEST_ASSERT_EQUAL_INT(1,route.turnCrossings[0]); // No north branch at M1; the next line is the opposite line.
    CoreNavigator single;TEST_ASSERT_TRUE(single.start("M1",command,route,0));TEST_ASSERT_TRUE(single.grant("M1",127));
    in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;single.update(in);
    in.now+=Config::TURN_MIN_MS+1;in.linePattern=0;single.update(in);++in.now;in.linePattern=2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(single.update(in)));
    CoreNavigator timeout;TEST_ASSERT_TRUE(timeout.start("M1",command,route,0));TEST_ASSERT_TRUE(timeout.grant("M1",128));
    in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=0;timeout.update(in);
    in.now+=Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::TurnTimeout),static_cast<int>(timeout.error()));
}

void test_manual_tag_relocation_does_not_claim_reverse_heading() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":66,"path":["GS1","M2"],"target":"M2"})", command));
    // Station convention overrides even a stale GS1 -> S1 incoming scan.
    TEST_ASSERT_TRUE(RoutePlanner::plan("GS1", "S1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(route.departure[0]));
    const char* incoming = RoutePlanner::confirmedIncoming(false, "GS1", "S1");
    TEST_ASSERT_NULL(incoming);
    TEST_ASSERT_TRUE(RoutePlanner::plan(incoming, "S1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(route.departure[1]));
    // Genuine expected arrivals must preserve the measured route heading.
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":67,"path":["M2"],"target":"M2"})", command));
    incoming = RoutePlanner::confirmedIncoming(true, "S1", "GS1");
    TEST_ASSERT_EQUAL_STRING("S1", incoming);
    TEST_ASSERT_TRUE(RoutePlanner::plan(incoming, "GS1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(route.departure[0]));
    // Manual relocation between ordinary tags cannot invent a boot heading.
    TEST_ASSERT_TRUE(RoutePlanner::plan(RoutePlanner::confirmedIncoming(false, "S1", "GS1"),
                                      "GS1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Unknown), static_cast<int>(route.departure[0]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":68,"path":["GS2","M3"],"target":"M3"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(RoutePlanner::confirmedIncoming(false, "GS2", "S2"),
                                      "S2", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(route.departure[0]));
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

void test_robot02_gs2_turn_waits_for_grant_and_stops_on_network_loss() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":64,"path":["GS2","M3","C3"],"target":"C3"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"S2",command,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[1]));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S2",command,route,0,false));
    ControlInput input;
    input.wifi=input.mqtt=input.motionConfigured=input.lineConfigured=input.distanceValid=true;
    input.linePattern=2;input.now=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(input)));
    nav.reachedNode("GS2",++input.now);
    TEST_ASSERT_EQUAL_STRING("M3",nav.missingZone());
    input.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(input)));
    TEST_ASSERT_TRUE(nav.grant("M3",125));
    input.linePattern=6;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(input)));
    input.linePattern=7;input.now+=Config::TURN_MIN_MS+1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(input)));
    input.linePattern=0;++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(input)));
    input.linePattern=2;++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(input)));
    input.mqtt=false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(input)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::NotConnected),static_cast<int>(nav.error()));
    TEST_ASSERT_FALSE(nav.active());
    // Reconnect/reset at GS2 does not establish heading without a prior RFID.
    TEST_ASSERT_TRUE(nav.clearFault());
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":65,"path":["M3","C3"],"target":"C3"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS2",command,route));
    TEST_ASSERT_FALSE(nav.start("GS2",command,route,input.now,false));
}

void test_trial_gently_recovers_from_small_line_offset() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":57,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("C1", "M1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("M1", command, route, 0, true));
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

void test_node_order_parser_and_correlation() {
    CoreCommand order, duplicate;
    const char* payload=R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":1})";
    TEST_ASSERT_TRUE(decode(payload,order));TEST_ASSERT_TRUE(decode(payload,duplicate));
    TEST_ASSERT_TRUE(CoreProtocol::sameCommand(order,duplicate));
    duplicate.routeCmdId=100;TEST_ASSERT_FALSE(CoreProtocol::sameCommand(order,duplicate));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":102,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":1})",order));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":33,"maneuver":"LEFT","turn_crossings":1})",order));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":0})",order));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"FORWARD","turn_crossings":0})",order));
}

static CoreCommand orderFor(const char* node,const char* next,const char* turn,uint8_t step,int32_t route,uint8_t crossings) {
    CoreCommand c;c.type=CoreCommandType::NodeOrder;c.cmdId=route+step+10;c.routeCmdId=route;
    std::strcpy(c.node,node);std::strcpy(c.nextNode,next);std::strcpy(c.maneuver,turn);
    c.pathIndex=step;c.turnCrossings=crossings;return c;
}

#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
void test_robot01_blank_line_waits_for_rfid_without_moving() {
    static_assert(Config::LINE_LOSS_CONFIRM_MS == 5000u, "Robot 01 blank-line hold must be five seconds");
    CoreCommand command; RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":160,"path":["GS1","M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    ControlInput in;
    in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;
    in.linePattern = 2;
    in.now = Config::RFID_STOP_DWELL_MS;

    CoreNavigator arrival;
    TEST_ASSERT_TRUE(arrival.start("S1", command, route, 0, false, true));
    TEST_ASSERT_TRUE(arrival.authorizeNodeOrder(orderFor("S1", "GS1", "STRAIGHT", 0, 160, 0)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(arrival.update(in)));
    const uint32_t blankAt = in.now + 10;
    in.now = blankAt; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(arrival.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(arrival.error()));
    in.now = blankAt + Config::LINE_LOSS_CONFIRM_MS - 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(arrival.update(in)));
    arrival.reachedNode("GS1", in.now); // RFID is still polled while the motor is stopped.
    TEST_ASSERT_TRUE(arrival.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(arrival.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(arrival.error()));

    CoreNavigator timeout;
    TEST_ASSERT_TRUE(timeout.start("S1", command, route, 0, false, true));
    TEST_ASSERT_TRUE(timeout.authorizeNodeOrder(orderFor("S1", "GS1", "STRAIGHT", 0, 160, 0)));
    in.now = Config::RFID_STOP_DWELL_MS; in.linePattern = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(timeout.update(in)));
    in.now = blankAt; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(timeout.update(in)));
    in.now = blankAt + Config::LINE_LOSS_CONFIRM_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(timeout.error()));
    TEST_ASSERT_FALSE(timeout.active());

    CoreNavigator recovered;
    TEST_ASSERT_TRUE(recovered.start("S1", command, route, 0, false, true));
    TEST_ASSERT_TRUE(recovered.authorizeNodeOrder(orderFor("S1", "GS1", "STRAIGHT", 0, 160, 0)));
    in.now = Config::RFID_STOP_DWELL_MS; in.linePattern = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(recovered.update(in)));
    in.now = blankAt; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    in.now = blankAt + 100; in.linePattern = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(recovered.update(in)));
    in.now = blankAt + 200; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    in.mqtt = false; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::NotConnected), static_cast<int>(recovered.error()));
}
#endif

#if defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
void test_robot02_still_faults_immediately_on_blank_line() {
    static_assert(Config::LINE_LOSS_CONFIRM_MS == 0u, "Robot 02 line-loss policy is unchanged");
    CoreCommand command; RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":161,"path":["GS2"],"target":"GS2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S2", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S2", command, route, 0, false, false));
    ControlInput in;
    in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;
    in.linePattern = 2;
    in.now = Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(in)));
    in.linePattern = 0; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(nav.error()));
}
#endif

void test_server_node_order_stops_then_pivots_left_with_mutex() {
    CoreCommand command;RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":101,"path":["GS1","M2"],"target":"M2"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"S1",command,route));
    CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("S1",command,route,0,false,true));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    in.now=5000;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    auto order=orderFor("S1","GS1","STRAIGHT",0,101,0);
    order.routeCmdId=100;TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order));order.routeCmdId=101;
    TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    nav.reachedNode("GS1",++in.now);in.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order)); // Old S1 departure cannot run at GS1.
    order=orderFor("GS1","M2","RIGHT",1,101,1);TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order));
    std::strcpy(order.maneuver,"LEFT");TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in))); // Order never bypasses mutex.
    TEST_ASSERT_TRUE(nav.grant("M2",201));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    in.linePattern=7;in.now+=Config::TURN_MIN_MS+1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    in.linePattern=0;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    in.linePattern=2;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order)); // Duplicate cannot restart a pivot during following.
    in.mqtt=false;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
}

void test_server_node_order_right_then_station_turn() {
    CoreCommand command;RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":111,"path":["S1"],"target":"S1"})",command));
    // A known west-facing front needs a physical right pivot to the north station line.
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS1",command,route,false,RobotHeading::West));
    CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("GS1",command,route,0,false,true));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
    auto order=orderFor("GS1","S1","RIGHT",0,111,1);TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    nav.reachedNode("S1",in.now);in.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(nav.stationTurnRequired());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    order=orderFor("S1","S1","STATION_TURN_RIGHT",1,111,1);TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_TRUE(nav.stationOriented());
    CoreNavigator timeout;TEST_ASSERT_TRUE(timeout.start("GS1",command,route,0,false,true));
    in.now=Config::SEGMENT_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::MotionTimeout),static_cast<int>(timeout.error()));
}

static ControlInput straightInput() {
    ControlInput in;
    in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2; in.now=Config::RFID_STOP_DWELL_MS+1;
    return in;
}
static bool startStraightTrip(CoreNavigator& nav, ControlInput& in, bool through = true) {
    CoreCommand c;
    if (!decode(through ? R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","C3"],"target":"C3","pass_through":[0,1,2]})"
                       : R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","C3"],"target":"C3"})",c)) return false;
    RoutePlan plan;
    if (!RoutePlanner::plan("","C4",c,plan,false,RobotHeading::West) ||
        !nav.start("C4",c,plan,0,false,true) || !nav.grant("M4",101)) return false;
    CoreCommand order;
    if (!decode(R"({"cmd":"NODE_ORDER","cmd_id":43,"route_cmd_id":42,"node":"C4","next_node":"M4","path_index":0,"maneuver":"STRAIGHT","turn_crossings":0})",order) ||
        !nav.authorizeNodeOrder(order)) return false;
    return nav.update(in) == MotorAction::Forward;
}
void test_pass_through_parser_dedup_and_bounds() {
    CoreCommand c, old;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","C3"],"target":"C3","pass_through":[0,1,2]})",c));
    TEST_ASSERT_TRUE(c.passThrough[0] && c.passThrough[1] && c.passThrough[2]);
    TEST_ASSERT_FALSE(c.passThrough[3]);old=c;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","C3"],"target":"C3"})",c));
    TEST_ASSERT_FALSE(CoreProtocol::sameCommand(old,c));
    for (const auto* list : {"[0,0]","[3]","[-1]","[true]","[1.5]","[\"0\"]","{}","null","[32]","[4294967295]"}) {
        const std::string wire=std::string(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","C3"],"target":"C3","pass_through":)")+list+"}";
        TEST_ASSERT_FALSE(decode(wire.c_str(),c));
    }
    std::string wire=R"({"cmd":"GOTO","cmd_id":50,"target":"GS2","path":[)";
    for (int i=0;i<32;++i) wire+=(i ? "," : "")+std::string(i%2 ? "\"GS2\"" : "\"M4\"");
    wire+="],\"pass_through\":[";
    for (int i=0;i<31;++i) wire+=(i ? "," : "")+std::to_string(i);
    wire+="]}";
    TEST_ASSERT_TRUE(decode(wire.c_str(),c));TEST_ASSERT_EQUAL_INT(32,c.pathLength);
    TEST_ASSERT_TRUE(c.passThrough[30]);TEST_ASSERT_FALSE(c.passThrough[31]);
}
void test_straight_trip_reports_each_rfid_without_dwell() {
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    TEST_ASSERT_FALSE(nav.grant("M3",202)); // Not a whole-route reservation at C4.
    TEST_ASSERT_TRUE(nav.canPassThrough("M4",in));nav.reachedNode("M4",++in.now,&in);
    TEST_ASSERT_TRUE(nav.following());TEST_ASSERT_FALSE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_INT(1,nav.pathIndex());
    TEST_ASSERT_EQUAL_STRING("M3",nav.requestedZone());TEST_ASSERT_TRUE(nav.grant("M3",202));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    MutexClearance clearance;clearance.grant("M4",101,"C4",true);clearance.reached("C4","M4");
    MutexClearance::Release release;TEST_ASSERT_FALSE(clearance.pending(release));
    TEST_ASSERT_TRUE(nav.canPassThrough("GS2",in));nav.reachedNode("GS2",++in.now,&in);
    clearance.reached("M4","GS2");TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M4",release.zone);TEST_ASSERT_EQUAL_INT(101,release.grantId);
    clearance.queued(release);nav.revokeGrant("M4",101);
    TEST_ASSERT_EQUAL_INT(2,nav.pathIndex());TEST_ASSERT_FALSE(nav.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.canPassThrough("M3",in));nav.reachedNode("M3",++in.now,&in);
    TEST_ASSERT_EQUAL_INT(3,nav.pathIndex());TEST_ASSERT_FALSE(nav.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.canPassThrough("C3",in));nav.reachedNode("C3",++in.now,&in);
    TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
}
void test_through_waits_at_gate_without_lookahead_grant() {
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    nav.reachedNode("M4",++in.now,&in);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_STRING("M3",nav.requestedZone());
    TEST_ASSERT_FALSE(nav.canPassThrough("GS2",in));nav.reachedNode("GS2",++in.now,&in);
    TEST_ASSERT_TRUE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_STRING("M3",nav.missingZone());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.grant("M3",202)); // Receipt cannot replace NODE_ORDER after stopping.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    CoreCommand order;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"NODE_ORDER","cmd_id":44,"route_cmd_id":42,"node":"GS2","next_node":"M3","path_index":2,"maneuver":"STRAIGHT","turn_crossings":0})",order));
    TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));in.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
}
void test_through_never_overrides_hazards_or_stop() {
    for (int hazard=0;hazard<6;++hazard) {
        CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
        if(hazard==0)in.wifi=false;
        if(hazard==1)in.mqtt=false;
        if(hazard==2)in.obstacle=true;
        if(hazard==3)in.linePattern=0;
        if(hazard==4)in.distanceValid=false;
        if(hazard==5)in.now+=Config::SEGMENT_TIMEOUT_MS;
        TEST_ASSERT_FALSE(nav.canPassThrough("M4",in));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    }
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    nav.revokeGrant("M4",101);TEST_ASSERT_FALSE(nav.canPassThrough("M4",in));
    nav.reachedNode("M4",++in.now,&in);TEST_ASSERT_TRUE(nav.needsNodeOrder());
    nav.stop();TEST_ASSERT_FALSE(nav.canPassThrough("GS2",in));TEST_ASSERT_FALSE(nav.active());
    CoreNavigator wrong;TEST_ASSERT_TRUE(startStraightTrip(wrong,in));
    wrong.reachedNode("M3",in.now,&in);TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::UnexpectedNode),static_cast<int>(wrong.error()));
    CoreNavigator disconnected;TEST_ASSERT_TRUE(startStraightTrip(disconnected,in));
    disconnected.resetGrants();TEST_ASSERT_FALSE(disconnected.active());
}
void test_through_rejects_turns_and_keeps_legacy_dwell() {
    CoreCommand c;RoutePlan plan;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","G5","K5"],"target":"K5","pass_through":[0]})",c));
    TEST_ASSERT_TRUE(RoutePlanner::plan("","C4",c,plan,false,RobotHeading::West));
    CoreNavigator turn;TEST_ASSERT_FALSE(turn.start("C4",c,plan,0,false,true));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("C4","M4","G5"));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("GS1","S1","GS1"));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("G5","K5","G5"));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("K1","G1","M5"));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["G1","M5"],"target":"M5","pass_through":[]})",c));
    TEST_ASSERT_TRUE(RoutePlanner::plan("","K1",c,plan));
    CoreNavigator exit;TEST_ASSERT_TRUE(exit.start("K1",c,plan,0,false,true));
    CoreNavigator legacy;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(legacy,in,false));
    TEST_ASSERT_FALSE(legacy.canPassThrough("M4",in));legacy.reachedNode("M4",++in.now,&in);
    TEST_ASSERT_TRUE(legacy.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(legacy.update(in)));
}
void test_sonar_pause_keeps_route_and_segment_clock() {
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    const auto routeId = nav.routeCommandId();
    in.now += 100;
    in.obstacle = true;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.obstaclePaused());
    TEST_ASSERT_TRUE(nav.active());
    TEST_ASSERT_EQUAL_INT(routeId,nav.routeCommandId());
    in.now += Config::SEGMENT_TIMEOUT_MS + 100;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),static_cast<int>(nav.error()));
    in.obstacle = false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.obstaclePaused());
    TEST_ASSERT_EQUAL_INT(routeId,nav.routeCommandId());
    in.obstacle = true;nav.update(in);nav.stop();
    TEST_ASSERT_FALSE(nav.obstaclePaused());
    TEST_ASSERT_FALSE(nav.active());
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
    RUN_TEST(test_pass_through_parser_dedup_and_bounds);
    RUN_TEST(test_straight_trip_reports_each_rfid_without_dwell);
    RUN_TEST(test_through_waits_at_gate_without_lookahead_grant);
    RUN_TEST(test_through_never_overrides_hazards_or_stop);
    RUN_TEST(test_through_rejects_turns_and_keeps_legacy_dwell);
    if (Config::REAR_ULTRASONIC_ECHO_PIN >= 0) RUN_TEST(test_sonar_pause_keeps_route_and_segment_clock);
    RUN_TEST(test_core_commands);
    RUN_TEST(test_grant_owns_payload_before_ack_publish);
    RUN_TEST(test_node_order_parser_and_correlation);
    RUN_TEST(test_server_node_order_stops_then_pivots_left_with_mutex);
    RUN_TEST(test_server_node_order_right_then_station_turn);
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
    RUN_TEST(test_robot01_blank_line_waits_for_rfid_without_moving);
#endif
#if defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
    RUN_TEST(test_robot02_still_faults_immediately_on_blank_line);
#endif
    RUN_TEST(test_bad_commands_stay_rejected);
    RUN_TEST(test_core_wire_formats);
    RUN_TEST(test_bench_voltage_interlock);
    RUN_TEST(test_mutex_clearance_requires_measured_next_tag);
    RUN_TEST(test_mutex_clearance_rejects_unvisited_or_wrong_route);
    RUN_TEST(test_directed_route_turns);
    RUN_TEST(test_fixed_bay_heading_and_reverse_exit);
    RUN_TEST(test_station_arrival_turns_at_station_before_ready);
    RUN_TEST(test_bay_exit_turnaround_counts_map_branches);
    RUN_TEST(test_manual_tag_relocation_does_not_claim_reverse_heading);
    RUN_TEST(test_solo_route_crosses_mutex_without_grant);
    RUN_TEST(test_core_navigator_stops_at_rfid);
    RUN_TEST(test_two_robot_navigation_requires_and_revokes_grant);
    RUN_TEST(test_turn_does_not_reacquire_junction_before_blank);
    RUN_TEST(test_robot02_gs2_turn_waits_for_grant_and_stops_on_network_loss);
    if (Config::ONE_ROBOT_TRIAL) RUN_TEST(test_trial_gently_recovers_from_small_line_offset);
    return UNITY_END();
}
