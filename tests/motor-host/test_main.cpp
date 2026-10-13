#include "motion/MotorModule.h"
#include <iostream>
#include <stdexcept>
static void check(bool result) { if (!result) throw std::runtime_error("Motor pivot bridge check failed"); }
int main() {
    MotorModule motor;motor.begin();check(motor.configured());
    motor.setLeftSpeed(Config::TURN_SPEED);motor.setRightSpeed(Config::TURN_SPEED);
    motor.motorLeft();check(motor.leftDuty()*motor.rightDuty()<0);
    check(std::abs(motor.leftDuty())==245 && std::abs(motor.rightDuty())==245);
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]]==245 && MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]]==245);
    const int left=motor.leftDuty(),right=motor.rightDuty();motor.motorRight();
    check(motor.leftDuty()==-left && motor.rightDuty()==-right);
    motor.motorStop();check(motor.leftDuty()==0 && motor.rightDuty()==0);
    check(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]]==0 && MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]]==0);
    std::cout<<"PASS motor adapter: both pivot wheels have opposite signs at 245, LEFT/RIGHT invert, STOP zeroes both bridges; physical direction still needs bench verification\n";
}
