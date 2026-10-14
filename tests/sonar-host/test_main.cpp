#include "sensors/SensorModule.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(false)
int main() {
    try {
        SensorModule front, rear(Config::REAR_ULTRASONIC_TRIG_PIN,Config::REAR_ULTRASONIC_ECHO_PIN);
        front.begin();rear.begin();
        CHECK(SonarIo::modes.at(12)==OUTPUT && SonarIo::modes.at(13)==INPUT);
        CHECK(SonarIo::modes.at(0)==OUTPUT && SonarIo::modes.at(15)==INPUT);
        CHECK(!front.valid() && !rear.valid());
        front.update();
        SonarIo::fire(13,HIGH,100);SonarIo::fire(13,LOW,974);front.update(false);
        CHECK(front.valid() && front.isObstacleDetected());
        CHECK(std::fabs(front.readDistanceCm()-14.9891f)<0.01f);
        CHECK(!rear.valid());
        SonarIo::timeUs=50000;rear.update();
        SonarIo::fire(15,HIGH,50100);SonarIo::fire(15,LOW,51558);rear.update(false);
        CHECK(rear.valid() && !rear.isObstacleDetected());
        CHECK(std::fabs(rear.readDistanceCm()-25.0047f)<0.01f);
        CHECK(front.isObstacleDetected());
        SonarIo::timeUs=100000;front.update();
        SonarIo::timeUs=131000;front.update(false); // ECHO LOW, ngoài tầm theo sketch xe 01.
        CHECK(front.valid() && !front.isObstacleDetected() && std::isnan(front.readDistanceCm()));
        SonarIo::timeUs=160000;front.update();
        SonarIo::fire(13,HIGH,160100); // Kẹt HIGH sau timeout.
        SonarIo::timeUs=191000;front.update(false);
        CHECK(!front.valid() && std::isnan(front.readDistanceCm()));
        SonarIo::timeUs=500000;rear.update(false);CHECK(!rear.valid());
        CHECK(SonarIo::triggered.size()==4);
        CHECK(SonarIo::triggered[0]==12 && SonarIo::triggered[1]==0);
        std::cout << "PASS dual sonar HAL: pinout, independent ISR/range, 15cm, no echo LOW, stuck HIGH, stale\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
