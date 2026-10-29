#pragma once
#include <Arduino.h>
#include "Config.h"

class CargoModule {
public:
    void begin();
    void update(uint32_t now);
    bool stable() const { return stable_; }
    bool present() const { return stable_ && value_; }
    bool absent() const { return stable_ && !value_; }
    int rawLevel() const { return rawLevel_; }
    uint32_t sequence() const { return sequence_; }
    uint32_t sampledAt() const { return sampledAtMs_; }
    uint32_t stableMs(uint32_t now) const { return stable_ ? static_cast<uint32_t>(now-changedAtMs_) : 0; }
    uint32_t unstableMs(uint32_t now) const { return stable_ ? 0 : static_cast<uint32_t>(now-lastStableMs_); }
private:
    bool raw_ = false, value_ = false, stable_ = false;
    int rawLevel_ = -1;
    uint32_t changedAtMs_ = 0;
    uint32_t sampledAtMs_ = 0,sequence_ = 0,lastStableMs_=0;
};
