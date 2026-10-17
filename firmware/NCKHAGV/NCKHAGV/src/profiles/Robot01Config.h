#pragma once

// Xe 01, môi trường esp32dev_two_robot_bench_01.
// Tuning xe 01: tiến 220, lùi 190; xoay 210 theo yêu cầu 15/10/2026.
// Chân GPIO và chốt an toàn ở Config.h.
#define AGV_PROFILE_ID "01"
#include "SharedRobotConfig.h"
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 230
#define AGV_BENCH_MOTOR_SPEED 220
#define AGV_BENCH_LEFT_SPEED_TRIM -30
#define AGV_BENCH_RIGHT_SPEED_TRIM 15
#define AGV_BENCH_TURN_SPEED 230
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 0
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 0
#define AGV_BENCH_CORRECTION_SPEED 235
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 235
// 16/10/2026: giu tuning tien/lui/chinh line; pivot230, bat line LEFT215.
#define AGV_REVERSE_SPEED 200
#define AGV_REVERSE_CORRECTION_SPEED 220

