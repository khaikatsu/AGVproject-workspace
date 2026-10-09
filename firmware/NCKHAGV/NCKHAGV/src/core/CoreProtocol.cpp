#include "CoreProtocol.h"
#include "ControlLogic.h"
#include <ArduinoJson.h>
#include <climits>
#include <cstdio>
#include <cstring>

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

    StaticJsonDocument<2048> doc;
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
    else return false;

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
    return command.pathLength == 0 || std::strcmp(command.path[command.pathLength - 1], command.target) == 0;
}

bool CoreProtocol::sameCommand(const CoreCommand& a, const CoreCommand& b) {
    if (a.type != b.type || a.cmdId != b.cmdId || a.pathLength != b.pathLength ||
        std::strcmp(a.target, b.target) != 0) return false;
    for (uint8_t i = 0; i < a.pathLength; ++i)
        if (std::strcmp(a.path[i], b.path[i]) != 0) return false;
    return true;
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
