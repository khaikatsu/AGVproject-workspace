#include "motion/MovementModule.h"
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main() {
    MotorModule motor; motor.begin();
    LineFollowModule line(motor);
    BuzzerModule buzzer; buzzer.begin();
    MovementModule movement(motor, line, buzzer);
#if defined(AGV_ROBOT_PROFILE_01)
    constexpr int forwardLeft = 180, forwardRight = 220;
    constexpr int reverseLeft = 150, reverseRight = 190, turn = 210;
    constexpr int correctionLeft = 180, correctionRight = 220;
    static_assert(Config::TURN_TIMEOUT_MS == 2500, "Robot01 turn timeout changed");
#elif defined(AGV_ROBOT_PROFILE_02)
    constexpr int forwardLeft = 245, forwardRight = 215;
    constexpr int reverseLeft = 215, reverseRight = 195, turn = 230;
    constexpr int correctionLeft = 250, correctionRight = 220;
    constexpr int reverseCorrectionLeft = 225, reverseCorrectionRight = 205;
    static_assert(Config::BASE_SPEED == 230 && Config::REVERSE_SPEED == 210,
                  "Robot02 forward/reverse base PWM changed");
    static_assert(Config::CORRECTION_SPEED == 235 && Config::GENTLE_CORRECTION_SPEED == 235,
                  "Robot02 correction PWM changed");
    static_assert(Config::TURN_TIMEOUT_MS == 2500, "Robot02 turn timeout changed");
#else
#error "Run the motor adapter with robot profile 01 or 02"
#endif
#if defined(AGV_ROBOT_PROFILE_01)
    constexpr int reverseCorrectionLeft = reverseLeft, reverseCorrectionRight = reverseRight;
#endif
    movement.apply(MotorAction::Forward);
    check(motor.leftDuty() == forwardLeft && motor.rightDuty() == forwardRight, "Forward tuning changed");
    movement.apply(MotorAction::Backward);
    check(motor.leftDuty() == -reverseLeft && motor.rightDuty() == -reverseRight, "Reverse PWM/sign incorrect");
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]] == reverseLeft &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]] == reverseRight, "Reverse PWM not applied to driver");
    check(MotorIo::pins[Config::BUZZER_PIN] == Config::BUZZER_ACTIVE_STATE, "Reverse buzzer inactive");
    check(buzzer.pinLevel() == LOW, "Reverse GPIO0 readback should be LOW");
    movement.apply(MotorAction::ReverseCorrectLeft);
    check(motor.leftDuty() == 0 && motor.rightDuty() == -reverseCorrectionRight, "Left reverse correction too fast or wrong sign");
    check(buzzer.pinLevel() == LOW, "Left reverse correction silenced buzzer");
    movement.apply(MotorAction::ReverseCorrectRight);
    check(motor.leftDuty() == -reverseCorrectionLeft && motor.rightDuty() == 0, "Right reverse correction too fast or wrong sign");
    check(buzzer.pinLevel() == LOW, "Right reverse correction silenced buzzer");
    movement.apply(MotorAction::Backward, false, Config::LINE_RECOVERY_PWM, true);
    check(motor.leftDuty() == -std::min(reverseLeft, Config::LEFT_TURN_CAPTURE_PWM) &&
          motor.rightDuty() == -std::min(reverseRight, Config::LEFT_TURN_CAPTURE_PWM),
          "Settling reverse did not use reverse-specific balance");
    movement.apply(MotorAction::PivotLeft);
    check(motor.leftDuty() == turn && motor.rightDuty() == -turn, "Pivot tuning changed");
    check(buzzer.pinLevel() == HIGH, "Pivot did not silence buzzer");
    movement.apply(MotorAction::PivotRight);
    check(motor.leftDuty() == -turn && motor.rightDuty() == turn, "Right pivot duty/sign incorrect");
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]] == turn &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]] == turn, "Pivot PWM not applied to both driver channels");
    movement.apply(MotorAction::CorrectLeft);
    check(motor.leftDuty() == correctionLeft && motor.rightDuty() == 0, "Forward correction tuning changed");
    movement.apply(MotorAction::CorrectRight);
    check(motor.leftDuty() == 0 && motor.rightDuty() == correctionRight, "Right correction tuning changed");
    movement.apply(MotorAction::GentleCorrectLeft);
    check(motor.leftDuty() == correctionLeft && motor.rightDuty() == correctionRight, "Gentle left correction tuning changed");
    movement.apply(MotorAction::GentleCorrectRight);
    check(motor.leftDuty() == correctionLeft && motor.rightDuty() == correctionRight, "Gentle right correction tuning changed");
    movement.apply(MotorAction::PivotLeft, true);
    check(motor.leftDuty() == 180 && motor.rightDuty() == -180, "Recovery left pivot duty/sign incorrect");
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]] == 180 &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]] == 180, "Recovery PWM not applied to driver");
    movement.apply(MotorAction::Stop, true);
    check(motor.leftDuty() == 0 && motor.rightDuty() == 0, "Recovery rest did not stop motors");
    movement.apply(MotorAction::PivotRight, true);
    check(motor.leftDuty() == -180 && motor.rightDuty() == 180, "Recovery right pivot duty/sign incorrect");
    check(buzzer.pinLevel() == HIGH, "Recovery pivot did not silence buzzer");
    movement.apply(MotorAction::Stop);
    check(motor.leftDuty() == 0 && motor.rightDuty() == 0 &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]] == 0 &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]] == 0, "STOP did not zero driver PWM");
    check(MotorIo::pins[Config::BUZZER_PIN] != Config::BUZZER_ACTIVE_STATE, "STOP did not silence buzzer");
    check(buzzer.pinLevel() == HIGH, "STOP GPIO0 readback should be HIGH");
    for (int i=0;i<50;++i) {
        movement.apply(MotorAction::Backward);
        check(buzzer.pinLevel() == LOW, "Repeated reverse update silenced buzzer");
    }
    movement.apply(MotorAction::Forward);
    check(buzzer.pinLevel() == HIGH, "Forward after reverse did not silence buzzer");
    movement.stop();
    std::cout << "PASS " << Config::AGV_ID << ": forward, reverse, correction, pivot, buzzer and STOP driver output\n";
}
