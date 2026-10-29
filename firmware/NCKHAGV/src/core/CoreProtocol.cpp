#include "CoreProtocol.h"
#include "ControlLogic.h"
#include <ArduinoJson.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace {
struct PayloadReader {
    const uint8_t* data;
    size_t length, offset = 0;
    int read() { return offset < length ? data[offset++] : -1; }
    size_t readBytes(char* buffer, size_t count) {
        const size_t available = length - offset;
        if (count > available) count = available;
        std::memcpy(buffer, data + offset, count);
        offset += count;
        return count;
    }
};

bool nodeText(JsonVariantConst value, char* output, size_t capacity) {
    if (!value.is<const char*>()) return false;
    const JsonString text = value.as<JsonString>();
    return text.size() == std::strlen(text.c_str()) &&
        AGVMath::copyText(output, capacity, text.c_str()) && AGVMath::validNode(output);
}
}

bool CoreProtocol::decodeCommand(const uint8_t* payload, size_t length, CoreCommand& command) {
    command = CoreCommand{};
    if (!payload || !length || length > Config::MAX_COMMAND_BYTES) return false;
    for (size_t i = 0; i < length; ++i) if (payload[i] == 0) return false;

    StaticJsonDocument<3072> doc;
    PayloadReader reader{payload, length};
    if (deserializeJson(doc, reader) || !doc.is<JsonObject>()) return false;
    for (size_t i = reader.offset; i < length; ++i)
        if (payload[i] != ' ' && payload[i] != '\t' && payload[i] != '\r' && payload[i] != '\n') return false;

    JsonVariantConst id = doc["cmd_id"];
    if ((!id.is<int32_t>() && !id.is<uint32_t>()) || id.as<int64_t>() < 1 || id.as<int64_t>() > INT_MAX) return false;
    command.cmdId = static_cast<int32_t>(id.as<int64_t>());
    if (!doc["cmd"].is<const char*>()) return false;
    const JsonString cmd = doc["cmd"].as<JsonString>();
    if (cmd.size() != std::strlen(cmd.c_str())) return false;
    if (std::strcmp(cmd.c_str(), "GOTO") == 0) command.type = CoreCommandType::Goto;
    else if (std::strcmp(cmd.c_str(), "WAIT") == 0) command.type = CoreCommandType::Wait;
    else if (std::strcmp(cmd.c_str(), "STOP") == 0) command.type = CoreCommandType::Stop;
    else if (std::strcmp(cmd.c_str(), "CHARGE_START") == 0) command.type = CoreCommandType::ChargeStart;
    else if (std::strcmp(cmd.c_str(), "RESET_FAULT") == 0) command.type = CoreCommandType::ResetFault;
    else if (std::strcmp(cmd.c_str(), "CONFIRM_HEADING") == 0) command.type = CoreCommandType::ConfirmHeading;
    else if (std::strcmp(cmd.c_str(), "NODE_ORDER") == 0) command.type = CoreCommandType::NodeOrder;
    else if (std::strcmp(cmd.c_str(), "LIFT_UP") == 0) command.type = CoreCommandType::LiftUp;
    else if (std::strcmp(cmd.c_str(), "LIFT_DOWN") == 0) command.type = CoreCommandType::LiftDown;
    else return false;

    if (command.type == CoreCommandType::ConfirmHeading) {
        if (!nodeText(doc["node"],command.node,sizeof(command.node)) || !doc["heading"].is<const char*>()) return false;
        const JsonString heading = doc["heading"].as<JsonString>();
        if (heading.size()!=1) return false;
        command.confirmedHeading=parseHeading(heading.c_str());
        return command.confirmedHeading!=RobotHeading::Unknown;
    }
    if (command.type == CoreCommandType::LiftUp || command.type == CoreCommandType::LiftDown)
        return Config::SERVO_SINGLE_TEST && nodeText(doc["node"],command.node,sizeof(command.node)) &&
            command.node[0]=='K' && command.node[1]>='1' && command.node[1]<='6' && !command.node[2];

    if (command.type == CoreCommandType::NodeOrder) {
        const auto route = doc["route_cmd_id"];
        if (!route.is<int32_t>() || route.as<int32_t>() < 1 || route.as<int32_t>() >= command.cmdId ||
            !doc["path_index"].is<unsigned int>() || doc["path_index"].as<unsigned int>() > Config::MAX_PATH_NODES ||
            !doc["turn_crossings"].is<unsigned int>() || doc["turn_crossings"].as<unsigned int>() > 2 ||
            !nodeText(doc["node"],command.node,sizeof(command.node)) ||
            !nodeText(doc["next_node"],command.nextNode,sizeof(command.nextNode)) ||
            !doc["maneuver"].is<const char*>()) return false;
        const JsonString maneuver = doc["maneuver"].as<JsonString>();
        if (maneuver.size() != std::strlen(maneuver.c_str()) ||
            !AGVMath::copyText(command.maneuver,sizeof(command.maneuver),maneuver.c_str())) return false;
        const bool pivot = std::strcmp(command.maneuver,"LEFT") == 0 || std::strcmp(command.maneuver,"RIGHT") == 0 ||
                           std::strcmp(command.maneuver,"TURN_AROUND_RIGHT") == 0 || std::strcmp(command.maneuver,"STATION_TURN_RIGHT") == 0;
        if (!pivot && std::strcmp(command.maneuver,"STRAIGHT") != 0 && std::strcmp(command.maneuver,"REVERSE") != 0) return false;
        command.routeCmdId = route.as<int32_t>();
        command.pathIndex = doc["path_index"].as<unsigned int>();
        command.turnCrossings = doc["turn_crossings"].as<unsigned int>();
        if (pivot ? command.turnCrossings < 1 : command.turnCrossings != 0) return false;
        if ((std::strcmp(command.maneuver,"LEFT") == 0 || std::strcmp(command.maneuver,"RIGHT") == 0 ||
             std::strcmp(command.maneuver,"STATION_TURN_RIGHT") == 0) && command.turnCrossings != 1) return false;
        if (doc.containsKey("reverse_after_turn")) {
            if (!doc["reverse_after_turn"].is<bool>()) return false;
            command.reverseAfterTurn = doc["reverse_after_turn"].as<bool>();
        }
        if (command.reverseAfterTurn && (!Config::REVERSE_HOME_SUPPORTED ||
            !((std::strcmp(command.node,"GS1") == 0 && std::strcmp(command.nextNode,"S1") == 0) ||
              (std::strcmp(command.node,"GS2") == 0 && std::strcmp(command.nextNode,"S2") == 0)) ||
            std::strcmp(command.maneuver,"STATION_TURN_RIGHT") == 0 || std::strcmp(command.maneuver,"REVERSE") == 0)) return false;
        return true;
    }
    if (command.type != CoreCommandType::Goto) return true;
    if (!nodeText(doc["target"], command.target, sizeof(command.target))) return false;
    JsonArrayConst path = doc["path"].as<JsonArrayConst>();
    if (path.isNull() || path.size() > Config::MAX_PATH_NODES) return false;
    command.pathLength = static_cast<uint8_t>(path.size());
    size_t i = 0;
    for (JsonVariantConst step : path) {
        if (!nodeText(step, command.path[i], sizeof(command.path[i]))) return false;
        if (i && std::strcmp(command.path[i - 1], command.path[i]) == 0) return false;
        ++i;
    }
    if (doc.containsKey("pass_through")) {
        if (!doc["pass_through"].is<JsonArrayConst>()) return false;
        const auto through = doc["pass_through"].as<JsonArrayConst>();
        if (through.size() > Config::MAX_PATH_NODES) return false;
        for (JsonVariantConst step : through) {
            if (!step.is<unsigned int>()) return false;
            const auto index = step.as<unsigned int>();
            if (index+1 >= command.pathLength || index >= Config::MAX_PATH_NODES || command.passThrough[index]) return false;
            command.passThrough[index] = true;
        }
    }
    return command.pathLength == 0 || std::strcmp(command.path[command.pathLength - 1], command.target) == 0;
}

bool CoreProtocol::sameCommand(const CoreCommand& a, const CoreCommand& b) {
    if (a.type != b.type || a.cmdId != b.cmdId || a.pathLength != b.pathLength ||
        a.routeCmdId != b.routeCmdId || a.pathIndex != b.pathIndex || a.turnCrossings != b.turnCrossings ||
        a.reverseAfterTurn != b.reverseAfterTurn || a.confirmedHeading != b.confirmedHeading ||
        std::strcmp(a.node,b.node) != 0 || std::strcmp(a.nextNode,b.nextNode) != 0 || std::strcmp(a.maneuver,b.maneuver) != 0 ||
        std::strcmp(a.target, b.target) != 0) return false;
    for (uint8_t i = 0; i < a.pathLength; ++i)
        if (std::strcmp(a.path[i], b.path[i]) != 0 || a.passThrough[i] != b.passThrough[i]) return false;
    return true;
}

bool CoreProtocol::decodeGrant(const uint8_t* payload, size_t length, CoreGrant& grant) {
    grant = CoreGrant{};
    if (!payload || !length || length > 128) return false;
    for (size_t i=0;i<length;++i) if (!payload[i]) return false;
    StaticJsonDocument<256> doc;
    PayloadReader reader{payload,length};
    if (deserializeJson(doc,reader) || !doc.is<JsonObject>()) return false;
    for (size_t i=reader.offset;i<length;++i)
        if (payload[i]!=' ' && payload[i]!='\t' && payload[i]!='\r' && payload[i]!='\n') return false;
    if (!nodeText(doc["zone"],grant.zone,sizeof(grant.zone)) ||
        grant.zone[0]!='M' || grant.zone[1]<'1' || grant.zone[1]>'6' || grant.zone[2] ||
        !doc["grant_id"].is<int32_t>() || doc["grant_id"].as<int32_t>()<1) return false;
    grant.grantId = doc["grant_id"].as<int32_t>();
    return true;
}
bool CoreProtocol::grantAck(const CoreGrant& grant, char* output, size_t capacity) {
    if (!output || !capacity || grant.grantId<1 || grant.zone[0]!='M' ||
        grant.zone[1]<'1' || grant.zone[1]>'6' || grant.zone[2]) return false;
    const int count=std::snprintf(output,capacity,"{\"zone\":\"%s\",\"grant_id\":%ld}",
        grant.zone,static_cast<long>(grant.grantId));
    return count>0 && static_cast<size_t>(count)<capacity;
}

bool CoreProtocol::telemetry(const char* node, int battery, bool obstacle, char* output, size_t capacity) {
    if (!node || (std::strcmp(node, "UNKNOWN") != 0 && !AGVMath::validNode(node)) ||
        battery < -1 || battery > 100 || !output || capacity == 0) return false;
    const int written = battery < 0
        ? std::snprintf(output, capacity, "%s|?|%d", node, obstacle ? 1 : 0)
        : std::snprintf(output, capacity, "%s|%d|%d", node, battery, obstacle ? 1 : 0);
    return written > 0 && static_cast<size_t>(written) < capacity;
}

bool CoreProtocol::ack(int32_t cmdId, char* output, size_t capacity) {
    if (cmdId < 1 || !output || capacity == 0) return false;
    const int written = std::snprintf(output, capacity, "%ld", static_cast<long>(cmdId));
    return written > 0 && static_cast<size_t>(written) < capacity;
}

bool CoreProtocol::odometry(const OdometrySnapshot& s, uint32_t bootId, const char* coreSession,
                            char* output, size_t capacity) {
    if (!output || !capacity || !bootId || !coreSession || !coreSession[0] || std::strlen(coreSession) > 95 ||
        !s.localSession || !std::isfinite(s.distanceCm) || !std::isfinite(s.speedCmS) ||
        s.distanceCm < 0 || s.speedCmS < 0) return false;
    StaticJsonDocument<1536> doc;
    auto o = doc.to<JsonObject>();
    o["schema_version"] = 1; o["robot_id"] = Config::AGV_ID; o["core_session_id"] = coreSession;
    o["boot_id"] = bootId; o["connection_session"] = s.localSession;
    o["seq"] = s.seq; o["uptime_ms"] = s.uptimeMs;
    o["from_node"] = s.fromNode; o["to_node"] = s.toNode;
    o["route_cmd_id"] = s.routeCmdId; o["path_index"] = s.pathIndex;
    o["tracking"] = s.tracking; o["translating"] = s.translating; o["reverse"] = s.reverse;
    o["left_ticks"] = s.leftTicks; o["right_ticks"] = s.rightTicks;
    o["distance_cm"] = s.distanceCm; o["speed_cm_s"] = s.speedCmS;
    o["pulses_per_100cm"] = Config::ENCODER_PULSES_PER_100CM;
    if (s.stopPositionValid) {
        auto proof = o.createNestedObject("stopped_position");
        proof["version"] = 1;
        proof["from_node"] = s.stopFrom; proof["to_node"] = s.stopTo;
        proof["route_cmd_id"] = s.stopRoute; proof["path_index"] = s.stopIndex;
        proof["stationary_ms"] = s.stationaryMs;
        proof["distance_cm"] = s.stopDistanceCm;
    }
    return !doc.overflowed() && measureJson(doc) < capacity && serializeJson(doc,output,capacity) > 0;
}
