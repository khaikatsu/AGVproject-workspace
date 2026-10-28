#pragma once

// Phần cứng/thông số chung theo xác nhận người dùng 16/10/2026.
// ID và toàn bộ PWM/trim vẫn do từng profile sở hữu.
#define AGV_ENCODER_ENABLED 1
#define AGV_ENCODER_LEFT_PIN 16
#define AGV_ENCODER_RIGHT_PIN 4
#define AGV_ENCODER_PULSES_PER_100CM 161.0f
#define AGV_REAR_SONAR_ENABLED 1
#define AGV_REVERSE_HOME_ENABLED 1
#define AGV_SINGLE_SERVO_ENABLED 1
#define AGV_TURN_RETRY_ENABLED 1
#define AGV_LINE_RECOVERY_ENABLED 1
// Chi xac nhan line co mat giua010/011/110; khong nhan mep001/100 hay giao rong.
#define AGV_LINE_RECOVERY_TRACK_CONFIRM 1
#define AGV_TURN_AROUND_CAPTURE_ENABLED 1
#define AGV_FAST_LINE_RESPONSE 1
#define AGV_BENCH_LINE_LOSS_CONFIRM_MS 5000
#define AGV_BENCH_SEGMENT_TIMEOUT_MS 15000
#define AGV_BENCH_RFID_STOP_DWELL_MS 1000
// Hai xe dung pin Li-ion 3S; so do 20/10/2026: khay 12.5 V, S/OUT 2.3 V.
// He so tam theo dong ho; can doi chieu batteryAdcMv tren tung ESP32 khi nap.
#define AGV_BENCH_BATTERY_SERIES_CELLS 3
#define AGV_BENCH_BATTERY_EMPTY_VOLTAGE 10.2f
#define AGV_BENCH_BATTERY_FULL_VOLTAGE 12.6f
#define AGV_BENCH_BATTERY_MAX_VOLTAGE 12.9f
// He so rieng duoc luu tach khoi profile PWM.
#include "BatteryCalibration.h"
#define AGV_BENCH_BATTERY_VOLTAGE_OFFSET 0.0f

// ----------------------------------------------------------------------------
// CẤU HÌNH PHỤC HỒI KHI VĂNG KHỎI LINE Ở KHÚC CUA (Corner Lost-Line Recovery)
// ----------------------------------------------------------------------------
// Khi xe chạy nhanh vào cua bị văng khỏi line (mắt đọc thấy 000 từ 100/110 hoặc 001/011):
// Xe sẽ tự động quay ngược lại hướng văng để bắt lại line trong khoảng thời gian đã định.
#ifndef AGV_CORNER_RECOVERY_ENABLED
#define AGV_CORNER_RECOVERY_ENABLED 1           // 1: Bật tính năng cứu văng line; 0: Tắt
#endif

#ifndef AGV_CORNER_RECOVERY_MS
#define AGV_CORNER_RECOVERY_MS 350              // Thời gian quay về line (ms) - tăng nếu cua rộng, giảm nếu muốn dừng sớm
#endif

#ifndef AGV_CORNER_RECOVERY_PWM
#define AGV_CORNER_RECOVERY_PWM 165             // Tốc độ PWM bánh xe khi quay tìm lại line
#endif

#ifndef AGV_CORNER_RECOVERY_MEMORY_MS
#define AGV_CORNER_RECOVERY_MEMORY_MS 600       // Thời gian lưu nhớ hướng văng trước đó (ms)
#endif

#ifndef AGV_CORNER_RECOVERY_STEERING_SIGN
#define AGV_CORNER_RECOVERY_STEERING_SIGN 1     // 1: Chuẩn (+100/110 -> quay trái về line; 001/011 -> quay phải về line)
#endif

// ----------------------------------------------------------------------------
// CẤU HÌNH BÙ TỐC ĐỘ THEO BẬC THANG % PIN (Battery Step Speed Compensation)
// ----------------------------------------------------------------------------
// Tự động điều chỉnh bù PWM động cơ theo từng nấc % pin để xe luôn giữ tốc độ ổn định:
// - Pin cao (>= 80%): giảm bớt PWM để không bị phóng nhanh quá trớn hoặc văng cua.
// - Pin chuẩn (50% - 79%): mức pin chuẩn lúc căn chỉnh xe, giữ nguyên PWM (offset = 0).
// - Pin trung bình (30% - 49%): bù thêm một chút PWM để xe không bị chậm lại.
// - Pin yếu (< 30%): bù thêm PWM nhiều hơn để chống ỳ / kẹt bánh khi điện áp sụt thấp.
#ifndef AGV_BATTERY_SPEED_COMP_ENABLED
#define AGV_BATTERY_SPEED_COMP_ENABLED 1        // 1: Bật bù tốc độ theo bậc pin; 0: Tắt
#endif

// Các mốc ngưỡng % pin phân chia bậc:
#ifndef AGV_BATTERY_STEP_HIGH_PERCENT
#define AGV_BATTERY_STEP_HIGH_PERCENT 80.0f     // Ngưỡng pin cao (>= 80%)
#endif

#ifndef AGV_BATTERY_STEP_MID_PERCENT
#define AGV_BATTERY_STEP_MID_PERCENT 50.0f      // Ngưỡng pin chuẩn (>= 50% đến < 80%)
#endif

#ifndef AGV_BATTERY_STEP_LOW_PERCENT
#define AGV_BATTERY_STEP_LOW_PERCENT 30.0f      // Ngưỡng pin trung bình (>= 30% đến < 50%)
#endif

// Giá trị bù PWM (PWM Offset) cho từng bậc thang:
#ifndef AGV_BATTERY_PWM_OFFSET_HIGH
#define AGV_BATTERY_PWM_OFFSET_HIGH -10         // Pin cao (>= 80%): giảm 10 PWM (giảm tốc)
#endif

#ifndef AGV_BATTERY_PWM_OFFSET_NOMINAL
#define AGV_BATTERY_PWM_OFFSET_NOMINAL 0        // Pin chuẩn (50% - 79%): giữ nguyên mức đã tune
#endif

#ifndef AGV_BATTERY_PWM_OFFSET_MID
#define AGV_BATTERY_PWM_OFFSET_MID 8            // Pin trung bình (30% - 49%): tăng thêm 8 PWM
#endif

#ifndef AGV_BATTERY_PWM_OFFSET_LOW
#define AGV_BATTERY_PWM_OFFSET_LOW 15           // Pin yếu (< 30%): tăng thêm 15 PWM chống ỳ
#endif
