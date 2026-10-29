#include "motion/MotorModule.h"
#include <iostream>
#include <cassert>
#include <cmath>

static void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

int main() {
    std::cout << "[TEST] Running battery step speed compensation host test..." << std::endl;

    // 1. Kiểm tra bảng tra batteryStepPwmOffset
    check(Config::batteryStepPwmOffset(NAN) == 0, "NaN percentage should return 0 offset");
    check(Config::batteryStepPwmOffset(-1.0f) == 0, "Negative percentage should return 0 offset");
    check(Config::batteryStepPwmOffset(100.0f) == Config::BATTERY_PWM_OFFSET_HIGH, "100% should be high offset (-10)");
    check(Config::batteryStepPwmOffset(85.0f) == Config::BATTERY_PWM_OFFSET_HIGH, "85% should be high offset (-10)");
    check(Config::batteryStepPwmOffset(80.0f) == Config::BATTERY_PWM_OFFSET_HIGH, "80.0% boundary should be high offset (-10)");
    check(Config::batteryStepPwmOffset(79.9f) == Config::BATTERY_PWM_OFFSET_NOMINAL, "79.9% should be nominal offset (0)");
    check(Config::batteryStepPwmOffset(65.0f) == Config::BATTERY_PWM_OFFSET_NOMINAL, "65% should be nominal offset (0)");
    check(Config::batteryStepPwmOffset(50.0f) == Config::BATTERY_PWM_OFFSET_NOMINAL, "50.0% boundary should be nominal offset (0)");
    check(Config::batteryStepPwmOffset(49.9f) == Config::BATTERY_PWM_OFFSET_MID, "49.9% should be mid offset (+8)");
    check(Config::batteryStepPwmOffset(35.0f) == Config::BATTERY_PWM_OFFSET_MID, "35% should be mid offset (+8)");
    check(Config::batteryStepPwmOffset(30.0f) == Config::BATTERY_PWM_OFFSET_MID, "30.0% boundary should be mid offset (+8)");
    check(Config::batteryStepPwmOffset(29.9f) == Config::BATTERY_PWM_OFFSET_LOW, "29.9% should be low offset (+15)");
    check(Config::batteryStepPwmOffset(15.0f) == Config::BATTERY_PWM_OFFSET_LOW, "15% should be low offset (+15)");
    check(Config::batteryStepPwmOffset(0.0f) == Config::BATTERY_PWM_OFFSET_LOW, "0% should be low offset (+15)");

    // 2. Kiểm tra bù tốc độ trên MotorModule
    MotorModule motor;
    motor.begin();
    check(motor.configured(), "Motor configured");

    // Baseline (offset = 0)
    motor.setBatteryCompOffset(0);
    motor.setLeftSpeed(140);
    motor.setRightSpeed(140);
    motor.motorForward();
    check(motor.leftDuty() == 140 && motor.rightDuty() == 140, "Baseline forward 140");

    // Pin cao (>= 80%): offset = -10 -> PWM giảm từ 140 xuống 130
    motor.setBatteryCompOffset(Config::batteryStepPwmOffset(90.0f));
    check(motor.batteryCompOffset() == -10, "Offset is -10 at 90%");
    motor.motorForward();
    check(motor.leftDuty() == 130 && motor.rightDuty() == 130, "High battery forward should drop by 10 PWM to 130");

    // Lùi xe khi pin cao: magnitude giảm từ 140 xuống 130 (-140 -> -130)
    motor.motorBackward();
    check(motor.leftDuty() == -130 && motor.rightDuty() == -130, "High battery backward should drop magnitude to -130");

    // Quay xe khi pin cao: (+140, -140) -> (+130, -130)
    motor.motorLeft();
    check(motor.leftDuty() == 130 && motor.rightDuty() == -130, "High battery pivot left should be (+130, -130)");

    // Pin yếu (< 30%): offset = +15 -> PWM tăng từ 140 lên 155
    motor.setBatteryCompOffset(Config::batteryStepPwmOffset(25.0f));
    check(motor.batteryCompOffset() == 15, "Offset is +15 at 25%");
    motor.motorForward();
    check(motor.leftDuty() == 155 && motor.rightDuty() == 155, "Low battery forward should boost by 15 PWM to 155");

    // Lùi xe khi pin yếu: magnitude tăng từ 140 lên 155 (-140 -> -155)
    motor.motorBackward();
    check(motor.leftDuty() == -155 && motor.rightDuty() == -155, "Low battery backward should boost magnitude to -155");

    // Dừng 1 bên bánh (bẻ cua một bánh dừng): bánh 0 PWM không được phép quay
    motor.setLeftSpeed(140);
    motor.setRightSpeed(0);
    motor.motorForward();
    check(motor.leftDuty() == 155 && motor.rightDuty() == 0, "Idle wheel (duty 0) must remain 0, never boosted");

    // Dừng toàn bộ (motorStop): cả 2 bánh = 0
    motor.motorStop();
    check(motor.leftDuty() == 0 && motor.rightDuty() == 0, "motorStop zeroes both duties");

    std::cout << "PASS battery step speed compensation host test: table lookups and motor drive offsets verified!" << std::endl;
    return 0;
}
