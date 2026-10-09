#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Cấu hình mạng riêng cho từng xe được ưu tiên; bản chung cũ vẫn dùng làm fallback.
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1 && \
    defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
#error "Select exactly one robot profile"
#endif
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
#include "profiles/Robot01Config.h"
#if __has_include("LocalNetworkConfig01.h")
#include "LocalNetworkConfig01.h"
#elif __has_include("LocalNetworkConfig.h")
#include "LocalNetworkConfig.h"
#endif
#elif defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
#include "profiles/Robot02Config.h"
#if __has_include("LocalNetworkConfig02.h")
#include "LocalNetworkConfig02.h"
#elif __has_include("LocalNetworkConfig.h")
#include "LocalNetworkConfig.h"
#endif
#elif __has_include("LocalNetworkConfig.h")
#include "LocalNetworkConfig.h"
#endif
#ifndef AGV_ROBOT_ID
#define AGV_ROBOT_ID ""
#endif
#ifndef AGV_WIFI_SSID
#define AGV_WIFI_SSID ""
#endif
#ifndef AGV_WIFI_PASSWORD
#define AGV_WIFI_PASSWORD ""
#endif
#ifndef AGV_MQTT_HOST
#define AGV_MQTT_HOST ""
#endif
#ifndef AGV_MQTT_USER
#define AGV_MQTT_USER ""
#endif
#ifndef AGV_MQTT_PASSWORD
#define AGV_MQTT_PASSWORD ""
#endif
#ifndef AGV_MQTT_PORT
#define AGV_MQTT_PORT 1883
#endif
#ifndef AGV_MQTT_TLS
#define AGV_MQTT_TLS false
#endif
#ifndef AGV_MQTT_CA_PEM
#define AGV_MQTT_CA_PEM ""
#endif
#ifndef AGV_NTP_SERVER
#define AGV_NTP_SERVER "pool.ntp.org"
#endif

namespace Config {
// Official wiring. Do not change these pins without a hardware revision.
constexpr int LEFT_ENA = 14;
constexpr int LEFT_IN1 = 27;
constexpr int LEFT_IN2 = 26;
constexpr int RIGHT_ENB = 25;
constexpr int RIGHT_IN3 = 33;
constexpr int RIGHT_IN4 = 32;
constexpr int LINE_LEFT_PIN = 39;
constexpr int LINE_CENTER_PIN = 34;
constexpr int LINE_RIGHT_PIN = 35;
constexpr int ULTRASONIC_TRIG_PIN = 12;
constexpr int ULTRASONIC_ECHO_PIN = 13;
constexpr int BATTERY_ADC_PIN = 36;
constexpr int RFID_SCK_PIN = 18;
constexpr int RFID_MISO_PIN = 19;
constexpr int RFID_MOSI_PIN = 23;
constexpr int RFID_SS_PIN = 5;
constexpr int RFID_RST_PIN = 22;
constexpr int SERVO_1_PIN = 15;
constexpr int SERVO_2_PIN = 17;
constexpr int SERVO_3_PIN = 21;
// Both are ESP32 strapping pins; verify boot with the attached modules.
constexpr int BUZZER_PIN = 0;
constexpr int CARGO_IR_PIN = 2;
constexpr int BUZZER_ACTIVE_STATE = 0;
constexpr int CARGO_IR_ACTIVE_STATE = 1;
constexpr uint32_t CARGO_IR_STABLE_MS = 150;
constexpr uint32_t REVERSE_STOP_MS = 150;
// Front-mounted line sensors have not been validated for backward steering.
constexpr bool REVERSE_STEERING_CONFIRMED = false;
// Explicit, supervised one-robot route trial. The ordinary esp32dev environment
// stays locked. The second robot must be physically absent from the route.
#if defined(AGV_ONE_ROBOT_TRIAL) && AGV_ONE_ROBOT_TRIAL == 1
constexpr bool ONE_ROBOT_TRIAL = true;
#else
constexpr bool ONE_ROBOT_TRIAL = false;
#endif
#if defined(AGV_TWO_ROBOT_BENCH_TRIAL) && AGV_TWO_ROBOT_BENCH_TRIAL == 1
constexpr bool TWO_ROBOT_BENCH_TRIAL = true;
#else
constexpr bool TWO_ROBOT_BENCH_TRIAL = false;
#endif
#if defined(AGV_BENCH_MOTOR_PROBE) && AGV_BENCH_MOTOR_PROBE == 1
constexpr bool BENCH_MOTOR_PROBE = true;
#else
constexpr bool BENCH_MOTOR_PROBE = false;
#endif
static_assert(!(ONE_ROBOT_TRIAL && TWO_ROBOT_BENCH_TRIAL), "Trial modes cannot be combined");
static_assert(!BENCH_MOTOR_PROBE || TWO_ROBOT_BENCH_TRIAL, "Motor probe requires the supervised bench mode");
#if (defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1) || \
    (defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1)
static_assert(TWO_ROBOT_BENCH_TRIAL, "Robot profile requires the supervised two-robot bench build");
#endif
#if defined(AGV_ROBOT_2_BENCH) && AGV_ROBOT_2_BENCH == 1
static_assert(TWO_ROBOT_BENCH_TRIAL, "Robot 02 bench ID requires the bench build");
#endif
#if defined(AGV_ROBOT_2_MONITOR) && AGV_ROBOT_2_MONITOR == 1
static_assert(!ONE_ROBOT_TRIAL && !TWO_ROBOT_BENCH_TRIAL,
              "Robot 02 monitor must keep motion locked");
#endif
constexpr bool ENABLE_CORE_MOTION = ONE_ROBOT_TRIAL || TWO_ROBOT_BENCH_TRIAL;
// Temporary single-robot trial without the ultrasonic module. Restore this
// guard after the HC-SR04 is wired and verified on the bench.
// The dual-robot bench build also has no ultrasonic and must remain on stands.
constexpr bool ULTRASONIC_MOTION_GUARD = !ENABLE_CORE_MOTION;
// Provisional ONE-ROBOT bench figure from the operator, 04/10/2026.
// This is not a permission to release a mutex: the map scale and
// swept rear corner have not been verified on the physical vehicle.
constexpr float MUTEX_EXIT_TRIAL_CM = 28.0f;
constexpr float ROBOT_LENGTH_ESTIMATE_CM = 18.0f;
constexpr float ROBOT_WIDTH_ESTIMATE_CM = 12.0f;
// Operator measured reader-to-rear distance on 09/10/2026; still not used
// to auto-release a zone without the RFID read-ahead/turning clearance check.
constexpr float RFID_TO_REAR_ESTIMATE_CM = 10.0f;

enum class MotorDriver { Unconfigured, L298N, BTS7960 };
// Operator confirms ESP32 DevKit V1 38-pin and L298N wiring. Pivot polarity
// still needs a wheels-lifted check before motor motion is enabled.
constexpr MotorDriver MOTOR_DRIVER = MotorDriver::L298N;
constexpr bool MOTOR_DIRECTION_CONFIRMED = true;
// Match the standalone W/S test sketch: HIGH/LOW on both bridges is forward.
constexpr bool LEFT_FORWARD_INVERTED = false;
constexpr bool RIGHT_FORWARD_INVERTED = false;
#ifndef AGV_BENCH_MOTOR_SPEED
#define AGV_BENCH_MOTOR_SPEED 120
#endif
constexpr int BENCH_MOTOR_SPEED = AGV_BENCH_MOTOR_SPEED;
constexpr int DEFAULT_MOTOR_SPEED = BENCH_MOTOR_PROBE ? 255 :
    (TWO_ROBOT_BENCH_TRIAL ? BENCH_MOTOR_SPEED : (ONE_ROBOT_TRIAL ? 220 : 210));
#if defined(AGV_PROFILE_ID)
// Các giá trị bench đến từ đúng Robot01Config.h hoặc Robot02Config.h.
constexpr int LEFT_SPEED_TRIM = AGV_BENCH_LEFT_SPEED_TRIM;
constexpr int RIGHT_SPEED_TRIM = AGV_BENCH_RIGHT_SPEED_TRIM;
constexpr int TURN_SPEED = AGV_BENCH_TURN_SPEED;
constexpr int TURN_LEFT_SPEED_TRIM = AGV_BENCH_TURN_LEFT_SPEED_TRIM;
constexpr int TURN_RIGHT_SPEED_TRIM = AGV_BENCH_TURN_RIGHT_SPEED_TRIM;
constexpr int CORRECTION_SPEED = AGV_BENCH_CORRECTION_SPEED;
constexpr int GENTLE_CORRECTION_SPEED = AGV_BENCH_GENTLE_CORRECTION_SPEED;
#else
// Bản cũ và motor probe giữ thông số trước khi tách profile.
constexpr int LEFT_SPEED_TRIM = BENCH_MOTOR_PROBE ? 0 : (ONE_ROBOT_TRIAL ? -30 : -40);
constexpr int RIGHT_SPEED_TRIM = 0;
// Slow the trial pivot after robot 01 overshot the line at GS1.
// Straight-line trims remain independent of pivot duty.
constexpr int TURN_SPEED = ONE_ROBOT_TRIAL ? 200 : DEFAULT_MOTOR_SPEED;
constexpr int TURN_LEFT_SPEED_TRIM = ENABLE_CORE_MOTION ? 0 : LEFT_SPEED_TRIM;
constexpr int TURN_RIGHT_SPEED_TRIM = RIGHT_SPEED_TRIM;
constexpr int CORRECTION_SPEED = DEFAULT_MOTOR_SPEED;
// With center plus one side sensor active, keep both wheels moving to avoid oversteering.
constexpr int GENTLE_CORRECTION_SPEED = ONE_ROBOT_TRIAL ? 140 : DEFAULT_MOTOR_SPEED;
#endif
constexpr int BASE_SPEED = DEFAULT_MOTOR_SPEED;
constexpr uint32_t MOTOR_PWM_HZ = 5000;
constexpr int MOTOR_PWM_BITS = 8;
constexpr std::array<int, 4> MOTOR_PWM_CHANNELS = {0, 1, 2, 3};

// Black line = 1; centered readings 010, 101 and 111 are valid.
constexpr int LINE_SENSOR_ACTIVE_STATE = 1;
// Stop immediately on 000, but confirm it persists before latching a trial fault.
#if defined(AGV_PROFILE_ID)
constexpr uint32_t LINE_LOSS_CONFIRM_MS = AGV_BENCH_LINE_LOSS_CONFIRM_MS;
#else
constexpr uint32_t LINE_LOSS_CONFIRM_MS = ONE_ROBOT_TRIAL ? 120 : 0;
#endif
// Operator requests stop at or below 15 cm; verify on the physical sensor.
constexpr float OBSTACLE_DISTANCE_CM = 15.0f;
// Bounded software trial, not a measured turn time. Pivot exits when a line
// is reacquired after leaving the original line; bench calibration is pending.
constexpr uint32_t TURN_MIN_MS = 100;
constexpr uint32_t TURN_TIMEOUT_MS = 2500;
// Software fail-safe only; measure actual route time before enabling PWM.
#if defined(AGV_PROFILE_ID)
constexpr uint32_t SEGMENT_TIMEOUT_MS = AGV_BENCH_SEGMENT_TIMEOUT_MS;
#else
constexpr uint32_t SEGMENT_TIMEOUT_MS = BENCH_MOTOR_PROBE ? 2000 : 15000;
#endif
// The 200 ms trial pause allowed the pivot to begin before the vehicle had
// visibly settled on the RFID. Keep the normal mode unchanged.
#if defined(AGV_PROFILE_ID)
constexpr uint32_t RFID_STOP_DWELL_MS = AGV_BENCH_RFID_STOP_DWELL_MS;
#else
constexpr uint32_t RFID_STOP_DWELL_MS = ENABLE_CORE_MOTION ? 1000 : 200;
#endif
constexpr uint32_t STRAIGHT_DEPARTURE_MS = 0;
constexpr uint32_t ULTRASONIC_INTERVAL_MS = 60;
constexpr uint32_t ULTRASONIC_ECHO_TIMEOUT_US = 25000;
constexpr uint32_t ULTRASONIC_STALE_MS = 200;
constexpr uint32_t ULTRASONIC_TRIGGER_US = 10;
constexpr float ULTRASONIC_SOUND_CM_PER_US = 0.0343f;

// Mechanical wheel diameter only. The firmware has no encoder/odometry input.
constexpr float WHEEL_DIAMETER_CM = 6.5f;
constexpr float LINE_WIDTH_CM = 4.5f;
constexpr float FRONT_LINE_SENSOR_TO_AXLE_CM = 5.0f;
constexpr float REAR_LINE_SENSOR_TO_AXLE_CM = 10.0f;

struct NodeMapping { const char* node; const char* uid; };
// TODO: USER CONFIGURATION: uppercase UID hex, no spaces/colons, never invent UIDs.
constexpr std::array<NodeMapping, 26> RFID_NODES = {{
    {"C1", "B7BC2A95"},
    {"C2", "B771B795"},
    {"C3", "A79A0695"},
    {"C4", "575A1995"},
    {"M1", "579E8495"},
    {"M2", "072F2495"},
    {"M3", "27CE0C95"},
    {"M4", "D78A3395"},
    {"M5", "17693195"},
    {"M6", "F7354695"},
    {"G1", "37F61F95"},
    {"G2", "B7A5BD95"},
    {"G3", "A7520C95"},
    {"G4", "27582F95"},
    {"G5", "E7CDB995"},
    {"G6", "37869F95"},
    {"K1", "57D42295"},
    {"K2", "C3008837"},
    {"K3", "9746BB95"},
    {"K4", "27EB4095"},
    {"K5", "A75E1595"},
    {"K6", "47683395"},
    {"GS1", "035F2539"},
    {"GS2", "97770296"},
    {"S1", "77B04295"},
    {"S2", "D7E21F95"}
}};
constexpr uint32_t RFID_POLL_INTERVAL_MS = 30;

constexpr std::array<int, 3> SERVO_PINS = {SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN};
// Channels 4/5/6 use LEDC timers 2/3; motor channels 0..3 use timers 0/1.
constexpr std::array<int, 3> SERVO_PWM_CHANNELS = {4, 5, 6};
constexpr uint32_t SERVO_PWM_HZ = 50;
constexpr int SERVO_PWM_BITS = 16;
// TODO: USER CONFIGURATION: enable only verified servos and assign roles (0..2).
constexpr std::array<bool, 3> SERVO_ENABLED = {false, false, false};
constexpr int SERVO_SCAN = 0;
constexpr int SERVO_LIFT = 1;
constexpr int SERVO_OTHER = 2;
// SERVO_LIFT and SERVO_OTHER are the independently calibrated lift pair.
constexpr std::array<int, 2> SERVO_LIFT_UP_ANGLES = {-1, -1};
constexpr std::array<int, 2> SERVO_LIFT_DOWN_ANGLES = {-1, -1};
// TODO: USER CONFIGURATION: verified pulse limits, mechanical angles and settling.
constexpr int SERVO_MIN_PULSE_US = 0;
constexpr int SERVO_MAX_PULSE_US = 0;
constexpr uint32_t SERVO_MOVE_TIME_MS = 0;

// Initial 2S 18650 estimate from main (1).cpp: 30k/7.5k divider, 6.4-8.4V.
// Confirm the divider output and calibrate against a multimeter on the robot.
#if defined(AGV_PROFILE_ID)
constexpr float BATTERY_VOLTAGE_SCALE = AGV_BENCH_BATTERY_VOLTAGE_SCALE;
constexpr float BATTERY_VOLTAGE_OFFSET = AGV_BENCH_BATTERY_VOLTAGE_OFFSET;
#else
constexpr float BATTERY_VOLTAGE_SCALE = 5.0f;
constexpr float BATTERY_VOLTAGE_OFFSET = 0.0f;
#endif
constexpr float BATTERY_EMPTY_VOLTAGE = 6.4f;
constexpr float BATTERY_FULL_VOLTAGE = 8.4f;
// The bench build only moves when the ADC-derived 2S pack voltage is plausible.
// This is a software interlock; the divider still needs a multimeter check.
constexpr float BENCH_MAX_VOLTAGE = 8.6f;
inline bool benchVoltageReady(float voltage) {
    return !TWO_ROBOT_BENCH_TRIAL ||
           (voltage >= BATTERY_EMPTY_VOLTAGE && voltage <= BENCH_MAX_VOLTAGE);
}
constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 500;

// Runtime MQTT topics are built from the robot ID using the Core v0 contract.
#if defined(AGV_PROFILE_ID)
constexpr const char* AGV_ID = AGV_PROFILE_ID;
#elif (defined(AGV_ROBOT_2_MONITOR) && AGV_ROBOT_2_MONITOR == 1) || \
    (defined(AGV_ROBOT_2_BENCH) && AGV_ROBOT_2_BENCH == 1)
constexpr const char* AGV_ID = "02";
#else
constexpr const char* AGV_ID = AGV_ROBOT_ID;
#endif
constexpr const char* WIFI_SSID = AGV_WIFI_SSID;
constexpr const char* WIFI_PASSWORD = AGV_WIFI_PASSWORD;
constexpr const char* MQTT_SERVER = AGV_MQTT_HOST;
constexpr uint16_t MQTT_PORT = AGV_MQTT_PORT;
constexpr const char* MQTT_USERNAME = AGV_MQTT_USER;
constexpr const char* MQTT_PASSWORD = AGV_MQTT_PASSWORD;
constexpr bool MQTT_USE_TLS = AGV_MQTT_TLS;
constexpr const char* MQTT_CA_PEM = AGV_MQTT_CA_PEM;
constexpr const char* NTP_SERVER = AGV_NTP_SERVER;
constexpr const char* MQTT_HEARTBEAT_TOPIC = "agv/server/heartbeat";
constexpr uint32_t CORE_HEARTBEAT_TIMEOUT_MS = 3000;
constexpr uint32_t WIFI_TIMEOUT_MS = 3000;
constexpr uint32_t MQTT_TIMEOUT_MS = 3000;

// Software limits/tuning, not measured hardware calibration.
constexpr uint32_t WIFI_RETRY_MS = 5000;
constexpr uint32_t MQTT_RETRY_MS = 3000;
constexpr uint16_t MQTT_SOCKET_TIMEOUT_SECONDS = 1;
constexpr uint16_t MQTT_KEEPALIVE_SECONDS = 5;
constexpr uint32_t NETWORK_WORKER_STALE_MS = 2000;
constexpr uint32_t COMMAND_MAX_AGE_MS = 2000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr uint32_t NETWORK_TASK_STACK_BYTES = 12288;
constexpr size_t MAX_PATH_NODES = 32;
constexpr size_t MAX_COMMAND_BYTES = 1024;
constexpr size_t MAX_PUBLISH_BYTES = 1536;
constexpr size_t COMMAND_QUEUE_LENGTH = 8;
constexpr size_t EVENT_QUEUE_LENGTH = 16;
constexpr size_t RECENT_COMMAND_IDS = 16;
constexpr bool DEBUG_ENABLED = true;
constexpr uint32_t SERIAL_BAUD = 115200;

constexpr std::array<int, 22> ALL_PINS = {
    LEFT_ENA, LEFT_IN1, LEFT_IN2, RIGHT_ENB, RIGHT_IN3, RIGHT_IN4,
    LINE_LEFT_PIN, LINE_CENTER_PIN, LINE_RIGHT_PIN,
    ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN, BATTERY_ADC_PIN,
    RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN, RFID_RST_PIN,
    SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN,
    BUZZER_PIN, CARGO_IR_PIN
};
constexpr bool pinsUnique() {
    for (size_t i = 0; i < ALL_PINS.size(); ++i)
        for (size_t j = i + 1; j < ALL_PINS.size(); ++j)
            if (ALL_PINS[i] == ALL_PINS[j]) return false;
    return true;
}
static_assert(pinsUnique(), "GPIO collision");
static_assert(LINE_LEFT_PIN >= 34 && LINE_CENTER_PIN >= 34 && LINE_RIGHT_PIN >= 34, "Line inputs must remain input-only pins");
static_assert(BATTERY_ADC_PIN == 36, "Battery must remain on ADC1 GPIO36");
static_assert(BUZZER_ACTIVE_STATE == 0 && CARGO_IR_ACTIVE_STATE == 1, "Unexpected buzzer/IR polarity");
static_assert(CARGO_IR_STABLE_MS > 0 && REVERSE_STOP_MS > 0, "Safety debounce/hold must be nonzero");
static_assert(BASE_SPEED >= 0 && BASE_SPEED <= 255 && TURN_SPEED >= 0 && TURN_SPEED <= 255 &&
              BASE_SPEED + LEFT_SPEED_TRIM > 0 && BASE_SPEED + LEFT_SPEED_TRIM <= 255 &&
              BASE_SPEED + RIGHT_SPEED_TRIM > 0 && BASE_SPEED + RIGHT_SPEED_TRIM <= 255 &&
              TURN_SPEED + TURN_LEFT_SPEED_TRIM > 0 && TURN_SPEED + TURN_LEFT_SPEED_TRIM <= 255 &&
              TURN_SPEED + TURN_RIGHT_SPEED_TRIM > 0 && TURN_SPEED + TURN_RIGHT_SPEED_TRIM <= 255 &&
              CORRECTION_SPEED >= 0 && CORRECTION_SPEED <= 255 &&
              CORRECTION_SPEED + LEFT_SPEED_TRIM > 0 && CORRECTION_SPEED + LEFT_SPEED_TRIM <= 255 &&
              CORRECTION_SPEED + RIGHT_SPEED_TRIM > 0 && CORRECTION_SPEED + RIGHT_SPEED_TRIM <= 255 &&
              GENTLE_CORRECTION_SPEED + LEFT_SPEED_TRIM > 0 &&
              GENTLE_CORRECTION_SPEED + LEFT_SPEED_TRIM <= 255 &&
              GENTLE_CORRECTION_SPEED + RIGHT_SPEED_TRIM > 0 &&
              GENTLE_CORRECTION_SPEED + RIGHT_SPEED_TRIM <= 255 &&
              (!ONE_ROBOT_TRIAL || GENTLE_CORRECTION_SPEED < CORRECTION_SPEED), "Invalid PWM duty");
}
