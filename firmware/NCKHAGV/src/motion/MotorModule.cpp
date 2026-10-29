#include "MotorModule.h"

bool MotorModule::configured() const {
    return initialized_ && Config::MOTOR_DRIVER != Config::MotorDriver::Unconfigured && Config::MOTOR_DIRECTION_CONFIRMED;
}
void MotorModule::begin() {
    for (int pin : {Config::LEFT_ENA, Config::LEFT_IN1, Config::LEFT_IN2,
                    Config::RIGHT_ENB, Config::RIGHT_IN3, Config::RIGHT_IN4}) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
    if (Config::MOTOR_DRIVER == Config::MotorDriver::L298N) {
        for (int channel : {Config::MOTOR_PWM_CHANNELS[0], Config::MOTOR_PWM_CHANNELS[1]}) {
            ledcSetup(channel, Config::MOTOR_PWM_HZ, Config::MOTOR_PWM_BITS);
            ledcWrite(channel, 0);
        }
        ledcAttachPin(Config::LEFT_ENA, Config::MOTOR_PWM_CHANNELS[0]);
        ledcAttachPin(Config::RIGHT_ENB, Config::MOTOR_PWM_CHANNELS[1]);
    } else if (Config::MOTOR_DRIVER == Config::MotorDriver::BTS7960) {
        const int pins[] = {Config::LEFT_IN1, Config::LEFT_IN2, Config::RIGHT_IN3, Config::RIGHT_IN4};
        for (size_t i = 0; i < Config::MOTOR_PWM_CHANNELS.size(); ++i) {
            ledcSetup(Config::MOTOR_PWM_CHANNELS[i], Config::MOTOR_PWM_HZ, Config::MOTOR_PWM_BITS);
            ledcWrite(Config::MOTOR_PWM_CHANNELS[i], 0);
            ledcAttachPin(pins[i], Config::MOTOR_PWM_CHANNELS[i]);
        }
    }
    initialized_ = true;
    motorStop();
}
void MotorModule::setLeftSpeed(int speed) { leftSpeed_ = constrain(speed, 0, 255); }
void MotorModule::setRightSpeed(int speed) { rightSpeed_ = constrain(speed, 0, 255); }
void MotorModule::setMotorSpeed(int speed) {
    setLeftSpeed(speed + Config::LEFT_SPEED_TRIM);
    setRightSpeed(speed + Config::RIGHT_SPEED_TRIM);
}
void MotorModule::motorForward() { drive(leftSpeed_, rightSpeed_); }
void MotorModule::motorBackward() { drive(-leftSpeed_, -rightSpeed_); }
// On robot 01, the physical GS1 turn shows this bridge pattern turns left.
void MotorModule::motorLeft() { drive(leftSpeed_, -rightSpeed_); }
void MotorModule::motorRight() { drive(-leftSpeed_, rightSpeed_); }
void MotorModule::motorStop() {
    if (!initialized_) return;
    writeSide(true, 0);
    writeSide(false, 0);
    appliedLeft_ = appliedRight_ = 0;
}
void MotorModule::drive(int left, int right) {
    if (!Config::ENABLE_CORE_MOTION || !configured()) { motorStop(); return; }
    int targetLeft = left;
    int targetRight = right;
    if (batteryCompOffset_ != 0) {
        if (targetLeft > 0) targetLeft = constrain(targetLeft + batteryCompOffset_, 1, 255);
        else if (targetLeft < 0) targetLeft = constrain(targetLeft - batteryCompOffset_, -255, -1);

        if (targetRight > 0) targetRight = constrain(targetRight + batteryCompOffset_, 1, 255);
        else if (targetRight < 0) targetRight = constrain(targetRight - batteryCompOffset_, -255, -1);
    }
    writeSide(true, targetLeft);
    writeSide(false, targetRight);
    appliedLeft_ = targetLeft;
    appliedRight_ = targetRight;
}
void MotorModule::applyStartupBoost(int boost) {
    if (!initialized_ || boost <= 0) return;
    if (appliedLeft_ == 0 && appliedRight_ == 0) return;
    int newLeft = appliedLeft_;
    int newRight = appliedRight_;
    if (newLeft > 0) newLeft = constrain(newLeft + boost, 0, 255);
    else if (newLeft < 0) newLeft = constrain(newLeft - boost, -255, 0);
    if (newRight > 0) newRight = constrain(newRight + boost, 0, 255);
    else if (newRight < 0) newRight = constrain(newRight - boost, -255, 0);
    if (newLeft != appliedLeft_) writeSide(true, newLeft);
    if (newRight != appliedRight_) writeSide(false, newRight);
    appliedLeft_ = newLeft;
    appliedRight_ = newRight;
}
void MotorModule::writeSide(bool left, int duty) {
    const int enable = left ? Config::LEFT_ENA : Config::RIGHT_ENB;
    const int a = left ? Config::LEFT_IN1 : Config::RIGHT_IN3;
    const int b = left ? Config::LEFT_IN2 : Config::RIGHT_IN4;
    const bool inverted = left ? Config::LEFT_FORWARD_INVERTED : Config::RIGHT_FORWARD_INVERTED;
    if (inverted) duty = -duty;
    if (Config::MOTOR_DRIVER == Config::MotorDriver::L298N) {
        const int channel = Config::MOTOR_PWM_CHANNELS[left ? 0 : 1];
        if (Config::FAST_LINE_RESPONSE) {
            const int priorLogical = left ? appliedLeft_ : appliedRight_;
            const int prior = inverted ? -priorLogical : priorLogical;
            if (duty == prior) return;
            if ((duty > 0 && prior > 0) || (duty < 0 && prior < 0)) {
                // Same direction: update PWM directly, no zero-duty notch.
                ledcWrite(channel, abs(duty));
                return;
            }
        }
        ledcWrite(channel, 0);
        digitalWrite(a, duty > 0 ? HIGH : LOW);
        digitalWrite(b, duty < 0 ? HIGH : LOW);
        ledcWrite(channel, abs(duty));
    } else if (Config::MOTOR_DRIVER == Config::MotorDriver::BTS7960) {
        // Per-side adapter: enable -> joined R_EN/L_EN, a -> RPWM, b -> LPWM.
        const size_t index = left ? 0 : 2;
        digitalWrite(enable, LOW);
        ledcWrite(Config::MOTOR_PWM_CHANNELS[index], 0);
        ledcWrite(Config::MOTOR_PWM_CHANNELS[index + 1], 0);
        if (duty) {
            ledcWrite(Config::MOTOR_PWM_CHANNELS[index + (duty < 0 ? 1 : 0)], abs(duty));
            digitalWrite(enable, HIGH);
        }
    } else {
        digitalWrite(enable, LOW);
        digitalWrite(a, LOW);
        digitalWrite(b, LOW);
    }
}
