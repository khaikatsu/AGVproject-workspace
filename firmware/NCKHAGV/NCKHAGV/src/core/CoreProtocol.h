#pragma once

#include "Types.h"

// Wire format used by src/AGVserver.cpp. This is separate from the older
// TASK/START codec, whose schema is not accepted by the running Core.
namespace CoreProtocol {
bool decodeCommand(const uint8_t* payload, size_t length, CoreCommand& command);
bool sameCommand(const CoreCommand& a, const CoreCommand& b);
bool telemetry(const char* node, int battery, bool obstacle, char* output, size_t capacity);
bool ack(int32_t cmdId, char* output, size_t capacity);
}
