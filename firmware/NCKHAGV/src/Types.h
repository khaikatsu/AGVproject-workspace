#pragma once

#include "Config.h"
#include <cstdint>
#include <limits>

constexpr size_t NODE_TEXT_SIZE = 8;
constexpr size_t UID_TEXT_SIZE = 21;
constexpr size_t COMMAND_ID_SIZE = 48;
constexpr float UNKNOWN_VALUE = std::numeric_limits<float>::quiet_NaN();

enum AGVState { AGV_IDLE, AGV_MOVING, AGV_LOADING, AGV_UNLOADING, AGV_WAITING,
                AGV_COMPLETED, AGV_SAFE_STOP, AGV_ERROR };
enum class Departure { FollowLine, Left, Right, Straight, Backward };
enum class MotorAction { Stop, Forward, Backward, ReverseCorrectLeft, ReverseCorrectRight,
                         CorrectLeft, CorrectRight, PivotLeft, PivotRight,
                         GentleCorrectLeft, GentleCorrectRight };
enum class ServoAction { None, LiftUp, LiftDown };
enum class CommandType { Task, Start, Stop, EmergencyStop, Restart, CancelTask, LiftUp, LiftDown, NodeOrder };
enum class CoreCommandType { Goto, Wait, Stop, ChargeStart, ResetFault, NodeOrder, LiftUp, LiftDown, ConfirmHeading };
enum class RobotHeading { Unknown, North, East, South, West };
enum class ErrorCode { None, Configuration, WifiTimeout, MqttTimeout, EmergencyStop,
    Obstacle, SensorInvalid, LineLost, AmbiguousLine, UnknownTag, UnexpectedNode,
    TurnTimeout, TurnConfiguration, TurnInterrupted, ServoConfiguration, InvalidTask,
    TaskBusy, NoTask, NotConnected, NotAtRouteStart, RestartDenied, QueueOverflow,
    InvalidCommand, StaleCommand, CargoMissing, CargoLost, CargoSensorUnstable, CargoMismatch,
    NodeOrderMismatch, ServoInterrupted, MotionLocked, ChargerUnconfigured, MotionTimeout,
    BatteryNotReady, BatteryUnderVoltage, BatteryOverVoltage, BatteryCalibrationRequired,
    MotionDisabled, ReverseSteeringUnconfirmed };

// Core MQTT v0 command. localSession is the ESP32 connection generation, not
// the Core session string; it prevents a queued command crossing reconnects.
struct CoreCommand {
    CoreCommandType type = CoreCommandType::Stop;
    int32_t cmdId = 0;
    uint32_t localSession = 0, receivedAtMs = 0;
    char target[NODE_TEXT_SIZE]{};
    char path[Config::MAX_PATH_NODES][NODE_TEXT_SIZE]{};
    uint8_t pathLength = 0;
    bool passThrough[Config::MAX_PATH_NODES]{};
    int32_t routeCmdId = 0;
    char node[NODE_TEXT_SIZE]{}, nextNode[NODE_TEXT_SIZE]{}, maneuver[24]{};
    uint8_t pathIndex = 0, turnCrossings = 0;
    bool reverseAfterTurn = false;
    RobotHeading confirmedHeading = RobotHeading::Unknown;
    bool preserveLift=false;
    bool verifiedRecovery=false;
};
struct CoreGrant {
    char zone[NODE_TEXT_SIZE]{};
    int32_t grantId = 0;
    uint32_t localSession = 0;
};

struct PathStep { char node[NODE_TEXT_SIZE]{}; Departure departure = Departure::FollowLine; };
struct AGVTask {
    char id[COMMAND_ID_SIZE]{};
    char startNode[NODE_TEXT_SIZE]{};
    char targetNode[NODE_TEXT_SIZE]{};
    PathStep path[Config::MAX_PATH_NODES]{};
    uint8_t pathLength = 0;
    bool active = false;
};
struct AGVPosition { char node[NODE_TEXT_SIZE]{}; };
struct RFIDEvent { char uid[UID_TEXT_SIZE]{}; char node[NODE_TEXT_SIZE]{}; bool newNode = false; };
struct LineReadings {
    int left = 0, center = 0, right = 0;
    uint8_t pattern = 0;
    bool configured = false;
};
struct AGVCommand {
    CommandType type = CommandType::Stop;
    char id[COMMAND_ID_SIZE]{};
    uint32_t session = 0;
    uint32_t receivedAtMs = 0;
    AGVTask task{};
    char taskId[COMMAND_ID_SIZE]{};
    char node[NODE_TEXT_SIZE]{};
    char nextNode[NODE_TEXT_SIZE]{};
    uint8_t pathIndex = 0;
    Departure departure = Departure::FollowLine;
};
struct ControlInput {
    uint32_t now = 0;
    bool wifi = false, mqtt = false;
    bool motionConfigured = false, liftConfigured = false;
    bool distanceValid = false, obstacle = false, servoBusy = false;
    bool lineConfigured = false;
    uint8_t linePattern = 0;
    bool cargoStable = false, cargoPresent = false;
};
struct ControlOutput { MotorAction motor = MotorAction::Stop; ServoAction servo = ServoAction::None; };
struct SafetySettings {
    uint32_t wifiTimeoutMs = 0, mqttTimeoutMs = 0;
    uint32_t turnMinMs = 0, turnTimeoutMs = 0, straightDepartureMs = 0;
    uint32_t servoMoveMs = 0;
    bool requireNodeOrders = false, requireLiftCommands = false;
    bool reverseSteeringConfirmed = false;
};
struct TelemetrySnapshot {
    AGVState state = AGV_IDLE;
    ErrorCode error = ErrorCode::None;
    AGVPosition position{};
    char nextNode[NODE_TEXT_SIZE]{};
    char taskId[COMMAND_ID_SIZE]{};
    bool taskActive = false, wifi = false, mqtt = false, obstacle = false;
    bool batteryCalibrated = false;
    uint8_t pathIndex = 0, pathLength = 0, linePattern = 0;
    int leftDuty = 0, rightDuty = 0;
    MotorAction motor = MotorAction::Stop;
    float ultrasonicCm = UNKNOWN_VALUE;
    float frontUltrasonicCm = UNKNOWN_VALUE, rearUltrasonicCm = UNKNOWN_VALUE;
    const char* obstacleDirection = "front";
    uint32_t batteryAdcMv = 0, uptimeMs = 0, localSession = 0;
    uint32_t rfidAgeMs = std::numeric_limits<uint32_t>::max();
    float batteryVoltage = UNKNOWN_VALUE, batteryPercentage = UNKNOWN_VALUE;
    bool cargoStable = false, cargoPresent = false;
    bool buzzerOn = false;
    uint32_t cargoSampleSeq=0,cargoSampledAtMs=0,cargoStableMs=0;
    int cargoRawLevel = -1;
    int buzzerPinLevel = -1;
    RobotHeading heading = RobotHeading::Unknown;
    bool headingBayDefault = false;
    int32_t headingCommandId = 0;
    bool stationReady = false;
    bool awaitingNodeOrder = false, stationTurnRequired = false;
    int32_t routeCmdId = 0;
    bool liftSupported = false, liftBusy = false, virtualCargoKnown = false, virtualCargo = false;
    bool maintenanceReady=false;
    int32_t liftCmdId = 0;
    uint32_t liftElapsedMs = 0;
    const char* liftState = "IDLE";
};

// Odometry thường chỉ hiển thị. Proof dừng tùy chọn cần Core xác minh riêng;
// không tự cấp quyền chuyển động hoặc hoàn thành nhiệm vụ.
struct OdometrySnapshot {
    uint32_t localSession = 0, seq = 0, uptimeMs = 0;
    uint32_t leftTicks = 0, rightTicks = 0;
    char fromNode[NODE_TEXT_SIZE]{}, toNode[NODE_TEXT_SIZE]{};
    int32_t routeCmdId = 0;
    uint8_t pathIndex = 0;
    bool tracking = false, translating = false, reverse = false;
    float distanceCm = 0, speedCmS = 0;
    bool stopPositionValid = false;
    char stopFrom[NODE_TEXT_SIZE]{}, stopTo[NODE_TEXT_SIZE]{};
    int32_t stopRoute = 0;
    uint8_t stopIndex = 0;
    uint32_t stationaryMs = 0;
    float stopDistanceCm = 0;
};

const char* stateName(AGVState state);
const char* errorName(ErrorCode error);
const char* motorActionName(MotorAction action);
const char* headingName(RobotHeading heading);
RobotHeading parseHeading(const char* heading);
