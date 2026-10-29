#pragma once

#include <array>
#include <cmath>
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
#if defined(AGV_REAR_SONAR_ENABLED) && AGV_REAR_SONAR_ENABLED == 1
constexpr int REAR_ULTRASONIC_TRIG_PIN = 0;
constexpr int REAR_ULTRASONIC_ECHO_PIN = 15;
#else
constexpr int REAR_ULTRASONIC_TRIG_PIN = -1;
constexpr int REAR_ULTRASONIC_ECHO_PIN = -1;
#endif
constexpr int BATTERY_ADC_PIN = 36;
constexpr int RFID_SCK_PIN = 18;
constexpr int RFID_MISO_PIN = 19;
constexpr int RFID_MOSI_PIN = 23;
constexpr int RFID_SS_PIN = 5;
constexpr int RFID_RST_PIN = 22;
constexpr int SERVO_1_PIN = REAR_ULTRASONIC_ECHO_PIN < 0 ? 15 : -1;
constexpr int SERVO_2_PIN = 17;
constexpr int SERVO_3_PIN = REAR_ULTRASONIC_ECHO_PIN < 0 ? 21 : -1;
// GPIO0 sonar sau (xe 01) và GPIO2 IR là boot strapping; thử boot khi đã nối thiết bị.
constexpr int BUZZER_PIN = REAR_ULTRASONIC_ECHO_PIN < 0 ? 0 : 21;
constexpr int CARGO_IR_PIN = 2;
constexpr int BUZZER_ACTIVE_STATE = 0;
// Flying Fish IR sensor (LM393): Active-LOW (0 = co hang [2 LED], 1 = trong trai [1 LED])
constexpr int CARGO_IR_ACTIVE_STATE = 0;
constexpr uint32_t CARGO_IR_STABLE_MS = 150;
constexpr uint32_t REVERSE_STOP_MS = 150;
// Front-mounted line sensors have not been validated for backward steering.
constexpr bool REVERSE_STEERING_CONFIRMED = false;
#if defined(AGV_REVERSE_HOME_ENABLED) && AGV_REVERSE_HOME_ENABLED == 1
constexpr bool REVERSE_HOME_SUPPORTED = true;
#else
constexpr bool REVERSE_HOME_SUPPORTED = false;
#endif
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
// Xe 01 kích chốt hai sonar; các profile motion khác giữ chính sách cũ.
// Chốt phần mềm vẫn cần thử bàn và đo quãng phanh trước khi chạy trên sàn.
constexpr bool ULTRASONIC_MOTION_GUARD = !ENABLE_CORE_MOTION || REAR_ULTRASONIC_ECHO_PIN >= 0;
// Physical measurements confirmed by the operator on 17/10/2026 for both
// robots. The release origin is the first successful read of the mutex tag,
// which can occur 3 cm before its centre.
constexpr float ROBOT_FRONT_TO_RFID_CM = 7.5f;
constexpr float RFID_TO_REAR_CM = 9.5f;
constexpr float ROBOT_LENGTH_CM = ROBOT_FRONT_TO_RFID_CM + RFID_TO_REAR_CM;
constexpr float ROBOT_WIDTH_ESTIMATE_CM = 12.0f;
constexpr float RFID_READ_RADIUS_CM = 3.0f;
constexpr float MUTEX_TAG_TO_EXIT_BOUNDARY_CM = 3.0f;
constexpr float MUTEX_CLEARANCE_MARGIN_CM = 3.0f;
constexpr float MUTEX_RELEASE_FROM_FIRST_READ_CM = RFID_READ_RADIUS_CM +
    MUTEX_TAG_TO_EXIT_BOUNDARY_CM + RFID_TO_REAR_CM + MUTEX_CLEARANCE_MARGIN_CM;

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
#if defined(AGV_LINE_PD_ENABLED) && AGV_LINE_PD_ENABLED == 1
constexpr bool LINE_PD_ENABLED = true;
#else
constexpr bool LINE_PD_ENABLED = false;
#endif
#ifndef AGV_BENCH_LINE_KP
#define AGV_BENCH_LINE_KP 0.0f
#endif
#ifndef AGV_BENCH_LINE_KD
#define AGV_BENCH_LINE_KD 0.0f
#endif
#ifndef AGV_LINE_PD_BASE_SPEED
#define AGV_LINE_PD_BASE_SPEED 160
#endif
#ifndef AGV_LINE_PD_MIN_RUNNING_PWM
#define AGV_LINE_PD_MIN_RUNNING_PWM 0
#endif
#ifndef AGV_LINE_PD_MAX_RUNNING_PWM
#define AGV_LINE_PD_MAX_RUNNING_PWM 255
#endif
#ifndef AGV_LINE_PD_CONTROL_MS
#define AGV_LINE_PD_CONTROL_MS 10
#endif
#ifndef AGV_LINE_PD_RESET_GAP_MS
#define AGV_LINE_PD_RESET_GAP_MS 100
#endif
#ifndef AGV_LINE_PD_STEERING_SIGN
#define AGV_LINE_PD_STEERING_SIGN 1
#endif
constexpr float LINE_KP = AGV_BENCH_LINE_KP, LINE_KD = AGV_BENCH_LINE_KD;
constexpr int LINE_PD_BASE_SPEED = AGV_LINE_PD_BASE_SPEED;
constexpr int LINE_PD_MIN_RUNNING_PWM = AGV_LINE_PD_MIN_RUNNING_PWM;
constexpr int LINE_PD_MAX_RUNNING_PWM = AGV_LINE_PD_MAX_RUNNING_PWM;
constexpr int LINE_PD_STEERING_SIGN = AGV_LINE_PD_STEERING_SIGN;
constexpr uint32_t LINE_PD_CONTROL_MS = AGV_LINE_PD_CONTROL_MS;
constexpr uint32_t LINE_PD_RESET_GAP_MS = AGV_LINE_PD_RESET_GAP_MS;
static_assert(LINE_KP >= 0 && LINE_KP <= 100 && LINE_KD >= 0 && LINE_KD <= 100, "Invalid PD gains");
static_assert(LINE_PD_CONTROL_MS > 0 && LINE_PD_CONTROL_MS <= 100 &&
              LINE_PD_RESET_GAP_MS >= 2*LINE_PD_CONTROL_MS && LINE_PD_RESET_GAP_MS <= 500, "Invalid PD timing");
static_assert(LINE_PD_STEERING_SIGN == 1 || LINE_PD_STEERING_SIGN == -1, "Invalid PD steering sign");
static_assert(LINE_PD_BASE_SPEED > 0 && LINE_PD_BASE_SPEED <= 255 &&
              (!LINE_PD_ENABLED || (LINE_PD_BASE_SPEED+LEFT_SPEED_TRIM > 0 && LINE_PD_BASE_SPEED+LEFT_SPEED_TRIM <= 255 &&
               LINE_PD_BASE_SPEED+RIGHT_SPEED_TRIM > 0 && LINE_PD_BASE_SPEED+RIGHT_SPEED_TRIM <= 255)), "Invalid PD base PWM");
static_assert(LINE_PD_MIN_RUNNING_PWM >= 0 && LINE_PD_MIN_RUNNING_PWM <= 255 &&
              LINE_PD_MAX_RUNNING_PWM >= 0 && LINE_PD_MAX_RUNNING_PWM <= 255 &&
              LINE_PD_MIN_RUNNING_PWM <= LINE_PD_MAX_RUNNING_PWM,
              "Invalid PD running PWM limits");
static_assert(!LINE_PD_ENABLED ||
              (LINE_PD_BASE_SPEED+LEFT_SPEED_TRIM >= LINE_PD_MIN_RUNNING_PWM &&
               LINE_PD_BASE_SPEED+LEFT_SPEED_TRIM <= LINE_PD_MAX_RUNNING_PWM &&
               LINE_PD_BASE_SPEED+RIGHT_SPEED_TRIM >= LINE_PD_MIN_RUNNING_PWM &&
               LINE_PD_BASE_SPEED+RIGHT_SPEED_TRIM <= LINE_PD_MAX_RUNNING_PWM),
              "PD straight duty must remain inside running PWM limits");

#if defined(AGV_BENCH_GENTLE_OUTER_SPEED)
constexpr int GENTLE_OUTER_SPEED = AGV_BENCH_GENTLE_OUTER_SPEED;
#else
constexpr int GENTLE_OUTER_SPEED = CORRECTION_SPEED;
#endif
#if defined(AGV_FAST_LINE_RESPONSE) && AGV_FAST_LINE_RESPONSE == 1
constexpr bool FAST_LINE_RESPONSE = true;
#else
constexpr bool FAST_LINE_RESPONSE = false;
#endif
#if defined(AGV_GENTLE_FORWARD_CORRECTION) && AGV_GENTLE_FORWARD_CORRECTION == 1
constexpr bool GENTLE_FORWARD_CORRECTION = true;
#else
constexpr bool GENTLE_FORWARD_CORRECTION = false;
#endif
constexpr uint32_t MOTION_LOG_INTERVAL_MS = 200;
#if defined(AGV_REVERSE_SPEED)
constexpr int REVERSE_SPEED = AGV_REVERSE_SPEED;
#else
constexpr int REVERSE_SPEED = BASE_SPEED;
#endif
#if defined(AGV_REVERSE_CORRECTION_SPEED)
constexpr int REVERSE_CORRECTION_SPEED = AGV_REVERSE_CORRECTION_SPEED;
#else
constexpr int REVERSE_CORRECTION_SPEED = CORRECTION_SPEED;
#endif
static_assert(REVERSE_SPEED > 0 && REVERSE_SPEED <= 255, "Invalid reverse PWM");
static_assert(REVERSE_CORRECTION_SPEED > 0 && REVERSE_CORRECTION_SPEED <= 255, "Invalid reverse correction PWM");
#if defined(AGV_REVERSE_LEFT_SPEED_TRIM)
constexpr int REVERSE_LEFT_SPEED_TRIM = AGV_REVERSE_LEFT_SPEED_TRIM;
#else
constexpr int REVERSE_LEFT_SPEED_TRIM = LEFT_SPEED_TRIM;
#endif
#if defined(AGV_REVERSE_RIGHT_SPEED_TRIM)
constexpr int REVERSE_RIGHT_SPEED_TRIM = AGV_REVERSE_RIGHT_SPEED_TRIM;
#else
constexpr int REVERSE_RIGHT_SPEED_TRIM = RIGHT_SPEED_TRIM;
#endif
static_assert(REVERSE_SPEED + REVERSE_LEFT_SPEED_TRIM > 0 && REVERSE_SPEED + REVERSE_LEFT_SPEED_TRIM <= 255 &&
              REVERSE_SPEED + REVERSE_RIGHT_SPEED_TRIM > 0 && REVERSE_SPEED + REVERSE_RIGHT_SPEED_TRIM <= 255 &&
              REVERSE_CORRECTION_SPEED + REVERSE_LEFT_SPEED_TRIM > 0 && REVERSE_CORRECTION_SPEED + REVERSE_LEFT_SPEED_TRIM <= 255 &&
              REVERSE_CORRECTION_SPEED + REVERSE_RIGHT_SPEED_TRIM > 0 && REVERSE_CORRECTION_SPEED + REVERSE_RIGHT_SPEED_TRIM <= 255,
              "Invalid reverse wheel PWM");
#if defined(AGV_REVERSE_GENTLE_CORRECTION) && AGV_REVERSE_GENTLE_CORRECTION == 1
constexpr bool REVERSE_GENTLE_CORRECTION = true;
constexpr int REVERSE_CORRECTION_DELTA = AGV_REVERSE_CORRECTION_DELTA;
#else
constexpr bool REVERSE_GENTLE_CORRECTION = false;
constexpr int REVERSE_CORRECTION_DELTA = 0;
#endif
static_assert(!REVERSE_GENTLE_CORRECTION ||
              (REVERSE_CORRECTION_DELTA > 0 &&
               REVERSE_CORRECTION_SPEED + REVERSE_LEFT_SPEED_TRIM - REVERSE_CORRECTION_DELTA > 0 &&
               REVERSE_CORRECTION_SPEED + REVERSE_RIGHT_SPEED_TRIM - REVERSE_CORRECTION_DELTA > 0),
              "Gentle reverse correction must keep both wheels powered");

#if defined(AGV_STARTUP_BOOST_PWM)
constexpr int STARTUP_BOOST_PWM = AGV_STARTUP_BOOST_PWM;
#else
constexpr int STARTUP_BOOST_PWM = 0;
#endif
static_assert(STARTUP_BOOST_PWM >= 0 && STARTUP_BOOST_PWM <= 255, "Invalid startup boost PWM");

#if defined(AGV_STARTUP_BOOST_DELAY_MS)
constexpr uint32_t STARTUP_BOOST_DELAY_MS = AGV_STARTUP_BOOST_DELAY_MS;
#else
constexpr uint32_t STARTUP_BOOST_DELAY_MS = 150;
#endif

#if defined(AGV_STARTUP_BOOST_STEP_PWM)
constexpr int STARTUP_BOOST_STEP_PWM = AGV_STARTUP_BOOST_STEP_PWM;
#else
constexpr int STARTUP_BOOST_STEP_PWM = 8;
#endif

#if defined(AGV_STARTUP_BOOST_INTERVAL_MS)
constexpr uint32_t STARTUP_BOOST_INTERVAL_MS = AGV_STARTUP_BOOST_INTERVAL_MS;
#else
constexpr uint32_t STARTUP_BOOST_INTERVAL_MS = 60;
#endif

#if defined(AGV_STARTUP_BOOST_MAX_PWM)
constexpr int STARTUP_BOOST_MAX_PWM = AGV_STARTUP_BOOST_MAX_PWM;
#else
constexpr int STARTUP_BOOST_MAX_PWM = 180;
#endif

constexpr uint32_t MOTOR_PWM_HZ = 5000;
constexpr int MOTOR_PWM_BITS = 8;
constexpr std::array<int, 4> MOTOR_PWM_CHANNELS = {0, 1, 2, 3};

// Black line = 1; centered readings 010, 101 and 111 are valid.
constexpr int LINE_SENSOR_ACTIVE_STATE = 1;
// Stop immediately on 000, but confirm it persists before latching a fault.
#if defined(AGV_PROFILE_ID)
constexpr uint32_t LINE_LOSS_CONFIRM_MS = AGV_BENCH_LINE_LOSS_CONFIRM_MS;
#else
constexpr uint32_t LINE_LOSS_CONFIRM_MS = ONE_ROBOT_TRIAL ? 120 : 0;
#endif
#if defined(AGV_LINE_RECOVERY_ENABLED) && AGV_LINE_RECOVERY_ENABLED == 1
constexpr bool LINE_RECOVERY_ENABLED = true;
#else
constexpr bool LINE_RECOVERY_ENABLED = false;
#endif
// Provisional bench limits: pulse a pivot, never translate with a blank line.
// Time limits are not calibrated angular limits.
constexpr int LINE_RECOVERY_PWM = 180;
constexpr uint32_t LINE_RECOVERY_STOP_MS = 100;
constexpr uint32_t LINE_RECOVERY_TIMEOUT_MS = 600;
constexpr uint32_t LINE_RECOVERY_PULSE_MS = 40;
constexpr uint32_t LINE_RECOVERY_REST_MS = 60;
constexpr uint32_t LINE_RECOVERY_CENTER_MS = 50;
constexpr uint32_t LINE_RECOVERY_MEMORY_MS = 750;
#if defined(AGV_LINE_RECOVERY_SIDE_CONFIRM) && AGV_LINE_RECOVERY_SIDE_CONFIRM == 1
constexpr bool LINE_RECOVERY_SIDE_CONFIRM = true;
#else
constexpr bool LINE_RECOVERY_SIDE_CONFIRM = false;
#endif
constexpr uint32_t LINE_RECOVERY_SAMPLE_GAP_MS = 50;
#if defined(AGV_LINE_RECOVERY_TRACK_CONFIRM) && AGV_LINE_RECOVERY_TRACK_CONFIRM == 1
constexpr bool LINE_RECOVERY_TRACK_CONFIRM = true;
#else
constexpr bool LINE_RECOVERY_TRACK_CONFIRM = false;
#endif
// Waiting longer never extends the 600ms motor-search budget.
constexpr uint32_t TURN_CAPTURE_CONFIRM_TIMEOUT_MS = LINE_RECOVERY_TRACK_CONFIRM ? 1500 : LINE_RECOVERY_TIMEOUT_MS;
static_assert(TURN_CAPTURE_CONFIRM_TIMEOUT_MS >= LINE_RECOVERY_TIMEOUT_MS,
              "Turn confirmation must include the bounded motor-search budget");
#if defined(AGV_LINE_LOSS_GRACE_MS)
constexpr uint32_t LINE_LOSS_GRACE_MS = AGV_LINE_LOSS_GRACE_MS;
#else
constexpr uint32_t LINE_LOSS_GRACE_MS = 0; // Keep legacy per-phase limits.
#endif
static_assert(LINE_LOSS_GRACE_MS == 0 ||
              (LINE_RECOVERY_ENABLED && LINE_LOSS_GRACE_MS >= LINE_RECOVERY_TIMEOUT_MS &&
               LINE_LOSS_GRACE_MS > LINE_RECOVERY_CENTER_MS),
              "Line-loss grace must include the bounded search and confirmation");
static_assert(LINE_RECOVERY_PWM > 0 && LINE_RECOVERY_PWM <= 255 &&
              (!LINE_RECOVERY_ENABLED || LINE_RECOVERY_PWM <= TURN_SPEED) &&
              LINE_RECOVERY_TIMEOUT_MS > LINE_RECOVERY_STOP_MS + LINE_RECOVERY_CENTER_MS &&
              LINE_RECOVERY_PULSE_MS > 0 && LINE_RECOVERY_REST_MS > 0,
              "Invalid line recovery limits");

// ----------------------------------------------------------------------------
// Phục hồi khi xe văng khỏi line ở khúc cua (Corner Lost-Line Recovery)
// ----------------------------------------------------------------------------
#if defined(AGV_CORNER_RECOVERY_ENABLED)
constexpr bool CORNER_RECOVERY_ENABLED = AGV_CORNER_RECOVERY_ENABLED == 1;
#else
constexpr bool CORNER_RECOVERY_ENABLED = true;
#endif

#if defined(AGV_CORNER_RECOVERY_MS)
constexpr uint32_t CORNER_RECOVERY_MS = AGV_CORNER_RECOVERY_MS;
#else
constexpr uint32_t CORNER_RECOVERY_MS = 350; // Thời gian quay về line (ms)
#endif

#if defined(AGV_CORNER_RECOVERY_PWM)
constexpr int CORNER_RECOVERY_PWM = AGV_CORNER_RECOVERY_PWM;
#else
constexpr int CORNER_RECOVERY_PWM = 165; // Tốc độ PWM khi quay về line
#endif

#if defined(AGV_CORNER_RECOVERY_MEMORY_MS)
constexpr uint32_t CORNER_RECOVERY_MEMORY_MS = AGV_CORNER_RECOVERY_MEMORY_MS;
#else
constexpr uint32_t CORNER_RECOVERY_MEMORY_MS = 600; // Thời gian nhớ hướng văng trước khi mất line (ms)
#endif

#if defined(AGV_CORNER_RECOVERY_STEERING_SIGN)
constexpr int CORNER_RECOVERY_STEERING_SIGN = AGV_CORNER_RECOVERY_STEERING_SIGN;
#else
constexpr int CORNER_RECOVERY_STEERING_SIGN = 1;
#endif

// Operator requests stop at or below 15 cm; verify on the physical sensor.
constexpr float OBSTACLE_DISTANCE_CM = 15.0f;
constexpr float OBSTACLE_CLEAR_CM = 20.0f;
// Bounded software trial, not a measured turn time. Pivot exits when a line
// is reacquired after leaving the original line; bench calibration is pending.
#if defined(AGV_BENCH_TURN_MIN_MS)
constexpr uint32_t TURN_MIN_MS = AGV_BENCH_TURN_MIN_MS;
#else
constexpr uint32_t TURN_MIN_MS = 100;
#endif
constexpr uint32_t TURN_TIMEOUT_MS = 2500;
#if defined(AGV_TURN_AROUND_CAPTURE_ENABLED) && AGV_TURN_AROUND_CAPTURE_ENABLED == 1
constexpr bool TURN_AROUND_CAPTURE_ENABLED = true;
#else
constexpr bool TURN_AROUND_CAPTURE_ENABLED = false;
#endif
// Target spacing for fresh 180-degree turn observations; not a real-time guarantee.
constexpr uint32_t TURN_LINE_SAMPLE_MS = 2;
static_assert(TURN_LINE_SAMPLE_MS > 0 && TURN_LINE_SAMPLE_MS <= 10,
              "Invalid turn line sample interval");
#ifndef AGV_TURN_AROUND_CONFIRM_MS
#define AGV_TURN_AROUND_CONFIRM_MS 100
#endif
// Independent of ordinary line recovery and left/right turn confirmation.
constexpr uint32_t TURN_AROUND_CONFIRM_MS = AGV_TURN_AROUND_CONFIRM_MS;
static_assert(TURN_AROUND_CONFIRM_MS >= 2 * TURN_LINE_SAMPLE_MS &&
              TURN_AROUND_CONFIRM_MS < TURN_TIMEOUT_MS,
              "Invalid turn-around confirmation time");
#ifndef AGV_TURN_180_MIN_MS
#define AGV_TURN_180_MIN_MS 350
#endif
constexpr uint32_t TURN_180_MIN_MS = AGV_TURN_180_MIN_MS;
#if defined(AGV_GS_TURN_PWM)
constexpr int GS_TURN_PWM = AGV_GS_TURN_PWM;
#else
constexpr int GS_TURN_PWM = 180;
#endif
static_assert(GS_TURN_PWM > 0 && GS_TURN_PWM <= TURN_SPEED, "Invalid GS turn PWM");

#if defined(AGV_GS_ALIGN_PWM)
constexpr int GS_ALIGN_PWM = AGV_GS_ALIGN_PWM;
#else
constexpr int GS_ALIGN_PWM = 155;
#endif
static_assert(GS_ALIGN_PWM > 0 && GS_ALIGN_PWM <= GS_TURN_PWM, "Invalid GS align PWM");

static_assert(TURN_180_MIN_MS >= TURN_MIN_MS && TURN_180_MIN_MS < TURN_TIMEOUT_MS,
              "Invalid turn-180 minimum duration");
#if defined(AGV_G1_TURN_AROUND_PWM)
constexpr int G1_TURN_AROUND_PWM = AGV_G1_TURN_AROUND_PWM;
#else
constexpr int G1_TURN_AROUND_PWM = 0; // Khong override o cac profile khac.
#endif

#if defined(AGV_TURN_180_RECOVERY_ENABLED)
constexpr bool TURN_180_RECOVERY_ENABLED = AGV_TURN_180_RECOVERY_ENABLED == 1;
#else
constexpr bool TURN_180_RECOVERY_ENABLED = true;
#endif

#if defined(AGV_TURN_180_RECOVERY_PWM)
constexpr int TURN_180_RECOVERY_PWM = AGV_TURN_180_RECOVERY_PWM;
#else
constexpr int TURN_180_RECOVERY_PWM = 165;
#endif

#if defined(AGV_TURN_180_RECOVERY_DELAY_MS)
constexpr uint32_t TURN_180_RECOVERY_DELAY_MS = AGV_TURN_180_RECOVERY_DELAY_MS;
#else
constexpr uint32_t TURN_180_RECOVERY_DELAY_MS = 100;
#endif
static_assert(TURN_180_RECOVERY_PWM > 0 && TURN_180_RECOVERY_PWM <= 255, "Invalid turn 180 recovery PWM");
static_assert(G1_TURN_AROUND_PWM == 0 ||
              (G1_TURN_AROUND_PWM > 0 && G1_TURN_AROUND_PWM <= 255 &&
               G1_TURN_AROUND_PWM <= TURN_SPEED), "Invalid G1 turn-around PWM");
#if defined(AGV_LEFT_TURN_CAPTURE_ENABLED) && AGV_LEFT_TURN_CAPTURE_ENABLED == 1
constexpr bool LEFT_TURN_CAPTURE_ENABLED = true;
#else
constexpr bool LEFT_TURN_CAPTURE_ENABLED = false;
#endif
#if defined(AGV_LEFT_TURN_CAPTURE_PWM)
constexpr int LEFT_TURN_CAPTURE_PWM = AGV_LEFT_TURN_CAPTURE_PWM;
#else
constexpr int LEFT_TURN_CAPTURE_PWM = LINE_RECOVERY_PWM;
#endif
static_assert(LEFT_TURN_CAPTURE_PWM > 0 && LEFT_TURN_CAPTURE_PWM <= 255 &&
              (!LEFT_TURN_CAPTURE_ENABLED || LEFT_TURN_CAPTURE_PWM <= TURN_SPEED),
              "Invalid left turn capture PWM");
#if defined(AGV_RIGHT_TURN_CAPTURE_ENABLED) && AGV_RIGHT_TURN_CAPTURE_ENABLED == 1
constexpr bool RIGHT_TURN_CAPTURE_ENABLED = true;
#else
constexpr bool RIGHT_TURN_CAPTURE_ENABLED = false;
#endif
#if defined(AGV_RIGHT_TURN_CAPTURE_PWM)
constexpr int RIGHT_TURN_CAPTURE_PWM = AGV_RIGHT_TURN_CAPTURE_PWM;
#else
constexpr int RIGHT_TURN_CAPTURE_PWM = LINE_RECOVERY_PWM;
#endif
static_assert(RIGHT_TURN_CAPTURE_PWM > 0 && RIGHT_TURN_CAPTURE_PWM <= 255 &&
              (!RIGHT_TURN_CAPTURE_ENABLED || RIGHT_TURN_CAPTURE_PWM <= TURN_SPEED),
              "Invalid right turn capture PWM");
// Bench values: sampled evidence, then a short capped-speed departure.
constexpr uint32_t LEFT_CAPTURE_SAMPLE_MS = 05;
constexpr uint32_t LEFT_CAPTURE_MAX_GAP_MS = 50;
constexpr uint32_t LEFT_CAPTURE_SETTLE_MS = 200;
#if defined(AGV_GS_CONFIRM_MS)
constexpr uint32_t GS_CONFIRM_MS = AGV_GS_CONFIRM_MS;
#else
constexpr uint32_t GS_CONFIRM_MS = 2 * LEFT_CAPTURE_SAMPLE_MS;
#endif
static_assert(GS_CONFIRM_MS >= 2 * LEFT_CAPTURE_SAMPLE_MS && GS_CONFIRM_MS < TURN_TIMEOUT_MS, "Invalid GS confirm MS");
#if defined(AGV_REVERSE_ALIGN_FORWARD_MS)
constexpr uint32_t REVERSE_ALIGN_FORWARD_MS = AGV_REVERSE_ALIGN_FORWARD_MS;
#else
constexpr uint32_t REVERSE_ALIGN_FORWARD_MS = 0;
#endif
constexpr uint32_t REVERSE_ALIGN_SETTLE_MS = 150;
#if defined(AGV_TURN_RETRY_ENABLED) && AGV_TURN_RETRY_ENABLED == 1
constexpr bool TURN_RETRY_ENABLED = true;
#else
constexpr bool TURN_RETRY_ENABLED = false;
#endif
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
constexpr uint32_t ULTRASONIC_MAX_RANGE_ECHO_US = 30000;
// Robot 01 logs show completed no-range pulses of 47487..47494 us.
// Allow these to finish; retain a bounded deadline for stuck HIGH.
constexpr uint32_t ULTRASONIC_ECHO_TIMEOUT_US = REAR_ULTRASONIC_ECHO_PIN >= 0 ? 60000 : 30000;
constexpr uint32_t ULTRASONIC_STALE_MS = 300;
constexpr uint32_t ULTRASONIC_TRIGGER_US = 10;
constexpr float ULTRASONIC_SOUND_CM_PER_US = 0.0343f;

// Encoder bật cho cả hai profile; cm/xung lấy từ phép đo 100 cm,
// không suy ra từ đường kính bánh.
#if defined(AGV_ENCODER_ENABLED) && AGV_ENCODER_ENABLED == 1
constexpr bool ENCODER_ENABLED = true;
constexpr int ENCODER_LEFT_PIN = AGV_ENCODER_LEFT_PIN;
constexpr int ENCODER_RIGHT_PIN = AGV_ENCODER_RIGHT_PIN;
constexpr float ENCODER_PULSES_PER_100CM = AGV_ENCODER_PULSES_PER_100CM;
#else
constexpr bool ENCODER_ENABLED = false;
constexpr int ENCODER_LEFT_PIN = -1, ENCODER_RIGHT_PIN = -1;
constexpr float ENCODER_PULSES_PER_100CM = 161.0f;
#endif
constexpr uint32_t ENCODER_DEBOUNCE_US = 300;
constexpr uint32_t ODOMETRY_INTERVAL_MS = 200;
constexpr bool MUTEX_ENCODER_RELEASE_ENABLED = TWO_ROBOT_BENCH_TRIAL && ENCODER_ENABLED;
// 18.5 cm * 161 pulse / 100 cm = 29.785; round upward, never downward.
constexpr uint32_t MUTEX_RELEASE_MIN_PULSES = 30;
// A single missed RC522 poll is not proof that the tag has been left.
constexpr uint32_t MUTEX_TAG_LOST_CONFIRM_MS = 100;
constexpr float WHEEL_DIAMETER_CM = 6.5f;
constexpr float LINE_WIDTH_CM = 4.5f;
constexpr float FRONT_LINE_SENSOR_TO_AXLE_CM = 5.0f;
constexpr float REAR_LINE_SENSOR_TO_AXLE_CM = 10.0f;

struct NodeMapping { const char* node; const char* uid; };
// TODO: USER CONFIGURATION: uppercase UID hex, no spaces/colons, never invent UIDs.
constexpr std::array<NodeMapping, 22> RFID_NODES = {{
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
// Match the first RC522 connection test; actual cadence also includes SPI/card response time.
constexpr uint32_t RFID_POLL_INTERVAL_MS = 20;

constexpr std::array<int, 3> SERVO_PINS = {SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN};
// Channels 4/5/6 use LEDC timers 2/3; motor channels 0..3 use timers 0/1.
constexpr std::array<int, 3> SERVO_PWM_CHANNELS = {4, 5, 6};
constexpr uint32_t SERVO_PWM_HZ = 50;
constexpr int SERVO_PWM_BITS = 16;
// Single-servo profiles retain their own pulse/angle calibration.
#if defined(AGV_SINGLE_SERVO_ENABLED) && AGV_SINGLE_SERVO_ENABLED == 1
constexpr bool SERVO_SINGLE_TEST = true;
#else
constexpr bool SERVO_SINGLE_TEST = false;
#endif
constexpr std::array<bool, 3> SERVO_ENABLED = {false, SERVO_SINGLE_TEST, false};
constexpr int SERVO_SCAN = 0;
constexpr int SERVO_LIFT = 1;
constexpr int SERVO_OTHER = 2;
// Other profiles retain the independently calibrated SERVO_LIFT/OTHER pair.
// Defaults preserve profiles without an explicit calibration override.
#ifndef AGV_SERVO_LIFT_UP_ANGLE
#define AGV_SERVO_LIFT_UP_ANGLE 45
#endif
#ifndef AGV_SERVO_LIFT_DOWN_ANGLE
#define AGV_SERVO_LIFT_DOWN_ANGLE 0
#endif
#ifndef AGV_SERVO_MIN_PULSE_US
#define AGV_SERVO_MIN_PULSE_US 1000
#endif
#ifndef AGV_SERVO_MAX_PULSE_US
#define AGV_SERVO_MAX_PULSE_US 2000
#endif
constexpr std::array<int, 2> SERVO_LIFT_UP_ANGLES = {SERVO_SINGLE_TEST ? AGV_SERVO_LIFT_UP_ANGLE : -1, -1};
constexpr std::array<int, 2> SERVO_LIFT_DOWN_ANGLES = {SERVO_SINGLE_TEST ? AGV_SERVO_LIFT_DOWN_ANGLE : -1, -1};
constexpr int SERVO_MIN_PULSE_US = SERVO_SINGLE_TEST ? AGV_SERVO_MIN_PULSE_US : 0;
constexpr int SERVO_MAX_PULSE_US = SERVO_SINGLE_TEST ? AGV_SERVO_MAX_PULSE_US : 0;
constexpr uint32_t SERVO_MOVE_TIME_MS = SERVO_SINGLE_TEST ? 5000 : 0;
static_assert(!SERVO_SINGLE_TEST ||
              (SERVO_MIN_PULSE_US > 0 && SERVO_MAX_PULSE_US > SERVO_MIN_PULSE_US &&
               SERVO_MAX_PULSE_US < 1000000 / SERVO_PWM_HZ &&
               SERVO_LIFT_UP_ANGLES[0] >= 0 && SERVO_LIFT_UP_ANGLES[0] <= 180 &&
               SERVO_LIFT_DOWN_ANGLES[0] >= 0 && SERVO_LIFT_DOWN_ANGLES[0] <= 180),
              "Invalid single-servo pulse/angle calibration");

// Profile 01/02 dung 3S; cac ban khong co profile giu cau hinh 2S cu.
// Nguong tong ap khong thay the bao ve qua xa/qua sac tung cell cua BMS.
#if defined(AGV_PROFILE_ID)
constexpr int BATTERY_SERIES_CELLS = AGV_BENCH_BATTERY_SERIES_CELLS;
constexpr float BATTERY_VOLTAGE_SCALE = AGV_BENCH_BATTERY_VOLTAGE_SCALE;
constexpr float BATTERY_VOLTAGE_OFFSET = AGV_BENCH_BATTERY_VOLTAGE_OFFSET;
constexpr float BATTERY_EMPTY_VOLTAGE = AGV_BENCH_BATTERY_EMPTY_VOLTAGE;
constexpr float BATTERY_FULL_VOLTAGE = AGV_BENCH_BATTERY_FULL_VOLTAGE;
constexpr float BENCH_MAX_VOLTAGE = AGV_BENCH_BATTERY_MAX_VOLTAGE;
#else
constexpr int BATTERY_SERIES_CELLS = 2;
constexpr float BATTERY_VOLTAGE_SCALE = 5.0f;
constexpr float BATTERY_VOLTAGE_OFFSET = 0.0f;
constexpr float BATTERY_EMPTY_VOLTAGE = 6.4f;
constexpr float BATTERY_FULL_VOLTAGE = 8.4f;
constexpr float BENCH_MAX_VOLTAGE = 8.6f;
#endif
static_assert((BATTERY_SERIES_CELLS == 2 || BATTERY_SERIES_CELLS == 3) &&
              BATTERY_VOLTAGE_SCALE > 0.0f && BATTERY_EMPTY_VOLTAGE > 0.0f &&
              BATTERY_FULL_VOLTAGE > BATTERY_EMPTY_VOLTAGE &&
              BENCH_MAX_VOLTAGE >= BATTERY_FULL_VOLTAGE,
              "Invalid battery pack configuration");
// Khong bo qua chot ap: ADC loi/ngoai khoang van cam chuyen dong.
inline bool benchVoltageReady(float voltage) {
    return !TWO_ROBOT_BENCH_TRIAL ||
           (voltage >= BATTERY_EMPTY_VOLTAGE && voltage <= BENCH_MAX_VOLTAGE);
}
constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 500;

// ----------------------------------------------------------------------------
// Bù tốc độ theo bậc thang % pin (Battery Step Speed Compensation)
// ----------------------------------------------------------------------------
#if defined(AGV_BATTERY_SPEED_COMP_ENABLED)
constexpr bool BATTERY_SPEED_COMP_ENABLED = AGV_BATTERY_SPEED_COMP_ENABLED == 1;
#else
constexpr bool BATTERY_SPEED_COMP_ENABLED = true;
#endif

#if defined(AGV_BATTERY_STEP_HIGH_PERCENT)
constexpr float BATTERY_STEP_HIGH_PERCENT = AGV_BATTERY_STEP_HIGH_PERCENT;
#else
constexpr float BATTERY_STEP_HIGH_PERCENT = 80.0f;
#endif

#if defined(AGV_BATTERY_STEP_MID_PERCENT)
constexpr float BATTERY_STEP_MID_PERCENT = AGV_BATTERY_STEP_MID_PERCENT;
#else
constexpr float BATTERY_STEP_MID_PERCENT = 50.0f;
#endif

#if defined(AGV_BATTERY_STEP_LOW_PERCENT)
constexpr float BATTERY_STEP_LOW_PERCENT = AGV_BATTERY_STEP_LOW_PERCENT;
#else
constexpr float BATTERY_STEP_LOW_PERCENT = 30.0f;
#endif

#if defined(AGV_BATTERY_PWM_OFFSET_HIGH)
constexpr int BATTERY_PWM_OFFSET_HIGH = AGV_BATTERY_PWM_OFFSET_HIGH;
#else
constexpr int BATTERY_PWM_OFFSET_HIGH = -10;
#endif

#if defined(AGV_BATTERY_PWM_OFFSET_NOMINAL)
constexpr int BATTERY_PWM_OFFSET_NOMINAL = AGV_BATTERY_PWM_OFFSET_NOMINAL;
#else
constexpr int BATTERY_PWM_OFFSET_NOMINAL = 0;
#endif

#if defined(AGV_BATTERY_PWM_OFFSET_MID)
constexpr int BATTERY_PWM_OFFSET_MID = AGV_BATTERY_PWM_OFFSET_MID;
#else
constexpr int BATTERY_PWM_OFFSET_MID = 8;
#endif

#if defined(AGV_BATTERY_PWM_OFFSET_LOW)
constexpr int BATTERY_PWM_OFFSET_LOW = AGV_BATTERY_PWM_OFFSET_LOW;
#else
constexpr int BATTERY_PWM_OFFSET_LOW = 15;
#endif

inline int batteryStepPwmOffset(float batteryPercent) {
    if (!BATTERY_SPEED_COMP_ENABLED || !std::isfinite(batteryPercent) || batteryPercent < 0.0f) {
        return 0;
    }
    if (batteryPercent >= BATTERY_STEP_HIGH_PERCENT) {
        return BATTERY_PWM_OFFSET_HIGH;
    }
    if (batteryPercent >= BATTERY_STEP_MID_PERCENT) {
        return BATTERY_PWM_OFFSET_NOMINAL;
    }
    if (batteryPercent >= BATTERY_STEP_LOW_PERCENT) {
        return BATTERY_PWM_OFFSET_MID;
    }
    return BATTERY_PWM_OFFSET_LOW;
}

// Runtime MQTT topics are built from the robot ID using the Core v0 contract.
#if defined(AGV_PROFILE_ID)
constexpr const char* AGV_ID = AGV_PROFILE_ID;
// Chan doi ID trong header xe02 roi nap nhu xe01 (hoac nguoc lai).
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
static_assert(AGV_ID[0] == '0' && AGV_ID[1] == '1' && AGV_ID[2] == '\0', "Profile01 must use ID 01");
#elif defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
static_assert(AGV_ID[0] == '0' && AGV_ID[1] == '2' && AGV_ID[2] == '\0', "Profile02 must use ID 02");
#endif
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
constexpr size_t MAX_PUBLISH_BYTES = SERVO_SINGLE_TEST ? 2560 : 1536;
constexpr size_t COMMAND_QUEUE_LENGTH = 8;
constexpr size_t EVENT_QUEUE_LENGTH = 16;
constexpr size_t RECENT_COMMAND_IDS = 16;
constexpr bool DEBUG_ENABLED = true;
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
constexpr const char* FIRMWARE_BUILD = "20261030-robot01-lm393-active-low-v27";
#elif defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
constexpr const char* FIRMWARE_BUILD = "20261030-robot02-lm393-active-low-v27";
#else
constexpr const char* FIRMWARE_BUILD = "20261015-battery-smoothing";
#endif
constexpr uint32_t SERIAL_BAUD = 115200;

constexpr std::array<int, 24> ALL_PINS = {
    LEFT_ENA, LEFT_IN1, LEFT_IN2, RIGHT_ENB, RIGHT_IN3, RIGHT_IN4,
    LINE_LEFT_PIN, LINE_CENTER_PIN, LINE_RIGHT_PIN,
    ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN, REAR_ULTRASONIC_TRIG_PIN, REAR_ULTRASONIC_ECHO_PIN, BATTERY_ADC_PIN,
    RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN, RFID_RST_PIN,
    SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN,
    BUZZER_PIN, CARGO_IR_PIN
};
constexpr bool pinsUnique() {
    for (size_t i = 0; i < ALL_PINS.size(); ++i)
        for (size_t j = i + 1; j < ALL_PINS.size(); ++j)
            if (ALL_PINS[i] >= 0 && ALL_PINS[i] == ALL_PINS[j]) return false;
    if (ENCODER_ENABLED) {
        if (ENCODER_LEFT_PIN < 0 || ENCODER_RIGHT_PIN < 0 || ENCODER_LEFT_PIN == ENCODER_RIGHT_PIN) return false;
        for (int pin : ALL_PINS) if (pin == ENCODER_LEFT_PIN || pin == ENCODER_RIGHT_PIN) return false;
    }
    return true;
}
static_assert(pinsUnique(), "GPIO collision");
static_assert(ENCODER_PULSES_PER_100CM > 0 && ODOMETRY_INTERVAL_MS >= 100, "Invalid encoder scale/rate");
static_assert(!MUTEX_ENCODER_RELEASE_ENABLED ||
              MUTEX_RELEASE_MIN_PULSES * 100.0f / ENCODER_PULSES_PER_100CM >=
                  MUTEX_RELEASE_FROM_FIRST_READ_CM,
              "Mutex release pulse threshold must round the measured clearance upward");
static_assert(LINE_LEFT_PIN >= 34 && LINE_CENTER_PIN >= 34 && LINE_RIGHT_PIN >= 34, "Line inputs must remain input-only pins");
static_assert(BATTERY_ADC_PIN == 36, "Battery must remain on ADC1 GPIO36");
static_assert(BUZZER_ACTIVE_STATE == 0 && (CARGO_IR_ACTIVE_STATE == 0 || CARGO_IR_ACTIVE_STATE == 1), "Unexpected buzzer/IR polarity");
static_assert(CARGO_IR_STABLE_MS > 0 && REVERSE_STOP_MS > 0, "Safety debounce/hold must be nonzero");
static_assert(BASE_SPEED >= 0 && BASE_SPEED <= 255 &&
              BASE_SPEED + LEFT_SPEED_TRIM > 0 && BASE_SPEED + LEFT_SPEED_TRIM <= 255 &&
              BASE_SPEED + RIGHT_SPEED_TRIM > 0 && BASE_SPEED + RIGHT_SPEED_TRIM <= 255,
              "Forward PWM + trim must be 1..255");
static_assert(TURN_SPEED >= 0 && TURN_SPEED <= 255 &&
              TURN_SPEED + TURN_LEFT_SPEED_TRIM > 0 && TURN_SPEED + TURN_LEFT_SPEED_TRIM <= 255 &&
              TURN_SPEED + TURN_RIGHT_SPEED_TRIM > 0 && TURN_SPEED + TURN_RIGHT_SPEED_TRIM <= 255,
              "Turn PWM + trim must be 1..255");
static_assert(
              CORRECTION_SPEED >= 0 && CORRECTION_SPEED <= 255 &&
              CORRECTION_SPEED + LEFT_SPEED_TRIM > 0 && CORRECTION_SPEED + LEFT_SPEED_TRIM <= 255 &&
              CORRECTION_SPEED + RIGHT_SPEED_TRIM > 0 && CORRECTION_SPEED + RIGHT_SPEED_TRIM <= 255,
              "Correction PWM + trim must be 1..255");
static_assert(
              GENTLE_CORRECTION_SPEED + LEFT_SPEED_TRIM > 0 &&
              GENTLE_CORRECTION_SPEED + LEFT_SPEED_TRIM <= 255 &&
              GENTLE_CORRECTION_SPEED + RIGHT_SPEED_TRIM > 0 &&
              GENTLE_CORRECTION_SPEED + RIGHT_SPEED_TRIM <= 255 &&
              GENTLE_OUTER_SPEED + LEFT_SPEED_TRIM > 0 &&
              GENTLE_OUTER_SPEED + LEFT_SPEED_TRIM <= 255 &&
              GENTLE_OUTER_SPEED + RIGHT_SPEED_TRIM > 0 &&
              GENTLE_OUTER_SPEED + RIGHT_SPEED_TRIM <= 255,
              "Gentle PWM + trim must be 1..255");
// Chi rang buoc quan he toc do gentle khi bang line thuc su dung nhanh nay.
static_assert(!(ONE_ROBOT_TRIAL || GENTLE_FORWARD_CORRECTION) ||
              (GENTLE_OUTER_SPEED <= BASE_SPEED &&
               GENTLE_OUTER_SPEED + LEFT_SPEED_TRIM > GENTLE_CORRECTION_SPEED + RIGHT_SPEED_TRIM &&
               GENTLE_OUTER_SPEED + RIGHT_SPEED_TRIM > GENTLE_CORRECTION_SPEED + LEFT_SPEED_TRIM),
              "Active gentle correction: outer must be <= forward and faster than inner after trims");
static_assert(!ONE_ROBOT_TRIAL || GENTLE_CORRECTION_SPEED < CORRECTION_SPEED,
              "Trial gentle correction must be slower than strong correction");
}
