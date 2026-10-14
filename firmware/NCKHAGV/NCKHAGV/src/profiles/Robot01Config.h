#pragma once

// Xe 01, môi trường esp32dev_two_robot_bench_01.
// Tuning xe 01: tiến 220, lùi 190; xoay giảm nhẹ theo yêu cầu 14/10/2026.
// Chân GPIO và chốt an toàn ở Config.h.
#define AGV_PROFILE_ID "01"
// Encoder OUT: trái GPIO16, phải GPIO4 theo sketch thử xe 01 ngày 14/10.
#define AGV_ENCODER_ENABLED 1
#define AGV_ENCODER_LEFT_PIN 16
#define AGV_ENCODER_RIGHT_PIN 4
#define AGV_ENCODER_PULSES_PER_100CM 161.0f
#define AGV_BENCH_MOTOR_SPEED 220
#define AGV_BENCH_LEFT_SPEED_TRIM -40
#define AGV_BENCH_RIGHT_SPEED_TRIM 0
// Giảm 220 -> 200 để có thêm thời gian lấy mẫu line khi pivot; cần thử trên sàn.
#define AGV_BENCH_TURN_SPEED 200
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 0
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 0
#define AGV_BENCH_CORRECTION_SPEED 220
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 220
// Giảm riêng PWM lùi/chỉnh line khi lùi; cần thử lực kéo khi có hàng.
#define AGV_REVERSE_SPEED 190
#define AGV_REVERSE_CORRECTION_SPEED 190
#define AGV_BENCH_LINE_LOSS_CONFIRM_MS 5000
#define AGV_BENCH_SEGMENT_TIMEOUT_MS 15000
#define AGV_BENCH_RFID_STOP_DWELL_MS 1000
#define AGV_BENCH_BATTERY_VOLTAGE_SCALE 5.0f
#define AGV_BENCH_BATTERY_VOLTAGE_OFFSET 0.0f

