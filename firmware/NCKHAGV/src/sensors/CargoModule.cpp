#include "CargoModule.h"

void CargoModule::begin() {
    pinMode(Config::CARGO_IR_PIN, INPUT_PULLUP);
    rawLevel_ = digitalRead(Config::CARGO_IR_PIN);
    raw_ = rawLevel_ == Config::CARGO_IR_ACTIVE_STATE;
    changedAtMs_ = millis();
    sampledAtMs_=lastStableMs_=changedAtMs_;sequence_=1;
    stable_ = false;
}
void CargoModule::update(uint32_t now) {
    sampledAtMs_=now;++sequence_;
    rawLevel_ = digitalRead(Config::CARGO_IR_PIN);
    const bool sample = rawLevel_ == Config::CARGO_IR_ACTIVE_STATE;
    if (sample != raw_) { raw_ = sample; changedAtMs_ = now; stable_ = false; }
    if (static_cast<uint32_t>(now - changedAtMs_) >= Config::CARGO_IR_STABLE_MS) {
        value_ = raw_;
        stable_ = true;
        lastStableMs_=now;
    }
}
