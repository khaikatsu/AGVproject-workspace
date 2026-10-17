#pragma once
#include "LineFollowModule.h"
#include "actuators/BuzzerModule.h"

class MovementModule {
public:
    MovementModule(MotorModule& motor, LineFollowModule& line, BuzzerModule& buzzer)
        : motor_(motor), line_(line), buzzer_(buzzer) {}
    void begin() { stop(); }
    void moveForward();
    void moveBackward();
    void correctBackwardLeft();
    void correctBackwardRight();
    void turnLeft();
    void turnRight();
    void stop();
    void followLine();
    void apply(MotorAction action, bool lineRecovery = false,
               int precisionPwm = Config::LINE_RECOVERY_PWM, bool settling = false);
    MotorAction action() const { return action_; }
private:
    void applyReverseCorrection(MotorAction action, int pwmCap = 255);
    MotorModule& motor_;
    LineFollowModule& line_;
    BuzzerModule& buzzer_;
    MotorAction action_ = MotorAction::Stop;
};
