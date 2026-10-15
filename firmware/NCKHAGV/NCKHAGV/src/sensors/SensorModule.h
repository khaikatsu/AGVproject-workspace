#pragma once
#include <Arduino.h>
#include "Types.h"

class SensorModule {
public:
    struct Diagnostic {
        int echoLevel;
        bool waiting, risingSeen, ready, valid;
        uint32_t pulseUs, resultAgeMs, triggers, edges;
        const char* result;
    };
    SensorModule(int trig = Config::ULTRASONIC_TRIG_PIN, int echo = Config::ULTRASONIC_ECHO_PIN)
        : trigPin_(trig), echoPin_(echo) {}
    void begin();
    void clearReading();
    void update(bool trigger = true);
    float readDistanceCm() const;
    bool isObstacleDetected() const;
    bool valid() const;
    Diagnostic diagnostic() const;
private:
    int trigPin_, echoPin_;
    static void IRAM_ATTR echoInterrupt(void* argument);
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile bool waiting_ = false, risingSeen_ = false, ready_ = false;
    volatile uint32_t risingUs_ = 0, durationUs_ = 0;
    volatile uint32_t edgeCount_ = 0;
    uint32_t triggerUs_ = 0, lastTriggerMs_ = 0, lastResultMs_ = 0;
    uint32_t triggerCount_ = 0, lastPulseUs_ = 0;
    const char* lastResult_ = "NONE";
    bool hasResult_ = false;
    bool noEcho_ = false;
    float distanceCm_ = UNKNOWN_VALUE;
};
