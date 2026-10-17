#pragma once

// Xe 02, môi trường esp32dev_two_robot_bench_02.
// Chỉ chỉnh các giá trị đã đo trên chính xe 02; chân GPIO và chốt an toàn ở Config.h.
#define AGV_PROFILE_ID "02"
#include "SharedRobotConfig.h"
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 230
#define AGV_BENCH_MOTOR_SPEED 230
#define AGV_BENCH_LEFT_SPEED_TRIM 15
#define AGV_BENCH_RIGHT_SPEED_TRIM -15
#define AGV_BENCH_TURN_SPEED 230
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 0
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 0
#define AGV_BENCH_CORRECTION_SPEED 235
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 235
// Reverse-only balance: reduce left wheel by 10 PWM, preserve forward/pivot.
#define AGV_REVERSE_LEFT_SPEED_TRIM 5
#define AGV_REVERSE_SPEED 210
#define AGV_REVERSE_CORRECTION_SPEED 220

