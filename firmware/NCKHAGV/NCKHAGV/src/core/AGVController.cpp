#include "AGVController.h"
#include "RoutePlanner.h"
#include <cstring>
#include <esp_system.h>

AGVController::AGVController() : line_(motor_), movement_(motor_, line_, buzzer_), mqtt_(wifi_),
    control_({Config::WIFI_TIMEOUT_MS, Config::MQTT_TIMEOUT_MS, Config::TURN_MIN_MS,
              Config::TURN_TIMEOUT_MS, Config::STRAIGHT_DEPARTURE_MS, Config::SERVO_MOVE_TIME_MS,
              true, true, Config::REVERSE_STEERING_CONFIRMED}) {}
void AGVController::updateSonars() {
    if (!Config::ULTRASONIC_MOTION_GUARD) return;
    if (Config::REAR_ULTRASONIC_ECHO_PIN < 0) { sensor_.update(); return; }
    const bool reverse = navigator_.active() ? navigator_.segmentReverse() : control_.segmentReverse();
    if (sonars_.update(reverse)) movement_.stop();
    // Apply the local stop before RFID/SPI reads and command processing. The
    // navigator still owns route pause/resume, hysteresis and sensor faults.
    const auto& active = sonars_.active();
    const bool invalid = !active.valid() && !sonars_.acquiring();
    const bool blocked = active.valid() && active.readDistanceCm() <=
        (navigator_.obstaclePaused() ? Config::OBSTACLE_CLEAR_CM : Config::OBSTACLE_DISTANCE_CM);
    if (invalid || blocked) movement_.stop();
}
void AGVController::logSonars() {
    if (!Config::DEBUG_ENABLED || Config::REAR_ULTRASONIC_ECHO_PIN < 0) return;
    const auto print = [](const char* side, const SensorModule& sensor, bool active, int trigPin, int echoPin) {
        const auto d = sensor.diagnostic();
        Serial.printf("[SONAR] side=%s active=%d trig=%d echoPin=%d echo=%d waiting=%d rise=%d ready=%d pulse_us=%lu result_age_ms=%lu triggers=%lu edges=%lu result=%s valid=%d cm=%.2f\n",
            side, active, trigPin, echoPin, d.echoLevel, d.waiting, d.risingSeen, d.ready,
            static_cast<unsigned long>(d.pulseUs), static_cast<unsigned long>(d.resultAgeMs),
            static_cast<unsigned long>(d.triggers), static_cast<unsigned long>(d.edges),
            d.result, d.valid, sensor.readDistanceCm());
    };
    print("front", sensor_, !sonars_.rearActive(), Config::ULTRASONIC_TRIG_PIN, Config::ULTRASONIC_ECHO_PIN);
    print("rear", rearSensor_, sonars_.rearActive(), Config::REAR_ULTRASONIC_TRIG_PIN, Config::REAR_ULTRASONIC_ECHO_PIN);
}
void AGVController::begin() {
    buzzer_.begin();
    motor_.begin();
    encoder_.begin();
    movement_.begin();
    line_.begin();
    if (Config::ULTRASONIC_MOTION_GUARD) {
        sensor_.begin();
        if (Config::REAR_ULTRASONIC_ECHO_PIN >= 0) rearSensor_.begin();
    }
    rfid_.begin();
    servo_.begin();
    battery_.begin();
    cargo_.begin();
    control_.begin(millis());
    if (!mqtt_.begin()) control_.fail(ErrorCode::Configuration);
    if (Config::DEBUG_ENABLED) {
        Serial.printf("[BOOT] firmware=%s sonarFront=%d/%d sonarRear=%d/%d\n", Config::FIRMWARE_BUILD,
            Config::ULTRASONIC_TRIG_PIN, Config::ULTRASONIC_ECHO_PIN,
            Config::REAR_ULTRASONIC_TRIG_PIN, Config::REAR_ULTRASONIC_ECHO_PIN);
        Serial.printf("[BOOT] xe=%s reset_reason=%d motion=%d bench=%d\n", Config::AGV_ID,
                      static_cast<int>(esp_reset_reason()), Config::ENABLE_CORE_MOTION, Config::TWO_ROBOT_BENCH_TRIAL);
        Serial.printf("[BATTERY] cells=%d range=%.2f..%.2fV full=%.2fV scale=%.6f offset=%.3fV\n",
                      Config::BATTERY_SERIES_CELLS, Config::BATTERY_EMPTY_VOLTAGE,
                      Config::BENCH_MAX_VOLTAGE, Config::BATTERY_FULL_VOLTAGE,
                      Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET);
        Serial.printf("[AGV] Core motion %s; mutex encoder release=%d threshold=%lu pulses/wheel\n",
                      Config::ENABLE_CORE_MOTION ? "configured" : "locked",
                      Config::MUTEX_ENCODER_RELEASE_ENABLED,
                      static_cast<unsigned long>(Config::MUTEX_RELEASE_MIN_PULSES));
    }
}
ControlInput AGVController::input() const {
    ControlInput result;
    const auto readings = line_.readSensors();
    result.now = millis();
    result.wifi = wifi_.isConnected();
    result.mqtt = mqtt_.isConnected();
    result.motionConfigured = Config::ENABLE_CORE_MOTION && motor_.configured() &&
                              (battery_.calibrated() || Config::ONE_ROBOT_TRIAL ||
                               Config::TWO_ROBOT_BENCH_TRIAL) &&
                              Config::benchVoltageReady(battery_.readVoltage()) &&
                              Config::OBSTACLE_DISTANCE_CM > 0;
    result.liftConfigured = servo_.liftConfigured();
    const auto& activeSonar = Config::REAR_ULTRASONIC_ECHO_PIN >= 0 ? sonars_.active() : sensor_;
    result.distanceValid = !Config::ULTRASONIC_MOTION_GUARD || activeSonar.valid();
    result.obstacle = Config::ULTRASONIC_MOTION_GUARD && activeSonar.valid() &&
        activeSonar.readDistanceCm() <= (navigator_.obstaclePaused() ? Config::OBSTACLE_CLEAR_CM : Config::OBSTACLE_DISTANCE_CM);
    result.servoBusy = servo_.busy();
    result.lineConfigured = readings.configured;
    result.linePattern = readings.pattern;
    result.cargoStable = cargo_.stable();
    result.cargoPresent = cargo_.present();
    return result;
}
bool AGVController::consumePriority() {
    bool inhibited = false;
    if (mqtt_.consumeStop()) { navigator_.stop(); control_.pause(); inhibited = true; }
    if (mqtt_.consumeCoreRestart()) {
        positionConfirmed_ = false;
        lastRfidSeenMs_ = 0;
        previousNode_[0] = '\0';
        previousSegmentReverse_ = false;
        heading_ = RobotHeading::Unknown; headingBayDefault_ = false;
        navigator_.resetGrants();
        clearance_.reset();
        control_.safeStop(ErrorCode::NotConnected);
        inhibited = true;
    }
    if (mqtt_.consumeQueueFault()) { navigator_.fault(ErrorCode::QueueOverflow); control_.safeStop(ErrorCode::QueueOverflow); inhibited = true; }
    if (mqtt_.consumeStorageFault()) { navigator_.fault(ErrorCode::Configuration); control_.safeStop(ErrorCode::Configuration); inhibited = true; }
    if (inhibited) {
        if(lift_.busy()) {lift_.interrupt();control_.safeStop(ErrorCode::ServoInterrupted);}
        movement_.stop();
    }
    return inhibited;
}
void AGVController::handleCommand(const CoreCommand& command, bool inhibited) {
    movement_.stop();
    if (command.type == CoreCommandType::Stop || command.type == CoreCommandType::Wait) {
        if(lift_.busy()) {lift_.interrupt();control_.safeStop(ErrorCode::ServoInterrupted);}
        navigator_.stop();
        control_.pause();
        return;
    }
    if (inhibited || command.localSession != mqtt_.session() ||
        static_cast<uint32_t>(millis() - command.receivedAtMs) > Config::COMMAND_MAX_AGE_MS) {
        if (navigator_.active()) navigator_.fault(ErrorCode::StaleCommand);
        mqtt_.publishError(ErrorCode::StaleCommand, millis());
        return;
    }
    if (command.type == CoreCommandType::ResetFault) {
        const ControlInput current = input();
        const auto voltageError = AGVMath::motionVoltageError(battery_.readVoltage());
        if (voltageError != ErrorCode::None) {
            control_.safeStop(voltageError);
            mqtt_.publishError(voltageError, millis());
            return;
        }
        if (navigator_.active() || !positionConfirmed_ || positionSession_ != mqtt_.session() ||
            !current.motionConfigured || !current.lineConfigured ||
            AGVMath::lineAction(current.linePattern) == MotorAction::Stop ||
            !current.wifi || !current.mqtt || !current.distanceValid || current.obstacle) {
            mqtt_.publishError(ErrorCode::RestartDenied, millis());
            return;
        }
        if (control_.state() == AGV_SAFE_STOP || control_.state() == AGV_ERROR) {
            ErrorCode reason = ErrorCode::None;
            if (!control_.restart(current, reason)) {
                mqtt_.publishError(reason, millis());
                return;
            }
        }
        navigator_.clearFault();
        publishTelemetry();
        return;
    }
    if (command.type == CoreCommandType::LiftUp || command.type == CoreCommandType::LiftDown) {
        const bool ready = Config::SERVO_SINGLE_TEST && servo_.liftConfigured() && !servo_.busy() && !lift_.busy() &&
            !navigator_.active() && !control_.task().active && control_.state()==AGV_IDLE &&
            navigator_.error()==ErrorCode::None && control_.error()==ErrorCode::None &&
            positionConfirmed_ && positionSession_==mqtt_.session() &&
            std::strcmp(command.node,control_.currentNode())==0 && wifi_.isConnected() && mqtt_.isConnected();
        if (!ready) { mqtt_.publishError(ErrorCode::ServoConfiguration,millis());return; }
        const bool moved=command.type==CoreCommandType::LiftUp ? servo_.liftUp() : servo_.liftDown();
        if (!moved || !lift_.start(command,millis())) {control_.safeStop(ErrorCode::ServoConfiguration);return;}
        if(Config::DEBUG_ENABLED) Serial.printf("[LIFT] cmd=%ld state=%s gpio=17 angle=%d duration=5000\n",
            static_cast<long>(command.cmdId),lift_.state(),servo_.getAngle(Config::SERVO_LIFT));
        publishTelemetry();return;
    }
    if (lift_.busy() || servo_.busy()) {
        lift_.interrupt();control_.safeStop(ErrorCode::ServoInterrupted);return;
    }
    if (command.type == CoreCommandType::ChargeStart) {
        // No measured charger or charging-done input exists in this firmware.
        if (navigator_.active()) navigator_.fault(ErrorCode::ChargerUnconfigured);
        control_.safeStop(ErrorCode::ChargerUnconfigured);
        return;
    }
    if (command.type == CoreCommandType::NodeOrder) {
        if (positionConfirmed_ && positionSession_ == mqtt_.session() &&
            navigator_.authorizeNodeOrder(command)) {
            if (Config::DEBUG_ENABLED) Serial.printf("[NODE_ORDER] server: %s %s -> %s route=%ld step=%u\n",
                command.maneuver,command.node,command.nextNode,static_cast<long>(command.routeCmdId),command.pathIndex);
        } else if (Config::DEBUG_ENABLED) Serial.println("[NODE_ORDER] rejected stale/mismatched order");
        return;
    }
    if (command.type == CoreCommandType::Goto) {
        // A new route while between tags has no reliable physical start node.
        if (navigator_.active()) {
            navigator_.fault(ErrorCode::InvalidCommand);
            control_.safeStop(ErrorCode::InvalidCommand);
            return;
        }
        // The directed map and Core path resolve route turns. This preflight
        // does not grant mutex ownership or prove physical zone clearance.
        RoutePlan route;
        if (!positionConfirmed_ || positionSession_ != mqtt_.session()) {
            control_.safeStop(ErrorCode::NotAtRouteStart);
            return;
        }
        if (!RoutePlanner::plan(previousNode_, control_.currentNode(), command, route,
                                previousSegmentReverse_, heading_)) {
            control_.safeStop(ErrorCode::InvalidCommand);
            return;
        }
        if (Config::DEBUG_ENABLED) {
            logSonars();
            for (uint8_t i = 0; i < route.count; ++i)
                Serial.printf("[ROUTE] %s -> %s: %s reverse_after_turn=%d\n", i ? command.path[i - 1] : control_.currentNode(),
                              command.path[i], RoutePlanner::turnName(route.departure[i]), route.reverseAfterTurn[i]);
        }
        const bool trialRoute = Config::ONE_ROBOT_TRIAL &&
            RoutePlanner::oneRobotTrialRoute(Config::AGV_ID, control_.currentNode(), command, route);
        bool reverseRequired = false;
        for (uint8_t i = 0; i < route.count; ++i)
            reverseRequired |= route.departure[i] == RouteTurn::Reverse || route.reverseAfterTurn[i];
        AGVMath::MotionStartConditions checks;
        checks.enabled = Config::ENABLE_CORE_MOTION;
        checks.routeAllowed = !Config::ONE_ROBOT_TRIAL || trialRoute;
        checks.voltage = battery_.readVoltage();
        checks.calibrationRequired = !trialRoute && !Config::TWO_ROBOT_BENCH_TRIAL;
        checks.calibrated = battery_.calibrated();
        checks.reverseAllowed = RoutePlanner::reverseAllowed(control_.currentNode(), command, route);
        checks.previousError = control_.state() == AGV_SAFE_STOP || control_.state() == AGV_ERROR
            ? control_.error() : ErrorCode::None;
        if ((control_.state() == AGV_SAFE_STOP || control_.state() == AGV_ERROR) &&
            checks.previousError == ErrorCode::None) checks.previousError = ErrorCode::MotionLocked;
        const auto lockReason = AGVMath::motionStartError(checks);
        if (lockReason != ErrorCode::None) {
            if (Config::DEBUG_ENABLED)
                Serial.printf("[MOTION_LOCK] xe=%s node=%s previous=%s motion=%d voltage=%.3f adc=%lumV range=%.2f..%.2fV reverse=%d state=%s reason=%s\n",
                              Config::AGV_ID, control_.currentNode(), previousNode_,
                              Config::ENABLE_CORE_MOTION, battery_.readVoltage(),
                              static_cast<unsigned long>(battery_.adcMillivolts()),
                              Config::BATTERY_EMPTY_VOLTAGE, Config::BENCH_MAX_VOLTAGE, reverseRequired,
                              stateName(control_.state()), errorName(lockReason));
            control_.safeStop(lockReason);
            return;
        }
        if (route.count && !navigator_.start(control_.currentNode(), command, route, millis(), trialRoute, true))
            control_.safeStop(ErrorCode::InvalidCommand);
        return;
    }
}
void AGVController::update() {
    encoder_.update(millis());
    // Do not latch a WiFi/MQTT timeout during initial association. Once the
    // first healthy Core heartbeat arrives, the normal watchdog stays active.
    if (!controlArmed_ && mqtt_.isConnected()) {
        control_.begin(millis());
        controlArmed_ = true;
    }
    if (!mqtt_.isConnected() && !navigator_.active()) {
        positionConfirmed_ = false; previousNode_[0] = '\0';
        lastRfidSeenMs_ = 0;
        previousSegmentReverse_ = false;
        heading_ = RobotHeading::Unknown; headingBayDefault_ = false;
        navigator_.resetGrants();
        clearance_.reset();
    }
    cargo_.update(millis());
    updateSonars();
    battery_.update(motor_.leftDuty() != 0 || motor_.rightDuty() != 0);
    servo_.update();
    bool inhibited = consumePriority();
    if (controlArmed_) control_.checkSafety(input());
    if (!navigator_.active() && control_.state() != AGV_MOVING) movement_.stop();
    if (navigator_.observeTurnLine(input())) {
        movement_.stop(); // Apply STOP before RFID/SPI and before any Serial log.
        if (Config::DEBUG_ENABLED) Serial.printf("[TURN] center_capture xe=%s node=%s next=%s line=%u\n",
            Config::AGV_ID, control_.currentNode(), navigator_.nextNode(), line_.readSensors().pattern);
    }
    // Sample and apply the latest line BEFORE RC522/SPI may wait for a card.
    // Only refresh a segment already in Following; all start/turn/grant gates
    // stay in the normal navigator path below. Stop takes effect immediately.
    if (Config::FAST_LINE_RESPONSE && !inhibited && !lift_.busy() && controlArmed_ &&
        positionConfirmed_ && positionSession_ == mqtt_.session() &&
        control_.state() != AGV_SAFE_STOP && control_.state() != AGV_ERROR &&
        !(Config::ULTRASONIC_MOTION_GUARD && Config::REAR_ULTRASONIC_ECHO_PIN >= 0 && sonars_.acquiring())) {
        MotorAction early = MotorAction::Stop;
        const ControlInput earlyInput = input();
        if (navigator_.refreshFollowing(earlyInput,early))
            movement_.apply(early, false, Config::LINE_RECOVERY_PWM, false,
                {navigator_.forwardLineTracking(), earlyInput.linePattern, earlyInput.now});
    }
    // A stationary pivot cannot arrive at a new route node. Defer blocking
    // RC522 transactions until the 180-degree turn and STOP confirmation finish.
    if (!navigator_.fastTurnSampling() && rfid_.readCard()) {
        const RFIDEvent& event = rfid_.event();
        if (event.node[0]) clearance_.seen(event.node, millis());
        if (!event.node[0]) {
            if (navigator_.active()) movement_.stop();
            navigator_.reachedNode("", millis());
            positionConfirmed_ = false;
            lastRfidSeenMs_ = 0;
            previousNode_[0] = '\0';
            previousSegmentReverse_ = false;
            heading_ = RobotHeading::Unknown; headingBayDefault_ = false;
            control_.reachedNode("", millis());
            if (!navigator_.active() && control_.state() != AGV_MOVING) movement_.stop();
        }
        else {
            // A scan made while Core is disconnected cannot establish the
            // position for the new server connection.
            const uint32_t observedSession = mqtt_.session();
            const bool priorConfirmed = positionConfirmed_ && positionSession_ == observedSession;
            positionConfirmed_ = mqtt_.isConnected() && observedSession == mqtt_.session();
            if (positionConfirmed_) {positionSession_ = observedSession;lastRfidSeenMs_ = millis();}
            else lastRfidSeenMs_ = 0;
            // Periodic telemetry reports a confirmed stationary tag.
            if (event.newNode) {
                // Keep moving only at the authorized RFID with safe sensors
                // and all required grants. Every actual RFID is still reported.
                const bool reverse = navigator_.segmentReverse() || control_.segmentReverse();
                const bool expected = priorConfirmed && positionConfirmed_ && navigator_.active() &&
                    std::strcmp(navigator_.nextNode(), event.node) == 0;
                const auto arrivalInput = input();
                const bool through = expected && control_.error() == ErrorCode::None &&
                    control_.state() != AGV_SAFE_STOP && control_.state() != AGV_ERROR &&
                    navigator_.canPassThrough(event.node,arrivalInput);
                if (navigator_.active() && !through) movement_.stop();
                navigator_.reachedNode(event.node, millis(),through ? &arrivalInput : nullptr);
                char fromNode[NODE_TEXT_SIZE]{};
                AGVMath::copyText(fromNode, sizeof(fromNode), control_.currentNode());
                if (expected && fromNode[0] && navigator_.error() == ErrorCode::None) {
                    const auto translation = encoder_.translationTicks();
                    clearance_.reached(fromNode, event.node, translation.left,
                                       translation.right, millis());
                }
                const char* incoming = RoutePlanner::confirmedIncoming(
                    expected && navigator_.error() == ErrorCode::None, fromNode, event.node);
                if (incoming)
                    AGVMath::copyText(previousNode_, sizeof(previousNode_), incoming);
                else previousNode_[0] = '\0';
                previousSegmentReverse_ = incoming && reverse;
                heading_ = incoming ? RoutePlanner::edgeHeading(fromNode, event.node, reverse, true) : RobotHeading::Unknown;
                headingBayDefault_ = false;
                control_.reachedNode(event.node, millis());
                if (!navigator_.active() && control_.state() != AGV_MOVING) movement_.stop();
                mqtt_.publishEvent("AGV_REACHED_NODE", event.node, millis(), control_.task().id, fromNode, reverse);
                publishTelemetry();
            }
        }
        if (positionConfirmed_ && !navigator_.active() && !control_.task().active &&
            navigator_.error() == ErrorCode::None && control_.error() == ErrorCode::None) {
            const auto docking = RoutePlanner::bayHeading(control_.currentNode());
            if (docking != RobotHeading::Unknown) { heading_ = docking; headingBayDefault_ = true; }
        }
        // Preserve RC522 HALT/WUPA for fresh repeated reads. At an authorized
        // straight-through RFID the motor keeps following during cleanup.
        rfid_.finishRead();
    }
    updateSonars();
    if (controlArmed_) control_.checkSafety(input());
    if (!navigator_.active() && control_.state() != AGV_MOVING) movement_.stop();
    if (control_.state() == AGV_LOADING && cargo_.stable() && !pickupIrReported_) {
        mqtt_.publishCargo("AGV_PICKUP_IR", control_.currentNode(), control_.task().id, cargo_.present(), millis());
        pickupIrReported_ = true;
    }
    if (control_.state() != AGV_LOADING) pickupIrReported_ = false;
    inhibited = consumePriority() || inhibited;
    CoreCommand command;
    for (size_t i = 0; i < Config::COMMAND_QUEUE_LENGTH && mqtt_.popCommand(command); ++i) {
        inhibited = consumePriority() || inhibited;
        handleCommand(command, inhibited);
        if (command.type == CoreCommandType::Stop || command.type == CoreCommandType::Wait) inhibited = true;
    }
    CoreGrant grant;
    while (mqtt_.popGrant(grant))
        if (grant.localSession == mqtt_.session() &&
            clearance_.canAcceptGrant(grant.zone, grant.grantId) &&
            navigator_.grant(grant.zone, grant.grantId))
            clearance_.grant(grant.zone, grant.grantId, control_.currentNode(),
                             positionConfirmed_ && positionSession_ == mqtt_.session());
    const auto clearanceTicks = encoder_.translationTicks();
    const MotorAction clearanceAction = movement_.action();
    const bool clearanceTranslating = clearanceAction != MotorAction::Stop &&
        clearanceAction != MotorAction::PivotLeft && clearanceAction != MotorAction::PivotRight;
    const bool safeClearance = controlArmed_ && mqtt_.isConnected() &&
        positionConfirmed_ && positionSession_ == mqtt_.session() && navigator_.active() &&
        navigator_.following() && navigator_.error() == ErrorCode::None &&
        control_.error() == ErrorCode::None && clearanceTranslating;
    clearance_.progress(clearanceTicks.left, clearanceTicks.right, millis(), safeClearance);
    MutexClearance::Release release;
    while (clearance_.pending(release)) {
        if (!mqtt_.releaseZone(release.zone, release.grantId)) break;
        if (Config::DEBUG_ENABLED) {
            Serial.printf("[MUTEX_RELEASE] zone=%s grant=%ld proof=%s left=%lu right=%lu threshold=%lu\n",
                release.zone, static_cast<long>(release.grantId),
                release.encoderProof ? "encoder" : "exit_rfid",
                static_cast<unsigned long>(release.leftPulses),
                static_cast<unsigned long>(release.rightPulses),
                static_cast<unsigned long>(Config::MUTEX_RELEASE_MIN_PULSES));
        }
        navigator_.completeZoneExit(release.zone, release.grantId);
        clearance_.queued(release);
    }
    inhibited = consumePriority() || inhibited;
    if (inhibited) { navigator_.stop(); control_.pause(); }
    updateSonars();
    const char* neededZone = navigator_.requestedZone();
    if (neededZone && static_cast<uint32_t>(millis() - lastZoneRequestMs_) >= 1000) {
        if (mqtt_.requestZone(neededZone)) lastZoneRequestMs_ = millis();
    }
    const ControlOutput output = controlArmed_ ? control_.update(input()) : ControlOutput{};
    if (navigator_.active() && (control_.state() == AGV_SAFE_STOP || control_.state() == AGV_ERROR))
        navigator_.fault(control_.error() == ErrorCode::None ? ErrorCode::Configuration : control_.error());
    // Newly selected sonar needs a fresh result before movement. This bounded
    // wait does not pass an unknown distance off as clear; after 200 ms the
    // navigator receives the invalid sample and takes its normal sensor fault.
    if (lift_.busy()) {
        movement_.stop();
        const bool safe=!inhibited && wifi_.isConnected() && mqtt_.isConnected() &&
            positionConfirmed_ && positionSession_==mqtt_.session() &&
            std::strcmp(control_.currentNode(),lift_.node())==0 &&
            !navigator_.active() && control_.error()==ErrorCode::None && navigator_.error()==ErrorCode::None;
        if(lift_.update(millis(),safe)) {
            if(!safe) control_.safeStop(ErrorCode::ServoInterrupted);
            publishTelemetry();
        }
    }
    const bool sonarAcquiring = Config::ULTRASONIC_MOTION_GUARD &&
        Config::REAR_ULTRASONIC_ECHO_PIN >= 0 && sonars_.acquiring();
    const ControlInput motionInput = input();
    const MotorAction coreMotor = navigator_.active() && !sonarAcquiring
        ? navigator_.update(motionInput) : MotorAction::Stop;
    if (navigator_.stationOriented()) {
        heading_ = RoutePlanner::bayHeading(control_.currentNode());
        headingBayDefault_ = true;
    } else if (coreMotor == MotorAction::PivotLeft || coreMotor == MotorAction::PivotRight) {
        heading_ = RobotHeading::Unknown; headingBayDefault_ = false;
    } else if (navigator_.following() && coreMotor != MotorAction::Stop) {
        // Reacquired route line fixes the front direction; backing does not flip it.
        heading_ = RoutePlanner::edgeHeading(control_.currentNode(), navigator_.nextNode(), navigator_.segmentReverse());
        headingBayDefault_ = false;
    }
    movement_.apply(navigator_.state() == AGV_SAFE_STOP ? MotorAction::Stop :
                    (navigator_.active() ? coreMotor : output.motor), navigator_.precisionPivot(),
                    navigator_.precisionPivotPwm(), navigator_.settlingTurnLine(),
                    {navigator_.forwardLineTracking(), motionInput.linePattern, motionInput.now});
    if (output.servo != ServoAction::None) {
        movement_.stop();
        const bool moved = output.servo == ServoAction::LiftUp ? servo_.liftUp() : servo_.liftDown();
        if (!moved) control_.fail(ErrorCode::ServoConfiguration);
    }
    if (navigator_.state() == AGV_SAFE_STOP ||
        (!navigator_.active() && control_.state() != AGV_MOVING)) movement_.stop();
    const MotorAction motorAction = movement_.action();
    const bool motionLog = motorAction != lastMotorAction_ && (!Config::FAST_LINE_RESPONSE ||
        motorAction == MotorAction::Stop ||
        static_cast<uint32_t>(millis()-lastMotionLogMs_) >= Config::MOTION_LOG_INTERVAL_MS);
    if (motionLog) lastMotionLogMs_ = millis();
    if (Config::ENCODER_ENABLED && Config::DEBUG_ENABLED && motionLog) {
        Serial.printf("[BUZZER] xe=%s motor=%s left=%d right=%d requested=%d gpio=%d level=%d activeLevel=%d\n",
            Config::AGV_ID, motorActionName(motorAction), motor_.leftDuty(), motor_.rightDuty(),
            AGVMath::reverseBuzzerRequired(motorAction, motor_.leftDuty(), motor_.rightDuty()),
            Config::BUZZER_PIN, buzzer_.pinLevel(), Config::BUZZER_ACTIVE_STATE);
    }
    if (Config::DEBUG_ENABLED && Config::ENABLE_CORE_MOTION && motionLog &&
        (std::strcmp(control_.currentNode(), "GS1") == 0 || std::strcmp(control_.currentNode(), "GS2") == 0 ||
         std::strcmp(control_.currentNode(), "G1") == 0)) {
        Serial.printf("[MOTOR] xe=%s node=%s action=%s left=%d right=%d line=%u next=%s zone=%s\n",
                      Config::AGV_ID, control_.currentNode(), motorActionName(motorAction),
                      motor_.leftDuty(), motor_.rightDuty(),
                      static_cast<unsigned int>(line_.readSensors().pattern), navigator_.nextNode(),
                      navigator_.missingZone() ? navigator_.missingZone() : "-");
    }
    lastMotorAction_ = motorAction;
    if (Config::ENCODER_ENABLED) {
        const bool localized = positionConfirmed_ && positionSession_ == mqtt_.session() && mqtt_.isConnected();
        const bool tracking = localized && navigator_.active() && navigator_.nextNode()[0] &&
            navigator_.error() == ErrorCode::None && control_.error() == ErrorCode::None;
        const bool translating = tracking && navigator_.following() && motorAction != MotorAction::Stop &&
            motorAction != MotorAction::PivotLeft && motorAction != MotorAction::PivotRight;
        encoder_.context(localized ? control_.currentNode() : "", tracking ? navigator_.nextNode() : "",
            tracking ? navigator_.routeCommandId() : 0, tracking ? navigator_.pathIndex() : 0,
            mqtt_.session(), tracking, translating, navigator_.segmentReverse());
        const uint32_t now = millis();
        if (static_cast<uint32_t>(now - lastOdometryMs_) >= Config::ODOMETRY_INTERVAL_MS) {
            mqtt_.publishOdometry(encoder_.snapshot());
            lastOdometryMs_ = now;
        }
    }
    observeState();
    if (!navigator_.fastTurnSampling() && static_cast<uint32_t>(millis() - lastSonarLogMs_) >= 1000) {
        lastSonarLogMs_ = millis();
        if (!Config::FAST_LINE_RESPONSE || !navigator_.following()) logSonars();
    }
    if (static_cast<uint32_t>(millis() - lastTelemetryMs_) >= Config::TELEMETRY_INTERVAL_MS) publishTelemetry();
    if (Config::DEBUG_ENABLED && !Config::ENABLE_CORE_MOTION &&
        static_cast<uint32_t>(millis() - lastBenchPrintMs_) >= 1000) {
        lastBenchPrintMs_ = millis();
        const LineReadings line = line_.readSensors();
        Serial.printf("[BENCH] line=%d%d%d pattern=%u; ultrasonic=%.1fcm valid=%d; battery_adc=%lumV\n",
                      line.left, line.center, line.right, static_cast<unsigned int>(line.pattern),
                      sensor_.readDistanceCm(), sensor_.valid() ? 1 : 0,
                      static_cast<unsigned long>(battery_.adcMillivolts()));
    }
}
void AGVController::observeState() {
    const AGVState state = navigator_.state() != AGV_IDLE ? navigator_.state() : control_.state();
    const ErrorCode error = navigator_.error() != ErrorCode::None ? navigator_.error() : control_.error();
    if (lastState_ == state && lastError_ == error) return;
    if (error == ErrorCode::SensorInvalid && error != lastError_) logSonars();
    if (Config::DEBUG_ENABLED) Serial.printf("[STATE] %s -> %s error=%s node=%s\n",
        stateName(lastState_), stateName(state), errorName(error), control_.currentNode());
    if (error != ErrorCode::None && error != lastError_) mqtt_.publishError(error, millis());
    if (state == AGV_COMPLETED) mqtt_.publishEvent("AGV_TASK_COMPLETED", control_.currentNode(), millis(), control_.task().id);
    lastState_ = state;
    lastError_ = error;
    publishTelemetry();
}
void AGVController::publishTelemetry() {
    TelemetrySnapshot snapshot;
    const uint32_t currentSession = mqtt_.session();
    snapshot.state = navigator_.state() != AGV_IDLE ? navigator_.state() : control_.state();
    if(lift_.busy()) snapshot.state=lift_.up() ? AGV_LOADING : AGV_UNLOADING;
    snapshot.error = navigator_.error() != ErrorCode::None ? navigator_.error() : control_.error();
    if (positionConfirmed_ && positionSession_ == currentSession)
        AGVMath::copyText(snapshot.position.node, sizeof(snapshot.position.node), control_.currentNode());
    if (navigator_.active())
        AGVMath::copyText(snapshot.nextNode, sizeof(snapshot.nextNode), navigator_.nextNode());
    else if (control_.task().active && control_.pathIndex() + 1 < control_.task().pathLength)
        AGVMath::copyText(snapshot.nextNode, sizeof(snapshot.nextNode),
                          control_.task().path[control_.pathIndex() + 1].node);
    AGVMath::copyText(snapshot.taskId, sizeof(snapshot.taskId), control_.task().id);
    snapshot.taskActive = navigator_.active() || control_.task().active || lift_.busy();
    snapshot.pathIndex = navigator_.active() ? navigator_.pathIndex() : control_.pathIndex();
    snapshot.pathLength = navigator_.active() ? navigator_.pathLength() : control_.task().pathLength;
    snapshot.motor = movement_.action();
    snapshot.linePattern = line_.readSensors().pattern;
    snapshot.leftDuty = motor_.leftDuty();
    snapshot.rightDuty = motor_.rightDuty();
    snapshot.wifi = wifi_.isConnected();
    snapshot.mqtt = mqtt_.isConnected();
    const auto& activeSonar = Config::REAR_ULTRASONIC_ECHO_PIN >= 0 ? sonars_.active() : sensor_;
    snapshot.obstacle = Config::ULTRASONIC_MOTION_GUARD && activeSonar.valid() &&
        activeSonar.readDistanceCm() <= (navigator_.obstaclePaused() ? Config::OBSTACLE_CLEAR_CM : Config::OBSTACLE_DISTANCE_CM);
    snapshot.ultrasonicCm = activeSonar.readDistanceCm();
    snapshot.frontUltrasonicCm = sensor_.readDistanceCm();
    snapshot.rearUltrasonicCm = Config::REAR_ULTRASONIC_ECHO_PIN >= 0 ? rearSensor_.readDistanceCm() : UNKNOWN_VALUE;
    snapshot.obstacleDirection = Config::REAR_ULTRASONIC_ECHO_PIN >= 0 && sonars_.rearActive() ? "rear" : "front";
    snapshot.batteryAdcMv = battery_.adcMillivolts();
    snapshot.batteryCalibrated = battery_.calibrated();
    snapshot.batteryVoltage = battery_.readVoltage();
    snapshot.batteryPercentage = battery_.getBatteryPercentage();
    snapshot.cargoStable = cargo_.stable();
    snapshot.cargoPresent = cargo_.present();
    snapshot.buzzerOn = AGVMath::reverseBuzzerRequired(movement_.action(), motor_.leftDuty(), motor_.rightDuty());
    snapshot.buzzerPinLevel = buzzer_.pinLevel();
    snapshot.uptimeMs = millis();
    if (positionConfirmed_ && positionSession_ == currentSession && lastRfidSeenMs_)
        snapshot.rfidAgeMs = static_cast<uint32_t>(snapshot.uptimeMs - lastRfidSeenMs_);
    snapshot.localSession = currentSession;
    snapshot.heading = heading_;
    snapshot.headingBayDefault = headingBayDefault_;
    snapshot.stationReady = Config::benchVoltageReady(snapshot.batteryVoltage) &&
        RoutePlanner::station(snapshot.position.node) &&
        snapshot.heading == RoutePlanner::bayHeading(snapshot.position.node) &&
        !snapshot.taskActive && snapshot.state == AGV_IDLE && snapshot.error == ErrorCode::None &&
        snapshot.motor == MotorAction::Stop && snapshot.leftDuty == 0 && snapshot.rightDuty == 0;
    snapshot.awaitingNodeOrder = navigator_.needsNodeOrder();
    snapshot.stationTurnRequired = navigator_.stationTurnRequired();
    snapshot.routeCmdId = navigator_.routeCommandId();
    snapshot.liftSupported=Config::SERVO_SINGLE_TEST && servo_.liftConfigured();
    snapshot.liftBusy=lift_.busy();snapshot.liftCmdId=lift_.cmdId();snapshot.liftState=lift_.state();
    snapshot.liftElapsedMs=lift_.elapsed(millis());
    snapshot.virtualCargoKnown=lift_.cargoKnown();snapshot.virtualCargo=lift_.cargo();
    mqtt_.publishStatus(snapshot);
    lastTelemetryMs_ = snapshot.uptimeMs;
}
