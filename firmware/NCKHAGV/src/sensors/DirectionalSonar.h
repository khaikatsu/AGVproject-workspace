#pragma once
#include "SensorModule.h"

// Only the requested travel direction transmits. Reverse intent stays active
// while stopped for a rear obstacle, so removing it can resume the same route.
class DirectionalSonar {
public:
    DirectionalSonar(SensorModule& front, SensorModule& rear) : front_(front), rear_(rear) {}
    // Returns true when the caller must stop motors for a fresh acquisition.
    bool update(bool reverse) {
        const uint32_t now = millis();
        const bool changed = !initialized_ || reverse != rearActive_;
        if (changed) {
            front_.clearReading();
            rear_.clearReading();
            rearActive_ = reverse;
            initialized_ = true;
            acquired_ = false;
            switchedAtMs_ = now;
        }
        // Let the previous acoustic pulse die away before changing sensors.
        selected().update(static_cast<uint32_t>(now - switchedAtMs_) >= SWITCH_QUIET_MS);
        if (selected().valid()) acquired_ = true;
        return changed;
    }
    const SensorModule& active() const { return rearActive_ ? rear_ : front_; }
    bool rearActive() const { return rearActive_; }
    bool acquiring() const {
        return initialized_ && !acquired_ &&
            static_cast<uint32_t>(millis() - switchedAtMs_) < ACQUIRE_TIMEOUT_MS;
    }
private:
    SensorModule& selected() { return rearActive_ ? rear_ : front_; }
    static constexpr uint32_t SWITCH_QUIET_MS = 50;
    static constexpr uint32_t ACQUIRE_TIMEOUT_MS = 200;
    SensorModule& front_;
    SensorModule& rear_;
    bool initialized_ = false, rearActive_ = false, acquired_ = false;
    uint32_t switchedAtMs_ = 0;
};
