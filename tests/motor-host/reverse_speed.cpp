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
    static_assert(Config::TURN_TIMEOUT_MS == 2500, "Robot01 turn timeout changed");
#elif defined(AGV_ROBOT_PROFILE_02)
    static_assert(Config::TURN_TIMEOUT_MS == 2500, "Robot02 turn timeout changed");
#else
#error "Run the motor adapter with robot profile 01 or 02"
#endif
    constexpr int forwardLeft = Config::BASE_SPEED + Config::LEFT_SPEED_TRIM;
    constexpr int forwardRight = Config::BASE_SPEED + Config::RIGHT_SPEED_TRIM;
    constexpr int reverseLeft = Config::REVERSE_SPEED + Config::REVERSE_LEFT_SPEED_TRIM;
    constexpr int reverseRight = Config::REVERSE_SPEED + Config::REVERSE_RIGHT_SPEED_TRIM;
    constexpr int turnLeft = Config::TURN_SPEED + Config::TURN_LEFT_SPEED_TRIM;
    constexpr int turnRight = Config::TURN_SPEED + Config::TURN_RIGHT_SPEED_TRIM;
    constexpr int correctionLeft = Config::CORRECTION_SPEED + Config::LEFT_SPEED_TRIM;
    constexpr int correctionRight = Config::CORRECTION_SPEED + Config::RIGHT_SPEED_TRIM;
    constexpr int reverseCorrectionLeft = Config::REVERSE_CORRECTION_SPEED + Config::REVERSE_LEFT_SPEED_TRIM;
    constexpr int reverseCorrectionRight = Config::REVERSE_CORRECTION_SPEED + Config::REVERSE_RIGHT_SPEED_TRIM;
    movement.apply(MotorAction::Forward);
    check(motor.leftDuty() == forwardLeft && motor.rightDuty() == forwardRight, "Forward tuning changed");
    movement.apply(MotorAction::Backward);
    check(motor.leftDuty() == -reverseLeft && motor.rightDuty() == -reverseRight, "Reverse PWM/sign incorrect");
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]] == reverseLeft &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]] == reverseRight, "Reverse PWM not applied to driver");
    check(MotorIo::pins[Config::BUZZER_PIN] == Config::BUZZER_ACTIVE_STATE, "Reverse buzzer inactive");
    check(buzzer.pinLevel() == LOW, "Reverse GPIO0 readback should be LOW");
    movement.apply(MotorAction::ReverseCorrectLeft);
    check(motor.leftDuty() == -(reverseCorrectionLeft-Config::REVERSE_CORRECTION_DELTA) &&
          motor.rightDuty() == -reverseCorrectionRight, "Left reverse correction too fast or wrong sign");
    check(buzzer.pinLevel() == LOW, "Left reverse correction silenced buzzer");
    movement.apply(MotorAction::ReverseCorrectRight);
    check(motor.leftDuty() == -reverseCorrectionLeft &&
          motor.rightDuty() == -(reverseCorrectionRight-Config::REVERSE_CORRECTION_DELTA), "Right reverse correction too fast or wrong sign");
    check(buzzer.pinLevel() == LOW, "Right reverse correction silenced buzzer");
    movement.apply(MotorAction::Backward, false, Config::LINE_RECOVERY_PWM, true);
    check(motor.leftDuty() == -std::min(reverseLeft, Config::LINE_RECOVERY_PWM) &&
          motor.rightDuty() == -std::min(reverseRight, Config::LINE_RECOVERY_PWM),
          "Settling reverse did not use reverse-specific balance");
    movement.apply(MotorAction::PivotLeft);
    check(motor.leftDuty() == turnLeft && motor.rightDuty() == -turnRight, "Pivot tuning changed");
    check(buzzer.pinLevel() == HIGH, "Pivot did not silence buzzer");
    movement.apply(MotorAction::PivotRight);
    check(motor.leftDuty() == -turnLeft && motor.rightDuty() == turnRight, "Right pivot duty/sign incorrect");
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]] == turnLeft &&
          MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]] == turnRight, "Pivot PWM not applied to both driver channels");
    movement.apply(MotorAction::CorrectLeft);
    check(motor.leftDuty() == correctionLeft && motor.rightDuty() == 0, "Forward correction tuning changed");
    movement.apply(MotorAction::CorrectRight);
    check(motor.leftDuty() == 0 && motor.rightDuty() == correctionRight, "Right correction tuning changed");
    movement.apply(MotorAction::GentleCorrectLeft);
    check(motor.leftDuty() == Config::GENTLE_OUTER_SPEED+Config::LEFT_SPEED_TRIM &&
          motor.rightDuty() == Config::GENTLE_CORRECTION_SPEED+Config::RIGHT_SPEED_TRIM, "Gentle left correction tuning changed");
    movement.apply(MotorAction::GentleCorrectRight);
    check(motor.leftDuty() == Config::GENTLE_CORRECTION_SPEED+Config::LEFT_SPEED_TRIM &&
          motor.rightDuty() == Config::GENTLE_OUTER_SPEED+Config::RIGHT_SPEED_TRIM, "Gentle right correction tuning changed");
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
