#pragma once
#include "Config.h"
#include <cstdint>

namespace NetworkHealth {
struct Snapshot {
    bool ready = false, wifi = false, heartbeatSeen = false, restarting = false;
    uint32_t servicedAt = 0, heartbeatAt = 0;
};
// Read the worker timestamps BEFORE reading millis(). A concurrently updated
// timestamp can otherwise be newer than 'now', underflow and falsely stop the robot.
template<class ReadStatus, class ReadClock>
bool connected(ReadStatus readStatus, ReadClock readClock) {
    const Snapshot status = readStatus();
    const uint32_t now = readClock();
    return status.ready && status.wifi && status.heartbeatSeen && !status.restarting &&
        static_cast<uint32_t>(now-status.servicedAt) < Config::NETWORK_WORKER_STALE_MS &&
        static_cast<uint32_t>(now-status.heartbeatAt) < Config::CORE_HEARTBEAT_TIMEOUT_MS;
}
}
