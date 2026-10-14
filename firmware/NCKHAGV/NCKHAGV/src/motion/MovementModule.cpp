#include "MovementModule.h"
#include "core/ControlLogic.h"

void MovementModule::moveForward() { motor_.setMotorSpeed(Config::BASE_SPEED); motor_.motorForward(); buzzer_.off(); action_ = MotorAction::Forward; }
void MovementModule::moveBackward() {
    motor_.setMotorSpeed(Config::REVERSE_SPEED);
    motor_.motorBackward();
    action_ = MotorAction::Backward;
    buzzer_.setReverse(AGVMath::reverseBuzzerRequired(action_, motor_.leftDuty(), motor_.rightDuty()));
}
void MovementModule::correctBackwardLeft() {
    // One-sided correction while reversing, separate from a pivot turn.
    motor_.setLeftSpeed(0);
    motor_.setRightSpeed(Config::REVERSE_CORRECTION_SPEED + Config::RIGHT_SPEED_TRIM);
    motor_.motorBackward();
    action_ = MotorAction::ReverseCorrectLeft;
    buzzer_.setReverse(AGVMath::reverseBuzzerRequired(action_, motor_.leftDuty(), motor_.rightDuty()));
}
void MovementModule::correctBackwardRight() {
    motor_.setLeftSpeed(Config::REVERSE_CORRECTION_SPEED + Config::LEFT_SPEED_TRIM);
    motor_.setRightSpeed(0);
    motor_.motorBackward();
    action_ = MotorAction::ReverseCorrectRight;
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
void MovementModule::apply(MotorAction action) {
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
