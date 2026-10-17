#include "MovementModule.h"
#include "core/ControlLogic.h"
#include <algorithm>

void MovementModule::moveForward() { motor_.setMotorSpeed(Config::BASE_SPEED); motor_.motorForward(); buzzer_.off(); action_ = MotorAction::Forward; }
void MovementModule::moveBackward() {
    motor_.setLeftSpeed(Config::REVERSE_SPEED + Config::REVERSE_LEFT_SPEED_TRIM);
    motor_.setRightSpeed(Config::REVERSE_SPEED + Config::REVERSE_RIGHT_SPEED_TRIM);
    motor_.motorBackward();
    action_ = MotorAction::Backward;
    buzzer_.setReverse(AGVMath::reverseBuzzerRequired(action_, motor_.leftDuty(), motor_.rightDuty()));
}
void MovementModule::correctBackwardLeft() {
    applyReverseCorrection(MotorAction::ReverseCorrectLeft);
}
void MovementModule::correctBackwardRight() {
    applyReverseCorrection(MotorAction::ReverseCorrectRight);
}
void MovementModule::applyReverseCorrection(MotorAction action, int pwmCap) {
    const bool slowLeft = action == MotorAction::ReverseCorrectLeft;
    int left = Config::REVERSE_CORRECTION_SPEED + Config::REVERSE_LEFT_SPEED_TRIM;
    int right = Config::REVERSE_CORRECTION_SPEED + Config::REVERSE_RIGHT_SPEED_TRIM;
    if (Config::REVERSE_GENTLE_CORRECTION) {
        // Preserve the existing steering direction and reverse wheel balance;
        // reduce one side slightly instead of stopping it completely.
        if (slowLeft) left -= Config::REVERSE_CORRECTION_DELTA;
        else right -= Config::REVERSE_CORRECTION_DELTA;
    } else {
        if (slowLeft) left = 0;
        else right = 0;
    }
    motor_.setLeftSpeed(std::min(left, pwmCap));
    motor_.setRightSpeed(std::min(right, pwmCap));
    motor_.motorBackward();
    action_ = action;
    buzzer_.setReverse(AGVMath::reverseBuzzerRequired(action_, motor_.leftDuty(), motor_.rightDuty()));
}
void MovementModule::turnLeft() {
    motor_.setLeftSpeed(Config::TURN_SPEED + Config::TURN_LEFT_SPEED_TRIM);
    motor_.setRightSpeed(Config::TURN_SPEED + Config::TURN_RIGHT_SPEED_TRIM);
    motor_.motorLeft(); buzzer_.off(); action_ = MotorAction::PivotLeft;
}
void MovementModule::turnRight() {
    motor_.setLeftSpeed(Config::TURN_SPEED + Config::TURN_LEFT_SPEED_TRIM);
    motor_.setRightSpeed(Config::TURN_SPEED + Config::TURN_RIGHT_SPEED_TRIM);
    motor_.motorRight(); buzzer_.off(); action_ = MotorAction::PivotRight;
}
void MovementModule::stop() { motor_.motorStop(); buzzer_.off(); action_ = MotorAction::Stop; }
void MovementModule::followLine() { apply(AGVMath::lineAction(line_.readSensors().pattern)); }
void MovementModule::apply(MotorAction action, bool lineRecovery, int precisionPwm, bool settling) {
    if (settling && Config::REVERSE_GENTLE_CORRECTION &&
        (action == MotorAction::ReverseCorrectLeft || action == MotorAction::ReverseCorrectRight)) {
        applyReverseCorrection(action, Config::LEFT_TURN_CAPTURE_PWM);
        return;
    }
    if (settling && action != MotorAction::Stop && action != MotorAction::PivotLeft && action != MotorAction::PivotRight) {
        const bool reverse = action == MotorAction::Backward || action == MotorAction::ReverseCorrectLeft ||
            action == MotorAction::ReverseCorrectRight;
        const bool leftOnly = action == MotorAction::CorrectLeft || action == MotorAction::ReverseCorrectRight;
        const bool rightOnly = action == MotorAction::CorrectRight || action == MotorAction::ReverseCorrectLeft;
        // Cap actual per-wheel PWM including trims, preserving zero on the
        // inactive wheel during correction and the reverse buzzer contract.
        const int base = reverse ? Config::REVERSE_SPEED : Config::BASE_SPEED;
        const int leftTrim = reverse ? Config::REVERSE_LEFT_SPEED_TRIM : Config::LEFT_SPEED_TRIM;
        const int rightTrim = reverse ? Config::REVERSE_RIGHT_SPEED_TRIM : Config::RIGHT_SPEED_TRIM;
        motor_.setLeftSpeed(rightOnly ? 0 : std::min(base + leftTrim, Config::LEFT_TURN_CAPTURE_PWM));
        motor_.setRightSpeed(leftOnly ? 0 : std::min(base + rightTrim, Config::LEFT_TURN_CAPTURE_PWM));
        if (reverse) motor_.motorBackward(); else motor_.motorForward();
        action_ = action;
        buzzer_.setReverse(AGVMath::reverseBuzzerRequired(action_, motor_.leftDuty(), motor_.rightDuty()));
        return;
    }
    if (lineRecovery && (action == MotorAction::PivotLeft || action == MotorAction::PivotRight)) {
        motor_.setLeftSpeed(precisionPwm);
        motor_.setRightSpeed(precisionPwm);
        if (action == MotorAction::PivotLeft) motor_.motorLeft();
        else motor_.motorRight();
        buzzer_.off(); action_ = action;
        return;
    }
    switch (action) {
        case MotorAction::Stop: stop(); break;
        case MotorAction::Forward: moveForward(); break;
        case MotorAction::Backward: moveBackward(); break;
        case MotorAction::ReverseCorrectLeft: correctBackwardLeft(); break;
        case MotorAction::ReverseCorrectRight: correctBackwardRight(); break;
        case MotorAction::PivotLeft: turnLeft(); break;
        case MotorAction::PivotRight: turnRight(); break;
        case MotorAction::CorrectLeft: line_.correctLeft(); buzzer_.off(); action_ = action; break;
        case MotorAction::CorrectRight: line_.correctRight(); buzzer_.off(); action_ = action; break;
        case MotorAction::GentleCorrectLeft: line_.gentleCorrectLeft(); buzzer_.off(); action_ = action; break;
        case MotorAction::GentleCorrectRight: line_.gentleCorrectRight(); buzzer_.off(); action_ = action; break;
    }
}
