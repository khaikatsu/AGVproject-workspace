#pragma once

#include "Types.h"

// Wire format used by src/AGVserver.cpp. This is separate from the older
// TASK/START codec, whose schema is not accepted by the running Core.
namespace CoreProtocol {
bool decodeCommand(const uint8_t* payload, size_t length, CoreCommand& command);
bool sameCommand(const CoreCommand& a, const CoreCommand& b);
// Own the zone text before any publish can overwrite PubSubClient's RX buffer.
bool decodeGrant(const uint8_t* payload, size_t length, CoreGrant& grant);
bool grantAck(const CoreGrant& grant, char* output, size_t capacity);
bool telemetry(const char* node, int battery, bool obstacle, char* output, size_t capacity);
bool ack(int32_t cmdId, char* output, size_t capacity);
}
