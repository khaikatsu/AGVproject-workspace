#pragma once

// Xe 01, môi trường esp32dev_two_robot_bench_01.
// Bộ thông số bám line và động cơ do người dùng chốt ngày 22/10/2026.
// PD giống xe02; PWM quay/lùi dưới đây vẫn là bộ riêng xe01.
#define AGV_PROFILE_ID "01"
// Servo xe01: mặc định/hạ 180°, nâng 100°; không phát góc khi boot.
#define AGV_SERVO_MIN_PULSE_US 544
#define AGV_SERVO_MAX_PULSE_US 2400
#define AGV_SERVO_LIFT_DOWN_ANGLE 180
#define AGV_SERVO_LIFT_UP_ANGLE 100

// PD tiến theo bộ thông số người dùng cung cấp.
#define AGV_LINE_PD_ENABLED 1
#define AGV_BENCH_LINE_KP 70.0f // Khả năng kéo xe trở về line
#define AGV_BENCH_LINE_KD 30.0f // Điều chỉnh dao động khi bám line
#define AGV_LINE_PD_BASE_SPEED 125 // Tốc độ đi thẳng
// Không khai báo sàn/trần riêng: dùng mặc định 0..255 để PD có thể hãm một bánh.
#define AGV_LINE_PD_CONTROL_MS 10
#define AGV_LINE_PD_RESET_GAP_MS 100
// +1 giữ quan hệ GPIO->ENA/ENB của bảng điều khiển hiện tại.
#define AGV_LINE_PD_STEERING_SIGN 1

// 1. TÌM LẠI LINE: tối đa 7s chờ xác nhận; quét motor vẫn giới hạn 600ms.
#define AGV_LINE_LOSS_GRACE_MS 7000
#include "SharedRobotConfig.h"
// Bắt lại line sau rẽ; tách khỏi tìm line trên chặng.
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 220 // PWM bắt lại line sau cua trái
#define AGV_RIGHT_TURN_CAPTURE_ENABLED 1
#define AGV_RIGHT_TURN_CAPTURE_PWM 200 // PWM bắt lại line sau cua phải

// 2. Đi thẳng dự phòng.
#define AGV_BENCH_MOTOR_SPEED 255
#define AGV_BENCH_LEFT_SPEED_TRIM 0 // kênh ENA
#define AGV_BENCH_RIGHT_SPEED_TRIM 0 // kênh ENB

// 3. Quay tại node; cùng áp dụng cho cả quay trái và quay phải.
#define AGV_BENCH_TURN_SPEED 220 // quay tại chỗ
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 0 // kênh ENA
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM -5 // kênh ENB
#define AGV_BENCH_TURN_MIN_MS 200 // thời gian tối thiểu quay tại chỗ (ms) trước khi bắt line mới

// Riêng quay đầu 180 độ tại G1.
#define AGV_G1_TURN_AROUND_PWM (AGV_BENCH_TURN_SPEED - 15)
// Xac nhan line sau quay 180 do; dong bo hai xe.
#define AGV_TURN_AROUND_CONFIRM_MS 500

// 4. BÁM LINE TIẾN:
#define AGV_BENCH_CORRECTION_SPEED 170
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 170

// 5. Lùi thẳng: độ lớn duty ENA/ENB = 190/130 sau trim.
#define AGV_REVERSE_LEFT_SPEED_TRIM 20 // giá trị +20, kênh ENA
#define AGV_REVERSE_RIGHT_SPEED_TRIM -20 // kênh ENB
#define AGV_REVERSE_SPEED 180 // lùi thẳng
#define AGV_REVERSE_CORRECTION_SPEED 150 // chỉnh lệch khi còn line; 000 do recovery xử lý
#define AGV_REVERSE_GENTLE_CORRECTION 1 // 0: dừng 1 bánh, 1: cả 2 bánh đều lùi, 1 bánh chậm
#define AGV_REVERSE_CORRECTION_DELTA 50 // độ chênh lệnh khi lùi mất line