#pragma once

// Xe 01, môi trường esp32dev_two_robot_bench_01.
#define AGV_PROFILE_ID "01"
// Servo xe01: dải xung người dùng hiệu chỉnh 21/10/2026, 0..180 độ.
// Người dùng xác nhận: mặc định/hạ180°, nâng100°; không phát góc khi boot.
#define AGV_SERVO_MIN_PULSE_US 544
#define AGV_SERVO_MAX_PULSE_US 2400
#define AGV_SERVO_LIFT_DOWN_ANGLE 180
#define AGV_SERVO_LIFT_UP_ANGLE 100
// PD tien.
#define AGV_LINE_PD_ENABLED 1
#define AGV_BENCH_LINE_KP 75.0f // Khả năng kéo xe trở về line
#define AGV_BENCH_LINE_KD 25.0f // Điều chỉnh dao động khi bám line
#define AGV_LINE_PD_BASE_SPEED 140 // chinh di thang
#define AGV_LINE_PD_MIN_RUNNING_PWM 110 // tranh mot banh roi duoi nguong thang ma sat
#define AGV_LINE_PD_MAX_RUNNING_PWM 230 // tranh banh ngoai vot len full duty khi lech line
#define AGV_LINE_PD_CONTROL_MS 10
#define AGV_LINE_PD_RESET_GAP_MS 100
// +1 giu quan he GPIO->ENA/ENB cua bang dieu khien hien tai.
#define AGV_LINE_PD_STEERING_SIGN 1

// 1. TÌM LẠI LINE: tối đa 7s chờ xác nhận; quét motor vẫn giới hạn 600ms.
#define AGV_LINE_LOSS_GRACE_MS 9000
#include "SharedRobotConfig.h"
// Bắt lại line sau rẽ; tách khỏi tìm line trên chặng.
#define AGV_LEFT_TURN_CAPTURE_ENABLED 1
#define AGV_LEFT_TURN_CAPTURE_PWM 210 // PWM bat lai line sau cua trai
#define AGV_RIGHT_TURN_CAPTURE_ENABLED 1
#define AGV_RIGHT_TURN_CAPTURE_PWM 225 // PWM bat lai line sau cua phai

// 2. Di thang du phong: ENA/ENB = 170/160; PD dung BASE_SPEED rieng.
#define AGV_BENCH_MOTOR_SPEED 160
#define AGV_BENCH_LEFT_SPEED_TRIM 10 // kênh ENA/GPIO14
#define AGV_BENCH_RIGHT_SPEED_TRIM 0 // kênh ENB/GPIO25

// 3. Quay tai node: ENA/ENB = 235/245, hai kenh chay nguoc chieu.
//cùng áp dụng cho cả quay trái và quay phải.
#define AGV_BENCH_TURN_SPEED 230 // quay tại chỗ
#define AGV_BENCH_TURN_LEFT_SPEED_TRIM -10 // trim kenh ENA
#define AGV_BENCH_TURN_RIGHT_SPEED_TRIM -10 // trim kenh ENB

// Rieng quay dau 180 do tai G1: giam 20 PWM, khong doi re trai/phai.
#define AGV_G1_TURN_AROUND_PWM (AGV_BENCH_TURN_SPEED - 20)

// 4. BÁM LINE TIẾN:
// Khong bat AGV_GENTLE_FORWARD_CORRECTION.
#define AGV_BENCH_CORRECTION_SPEED 160
// Giá trị dự phòng; không dùng bởi bảng lineAction tiến của profile01 hiện tại.
#define AGV_BENCH_GENTLE_CORRECTION_SPEED 160

// 5. Lui thang: ENA/ENB = 165/130; trim rieng voi di tien.
#define AGV_REVERSE_LEFT_SPEED_TRIM 05 // giá trị +5, kênh ENA
#define AGV_REVERSE_RIGHT_SPEED_TRIM -30 // kênh ENB
#define AGV_REVERSE_SPEED 160 // lùi thẳng
#define AGV_REVERSE_CORRECTION_SPEED 150 // lùi mất line
#define AGV_REVERSE_GENTLE_CORRECTION 1 // 0: dừng 1 bánh, 1: cả 2 bánh đều lùi, 1 bánh chậm
#define AGV_REVERSE_CORRECTION_DELTA 30 // độ chênh lệnh khi lùi mất line
