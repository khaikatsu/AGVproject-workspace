#pragma once
#include <stdint.h>
#include <stddef.h>

// GPIO numbers from bang-phan-chan-agv.md (03/10/2026), not board pin order.
namespace Config {
enum class MotorDriver : uint8_t { L298N, BTS7960 };
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint8_t LEFT_ENA = 14, LEFT_IN1 = 27, LEFT_IN2 = 26;
constexpr uint8_t RIGHT_ENB = 25, RIGHT_IN3 = 33, RIGHT_IN4 = 32;
constexpr uint8_t LINE_LEFT_PIN = 39, LINE_CENTER_PIN = 34, LINE_RIGHT_PIN = 35;
constexpr uint8_t ULTRASONIC_TRIG_PIN = 12, ULTRASONIC_ECHO_PIN = 13;
constexpr uint8_t RFID_SS_PIN = 5;
constexpr uint8_t SERVO_PINS[] = {15, 17, 21};
constexpr uint8_t BATTERY_ADC_PIN = 36, BUZZER_PIN = 0, CARGO_IR_PIN = 2;
constexpr uint32_t MOTOR_PWM_HZ = 20000;
constexpr uint8_t MOTOR_PWM_BITS = 8;
constexpr uint32_t SERVO_PWM_HZ = 50;
constexpr uint8_t SERVO_PWM_BITS = 16;
}  // namespace Config
