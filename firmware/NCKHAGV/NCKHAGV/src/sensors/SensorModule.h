#pragma once
#include <Arduino.h>
#include "Types.h"

class SensorModule {
public:
    SensorModule(int trig = Config::ULTRASONIC_TRIG_PIN, int echo = Config::ULTRASONIC_ECHO_PIN)
        : trigPin_(trig), echoPin_(echo) {}
    void begin();
    void update(bool trigger = true);
    float readDistanceCm() const;
    bool isObstacleDetected() const;
    bool valid() const;
private:
    int trigPin_, echoPin_;
    static void IRAM_ATTR echoInterrupt(void* argument);
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile bool waiting_ = false, risingSeen_ = false, ready_ = false;
    volatile uint32_t risingUs_ = 0, durationUs_ = 0;
    uint32_t triggerUs_ = 0, lastTriggerMs_ = 0, lastResultMs_ = 0;
    bool noEcho_ = false;
    float distanceCm_ = UNKNOWN_VALUE;
};
