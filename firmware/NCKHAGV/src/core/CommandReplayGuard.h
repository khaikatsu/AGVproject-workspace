#pragma once
#include "CoreProtocol.h"

// Receipt deduplication is independent of a route's motion state or Core session.
// An older command missing from the cache must never rewind the NVS high-water.
namespace CommandReplayGuard {
enum class Result { Fresh, Duplicate, Conflict, Stale };

template <size_t N>
Result classify(const CoreCommand& command, int32_t highwater,
                const CoreCommand (&recent)[N]) {
    if (command.cmdId > highwater) return Result::Fresh;
    for (const auto& item : recent) {
        if (item.cmdId == command.cmdId)
            return CoreProtocol::sameCommand(item, command) ? Result::Duplicate : Result::Conflict;
    }
    return Result::Stale;
}
}
