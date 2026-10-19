#pragma once

// Xe 02, môi trường esp32dev_two_robot_bench_02.
// Bảng tra: docs/firmware-tuning-01-02.md (cập nhật 20/10/2026).
// PWM 0..255. LEFT = kênh ENA/GPIO14, RIGHT = ENB/GPIO25 trong mã.
#define AGV_PROFILE_ID "02"
// 1. TÌM LẠI LINE: tối đa 7s chờ xác nhận; quét motor vẫn giới hạn 600ms.
#define AGV_LINE_LOSS_GRACE_MS 7000
#include "SharedRobotConfig.h"
// Bắt line sau quay trái, riêng xe02; khác recovery mất line trên chặng.
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 230

// 2. ĐI THẲNG: duty = MOTOR_SPEED + trim từng kênh -> ENA/ENB = 245/240.
#define AGV_BENCH_MOTOR_SPEED 240
#define AGV_BENCH_LEFT_SPEED_TRIM 5 // kênh ENA; comment cũ của người dùng: bánh phải
#define AGV_BENCH_RIGHT_SPEED_TRIM 0 // kênh ENB; comment cũ của người dùng: bánh trái

// 3. QUAY TẠI NODE: 230+25 = 255 mỗi kênh; hai kênh chạy ngược chiều.
// Hai trim dưới đây là trim KÊNH, cùng áp dụng cho cả quay trái và quay phải.
#define AGV_BENCH_TURN_SPEED 230
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 25
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 25

// 4. BÁM LINE TIẾN: 100/110 -> 245/0; 001/011 -> 0/240.
// Xe02 không bật AGV_GENTLE_FORWARD_CORRECTION, nên 110/011 vẫn chỉnh mạnh.
#define AGV_BENCH_CORRECTION_SPEED 240
// Giá trị dự phòng; không dùng bởi bảng lineAction tiến của profile02 hiện tại.
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 240

// 5. CHẠY LÙI: trim riêng; độ lớn duty thẳng = 205/190.
#define AGV_REVERSE_LEFT_SPEED_TRIM 05 // giá trị +5, kênh ENA
#define AGV_REVERSE_RIGHT_SPEED_TRIM -10 // kênh ENB
#define AGV_REVERSE_SPEED 200
#define AGV_REVERSE_CORRECTION_SPEED 240
// Reverse gentle: 100/110 -> 215/230; 001/011 -> 245/200 (đều lùi).
#define AGV_REVERSE_GENTLE_CORRECTION 1
// Giảm kênh chậm 30 so với REVERSE_CORRECTION_SPEED + trim; không so với tốc độ lùi thẳng.
#define AGV_REVERSE_CORRECTION_DELTA 30

