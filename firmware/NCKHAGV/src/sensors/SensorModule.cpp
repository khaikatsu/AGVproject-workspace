#include "SensorModule.h"
#include <cmath>

void SensorModule::begin() {
    digitalWrite(trigPin_, LOW);
    pinMode(trigPin_, OUTPUT);
    pinMode(echoPin_, INPUT);
    attachInterruptArg(echoPin_, echoInterrupt, this, CHANGE);
    lastTriggerMs_ = millis() - Config::ULTRASONIC_INTERVAL_MS;
}
void SensorModule::clearReading() {
    portENTER_CRITICAL(&mux_);
    waiting_ = risingSeen_ = ready_ = false;
    durationUs_ = 0;
    portEXIT_CRITICAL(&mux_);
    noEcho_ = false;
    distanceCm_ = UNKNOWN_VALUE;
    hasResult_ = false;
    lastPulseUs_ = 0;
    lastResult_ = "NONE";
}
void IRAM_ATTR SensorModule::echoInterrupt(void* argument) {
    auto* self = static_cast<SensorModule*>(argument);
    const uint32_t now = micros();
    const bool high = digitalRead(self->echoPin_) == HIGH;
    portENTER_CRITICAL_ISR(&self->mux_);
    ++self->edgeCount_;
    if (self->waiting_) {
        if (high) { self->risingUs_ = now; self->risingSeen_ = true; }
        else if (self->risingSeen_) {
            self->durationUs_ = now - self->risingUs_;
            self->ready_ = true;
            self->waiting_ = false;
        }
    }
    portEXIT_CRITICAL_ISR(&self->mux_);
}
void SensorModule::update(bool trigger) {
    const uint32_t now = millis();
    uint32_t duration = 0;
    bool ready, waiting;
    portENTER_CRITICAL(&mux_);
    ready = ready_;
    if (ready) { duration = durationUs_; ready_ = false; }
    if (waiting_ && static_cast<uint32_t>(micros() - triggerUs_) >= Config::ULTRASONIC_ECHO_TIMEOUT_US) {
        waiting_ = false;
        distanceCm_ = UNKNOWN_VALUE;
        // Sketch gốc coi timeout LOW là ngoài tầm đo, không có vật cản.
        // ECHO kẹt HIGH vẫn là lỗi cảm biến và phải dừng.
        noEcho_ = Config::REAR_ULTRASONIC_ECHO_PIN >= 0 && digitalRead(echoPin_) == LOW;
        lastResultMs_ = now;
        hasResult_ = true;
        lastPulseUs_ = 0;
        lastResult_ = noEcho_ ? "NO_ECHO_LOW" : "TIMEOUT_INVALID";
    }
    waiting = waiting_;
    portEXIT_CRITICAL(&mux_);
    if (ready) {
        distanceCm_ = duration > 0 && duration <= Config::ULTRASONIC_MAX_RANGE_ECHO_US
            ? duration * Config::ULTRASONIC_SOUND_CM_PER_US / 2.0f : UNKNOWN_VALUE;
        // A completed pulse beyond the distance range (38 ms nominally,
        // about 47.5 ms on robot 01) is not a range reading or stuck HIGH.
        noEcho_ = Config::REAR_ULTRASONIC_ECHO_PIN >= 0 &&
            duration > Config::ULTRASONIC_MAX_RANGE_ECHO_US &&
            duration <= Config::ULTRASONIC_ECHO_TIMEOUT_US;
        lastResultMs_ = now;
        hasResult_ = true;
        lastPulseUs_ = duration;
        lastResult_ = noEcho_ ? "NO_TARGET_PULSE" :
            (std::isfinite(distanceCm_) ? "RANGE" : "INVALID_PULSE");
    }
    if (trigger && !waiting && static_cast<uint32_t>(now - lastTriggerMs_) >= Config::ULTRASONIC_INTERVAL_MS &&
        (Config::REAR_ULTRASONIC_ECHO_PIN < 0 || digitalRead(echoPin_) == LOW)) {
        portENTER_CRITICAL(&mux_);
        waiting_ = true;
        risingSeen_ = false;
        triggerUs_ = micros();
        portEXIT_CRITICAL(&mux_);
        digitalWrite(trigPin_, HIGH);
        delayMicroseconds(Config::ULTRASONIC_TRIGGER_US);
        digitalWrite(trigPin_, LOW);
        lastTriggerMs_ = now;
        ++triggerCount_;
    }
}
bool SensorModule::valid() const {
    return (noEcho_ || std::isfinite(distanceCm_)) && static_cast<uint32_t>(millis() - lastResultMs_) <= Config::ULTRASONIC_STALE_MS;
}
float SensorModule::readDistanceCm() const { return valid() ? distanceCm_ : UNKNOWN_VALUE; }
bool SensorModule::isObstacleDetected() const { return valid() && readDistanceCm() <= Config::OBSTACLE_DISTANCE_CM; }
SensorModule::Diagnostic SensorModule::diagnostic() const {
    Diagnostic result{};
    portENTER_CRITICAL(&mux_);
    result.echoLevel = digitalRead(echoPin_);
    result.waiting = waiting_;
    result.risingSeen = risingSeen_;
    result.ready = ready_;
    result.edges = edgeCount_;
    portEXIT_CRITICAL(&mux_);
    result.valid = valid();
    result.pulseUs = lastPulseUs_;
    result.resultAgeMs = hasResult_ ? static_cast<uint32_t>(millis() - lastResultMs_) : UINT32_MAX;
    result.triggers = triggerCount_;
    result.result = lastResult_;
    return result;
}
