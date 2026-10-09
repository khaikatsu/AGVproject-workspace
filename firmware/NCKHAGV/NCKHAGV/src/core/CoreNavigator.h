#pragma once

#include "RoutePlanner.h"
#include "ControlLogic.h"

// A GOTO path is consumed one RFID at a time. The controller owns the measured
// clearance check and only then revokes the local grant after queueing release.
class CoreNavigator {
public:
    bool start(const char* current, const CoreCommand& command, const RoutePlan& route, uint32_t now,
               bool soloTrial = false);
    void stop();
    bool clearFault();
    void fault(ErrorCode reason);
    void reachedNode(const char* node, uint32_t now);
    bool grant(const char* zone, int32_t grantId);
    void revokeGrant(const char* zone, int32_t grantId);
    void resetGrants();
    MotorAction update(const ControlInput& input);
    const char* missingZone() const;
    bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Fault; }
    AGVState state() const;
    ErrorCode error() const { return error_; }
    const char* nextNode() const;
    uint8_t pathIndex() const { return index_; }
    uint8_t pathLength() const { return length_; }
    bool segmentReverse() const {
        return active() && index_ < length_ && turns_[index_] == RouteTurn::Reverse;
    }
private:
    enum class Phase { Idle, WaitingGrant, Turning, Following, Fault };
    static int zoneIndex(const char* node);
    bool hasGrant(const char* node) const;
    Phase phase_ = Phase::Idle;
    ErrorCode error_ = ErrorCode::None;
    char current_[NODE_TEXT_SIZE]{};
    char path_[Config::MAX_PATH_NODES][NODE_TEXT_SIZE]{};
    RouteTurn turns_[Config::MAX_PATH_NODES]{};
    int32_t grantIds_[6]{};
    uint8_t index_ = 0, length_ = 0;
    uint32_t segmentStartedMs_ = 0, turnStartedMs_ = 0;
    uint32_t stoppedAtMs_ = 0;
    bool oldLineDeparted_ = false;
    uint32_t lineLossStartedMs_ = 0;
    bool lineLossPending_ = false;
    bool soloTrial_ = false;
};
