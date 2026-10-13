#pragma once
#include "sensors/BatteryModule.h"
#include "sensors/RFIDModule.h"
#include "sensors/SensorModule.h"
#include "actuators/ServoModule.h"
#include "motion/MovementModule.h"
#include "network/MQTTModule.h"
#include "ControlLogic.h"
#include "CoreNavigator.h"
#include "MutexClearance.h"
#include "sensors/CargoModule.h"
#include "actuators/BuzzerModule.h"

class AGVController {
public:
    AGVController();
    void begin();
    void update();
    AGVState state() const { return control_.state(); }
private:
    ControlInput input() const;
    bool consumePriority();
    void handleCommand(const CoreCommand& command, bool inhibited);
    void publishTelemetry();
    void observeState();
    MotorModule motor_;
    SensorModule sensor_;
    RFIDModule rfid_;
    ServoModule servo_;
    LineFollowModule line_;
    BuzzerModule buzzer_;
    MovementModule movement_;
    BatteryModule battery_;
    CargoModule cargo_;
    WiFiModule wifi_;
    MQTTModule mqtt_;
    ControlLogic control_;
    CoreNavigator navigator_;
    MutexClearance clearance_;
    uint32_t lastTelemetryMs_ = 0;
    uint32_t lastZoneRequestMs_ = 0;
    uint32_t lastBenchPrintMs_ = 0;
    MotorAction lastMotorAction_ = MotorAction::Stop;
    AGVState lastState_ = AGV_IDLE;
    ErrorCode lastError_ = ErrorCode::None;
    bool pickupIrReported_ = false;
    bool controlArmed_ = false;
    bool positionConfirmed_ = false;
    uint32_t positionSession_ = 0;
    uint32_t lastRfidSeenMs_ = 0;
    char previousNode_[NODE_TEXT_SIZE]{};
    bool previousSegmentReverse_ = false;
    RobotHeading heading_ = RobotHeading::Unknown;
    bool headingBayDefault_ = false;
};
