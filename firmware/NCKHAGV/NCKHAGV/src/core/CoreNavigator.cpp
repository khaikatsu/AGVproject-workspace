#include "CoreNavigator.h"
#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include <cstring>

void CoreNavigator::resetLineTracking() {
    lineLossPending_ = lineRecoveryUsed_ = false;
    searchPulse_ = false;
    captureHistory_ = captureCount_ = 0;
    turnSettling_ = false;
    lastLinePattern_ = centerSamples_ = 0;
    lastLineMotor_ = lastCorrection_ = searchDirection_ = MotorAction::Stop;
}
void CoreNavigator::rememberLine(const ControlInput& in, MotorAction action) {
    lastLinePattern_ = in.linePattern;
    lastLineMotor_ = action;
    if (action == MotorAction::CorrectLeft || action == MotorAction::GentleCorrectLeft ||
        action == MotorAction::ReverseCorrectLeft) {
        lastCorrection_ = MotorAction::PivotLeft;
        lastCorrectionMs_ = in.now;
    } else if (action == MotorAction::CorrectRight || action == MotorAction::GentleCorrectRight ||
               action == MotorAction::ReverseCorrectRight) {
        lastCorrection_ = MotorAction::PivotRight;
        lastCorrectionMs_ = in.now;
    }
}
MotorAction CoreNavigator::recoverLine(const ControlInput& in) {
    if (!lineLossPending_) {
        lineLossPending_ = true;
        lineLossStartedMs_ = in.now;
        searchMotorChangedMs_ = in.now;
        searchPulse_ = false;
        centerSamples_ = 0;
        searchDirection_ = MotorAction::Stop;
        if (!lineRecoveryUsed_ && lastCorrection_ != MotorAction::Stop &&
            static_cast<uint32_t>(in.now-lastCorrectionMs_) <= Config::LINE_RECOVERY_MEMORY_MS) {
            searchDirection_ = lastCorrection_;
            lineRecoveryUsed_ = true;
        }
#if defined(ARDUINO)
        if (Config::DEBUG_ENABLED) Serial.printf(
            "[LINE] search=%s lastPattern=%u lastMotor=%s node=%s next=%s\n",
            motorActionName(searchDirection_), lastLinePattern_, motorActionName(lastLineMotor_), current_, nextNode());
#endif
        return MotorAction::Stop;
    }
    const uint32_t elapsed = in.now-lineLossStartedMs_;
    const uint32_t limit = Config::LINE_LOSS_GRACE_MS ? Config::LINE_LOSS_GRACE_MS :
        (searchDirection_ != MotorAction::Stop ? Config::LINE_RECOVERY_TIMEOUT_MS : Config::LINE_LOSS_CONFIRM_MS);
    if (elapsed >= limit) {
#if defined(ARDUINO)
        if (Config::DEBUG_ENABLED) Serial.printf(
            "[LINE] search_timeout pattern=%u samples=%u elapsed=%lums limit=%lums node=%s next=%s\n",
            in.linePattern, centerSamples_, static_cast<unsigned long>(elapsed),
            static_cast<unsigned long>(limit), current_, nextNode());
#endif
        fault(ErrorCode::LineLost);
        return MotorAction::Stop;
    }
    if (missingZone()) {
        centerSamples_ = 0;
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    // Forward profile01 may reacquire the same side across 100/110 or 001/011.
    // Center, left and right are separate evidence; wide junctions are not proof.
    const bool sideConfirm = Config::LINE_RECOVERY_SIDE_CONFIRM && !segmentReverse();
    uint8_t candidate = in.linePattern == 2 ? 2 : 0;
    if (sideConfirm) {
        if (in.linePattern == 4 || in.linePattern == 6) candidate = 4;
        if (in.linePattern == 1 || in.linePattern == 3) candidate = 1;
    }
    // Stop throughout validation; require elapsed time AND distinct fresh samples.
    if (candidate) {
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        if (!centerSamples_ || recoveryCandidate_ != candidate ||
            (sideConfirm && static_cast<uint32_t>(in.now-centerSampleMs_) > Config::LINE_RECOVERY_SAMPLE_GAP_MS)) {
            recoveryCandidate_ = candidate;
            centerSamples_ = 1;
            centerSinceMs_ = centerSampleMs_ = in.now;
        } else if (in.now != centerSampleMs_) {
            if (centerSamples_ < 3) ++centerSamples_;
            centerSampleMs_ = in.now;
        }
        if (centerSamples_ >= 3 && static_cast<uint32_t>(in.now-centerSinceMs_) >= Config::LINE_RECOVERY_CENTER_MS) {
            lineLossPending_ = false;
            centerSamples_ = 0;
            const auto action = segmentReverse() ? MotorAction::Backward : AGVMath::lineAction(in.linePattern);
            rememberLine(in, action);
#if defined(ARDUINO)
            if (Config::DEBUG_ENABLED) Serial.printf("[LINE] search_recovered pattern=%u motor=%s node=%s next=%s\n",
                in.linePattern, motorActionName(action), current_, nextNode());
#endif
            return action;
        }
        return MotorAction::Stop;
    }
    centerSamples_ = 0;
    // The task grace is not permission to keep rotating. After the original
    // search budget, remain stopped but keep accepting a confirmed line.
    if (Config::LINE_LOSS_GRACE_MS && elapsed >= Config::LINE_RECOVERY_TIMEOUT_MS) {
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    if (searchDirection_ == MotorAction::Stop || elapsed < Config::LINE_RECOVERY_STOP_MS)
        return MotorAction::Stop;
    // Advance from actual output transitions, not a modulo clock. A delayed
    // control iteration must emit STOP before another pulse, never skip rest.
    const uint32_t sinceChange = in.now-searchMotorChangedMs_;
    if (searchPulse_) {
        if (sinceChange < Config::LINE_RECOVERY_PULSE_MS) return searchDirection_;
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    if (sinceChange < Config::LINE_RECOVERY_REST_MS) return MotorAction::Stop;
    searchPulse_ = true;
    searchMotorChangedMs_ = in.now;
    return searchDirection_;
}

MotorAction CoreNavigator::alignTurnLine(const ControlInput& in) {
    if (static_cast<uint32_t>(in.now-lineLossStartedMs_) >= Config::LINE_RECOVERY_TIMEOUT_MS) {
        fault(ErrorCode::LineLost);
        return MotorAction::Stop;
    }
    if (missingZone()) {
        captureHistory_ = captureCount_ = 0;
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    const bool narrow = in.linePattern == 2 || in.linePattern == 3 || in.linePattern == 6;
    // Samples must be spaced and fresh. Side+center counts as line evidence,
    // not failed centering. A wide junction never authorizes translation.
    if (captureCount_ && static_cast<uint32_t>(in.now-captureSampleMs_) > Config::LEFT_CAPTURE_MAX_GAP_MS)
        captureHistory_ = captureCount_ = 0;
    const bool sampled = !captureCount_ || static_cast<uint32_t>(in.now-captureSampleMs_) >= Config::LEFT_CAPTURE_SAMPLE_MS;
    if (sampled) {
        captureHistory_ = static_cast<uint8_t>(((captureHistory_ << 1) | (narrow ? 1 : 0)) & 7u);
        if (captureCount_ < 3) ++captureCount_;
        captureSampleMs_ = in.now;
    }
    const unsigned votes = (captureHistory_ & 1u) + ((captureHistory_ >> 1) & 1u) + ((captureHistory_ >> 2) & 1u);
    if (sampled && in.linePattern == 2 && captureCount_ >= 2 && votes >= 2) {
        phase_ = Phase::Following;
        segmentStartedMs_ = in.now;
        resetLineTracking();
        turnSettling_ = true;
        return MotorAction::Stop; // Rear-sonar handoff still precedes translation.
    }
    MotorAction direction = MotorAction::Stop;
    if (in.linePattern == 3 || in.linePattern == 1) direction = MotorAction::PivotRight;
    if (in.linePattern == 6 || in.linePattern == 4) direction = MotorAction::PivotLeft;
    // Blank after capture may be overshoot: only a bounded countersteer,
    // never blind translation. Wide 111/101 remain stopped.
    if (in.linePattern == 0 && static_cast<uint32_t>(in.now-lineLossStartedMs_) >= Config::LINE_RECOVERY_STOP_MS)
        direction = searchDirection_;
    if (direction == MotorAction::Stop) {
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    if (direction != searchDirection_) {
        searchDirection_ = direction;
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    const uint32_t sinceChange = in.now-searchMotorChangedMs_;
    if (searchPulse_) {
        if (sinceChange < Config::LINE_RECOVERY_PULSE_MS) return direction;
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    if (sinceChange < Config::LINE_RECOVERY_REST_MS) return MotorAction::Stop;
    searchPulse_ = true;
    searchMotorChangedMs_ = in.now;
    return direction;
}

int CoreNavigator::zoneIndex(const char* node) {
    return node && node[0] == 'M' && node[1] >= '1' && node[1] <= '6' && !node[2]
        ? node[1] - '1' : -1;
}
bool CoreNavigator::hasGrant(const char* node) const {
    const int index = zoneIndex(node);
    return index < 0 || grantIds_[index] > 0;
}
bool CoreNavigator::currentZonePermitted() const {
    return currentZoneCleared_ || hasGrant(current_);
}
const char* CoreNavigator::missingZone() const {
    if (soloTrial_) return nullptr;
    if (!active() || index_ >= length_) return nullptr;
    if (!currentZonePermitted()) return current_;
    if (!hasGrant(path_[index_])) return path_[index_];
    return nullptr;
}
const char* CoreNavigator::requestedZone() const {
    if (const auto* zone = missingZone()) return zone;
    // Ask at most one segment ahead, only while approaching an authorized
    // straight-through RFID. Missing lookahead never inhibits this segment.
    if (!soloTrial_ && following() && index_+1 < length_ &&
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
        (soloTrial_ || (currentZonePermitted() && hasGrant(node) && hasGrant(path_[index_+1])));
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
        if (route.reverseAfterTurn[i] && !RoutePlanner::reverseHome(i ? command.path[i-1] : current,command.path[i])) return false;
        if (command.passThrough[i] && (i+1 >= route.count || route.reverseAfterTurn[i] || route.departure[i] == RouteTurn::Reverse ||
            route.departure[i+1] != RouteTurn::Straight ||
            !RoutePlanner::straightThrough(i ? command.path[i-1] : current,command.path[i],command.path[i+1]))) return false;
    }
    if (!AGVMath::copyText(current_, sizeof(current_), current)) return false;
    for (uint8_t i = 0; i < route.count; ++i) {
        if (!AGVMath::copyText(path_[i], sizeof(path_[i]), command.path[i])) return false;
        turns_[i] = route.departure[i];
        turnCrossings_[i] = route.turnCrossings[i];
        passThrough_[i] = command.passThrough[i];
        reverseAfterTurn_[i] = route.reverseAfterTurn[i];
    }
    index_ = 0;
    currentZoneCleared_ = false;
    requireNodeOrders_ = requireNodeOrders;
    nodeOrderReceived_ = false;
    routeCmdId_ = command.cmdId;
    stationOriented_ = false;
    obstaclePaused_ = false;
    length_ = route.count;
    segmentStartedMs_ = turnStartedMs_ = now;
    stoppedAtMs_ = now;
    oldLineDeparted_ = false;
    turnRetryUsed_ = false;
    resetLineTracking();
    soloTrial_ = soloTrial;
    error_ = ErrorCode::None;
    phase_ = Phase::WaitingGrant;
    return true;
}
void CoreNavigator::stop() {
    currentZoneCleared_ = false;
    obstaclePaused_ = false;
    routeCmdId_ = 0; nodeOrderReceived_ = false;
    stationOriented_ = false;
    if (phase_ != Phase::Fault) phase_ = Phase::Idle;
    length_ = index_ = 0;
    for (auto& id : grantIds_) id = 0;
    soloTrial_ = false;
    turnRetryUsed_ = false;
    resetLineTracking();
}
bool CoreNavigator::clearFault() {
    obstaclePaused_ = false;
    if (active()) return false;
    currentZoneCleared_ = false;
    phase_ = Phase::Idle;
    stationOriented_ = false;
    error_ = ErrorCode::None;
    length_ = index_ = 0;
    for (auto& id : grantIds_) id = 0;
    soloTrial_ = false;
    turnRetryUsed_ = false;
    resetLineTracking();
    return true;
}
void CoreNavigator::fault(ErrorCode reason) {
    currentZoneCleared_ = false;
    obstaclePaused_ = false;
    stationOriented_ = false;
    phase_ = Phase::Fault;
    error_ = reason;
    length_ = index_ = 0;
    for (auto& id : grantIds_) id = 0;
    soloTrial_ = false;
    turnRetryUsed_ = false;
    resetLineTracking();
}
void CoreNavigator::reachedNode(const char* node, uint32_t now, const ControlInput* input) {
    if (!active()) return;
    if (!node || !node[0] || index_ >= length_ || std::strcmp(node, path_[index_]) != 0) {
        fault(node && node[0] ? ErrorCode::UnexpectedNode : ErrorCode::UnknownTag);
        return;
    }
    const bool through = input && canPassThrough(node,*input);
    const bool parkedReverse = reverseAfterTurn_[index_];
    // A tag seen while rotating/waiting cannot prove the reverse leg happened.
    if (parkedReverse && phase_ != Phase::Following) { fault(ErrorCode::UnexpectedNode); return; }
    currentZoneCleared_ = false;
    resetLineTracking();
    AGVMath::copyText(current_, sizeof(current_), node);
    ++index_;
    nodeOrderReceived_ = false;
    if (RoutePlanner::station(node)) {
        if (parkedReverse) {
            if (index_ == length_) stop();
            else {
                phase_ = Phase::WaitingGrant;
                stoppedAtMs_ = segmentStartedMs_ = now;
                oldLineDeparted_ = lineLossPending_ = false;
                turnRetryUsed_ = false;
            }
            stationOriented_ = true;
            return;
        }
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
    turnRetryUsed_ = false;
    lineLossPending_ = false;
    if (through) { phase_ = Phase::Following; nodeOrderReceived_ = true; }
    else phase_ = Phase::WaitingGrant;
}
bool CoreNavigator::grant(const char* zone, int32_t grantId) {
    const int index = zoneIndex(zone);
    // Ignore an in-flight regrant for a zone already left on this segment.
    if (currentZoneCleared_ && zone && std::strcmp(zone, current_) == 0) return false;
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
bool CoreNavigator::completeZoneExit(const char* zone, int32_t grantId) {
    const int index = zoneIndex(zone);
    if (index < 0 || grantId <= 0 || grantIds_[index] != grantId) return false;
    grantIds_[index] = 0;
    if (following() && !segmentReverse() && std::strcmp(zone, current_) == 0)
        currentZoneCleared_ = true;
    return true;
}
void CoreNavigator::resetGrants() {
    currentZoneCleared_ = false;
    for (auto& id : grantIds_) id = 0;
    if (active()) fault(ErrorCode::NotConnected);
}
bool CoreNavigator::observeTurnLine(const ControlInput& in) {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED || phase_ != Phase::Turning ||
        turns_[index_] != RouteTurn::Left) return false;
    // The center leaving its old stripe is sufficient: the side eyes may
    // overlap the junction throughout, so a fully blank 000 may never occur.
    // 101 still straddles a wide stripe/junction; do not use it as departure.
    if (in.linePattern == 0 || in.linePattern == 1 || in.linePattern == 4) oldLineDeparted_ = true;
    if (!oldLineDeparted_ || !(in.linePattern & 2u)) return false;
    phase_ = Phase::TurnCapture;
    captureHistory_ = captureCount_ = 0;
    lineLossPending_ = true;
    lineLossStartedMs_ = searchMotorChangedMs_ = in.now;
    centerSamples_ = 0;
    searchPulse_ = false;
    // If inertia carries the sensors past the captured stripe, search BACK
    // briefly, never continue a long left rotation to another branch.
    searchDirection_ = MotorAction::PivotRight;
    return true;
}
bool CoreNavigator::refreshFollowing(const ControlInput& in, MotorAction& action) {
    if (!Config::FAST_LINE_RESPONSE || phase_ != Phase::Following || turnSettling_ ||
        lineLossPending_ || obstaclePaused_) return false;
    // A revoked grant must stop immediately, never authorize an early step.
    action = missingZone() ? MotorAction::Stop : update(in);
    return true;
}
MotorAction CoreNavigator::update(const ControlInput& in) {
    if (!active()) return MotorAction::Stop;
    if (!in.wifi || !in.mqtt) { fault(ErrorCode::NotConnected); return MotorAction::Stop; }
    if (!in.motionConfigured || !in.lineConfigured) { fault(ErrorCode::Configuration); return MotorAction::Stop; }
    if (!in.distanceValid) { fault(ErrorCode::SensorInvalid); return MotorAction::Stop; }
    if (in.obstacle) {
        if (Config::REAR_ULTRASONIC_ECHO_PIN < 0) { fault(ErrorCode::Obstacle); return MotorAction::Stop; }
        if (!obstaclePaused_) { obstaclePaused_ = true; obstaclePausedAtMs_ = in.now; }
        centerSamples_ = 0;
        captureHistory_ = captureCount_ = 0;
        searchPulse_ = false;
        searchMotorChangedMs_ = in.now;
        return MotorAction::Stop;
    }
    if (obstaclePaused_) {
        const uint32_t held = static_cast<uint32_t>(in.now - obstaclePausedAtMs_);
        segmentStartedMs_ += held;
        turnStartedMs_ += held;
        stoppedAtMs_ += held;
        lineLossStartedMs_ += held;
        lastCorrectionMs_ += held;
        if (recoveringLine()) searchMotorChangedMs_ = in.now;
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
        if (observeTurnLine(in)) return MotorAction::Stop;
        if (static_cast<uint32_t>(in.now - turnStartedMs_) >= Config::TURN_TIMEOUT_MS) {
            const bool retryable = Config::TURN_RETRY_ENABLED && !turnRetryUsed_ &&
                !(Config::LEFT_TURN_CAPTURE_ENABLED && turns_[index_] == RouteTurn::Left) &&
                (turns_[index_] == RouteTurn::Left || turns_[index_] == RouteTurn::Right);
            if (retryable) {
                // One local retry keeps the route and mutex intact. Stop for this
                // control cycle, then repeat the same pivot direction from a clean
                // line-crossing detector. Safety/network checks still run first.
                turnRetryUsed_ = true;
                turnStartedMs_ = lastCrossingMs_ = in.now;
                crossedLines_ = 0;
                oldLineDeparted_ = false;
#if defined(ARDUINO)
                if (Config::DEBUG_ENABLED) Serial.printf("[LINE] turn_retry=1 maneuver=%s node=%s next=%s\n",
                    RoutePlanner::turnName(turns_[index_]), current_, nextNode());
#endif
                return MotorAction::Stop;
            }
#if defined(ARDUINO)
            if (Config::DEBUG_ENABLED && turnRetryUsed_) Serial.printf(
                "[LINE] turn_retry_failed maneuver=%s node=%s next=%s\n",
                RoutePlanner::turnName(turns_[index_]), current_, nextNode());
#endif
            fault(turnRetryUsed_ || (Config::LEFT_TURN_CAPTURE_ENABLED && turns_[index_] == RouteTurn::Left) ?
                ErrorCode::LineLost : ErrorCode::TurnTimeout);
            return MotorAction::Stop;
        }
        if (Config::LEFT_TURN_CAPTURE_ENABLED && turns_[index_] == RouteTurn::Left)
            return MotorAction::PivotLeft;
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
                // Give the controller one STOP cycle to switch to the rear
                // sonar and acquire a fresh sample before any reverse PWM.
                if (reverseAfterTurn_[index_]) return MotorAction::Stop;
            } else return MotorAction::PivotRight;
        } else return turns_[index_] == RouteTurn::Left ? MotorAction::PivotLeft : MotorAction::PivotRight;
    }
    if (phase_ == Phase::TurnCapture) {
        return alignTurnLine(in);
    }
    if (turnSettling_ && static_cast<uint32_t>(in.now-segmentStartedMs_) >= Config::LEFT_CAPTURE_SETTLE_MS)
        turnSettling_ = false;
    if (static_cast<uint32_t>(in.now - segmentStartedMs_) >= Config::SEGMENT_TIMEOUT_MS) {
        fault(ErrorCode::MotionTimeout); return MotorAction::Stop;
    }
    if (missingZone()) {
        // Evidence gathered before grant revocation must not survive the hold.
        centerSamples_ = 0;
        return MotorAction::Stop;
    }
    const MotorAction action = segmentReverse()
        ? AGVMath::reverseLineAction(in.linePattern) : AGVMath::lineAction(in.linePattern);
    if (Config::LINE_RECOVERY_ENABLED && (lineLossPending_ || in.linePattern == 0))
        return recoverLine(in);
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
    } else rememberLine(in, action);
    return action;
}
AGVState CoreNavigator::state() const {
    if (obstaclePaused_) return AGV_WAITING;
    if (phase_ == Phase::Fault) return AGV_SAFE_STOP;
    if (phase_ == Phase::WaitingGrant || phase_ == Phase::StationDwell) return AGV_WAITING;
    if (phase_ == Phase::Turning || phase_ == Phase::TurnCapture || phase_ == Phase::Following || phase_ == Phase::StationTurning) return AGV_MOVING;
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
        order.turnCrossings != crossings || order.reverseAfterTurn != (!parking && reverseAfterTurn_[index_])) return false;
    nodeOrderReceived_ = true;
    return true;
}
