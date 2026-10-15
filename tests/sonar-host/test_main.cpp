#include "sensors/SensorModule.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstring>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(false)
int main() {
    try {
        SensorModule front, rear(Config::REAR_ULTRASONIC_TRIG_PIN,Config::REAR_ULTRASONIC_ECHO_PIN);
        front.begin();rear.begin();
        CHECK(SonarIo::modes.at(12)==OUTPUT && SonarIo::modes.at(13)==INPUT);
        CHECK(SonarIo::modes.at(0)==OUTPUT && SonarIo::modes.at(15)==INPUT);
        CHECK(!front.valid() && !rear.valid());
        CHECK(front.diagnostic().resultAgeMs == UINT32_MAX);
        CHECK(front.diagnostic().triggers == 0 && front.diagnostic().edges == 0);
        front.update();
        CHECK(front.diagnostic().waiting && front.diagnostic().triggers == 1);
        SonarIo::fire(13,HIGH,100);SonarIo::fire(13,LOW,974);
        CHECK(front.diagnostic().ready); // Inspecting must not consume the ISR result.
        CHECK(front.diagnostic().ready);
        front.update(false);
        CHECK(front.valid() && front.isObstacleDetected());
        CHECK(std::fabs(front.readDistanceCm()-14.9891f)<0.01f);
        CHECK(!rear.valid());
        CHECK(front.diagnostic().pulseUs == 874 && front.diagnostic().edges == 2);
        CHECK(std::strcmp(front.diagnostic().result, "RANGE") == 0);
        SonarIo::timeUs=50000;rear.update();
        SonarIo::fire(15,HIGH,50100);SonarIo::fire(15,LOW,51558);rear.update(false);
        CHECK(rear.valid() && !rear.isObstacleDetected());
        CHECK(std::fabs(rear.readDistanceCm()-25.0047f)<0.01f);
        CHECK(front.isObstacleDetected());
        SonarIo::timeUs=100000;front.update();
        SonarIo::timeUs=100000+Config::ULTRASONIC_ECHO_TIMEOUT_US;front.update(false);
        CHECK(front.valid() && !front.isObstacleDetected() && std::isnan(front.readDistanceCm()));
        CHECK(std::strcmp(front.diagnostic().result, "NO_ECHO_LOW") == 0);
        SonarIo::timeUs=170000;front.update();
        SonarIo::fire(13,HIGH,170100); // Kẹt HIGH sau timeout.
        const uint32_t faultTime = 170000+Config::ULTRASONIC_ECHO_TIMEOUT_US+1000;
        SonarIo::timeUs=faultTime;front.update(false);
        CHECK(!front.valid() && std::isnan(front.readDistanceCm()));
        CHECK(front.diagnostic().echoLevel == HIGH);
        CHECK(std::strcmp(front.diagnostic().result, "TIMEOUT_INVALID") == 0);
        const auto before = SonarIo::triggered.size();
        front.diagnostic();
        CHECK(SonarIo::triggered.size() == before); // Diagnostic never triggers a measurement.
        SonarIo::timeUs=500000;rear.update(false);CHECK(!rear.valid());
        CHECK(SonarIo::triggered.size()==4);
        CHECK(SonarIo::triggered[0]==12 && SonarIo::triggered[1]==0);
        CHECK(front.diagnostic().resultAgeMs == 500-faultTime/1000);
        front.clearReading();
        CHECK(front.diagnostic().resultAgeMs == UINT32_MAX && !front.diagnostic().valid);
        std::cout << "PASS dual sonar HAL: pinout, independent ISR/range, 15cm, no echo LOW, stuck HIGH, stale\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
