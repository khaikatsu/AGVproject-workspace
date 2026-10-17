#pragma once

// Xe 02, môi trường esp32dev_two_robot_bench_02.
// Chỉ chỉnh các giá trị đã đo trên chính xe 02; chân GPIO và chốt an toàn ở Config.h.
#define AGV_PROFILE_ID "01"
#define AGV_LINE_LOSS_GRACE_MS 7000
#include "SharedRobotConfig.h"
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 230
#define AGV_BENCH_MOTOR_SPEED 200
#define AGV_BENCH_LEFT_SPEED_TRIM 5 // nay la phai
#define AGV_BENCH_RIGHT_SPEED_TRIM 0 // nay la trai
#define AGV_BENCH_TURN_SPEED 230
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 25 // tốc độ  khi quay trái
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 25 // tốc độ khi quay phải
#define AGV_BENCH_CORRECTION_SPEED 250
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 240 // tốc độ trờ về line
// Reverse-only balance: reduce left wheel by 10 PWM, preserve forward/pivot.
#define AGV_REVERSE_LEFT_SPEED_TRIM 05 // cai nay trai
#define AGV_REVERSE_RIGHT_SPEED_TRIM -10 // nay la phai
#define AGV_REVERSE_SPEED 200
#define AGV_REVERSE_CORRECTION_SPEED 240
// Front-mounted line sensors: provisional gentle reverse steering, both wheels reverse.
#define AGV_REVERSE_GENTLE_CORRECTION 1
#define AGV_REVERSE_CORRECTION_DELTA 30

