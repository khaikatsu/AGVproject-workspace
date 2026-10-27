#pragma once
#include <Arduino.h>
#include "core/OdometryTracker.h"

class EncoderModule {
public:
    void begin();
    void update(uint32_t now);
    void context(const char* from, const char* to, int32_t route, uint8_t index,
                 uint32_t session, bool tracking, bool translating, bool reverse);
    OdometrySnapshot snapshot() { return tracker_.snapshot(++seq_); }
    OdometryTracker::TranslationTicks translationTicks() const { return tracker_.translationTicks(); }
    uint32_t leftTicks() {
        portENTER_CRITICAL(&lock_);
        const uint32_t val = leftTicks_;
        portEXIT_CRITICAL(&lock_);
        return val;
    }
    uint32_t rightTicks() {
        portENTER_CRITICAL(&lock_);
        const uint32_t val = rightTicks_;
        portEXIT_CRITICAL(&lock_);
        return val;
    }
private:
    static void IRAM_ATTR leftInterrupt(void* self);
    static void IRAM_ATTR rightInterrupt(void* self);
    void IRAM_ATTR pulse(bool left);
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t leftTicks_ = 0, rightTicks_ = 0, lastLeftUs_ = 0, lastRightUs_ = 0;
    OdometryTracker tracker_;
    uint32_t seq_ = 0;
};
