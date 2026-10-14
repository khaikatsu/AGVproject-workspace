#include "CoreNavigator.h"
#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include <cstring>

int CoreNavigator::zoneIndex(const char* node) {
    return node && node[0] == 'M' && node[1] >= '1' && node[1] <= '6' && !node[2]
        ? node[1] - '1' : -1;
}
bool CoreNavigator::hasGrant(const char* node) const {
    const int index = zoneIndex(node);
    return index < 0 || grantIds_[index] > 0;
}
const char* CoreNavigator::missingZone() const {
    if (soloTrial_) return nullptr;
    if (!active() || index_ >= length_) return nullptr;
    if (!hasGrant(current_)) return current_;
    if (!hasGrant(path_[index_])) return path_[index_];
    return nullptr;
}
const char* CoreNavigator::requestedZone() const {
    if (const auto* zone = missingZone()) return zone;
    // Ask at most one segment ahead, only while approaching an authorized
    // straight-through RFID. Missing lookahead never inhibits this segment.
    if (!soloTrial_ && phase_ == Phase::Following && index_+1 < length_ &&
        passThrough_[index_] && !hasGrant(path_[index_+1])) return path_[index_+1];
    return nullptr;
}
bool CoreNavigator::canPassThrough(const char* node, const ControlInput& in) const {
    return phase_ == Phase::Following && !obstaclePaused_ && node && index_+1 < length_ &&
        std::strcmp(node,path_[index_]) == 0 && passThrough_[index_] &&
        !segmentReverse() && turns_[index_+1] == RouteTurn::Straight &&
        !lineLossPending_ && in.wifi && in.mqtt && in.motionConfigured &&
        in.lineConfigured && in.distanceValid && !in.obstacle &&
        AGVMath::lineAction(in.linePattern) != MotorAction::Stop &&
        static_cast<uint32_t>(in.now-segmentStartedMs_) < Config::SEGMENT_TIMEOUT_MS &&
        (soloTrial_ || (hasGrant(current_) && hasGrant(node) && hasGrant(path_[index_+1])));
}
bool CoreNavigator::start(const char* current, const CoreCommand& command, const RoutePlan& route,
                          uint32_t now, bool soloTrial, bool requireNodeOrders) {
    if (phase_ == Phase::Fault || active() || !current || !current[0] ||
        command.type != CoreCommandType::Goto || !command.pathLength ||
        command.pathLength != route.count || command.pathLength > Config::MAX_PATH_NODES) return false;
    for (uint8_t i = 0; i < route.count; ++i) {
        if (!RoutePlanner::directedEdge(i ? command.path[i - 1] : current, command.path[i]) ||
            route.departure[i] == RouteTurn::Unknown ||
            (route.departure[i] == RouteTurn::TurnAround &&
             (route.turnCrossings[i] < 1 || route.turnCrossings[i] > 2))) return false;
        if (command.passThrough[i] && (i+1 >= route.count || route.departure[i] == RouteTurn::Reverse ||
            route.departure[i+1] != RouteTurn::Straight ||
            !RoutePlanner::straightThrough(i ? command.path[i-1] : current,command.path[i],command.path[i+1]))) return false;
    }
    if (!AGVMath::copyText(current_, sizeof(current_), current)) return false;
    for (uint8_t i = 0; i < route.count; ++i) {
        if (!AGVMath::copyText(path_[i], sizeof(path_[i]), command.path[i])) return false;
        turns_[i] = route.departure[i];
        turnCrossings_[i] = route.turnCrossings[i];
        passThrough_[i] = command.passThrough[i];
    }
    index_ = 0;
    requireNodeOrders_ = requireNodeOrders;
    nodeOrderReceived_ = false;
    routeCmdId_ = command.cmdId;
    stationOriented_ = false;
    obstaclePaused_ = false;
    length_ = route.count;
    segmentStartedMs_ = turnStartedMs_ = now;
    stoppedAtMs_ = now;
    oldLineDeparted_ = false;
    lineLossPending_ = false;
    soloTrial_ = soloTrial;
    error_ = ErrorCode::None;
    phase_ = Phase::WaitingGrant;
    return true;
}
void CoreNavigator::stop() {
    obstaclePaused_ = false;
    routeCmdId_ = 0; nodeOrderReceived_ = false;
    stationOriented_ = false;
    if (phase_ != Phase::Fault) phase_ = Phase::Idle;
    length_ = index_ = 0;
    for (auto& id : grantIds_) id = 0;
    soloTrial_ = false;
    lineLossPending_ = false;
}
bool CoreNavigator::clearFault() {
    obstaclePaused_ = false;
    if (active()) return false;
    phase_ = Phase::Idle;
    stationOriented_ = false;
    error_ = ErrorCode::None;
    length_ = index_ = 0;
    for (auto& id : grantIds_) id = 0;
    soloTrial_ = false;
    lineLossPending_ = false;
    return true;
}
void CoreNavigator::fault(ErrorCode reason) {
    obstaclePaused_ = false;
    stationOriented_ = false;
    phase_ = Phase::Fault;
    error_ = reason;
    length_ = index_ = 0;
    for (auto& id : grantIds_) id = 0;
    soloTrial_ = false;
    lineLossPending_ = false;
}
void CoreNavigator::reachedNode(const char* node, uint32_t now, const ControlInput* input) {
    if (!active()) return;
    if (!node || !node[0] || index_ >= length_ || std::strcmp(node, path_[index_]) != 0) {
        fault(node && node[0] ? ErrorCode::UnexpectedNode : ErrorCode::UnknownTag);
        return;
    }
    const bool through = input && canPassThrough(node,*input);
    AGVMath::copyText(current_, sizeof(current_), node);
    ++index_;
    nodeOrderReceived_ = false;
    if (RoutePlanner::station(node)) {
        // The front enters S toward the tag. Turn physically at the station
        // before declaring readiness; the next route then departs forward.
        stoppedAtMs_ = now;
        oldLineDeparted_ = false;
        lineLossPending_ = false;
        stationOriented_ = false;
        phase_ = Phase::StationDwell;
        return;
    }
    if (index_ == length_) { stop(); return; }
    segmentStartedMs_ = turnStartedMs_ = now;
    stoppedAtMs_ = now;
    oldLineDeparted_ = false;
    lineLossPending_ = false;
    if (through) { phase_ = Phase::Following; nodeOrderReceived_ = true; }
    else phase_ = Phase::WaitingGrant;
}
bool CoreNavigator::grant(const char* zone, int32_t grantId) {
    const int index = zoneIndex(zone);
    const char* wanted = requestedZone();
    if (active() && index >= 0 && grantId > 0 &&
        ((std::strcmp(zone, current_) == 0) ||
         (index_ < length_ && std::strcmp(zone, path_[index_]) == 0) ||
         (wanted && std::strcmp(zone,wanted) == 0))) {
        grantIds_[index] = grantId;
        return true;
    }
    return false;
}
void CoreNavigator::revokeGrant(const char* zone, int32_t grantId) {
    const int index = zoneIndex(zone);
    if (index >= 0 && grantIds_[index] == grantId) grantIds_[index] = 0;
}
void CoreNavigator::resetGrants() {
    for (auto& id : grantIds_) id = 0;
    if (active()) fault(ErrorCode::NotConnected);
}
MotorAction CoreNavigator::update(const ControlInput& in) {
    if (!active()) return MotorAction::Stop;
    if (!in.wifi || !in.mqtt) { fault(ErrorCode::NotConnected); return MotorAction::Stop; }
    if (!in.motionConfigured || !in.lineConfigured) { fault(ErrorCode::Configuration); return MotorAction::Stop; }
    if (!in.distanceValid) { fault(ErrorCode::SensorInvalid); return MotorAction::Stop; }
    if (in.obstacle) {
        if (Config::REAR_ULTRASONIC_ECHO_PIN < 0) { fault(ErrorCode::Obstacle); return MotorAction::Stop; }
        if (!obstaclePaused_) { obstaclePaused_ = true; obstaclePausedAtMs_ = in.now; }
        return MotorAction::Stop;
    }
    if (obstaclePaused_) {
        const uint32_t held = static_cast<uint32_t>(in.now - obstaclePausedAtMs_);
        segmentStartedMs_ += held;
        turnStartedMs_ += held;
        stoppedAtMs_ += held;
        lineLossStartedMs_ += held;
        obstaclePaused_ = false;
    }
    if (needsNodeOrder()) {
        if (static_cast<uint32_t>(in.now - stoppedAtMs_) >= Config::SEGMENT_TIMEOUT_MS)
            fault(ErrorCode::MotionTimeout);
        return MotorAction::Stop;
    }
    if (phase_ == Phase::StationDwell) {
        if (static_cast<uint32_t>(in.now - stoppedAtMs_) < Config::RFID_STOP_DWELL_MS)
            return MotorAction::Stop;
        if (!Config::TURN_TIMEOUT_MS || Config::TURN_TIMEOUT_MS <= Config::TURN_MIN_MS) {
            fault(ErrorCode::TurnConfiguration); return MotorAction::Stop;
        }
        phase_ = Phase::StationTurning;
        turnStartedMs_ = in.now;
    }
    if (phase_ == Phase::StationTurning) {
        if (static_cast<uint32_t>(in.now - turnStartedMs_) >= Config::TURN_TIMEOUT_MS) {
            fault(ErrorCode::TurnTimeout); return MotorAction::Stop;
        }
        // S is a leaf with one line toward GS. Depart the incoming line and
        // reacquire that line after a 180-degree pivot, facing the exit.
        if (in.linePattern == 0) oldLineDeparted_ = true;
        const bool centered = in.linePattern == 2 || in.linePattern == 5 || in.linePattern == 7;
        if (!oldLineDeparted_ || !centered ||
            static_cast<uint32_t>(in.now - turnStartedMs_) < Config::TURN_MIN_MS)
            return MotorAction::PivotRight;
        if (index_ == length_) stop();
        else {
            phase_ = Phase::WaitingGrant;
            nodeOrderReceived_ = false;
            stoppedAtMs_ = segmentStartedMs_ = in.now;
            oldLineDeparted_ = false;
            lineLossPending_ = false;
        }
        stationOriented_ = true;
        return MotorAction::Stop;
    }
    if (phase_ == Phase::WaitingGrant) {
        if (missingZone()) return MotorAction::Stop;
        if (static_cast<uint32_t>(in.now - stoppedAtMs_) < Config::RFID_STOP_DWELL_MS)
            return MotorAction::Stop;
        stationOriented_ = false;
        if (turns_[index_] == RouteTurn::Left || turns_[index_] == RouteTurn::Right ||
            turns_[index_] == RouteTurn::TurnAround) {
            if (!Config::TURN_TIMEOUT_MS || Config::TURN_TIMEOUT_MS <= Config::TURN_MIN_MS) {
                fault(ErrorCode::TurnConfiguration); return MotorAction::Stop;
            }
            phase_ = Phase::Turning;
            turnStartedMs_ = in.now;
            crossedLines_ = 0;
            lastCrossingMs_ = in.now;
        } else {
            phase_ = Phase::Following;
            segmentStartedMs_ = in.now;
        }
    }
    if (phase_ == Phase::Turning) {
        if (static_cast<uint32_t>(in.now - turnStartedMs_) >= Config::TURN_TIMEOUT_MS) {
            fault(ErrorCode::TurnTimeout); return MotorAction::Stop;
        }
        const bool centered = in.linePattern == 2 || in.linePattern == 5 || in.linePattern == 7;
        // Side-only patterns can occur while the sensors are still over the
        // RFID junction. Require a fully blank sample before accepting a
        // centered pattern as the new branch; otherwise 011 -> 111 can end
        // the pivot almost immediately and the robot drives off the line.
        if (in.linePattern == 0) oldLineDeparted_ = true;
        if (oldLineDeparted_ && centered &&
            static_cast<uint32_t>(in.now - lastCrossingMs_) >= Config::TURN_MIN_MS) {
            ++crossedLines_;
            oldLineDeparted_ = false;
            lastCrossingMs_ = in.now;
            const uint8_t required = turns_[index_] == RouteTurn::TurnAround ? turnCrossings_[index_] : 1;
            if (crossedLines_ >= required) {
                phase_ = Phase::Following;
                segmentStartedMs_ = in.now;
            } else return MotorAction::PivotRight;
        } else return turns_[index_] == RouteTurn::Left ? MotorAction::PivotLeft : MotorAction::PivotRight;
    }
    if (static_cast<uint32_t>(in.now - segmentStartedMs_) >= Config::SEGMENT_TIMEOUT_MS) {
        fault(ErrorCode::MotionTimeout); return MotorAction::Stop;
    }
    const MotorAction action = segmentReverse()
        ? AGVMath::reverseLineAction(in.linePattern) : AGVMath::lineAction(in.linePattern);
    if (in.linePattern == 0 && Config::LINE_LOSS_CONFIRM_MS > 0) {
        if (!lineLossPending_) {
            lineLossPending_ = true;
            lineLossStartedMs_ = in.now;
#if defined(ARDUINO)
            if (Config::DEBUG_ENABLED) Serial.printf("[LINE] blank=000 node=%s next=%s\n", current_, nextNode());
#endif
        }
        // Never drive blind while waiting to see whether this was a brief gap.
        if (static_cast<uint32_t>(in.now - lineLossStartedMs_) < Config::LINE_LOSS_CONFIRM_MS)
            return MotorAction::Stop;
    } else if (lineLossPending_) {
#if defined(ARDUINO)
        if (Config::DEBUG_ENABLED) Serial.printf("[LINE] recovered=%u after=%lums\n",
            static_cast<unsigned int>(in.linePattern),
            static_cast<unsigned long>(in.now - lineLossStartedMs_));
#endif
        lineLossPending_ = false;
    }
    if (action == MotorAction::Stop) {
#if defined(ARDUINO)
        if (Config::DEBUG_ENABLED) Serial.printf("[LINE] fault=%s pattern=%u node=%s next=%s\n",
            in.linePattern == 0 ? "LINE_LOST" : "AMBIGUOUS_LINE",
            static_cast<unsigned int>(in.linePattern), current_, nextNode());
#endif
        fault(in.linePattern == 0 ? ErrorCode::LineLost : ErrorCode::AmbiguousLine);
    }
    return action;
}
AGVState CoreNavigator::state() const {
    if (obstaclePaused_) return AGV_WAITING;
    if (phase_ == Phase::Fault) return AGV_SAFE_STOP;
    if (phase_ == Phase::WaitingGrant || phase_ == Phase::StationDwell) return AGV_WAITING;
    if (phase_ == Phase::Turning || phase_ == Phase::Following || phase_ == Phase::StationTurning) return AGV_MOVING;
    return AGV_IDLE;
}
const char* CoreNavigator::nextNode() const { return active() && index_ < length_ ? path_[index_] : ""; }

bool CoreNavigator::needsNodeOrder() const {
    return requireNodeOrders_ && !nodeOrderReceived_ &&
        (phase_ == Phase::WaitingGrant || phase_ == Phase::StationDwell);
}
bool CoreNavigator::authorizeNodeOrder(const CoreCommand& order) {
    if (!needsNodeOrder() || order.type != CoreCommandType::NodeOrder ||
        order.routeCmdId != routeCmdId_ || order.pathIndex != index_ ||
        std::strcmp(order.node,current_) != 0) return false;
    const bool parking = phase_ == Phase::StationDwell;
    const char* next = parking ? current_ : nextNode();
    const char* maneuver = parking ? "STATION_TURN_RIGHT" : RoutePlanner::turnName(turns_[index_]);
    const uint8_t crossings = parking ? 1 : turns_[index_] == RouteTurn::TurnAround ? turnCrossings_[index_] :
        (turns_[index_] == RouteTurn::Left || turns_[index_] == RouteTurn::Right ? 1 : 0);
    if (std::strcmp(order.nextNode,next) != 0 || std::strcmp(order.maneuver,maneuver) != 0 ||
        order.turnCrossings != crossings) return false;
    nodeOrderReceived_ = true;
    return true;
}
