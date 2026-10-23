#include "core/CommandReplayGuard.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>

static CoreCommand decode(const char* wire) {
    CoreCommand command;
    assert(CoreProtocol::decodeCommand(reinterpret_cast<const uint8_t*>(wire), std::strlen(wire), command));
    return command;
}
int main() {
    using CommandReplayGuard::Result;
    CoreCommand cache[Config::RECENT_COMMAND_IDS]{};
    const auto route = decode(R"({"cmd":"GOTO","cmd_id":5000,"path":["GS1"],"target":"GS1"})");
    assert(CommandReplayGuard::classify(route, 4999, cache) == Result::Fresh);
    cache[0] = route;
    assert(CommandReplayGuard::classify(route, 5000, cache) == Result::Duplicate);
    const auto changed = decode(R"({"cmd":"GOTO","cmd_id":5000,"path":["GS2"],"target":"GS2"})");
    assert(CommandReplayGuard::classify(changed, 5000, cache) == Result::Conflict);
    // Eviction and reboot clear RAM only. The persisted high-water still rejects replay.
    cache[0] = CoreCommand{};
    assert(CommandReplayGuard::classify(route, 5000, cache) == Result::Stale);
    assert(CommandReplayGuard::classify(route, 6000, cache) == Result::Stale);
    const auto reset = decode(R"({"cmd":"RESET_FAULT","cmd_id":6001})");
    assert(CommandReplayGuard::classify(reset, 6000, cache) == Result::Fresh);
    const auto oldReset = decode(R"({"cmd":"RESET_FAULT","cmd_id":42})");
    assert(CommandReplayGuard::classify(oldReset, 6000, cache) == Result::Stale);
    // WAIT/STOP may brake immediately; an uncached old ID is never queued as fresh.
    for (const auto* wire : {R"({"cmd":"STOP","cmd_id":42})", R"({"cmd":"WAIT","cmd_id":42})"})
        assert(CommandReplayGuard::classify(decode(wire), 6000, cache) == Result::Stale);
    cache[1] = reset;
    assert(CommandReplayGuard::classify(reset, 6002, cache) == Result::Duplicate);
    const auto maximum = decode(R"({"cmd":"STOP","cmd_id":2147483647})");
    assert(CommandReplayGuard::classify(maximum, INT_MAX - 1, cache) == Result::Fresh);
    assert(CommandReplayGuard::classify(maximum, INT_MAX, cache) == Result::Stale);
    std::cout << "PASS command replay guard profile " << Config::AGV_ID
              << ": retry, conflicting payload, eviction/reboot, stale GOTO/RESET/STOP/WAIT, counter resync and INT_MAX\n";
}
