#pragma once

#include "RoutePlanner.h"
#include "ControlLogic.h"

// A GOTO path is consumed one RFID at a time. The controller owns the measured
// clearance check and records completed exit after queueing the zone release.
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
    // Call only after measured clearance and successful queueing of release.
    // Allows finishing the outbound segment without retaining zone ownership.
    bool completeZoneExit(const char* zone, int32_t grantId);
    void resetGrants();
    MotorAction update(const ControlInput& input);
    // Refresh an already authorized following phase before blocking I/O.
    // Cannot start a route, leave a waiting node or advance turn capture.
    bool refreshFollowing(const ControlInput& input, MotorAction& action);
    // Observe before potentially slow RFID work; caller must STOP immediately
    // when a new center-line candidate is latched. Does not authorize motion.
    bool observeTurnLine(const ControlInput& input);
    bool capturingTurnLine() const { return phase_ == Phase::TurnCapture; }
    bool settlingTurnLine() const { return phase_ == Phase::Following && turnSettling_; }
    bool turnCaptureEnabled() const {
        return index_ < length_ &&
            ((turns_[index_] == RouteTurn::Left && Config::LEFT_TURN_CAPTURE_ENABLED) ||
             (turns_[index_] == RouteTurn::Right && Config::RIGHT_TURN_CAPTURE_ENABLED));
    }
    int turnCapturePwm() const {
        return index_ < length_ && turns_[index_] == RouteTurn::Right
            ? Config::RIGHT_TURN_CAPTURE_PWM : Config::LEFT_TURN_CAPTURE_PWM;
    }
    bool precisionPivot() const {
        return g1TurnAroundPrecision() || recoveringLine() ||
            (turnCaptureEnabled() && phase_ == Phase::Turning && oldLineDeparted_);
    }
    int precisionPivotPwm() const {
        if (g1TurnAroundPrecision()) return Config::G1_TURN_AROUND_PWM;
        // Turn capture uses its direction-specific PWM; line-loss recovery keeps its own limit.
        return (phase_ == Phase::TurnCapture || (phase_ == Phase::Turning &&
                turnCaptureEnabled() && oldLineDeparted_) ||
                (phase_ == Phase::Following && turnSettling_))
            ? turnCapturePwm() : Config::LINE_RECOVERY_PWM;
    }
    const char* missingZone() const;
    const char* requestedZone() const;
    bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Fault; }
    bool following() const { return phase_ == Phase::Following && !recoveringLine(); }
    bool recoveringLine() const { return Config::LINE_RECOVERY_ENABLED && lineLossPending_; }
    bool obstaclePaused() const { return obstaclePaused_; }
    bool networkPaused() const { return networkPaused_; }
    bool forwardLineTracking() const {
        return following() && !segmentReverse() && !obstaclePaused_ && !networkPaused_ && !missingZone();
    }
    bool stationOriented() const { return stationOriented_; }
    AGVState state() const;
    ErrorCode error() const { return error_; }
    const char* nextNode() const;
    uint8_t pathIndex() const { return index_; }
    uint8_t pathLength() const { return length_; }
    bool segmentReverse() const {
        return active() && index_ < length_ && (turns_[index_] == RouteTurn::Reverse ||
            (reverseAfterTurn_[index_] && (phase_ == Phase::Following ||
             (phase_ == Phase::WaitingGrant && turns_[index_] == RouteTurn::Straight))));
    }
private:
    enum class Phase { Idle, WaitingGrant, Turning, TurnCapture, Following, StationDwell, StationTurning, Fault };
    bool g1TurnAroundPrecision() const {
        return Config::G1_TURN_AROUND_PWM > 0 && phase_ == Phase::Turning && index_ < length_ &&
            turns_[index_] == RouteTurn::TurnAround && std::strcmp(current_, "G1") == 0;
    }
    static int zoneIndex(const char* node);
    bool hasGrant(const char* node) const;
    bool currentZonePermitted() const;
    void resetLineTracking();
    void rememberLine(const ControlInput& input, MotorAction action);
    MotorAction recoverLine(const ControlInput& input);
    MotorAction alignTurnLine(const ControlInput& input);
    bool confirmTrackedLine(const ControlInput& input);
    Phase phase_ = Phase::Idle;
    ErrorCode error_ = ErrorCode::None;
    char current_[NODE_TEXT_SIZE]{};
    char path_[Config::MAX_PATH_NODES][NODE_TEXT_SIZE]{};
    RouteTurn turns_[Config::MAX_PATH_NODES]{};
    uint8_t turnCrossings_[Config::MAX_PATH_NODES]{};
    bool passThrough_[Config::MAX_PATH_NODES]{};
    bool reverseAfterTurn_[Config::MAX_PATH_NODES]{};
    bool stationOriented_ = false;
    uint8_t crossedLines_ = 0;
    uint32_t lastCrossingMs_ = 0;
    int32_t grantIds_[6]{};
    // Scoped to the current outbound segment; never an entry grant.
    bool currentZoneCleared_ = false;
    uint8_t index_ = 0, length_ = 0;
    uint32_t segmentStartedMs_ = 0, turnStartedMs_ = 0;
    uint32_t stoppedAtMs_ = 0;
    bool oldLineDeparted_ = false;
    bool turnRetryUsed_ = false;
    uint32_t lineLossStartedMs_ = 0;
    bool lineLossPending_ = false;
    uint8_t lastLinePattern_ = 0, centerSamples_ = 0, recoveryCandidate_ = 0;
    MotorAction lastLineMotor_ = MotorAction::Stop;
    MotorAction lastCorrection_ = MotorAction::Stop, searchDirection_ = MotorAction::Stop;
    uint32_t lastCorrectionMs_ = 0, centerSinceMs_ = 0, centerSampleMs_ = 0;
    uint32_t searchMotorChangedMs_ = 0;
    bool lineRecoveryUsed_ = false, searchPulse_ = false;
    uint8_t captureHistory_ = 0, captureCount_ = 0;
    uint32_t captureSampleMs_ = 0;
    bool turnSettling_ = false;
    bool soloTrial_ = false;
    bool requireNodeOrders_ = false, nodeOrderReceived_ = false;
    int32_t routeCmdId_ = 0;
    bool obstaclePaused_ = false;
    uint32_t obstaclePausedAtMs_ = 0;
    bool networkPaused_ = false;
    uint32_t networkPausedAtMs_ = 0;
};
