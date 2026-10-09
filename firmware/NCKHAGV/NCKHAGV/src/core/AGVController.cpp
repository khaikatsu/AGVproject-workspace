#include "AGVController.h"
#include "RoutePlanner.h"
#include <cstring>

AGVController::AGVController() : line_(motor_), movement_(motor_, line_, buzzer_), mqtt_(wifi_),
    control_({Config::WIFI_TIMEOUT_MS, Config::MQTT_TIMEOUT_MS, Config::TURN_MIN_MS,
              Config::TURN_TIMEOUT_MS, Config::STRAIGHT_DEPARTURE_MS, Config::SERVO_MOVE_TIME_MS,
              true, true, Config::REVERSE_STEERING_CONFIRMED}) {}
void AGVController::begin() {
    buzzer_.begin();
    motor_.begin();
    movement_.begin();
    line_.begin();
    if (Config::ULTRASONIC_MOTION_GUARD) sensor_.begin();
    rfid_.begin();
    servo_.begin();
    battery_.begin();
    cargo_.begin();
    control_.begin(millis());
    if (!mqtt_.begin()) control_.fail(ErrorCode::Configuration);
    if (Config::DEBUG_ENABLED) {
        Serial.printf("[AGV] Core motion %s; mutex release requires verified exit RFID\n",
                      Config::ENABLE_CORE_MOTION ? "configured" : "locked");
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
    result.distanceValid = !Config::ULTRASONIC_MOTION_GUARD || sensor_.valid();
    result.obstacle = Config::ULTRASONIC_MOTION_GUARD && sensor_.isObstacleDetected();
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
        navigator_.resetGrants();
        clearance_.reset();
        control_.safeStop(ErrorCode::NotConnected);
        inhibited = true;
    }
    if (mqtt_.consumeQueueFault()) { navigator_.fault(ErrorCode::QueueOverflow); control_.safeStop(ErrorCode::QueueOverflow); inhibited = true; }
    if (mqtt_.consumeStorageFault()) { navigator_.fault(ErrorCode::Configuration); control_.safeStop(ErrorCode::Configuration); inhibited = true; }
    if (inhibited) movement_.stop();
    return inhibited;
}
void AGVController::handleCommand(const CoreCommand& command, bool inhibited) {
    movement_.stop();
    if (command.type == CoreCommandType::Stop || command.type == CoreCommandType::Wait) {
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
    if (command.type == CoreCommandType::ChargeStart) {
        // No measured charger or charging-done input exists in this firmware.
        if (navigator_.active()) navigator_.fault(ErrorCode::ChargerUnconfigured);
        control_.safeStop(ErrorCode::ChargerUnconfigured);
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
                                previousSegmentReverse_)) {
            control_.safeStop(ErrorCode::InvalidCommand);
            return;
        }
        if (Config::DEBUG_ENABLED) {
            for (uint8_t i = 0; i < route.count; ++i)
                Serial.printf("[ROUTE] %s -> %s: %s\n", i ? command.path[i - 1] : control_.currentNode(),
                              command.path[i], RoutePlanner::turnName(route.departure[i]));
        }
        const bool trialRoute = Config::ONE_ROBOT_TRIAL &&
            RoutePlanner::oneRobotTrialRoute(Config::AGV_ID, control_.currentNode(), command, route);
        bool reverseRequired = false;
        for (uint8_t i = 0; i < route.count; ++i)
            reverseRequired |= route.departure[i] == RouteTurn::Reverse;
        if (!Config::ENABLE_CORE_MOTION || (Config::ONE_ROBOT_TRIAL && !trialRoute) ||
            !Config::benchVoltageReady(battery_.readVoltage()) ||
            (!battery_.calibrated() && !trialRoute && !Config::TWO_ROBOT_BENCH_TRIAL) ||
            (reverseRequired && !Config::REVERSE_STEERING_CONFIRMED && !trialRoute) ||
            control_.state() == AGV_SAFE_STOP || control_.state() == AGV_ERROR) {
            control_.safeStop(ErrorCode::MotionLocked);
            return;
        }
        if (route.count && !navigator_.start(control_.currentNode(), command, route, millis(), trialRoute))
            control_.safeStop(ErrorCode::InvalidCommand);
        return;
    }
}
void AGVController::update() {
    // Do not latch a WiFi/MQTT timeout during initial association. Once the
    // first healthy Core heartbeat arrives, the normal watchdog stays active.
    if (!controlArmed_ && mqtt_.isConnected()) {
        control_.begin(millis());
        controlArmed_ = true;
    }
    if (!mqtt_.isConnected()) {
        positionConfirmed_ = false; previousNode_[0] = '\0';
        lastRfidSeenMs_ = 0;
        previousSegmentReverse_ = false;
        navigator_.resetGrants();
        clearance_.reset();
    }
    cargo_.update(millis());
    if (Config::ULTRASONIC_MOTION_GUARD) sensor_.update();
    battery_.update();
    servo_.update();
    bool inhibited = consumePriority();
    if (controlArmed_) control_.checkSafety(input());
    if (!navigator_.active() && control_.state() != AGV_MOVING) movement_.stop();
    if (rfid_.readCard()) {
        const RFIDEvent& event = rfid_.event();
        if (!event.node[0]) {
            if (navigator_.active()) movement_.stop();
            navigator_.reachedNode("", millis());
            positionConfirmed_ = false;
            lastRfidSeenMs_ = 0;
            previousNode_[0] = '\0';
            previousSegmentReverse_ = false;
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
                // RFID is the only position source. Stop before changing the
                // route state; ACK and elapsed time never imply arrival.
                const bool reverse = navigator_.segmentReverse() || control_.segmentReverse();
                const bool expected = priorConfirmed && positionConfirmed_ && navigator_.active() &&
                    std::strcmp(navigator_.nextNode(), event.node) == 0;
                if (navigator_.active()) movement_.stop();
                navigator_.reachedNode(event.node, millis());
                char fromNode[NODE_TEXT_SIZE]{};
                AGVMath::copyText(fromNode, sizeof(fromNode), control_.currentNode());
                if (expected && fromNode[0] && navigator_.error() == ErrorCode::None)
                    clearance_.reached(fromNode, event.node);
                if (priorConfirmed && fromNode[0] && std::strcmp(fromNode, event.node) != 0)
                    AGVMath::copyText(previousNode_, sizeof(previousNode_), fromNode);
                else previousNode_[0] = '\0';
                previousSegmentReverse_ = priorConfirmed && reverse && previousNode_[0];
                control_.reachedNode(event.node, millis());
                if (!navigator_.active() && control_.state() != AGV_MOVING) movement_.stop();
                mqtt_.publishEvent("AGV_REACHED_NODE", event.node, millis(), control_.task().id, fromNode, reverse);
                publishTelemetry();
            }
        }
    }
    if (Config::ULTRASONIC_MOTION_GUARD) sensor_.update();
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
    MutexClearance::Release release;
    while (clearance_.pending(release)) {
        if (!mqtt_.releaseZone(release.zone, release.grantId)) break;
        navigator_.revokeGrant(release.zone, release.grantId);
        clearance_.queued(release);
    }
    inhibited = consumePriority() || inhibited;
    if (inhibited) { navigator_.stop(); control_.pause(); }
    if (Config::ULTRASONIC_MOTION_GUARD) sensor_.update();
    const char* neededZone = navigator_.missingZone();
    if (neededZone && static_cast<uint32_t>(millis() - lastZoneRequestMs_) >= 1000) {
        if (mqtt_.requestZone(neededZone)) lastZoneRequestMs_ = millis();
    }
    const ControlOutput output = controlArmed_ ? control_.update(input()) : ControlOutput{};
    if (navigator_.active() && (control_.state() == AGV_SAFE_STOP || control_.state() == AGV_ERROR))
        navigator_.fault(control_.error() == ErrorCode::None ? ErrorCode::Configuration : control_.error());
    const MotorAction coreMotor = navigator_.active() ? navigator_.update(input()) : MotorAction::Stop;
    movement_.apply(navigator_.state() == AGV_SAFE_STOP ? MotorAction::Stop :
                    (navigator_.active() ? coreMotor : output.motor));
    if (output.servo != ServoAction::None) {
        movement_.stop();
        const bool moved = output.servo == ServoAction::LiftUp ? servo_.liftUp() : servo_.liftDown();
        if (!moved) control_.fail(ErrorCode::ServoConfiguration);
    }
    if (navigator_.state() == AGV_SAFE_STOP ||
        (!navigator_.active() && control_.state() != AGV_MOVING)) movement_.stop();
    const MotorAction motorAction = movement_.action();
    if (Config::DEBUG_ENABLED && Config::ONE_ROBOT_TRIAL && motorAction != lastMotorAction_ &&
        std::strcmp(control_.currentNode(), "GS1") == 0) {
        Serial.printf("[MOTOR] %s left=%d right=%d line=%u next=%s\n",
                      motorActionName(motorAction), motor_.leftDuty(), motor_.rightDuty(),
                      static_cast<unsigned int>(line_.readSensors().pattern), navigator_.nextNode());
    }
    lastMotorAction_ = motorAction;
    observeState();
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
    snapshot.error = navigator_.error() != ErrorCode::None ? navigator_.error() : control_.error();
    if (positionConfirmed_ && positionSession_ == currentSession)
        AGVMath::copyText(snapshot.position.node, sizeof(snapshot.position.node), control_.currentNode());
    if (navigator_.active())
        AGVMath::copyText(snapshot.nextNode, sizeof(snapshot.nextNode), navigator_.nextNode());
    else if (control_.task().active && control_.pathIndex() + 1 < control_.task().pathLength)
        AGVMath::copyText(snapshot.nextNode, sizeof(snapshot.nextNode),
                          control_.task().path[control_.pathIndex() + 1].node);
    AGVMath::copyText(snapshot.taskId, sizeof(snapshot.taskId), control_.task().id);
    snapshot.taskActive = navigator_.active() || control_.task().active;
    snapshot.pathIndex = navigator_.active() ? navigator_.pathIndex() : control_.pathIndex();
    snapshot.pathLength = navigator_.active() ? navigator_.pathLength() : control_.task().pathLength;
    snapshot.motor = movement_.action();
    snapshot.linePattern = line_.readSensors().pattern;
    snapshot.leftDuty = motor_.leftDuty();
    snapshot.rightDuty = motor_.rightDuty();
    snapshot.wifi = wifi_.isConnected();
    snapshot.mqtt = mqtt_.isConnected();
    snapshot.obstacle = sensor_.isObstacleDetected();
    snapshot.ultrasonicCm = sensor_.readDistanceCm();
    snapshot.batteryAdcMv = battery_.adcMillivolts();
    snapshot.batteryCalibrated = battery_.calibrated();
    snapshot.batteryVoltage = battery_.readVoltage();
    snapshot.batteryPercentage = battery_.getBatteryPercentage();
    snapshot.cargoStable = cargo_.stable();
    snapshot.cargoPresent = cargo_.present();
    snapshot.buzzerOn = AGVMath::reverseBuzzerRequired(movement_.action(), motor_.leftDuty(), motor_.rightDuty());
    snapshot.uptimeMs = millis();
    if (positionConfirmed_ && positionSession_ == currentSession && lastRfidSeenMs_)
        snapshot.rfidAgeMs = static_cast<uint32_t>(snapshot.uptimeMs - lastRfidSeenMs_);
    snapshot.localSession = currentSession;
    mqtt_.publishStatus(snapshot);
    lastTelemetryMs_ = snapshot.uptimeMs;
}
