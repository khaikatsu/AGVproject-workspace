#pragma once

// Xe 02, môi trường esp32dev_two_robot_bench_02.
#define AGV_PROFILE_ID "02"
// Servo xe02 giống xe01 theo xác nhận người dùng21/10/2026.
// Mặc định/hạ180°, nâng100°; boot không tự phát góc.
#define AGV_SERVO_MIN_PULSE_US 544
#define AGV_SERVO_MAX_PULSE_US 2400
#define AGV_SERVO_LIFT_DOWN_ANGLE 180
#define AGV_SERVO_LIFT_UP_ANGLE 100
// PD tien.
#define AGV_LINE_PD_ENABLED 1
#define AGV_BENCH_LINE_KP 65.0f // Khả năng kéo xe trở về line
#define AGV_BENCH_LINE_KD 25.0f // Điều chỉnh dao động khi bám line
#define AGV_LINE_PD_BASE_SPEED 135 // toc do bam line tien
#define AGV_LINE_PD_CONTROL_MS 15 // chu ky tinh PD, ms
#define AGV_LINE_PD_RESET_GAP_MS 100
// +1 giu quan he GPIO->ENA/ENB cua bang dieu khien hien tai.
#define AGV_LINE_PD_STEERING_SIGN 1

// 1. TÌM LẠI LINE: tối đa 7s chờ xác nhận; quét motor vẫn giới hạn 600ms.
#define AGV_LINE_LOSS_GRACE_MS 7000
#include "SharedRobotConfig.h"
// Bắt line sau quay trái, riêng xe02; khác recovery mất line trên chặng.
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 200 // PWM bat lai line sau cua trai
#define AGV_RIGHT_TURN_CAPTURE_ENABLED 1
#define AGV_RIGHT_TURN_CAPTURE_PWM 200 // PWM bat lai line sau cua phai

// 2. Di thang du phong: ENA/ENB =
#define AGV_BENCH_MOTOR_SPEED 160
#define AGV_BENCH_LEFT_SPEED_TRIM 0 // kênh ENA bánh phải
#define AGV_BENCH_RIGHT_SPEED_TRIM 0 // kênh ENB bánh trái

// 3. Quay tai node: ENA/ENB = 205/210, hai kenh chay nguoc chieu.
//cùng áp dụng cho cả quay trái và quay phải.
#define AGV_BENCH_TURN_SPEED 200 // quay tai cho
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM 5 // kenh ENA khi quay
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM 10 // kenh ENB khi quay

// Rieng quay dau 180 do tai G1: giam 30 PWM truoc trim.
#define AGV_G1_TURN_AROUND_PWM (AGV_BENCH_TURN_SPEED - 30)
// Xac nhan line sau quay 180 do; dong bo hai xe.
#define AGV_TURN_AROUND_CONFIRM_MS 1000

// 4. BÁM LINE TIẾN:
// Xe02 không bật AGV_GENTLE_FORWARD_CORRECTION
#define AGV_BENCH_CORRECTION_SPEED 160
// Giá trị dự phòng; không dùng bởi bảng lineAction tiến của profile02 hiện tại.
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 160

// 5. Lui thang: ENA/ENB = 150/95; trim rieng voi di tien.
#define AGV_REVERSE_LEFT_SPEED_TRIM 0 // kenh ENA
#define AGV_REVERSE_RIGHT_SPEED_TRIM 0 // kênh ENB
#define AGV_REVERSE_SPEED 140 // lui thang
#define AGV_REVERSE_CORRECTION_SPEED 140 // chinh lech khi lui con line
#define AGV_REVERSE_GENTLE_CORRECTION 1 // 0: dừng 1 bánh, 1: cả 2 bánh đều lùi, 1 bánh chậm
#define AGV_REVERSE_CORRECTION_DELTA 50 // giam PWM kenh cham khi lui chinh lech

