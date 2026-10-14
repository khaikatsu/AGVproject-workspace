#pragma once

#include "RoutePlanner.h"
#include "ControlLogic.h"

// A GOTO path is consumed one RFID at a time. The controller owns the measured
// clearance check and only then revokes the local grant after queueing release.
class CoreNavigator {
public:
    bool start(const char* current, const CoreCommand& command, const RoutePlan& route, uint32_t now,
               bool soloTrial = false, bool requireNodeOrders = false);
    bool authorizeNodeOrder(const CoreCommand& order);
    bool needsNodeOrder() const;
    bool stationTurnRequired() const { return phase_ == Phase::StationDwell; }
    int32_t routeCommandId() const { return routeCmdId_; }
    void stop();
    bool clearFault();
    void fault(ErrorCode reason);
    void reachedNode(const char* node, uint32_t now, const ControlInput* input = nullptr);
    bool canPassThrough(const char* node, const ControlInput& input) const;
    bool grant(const char* zone, int32_t grantId);
    void revokeGrant(const char* zone, int32_t grantId);
    void resetGrants();
    MotorAction update(const ControlInput& input);
    const char* missingZone() const;
    const char* requestedZone() const;
    bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Fault; }
    bool following() const { return phase_ == Phase::Following; }
    bool obstaclePaused() const { return obstaclePaused_; }
    bool stationOriented() const { return stationOriented_; }
    AGVState state() const;
    ErrorCode error() const { return error_; }
    const char* nextNode() const;
    uint8_t pathIndex() const { return index_; }
    uint8_t pathLength() const { return length_; }
    bool segmentReverse() const {
        return active() && index_ < length_ && turns_[index_] == RouteTurn::Reverse;
    }
private:
    enum class Phase { Idle, WaitingGrant, Turning, Following, StationDwell, StationTurning, Fault };
    static int zoneIndex(const char* node);
    bool hasGrant(const char* node) const;
    Phase phase_ = Phase::Idle;
    ErrorCode error_ = ErrorCode::None;
    char current_[NODE_TEXT_SIZE]{};
    char path_[Config::MAX_PATH_NODES][NODE_TEXT_SIZE]{};
    RouteTurn turns_[Config::MAX_PATH_NODES]{};
    uint8_t turnCrossings_[Config::MAX_PATH_NODES]{};
    bool passThrough_[Config::MAX_PATH_NODES]{};
    bool stationOriented_ = false;
    uint8_t crossedLines_ = 0;
    uint32_t lastCrossingMs_ = 0;
    int32_t grantIds_[6]{};
    uint8_t index_ = 0, length_ = 0;
    uint32_t segmentStartedMs_ = 0, turnStartedMs_ = 0;
    uint32_t stoppedAtMs_ = 0;
    bool oldLineDeparted_ = false;
    uint32_t lineLossStartedMs_ = 0;
    bool lineLossPending_ = false;
    bool soloTrial_ = false;
    bool requireNodeOrders_ = false, nodeOrderReceived_ = false;
    int32_t routeCmdId_ = 0;
    bool obstaclePaused_ = false;
    uint32_t obstaclePausedAtMs_ = 0;
};
