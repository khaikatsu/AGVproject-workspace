#pragma once

// Xe 01, môi trường esp32dev_two_robot_bench_01.
// Bảng tra: docs/firmware-tuning-01-02.md (cập nhật 20/10/2026).
// PWM 0..255; các mức gentle hiện tại là mức thử, chưa đo trên xe thật.
// LEFT = kênh ENA/GPIO14, RIGHT = ENB/GPIO25 trong mã; xác minh phía bánh thực tế.
#define AGV_PROFILE_ID "01"
#include "SharedRobotConfig.h"

// 1. ĐI THẲNG: duty = MOTOR_SPEED + trim từng kênh -> ENA/ENB = 210/230.
#define AGV_BENCH_MOTOR_SPEED 230
#define AGV_BENCH_LEFT_SPEED_TRIM -20 // kênh ENA; comment cũ của người dùng: bánh phải
#define AGV_BENCH_RIGHT_SPEED_TRIM 0 // kênh ENB; comment cũ của người dùng: bánh trái

// 2. QUAY TẠI NODE: 230+25 = 255 mỗi kênh; hai kênh chạy ngược chiều.
// Hai trim dưới đây là trim KÊNH, cùng áp dụng cho cả quay trái và quay phải.
#define AGV_BENCH_TURN_SPEED 230
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 25 // trim kênh ENA khi quay
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 25 // trim kênh ENB khi quay

// 3. BÁM LINE TIẾN: 100/001 chỉnh mạnh -> 220/0 hoặc 0/240.
#define AGV_BENCH_CORRECTION_SPEED 240
// 110/011 dùng gentle: outer/inner + trim -> 190/150 hoặc 130/210.
// Outer phải <= MOTOR_SPEED; phải giữ chênh duty đúng phía sau khi cộng trim.
#define AGV_BENCH_GENTLE_OUTER_SPEED 210
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 150 // bánh trong cua; cần thử tải thực tế
#define AGV_GENTLE_FORWARD_CORRECTION 1

// 4. TÌM LẠI LINE: tham số thời gian/PWM quét nằm trong Config.h.
// Sau 000, nhận lại line cùng phía khi đủ mẫu ổn định; chỉ dùng khi tiến.
#define AGV_LINE_RECOVERY_SIDE_CONFIRM 1

// 5. CHẠY LÙI: trim riêng, không lấy trim tiến; độ lớn duty thẳng = 205/190.
// Xe01 không bật reverse gentle: 100/110 -> 0/230; 001/011 -> 245/0 (đều lùi).
#define AGV_REVERSE_LEFT_SPEED_TRIM 05 // giá trị +5, kênh ENA
#define AGV_REVERSE_RIGHT_SPEED_TRIM -10 // kênh ENB
#define AGV_REVERSE_SPEED 200
#define AGV_REVERSE_CORRECTION_SPEED 240
