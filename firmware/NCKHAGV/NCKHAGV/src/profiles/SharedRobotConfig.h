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
