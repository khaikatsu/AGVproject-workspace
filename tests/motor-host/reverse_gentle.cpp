#include "motion/MovementModule.h"
#include "core/ControlLogic.h"
#include <iostream>
#include <stdexcept>

#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (false)
int main() {
    try {
        MotorModule motor; motor.begin();
        LineFollowModule line(motor); BuzzerModule buzzer; buzzer.begin();
        MovementModule movement(motor,line,buzzer);
        movement.apply(MotorAction::Backward);
        CHECK(motor.leftDuty()==-(Config::REVERSE_SPEED+Config::REVERSE_LEFT_SPEED_TRIM));
        CHECK(motor.rightDuty()==-(Config::REVERSE_SPEED+Config::REVERSE_RIGHT_SPEED_TRIM));
#if defined(AGV_ROBOT_PROFILE_02)
        static_assert(Config::REVERSE_GENTLE_CORRECTION && Config::REVERSE_SPEED==200 &&
                      Config::REVERSE_CORRECTION_SPEED==200 && Config::REVERSE_CORRECTION_DELTA==20);
        CHECK(motor.leftDuty()==-205 && motor.rightDuty()==-190);
        for(bool settling : {false,true}) {
            for(unsigned pattern : {1u,3u,4u,6u}) {
                movement.apply(AGVMath::reverseLineAction(pattern),false,Config::LINE_RECOVERY_PWM,settling);
                const bool rightEye=pattern==1 || pattern==3;
                CHECK(motor.leftDuty()==-(rightEye ? 205 : 185));
                CHECK(motor.rightDuty()==-(rightEye ? 170 : 190));
                CHECK(buzzer.pinLevel()==Config::BUZZER_ACTIVE_STATE);
                CHECK(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]]==static_cast<int>(-motor.leftDuty()));
                CHECK(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]]==static_cast<int>(-motor.rightDuty()));
            }
        }
#else
        static_assert(!Config::REVERSE_GENTLE_CORRECTION);
        movement.apply(MotorAction::ReverseCorrectLeft);
        CHECK(motor.leftDuty()==0 && motor.rightDuty()==-(Config::REVERSE_CORRECTION_SPEED+Config::REVERSE_RIGHT_SPEED_TRIM));
        movement.apply(MotorAction::ReverseCorrectRight);
        CHECK(motor.rightDuty()==0 && motor.leftDuty()==-(Config::REVERSE_CORRECTION_SPEED+Config::REVERSE_LEFT_SPEED_TRIM));
#endif
        movement.apply(MotorAction::Forward);
        CHECK(motor.leftDuty()==Config::BASE_SPEED+Config::LEFT_SPEED_TRIM);
        CHECK(motor.rightDuty()==Config::BASE_SPEED+Config::RIGHT_SPEED_TRIM);
        movement.apply(MotorAction::PivotLeft);
        CHECK(motor.leftDuty()==Config::TURN_SPEED+Config::TURN_LEFT_SPEED_TRIM);
        CHECK(motor.rightDuty()==-(Config::TURN_SPEED+Config::TURN_RIGHT_SPEED_TRIM));
        movement.apply(AGVMath::reverseLineAction(0));
        CHECK(motor.leftDuty()==0 && motor.rightDuty()==0);
        CHECK(MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[0]]==0 && MotorIo::pwm[Config::MOTOR_PWM_CHANNELS[1]]==0);
        CHECK(buzzer.pinLevel()!=Config::BUZZER_ACTIVE_STATE);
        std::cout << "PASS " << Config::AGV_ID << ": reverse balance/correction, settling, PWM, buzzer, forward, pivot, STOP\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
