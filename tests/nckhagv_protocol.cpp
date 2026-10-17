#include "core/CoreProtocol.h"
#include "core/ControlLogic.h"
#include "core/RoutePlanner.h"
#include "core/CoreNavigator.h"
#include <cassert>
#include <cstring>
#include <set>
#include <string>

static bool decode(const char* json, CoreCommand& command) {
    return CoreProtocol::decodeCommand(reinterpret_cast<const uint8_t*>(json), std::strlen(json), command);
}

int main() {
    static_assert(!Config::ENABLE_CORE_MOTION, "Motor must remain locked until zone clearance and motion are validated");
    std::set<std::string> nodes, uids;
    for (const auto& mapping : Config::RFID_NODES) {
        assert(AGVMath::validNode(mapping.node));
        assert(std::strlen(mapping.uid) == 8);
        assert(nodes.insert(mapping.node).second && uids.insert(mapping.uid).second);
    }
    assert(nodes.size() == 22);
    for (const auto& mapNode : RouteMap::NODES) assert(AGVMath::validNode(mapNode.id));
    for (const auto& from : RouteMap::NODES)
        for (const auto& via : RouteMap::NODES)
            if (RoutePlanner::directedEdge(from.id, via.id))
                for (const auto& to : RouteMap::NODES)
                    if (RoutePlanner::directedEdge(via.id, to.id))
                        assert(RoutePlanner::turn(from.id, via.id, to.id) != RouteTurn::Unknown);
    CoreCommand command;
    assert(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"})", command));
    assert(command.type == CoreCommandType::Goto && command.cmdId == 42);
    assert(command.pathLength == 2 && std::strcmp(command.path[0], "M1") == 0);
    assert(std::strcmp(command.target, "GS1") == 0);
    CoreCommand original = command, repeated, altered;
    assert(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"})", repeated));
    assert(CoreProtocol::sameCommand(original, repeated));
    assert(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["G1","GS1"],"target":"GS1"})", altered));
    assert(!CoreProtocol::sameCommand(original, altered));
    assert(decode(R"({"cmd":"STOP","cmd_id":43})", command));
    assert(command.type == CoreCommandType::Stop && command.cmdId == 43);
    assert(decode(R"({"cmd":"WAIT","cmd_id":44})", command));
    assert(command.type == CoreCommandType::Wait);
    assert(decode(R"({"cmd":"CHARGE_START","cmd_id":45})", command));
    assert(command.type == CoreCommandType::ChargeStart);

    assert(!decode(R"({"command":"TASK","cmd_id":46})", command));
    assert(!decode(R"({"cmd":"GOTO","cmd_id":0,"path":["M1"],"target":"M1"})", command));
    assert(!decode(R"({"cmd":"GOTO","cmd_id":47,"path":["M1"],"target":"G1"})", command));
    assert(!decode(R"({"cmd":"GOTO","cmd_id":48,"path":["NOPE"],"target":"NOPE"})", command));
    assert(!decode("{\"cmd\":\"STOP\",\"cmd_id\":49} x", command));
    std::string oversized(Config::MAX_COMMAND_BYTES + 1, ' ');
    assert(!CoreProtocol::decodeCommand(reinterpret_cast<const uint8_t*>(oversized.data()), oversized.size(), command));
    const uint8_t nul[] = {'{', '}', 0};
    assert(!CoreProtocol::decodeCommand(nul, sizeof(nul), command));

    char wire[64];
    assert(CoreProtocol::telemetry("UNKNOWN", 0, false, wire, sizeof(wire)));
    assert(std::strcmp(wire, "UNKNOWN|0|0") == 0);
    assert(CoreProtocol::telemetry("S1", 85, true, wire, sizeof(wire)));
    assert(std::strcmp(wire, "S1|85|1") == 0);
    assert(CoreProtocol::telemetry("S1", -1, false, wire, sizeof(wire)));
    assert(std::strcmp(wire, "S1|?|0") == 0);
    assert(!CoreProtocol::telemetry("S1", -2, false, wire, sizeof(wire)));
    assert(!CoreProtocol::telemetry("S1", 101, false, wire, sizeof(wire)));
    assert(CoreProtocol::ack(42, wire, sizeof(wire)) && std::strcmp(wire, "42") == 0);
    assert(!CoreProtocol::ack(0, wire, sizeof(wire)));

    assert(RoutePlanner::directedEdge("M1", "G2"));
    assert(!RoutePlanner::directedEdge("M5", "G2"));
    assert(RoutePlanner::turn("G1", "M1", "G2") == RouteTurn::Right);
    assert(RoutePlanner::turn("G1", "M1", "GS1") == RouteTurn::Straight);
    assert(RoutePlanner::turn("G2", "M5", "M6") == RouteTurn::Left);
    assert(RoutePlanner::turn("G5", "M6", "G6") == RouteTurn::Right);
    assert(RoutePlanner::turn("G2", "K2", "G2") == RouteTurn::Reverse);
    RoutePlan route;
    assert(decode(R"({"cmd":"GOTO","cmd_id":50,"path":["M5","M6","G6"],"target":"G6"})", command));
    assert(RoutePlanner::plan("M1", "G2", command, route));
    assert(route.count == 3 && route.departure[0] == RouteTurn::Straight &&
           route.departure[1] == RouteTurn::Left && route.departure[2] == RouteTurn::Straight);
    assert(RoutePlanner::plan(nullptr, "G2", command, route));
    assert(route.departure[0] == RouteTurn::Unknown);
    assert(!RoutePlanner::plan("M5", "G2", command, route));
    assert(decode(R"({"cmd":"GOTO","cmd_id":51,"path":["G2"],"target":"G2"})", command));
    assert(!RoutePlanner::plan(nullptr, "M5", command, route));
    assert(decode(R"({"cmd":"GOTO","cmd_id":52,"path":["GS1","M2"],"target":"M2"})", command));
    assert(RoutePlanner::plan(nullptr, "S1", command, route));
    assert(route.departure[0] == RouteTurn::Straight && route.departure[1] == RouteTurn::Left);
    assert(decode(R"({"cmd":"GOTO","cmd_id":57,"path":["GS2"],"target":"GS2"})", command));
    assert(RoutePlanner::plan(nullptr, "S2", command, route));
    assert(route.departure[0] == RouteTurn::Straight);
    assert(AGVMath::reverseLineAction(2) == MotorAction::Backward);
    assert(AGVMath::reverseLineAction(5) == MotorAction::Backward);
    assert(AGVMath::reverseLineAction(7) == MotorAction::Backward);
    assert(AGVMath::reverseLineAction(4) == MotorAction::ReverseCorrectLeft);
    assert(AGVMath::reverseLineAction(1) == MotorAction::ReverseCorrectRight);
    assert(AGVMath::reverseLineAction(0) == MotorAction::Stop);

    // A centered reading on the old line must not end a pivot immediately.
    SafetySettings safety{1000, 1000, 0, 100, 0, 50, false, false, false};
    ControlLogic motion(safety);
    motion.begin(0);
    motion.reachedNode("S1", 0);
    AGVTask task;
    AGVMath::copyText(task.id, sizeof(task.id), "bench");
    AGVMath::copyText(task.startNode, sizeof(task.startNode), "GS1");
    AGVMath::copyText(task.targetNode, sizeof(task.targetNode), "M2");
    task.pathLength = 3;
    AGVMath::copyText(task.path[0].node, sizeof(task.path[0].node), "S1");
    AGVMath::copyText(task.path[1].node, sizeof(task.path[1].node), "GS1");
    AGVMath::copyText(task.path[2].node, sizeof(task.path[2].node), "M2");
    task.path[0].departure = Departure::Left;
    ErrorCode reason;
    ControlInput in;
    in.wifi = in.mqtt = in.motionConfigured = in.liftConfigured = in.distanceValid = in.lineConfigured = true;
    in.linePattern = 2;
    assert(motion.setTask(task, reason) && motion.start(in, reason));
    in.now = 1;
    assert(motion.update(in).motor == MotorAction::PivotLeft);
    in.now = 2; in.linePattern = 0;
    assert(motion.update(in).motor == MotorAction::PivotLeft);
    in.now = 3; in.linePattern = 5;
    assert(motion.update(in).motor == MotorAction::Forward);

    // S1 -> GS1 remains on the line while moving backward. Loss of all
    // three eyes must stop instead of driving blindly toward the station.
    SafetySettings reverseSafety{1000, 1000, 0, 100, 0, 50, false, false, true};
    ControlLogic reverseMotion(reverseSafety);
    reverseMotion.begin(0);
    reverseMotion.reachedNode("S1", 0);
    task.path[0].departure = Departure::Backward;
    assert(reverseMotion.setTask(task, reason) && reverseMotion.start(in, reason));
    in.now = 4; in.linePattern = 2;
    assert(reverseMotion.update(in).motor == MotorAction::Backward);
    in.now = 5; in.linePattern = 4;
    assert(reverseMotion.update(in).motor == MotorAction::ReverseCorrectLeft);
    in.now = 6; in.linePattern = 1;
    assert(reverseMotion.update(in).motor == MotorAction::ReverseCorrectRight);
    in.now = 7; in.linePattern = 0;
    assert(reverseMotion.update(in).motor == MotorAction::Stop);
    assert(reverseMotion.state() == AGV_SAFE_STOP);

    SafetySettings orderedReverseSafety{1000, 1000, 0, 100, 0, 50, true, false, true};
    ControlLogic orderedReverse(orderedReverseSafety);
    orderedReverse.begin(0);
    orderedReverse.reachedNode("S1", 0);
    assert(orderedReverse.setTask(task, reason) && orderedReverse.start(in, reason));
    AGVCommand order;
    order.type = CommandType::NodeOrder;
    AGVMath::copyText(order.taskId, sizeof(order.taskId), task.id);
    AGVMath::copyText(order.node, sizeof(order.node), "S1");
    AGVMath::copyText(order.nextNode, sizeof(order.nextNode), "GS1");
    order.pathIndex = 0;
    order.departure = Departure::Backward;
    assert(orderedReverse.setNodeOrder(order, 8, reason));
    in.now = 9; in.linePattern = 2;
    assert(orderedReverse.update(in).motor == MotorAction::Backward);

    CoreCommand single;
    assert(decode(R"({"cmd":"GOTO","cmd_id":53,"path":["GS1"],"target":"GS1"})", single));
    assert(RoutePlanner::plan(nullptr, "S1", single, route));
    assert(RoutePlanner::oneRobotTrialRoute("01", "S1", single, route));
    assert(!RoutePlanner::oneRobotTrialRoute("02", "S1", single, route));
    assert(!RoutePlanner::oneRobotTrialRoute("01", "GS1", single, route));
    CoreNavigator navigator;
    assert(navigator.start("S1", single, route, 0));
    in.now = 100; in.linePattern = 2;
    assert(navigator.update(in) == MotorAction::Stop); // RFID dwell
    in.now = 201;
    assert(navigator.update(in) == MotorAction::Forward);
    navigator.reachedNode("GS1", 202);
    assert(!navigator.active() && navigator.state() == AGV_IDLE);

    CoreNavigator lost;
    assert(lost.start("S1", single, route, 0));
    in.now = 201; in.linePattern = 0;
    assert(lost.update(in) == MotorAction::Stop);
    assert(lost.error() == ErrorCode::LineLost);
    assert(!lost.start("S1", single, route, 300)); // fault is latched

    CoreNavigator timedOut;
    assert(timedOut.start("S1", single, route, 0));
    in.now = 201; in.linePattern = 2;
    assert(timedOut.update(in) == MotorAction::Forward);
    in.now = 201 + Config::SEGMENT_TIMEOUT_MS;
    assert(timedOut.update(in) == MotorAction::Stop);
    assert(timedOut.error() == ErrorCode::MotionTimeout);

    CoreNavigator obstructed;
    assert(obstructed.start("S1", single, route, 0));
    in.now = 201; in.obstacle = true;
    assert(obstructed.update(in) == MotorAction::Stop);
    assert(obstructed.error() == ErrorCode::Obstacle);
    in.obstacle = false;

    CoreNavigator wrongTag;
    assert(wrongTag.start("S1", single, route, 0));
    wrongTag.reachedNode("M1", 50);
    assert(wrongTag.error() == ErrorCode::UnexpectedNode);

    CoreCommand guarded;
    assert(decode(R"({"cmd":"GOTO","cmd_id":54,"path":["M2"],"target":"M2"})", guarded));
    assert(RoutePlanner::plan("S1", "GS1", guarded, route, true));
    assert(route.departure[0] == RouteTurn::Right);
    CoreNavigator zone;
    assert(zone.start("GS1", guarded, route, 0));
    assert(std::strcmp(zone.missingZone(), "M2") == 0);
    in.now = 500; in.linePattern = 2;
    assert(zone.update(in) == MotorAction::Stop);
    zone.grant("M2", 99);
    assert(zone.missingZone() == nullptr);
    assert(zone.update(in) == MotorAction::PivotRight);
    in.now = 550; in.linePattern = 0;
    assert(zone.update(in) == MotorAction::PivotRight);
    in.now = 610; in.linePattern = 2;
    assert(zone.update(in) == MotorAction::Forward);
    zone.reachedNode("M2", 611);
    assert(zone.state() == AGV_IDLE);

    CoreNavigator turnTimeout;
    assert(turnTimeout.start("GS1", guarded, route, 0));
    turnTimeout.grant("M2", 100);
    in.now = 300; in.linePattern = 2;
    assert(turnTimeout.update(in) == MotorAction::PivotRight);
    in.now = 2801;
    assert(turnTimeout.update(in) == MotorAction::Stop);
    assert(turnTimeout.error() == ErrorCode::TurnTimeout);

    CoreCommand leaveZone;
    assert(decode(R"({"cmd":"GOTO","cmd_id":55,"path":["GS1"],"target":"GS1"})", leaveZone));
    RoutePlan straight;
    straight.count = 1;
    straight.departure[0] = RouteTurn::Straight;
    CoreNavigator owned;
    assert(owned.start("M1", leaveZone, straight, 0));
    assert(std::strcmp(owned.missingZone(), "M1") == 0);
    in.now = 300; in.linePattern = 2;
    assert(owned.update(in) == MotorAction::Stop);
    owned.grant("M1", 56);
    assert(owned.update(in) == MotorAction::Forward);
    in.now = 301; in.mqtt = false;
    assert(owned.update(in) == MotorAction::Stop);
    assert(owned.error() == ErrorCode::NotConnected);
    in.mqtt = true;
}
