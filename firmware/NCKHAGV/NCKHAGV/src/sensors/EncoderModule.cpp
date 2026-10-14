#include "EncoderModule.h"

void EncoderModule::begin() {
    if (!Config::ENCODER_ENABLED) return;
    pinMode(Config::ENCODER_LEFT_PIN, INPUT_PULLUP);
    pinMode(Config::ENCODER_RIGHT_PIN, INPUT_PULLUP);
    attachInterruptArg(Config::ENCODER_LEFT_PIN, leftInterrupt, this, RISING);
    attachInterruptArg(Config::ENCODER_RIGHT_PIN, rightInterrupt, this, RISING);
    tracker_.begin(0, 0, millis());
}
void IRAM_ATTR EncoderModule::leftInterrupt(void* self) { static_cast<EncoderModule*>(self)->pulse(true); }
void IRAM_ATTR EncoderModule::rightInterrupt(void* self) { static_cast<EncoderModule*>(self)->pulse(false); }
void IRAM_ATTR EncoderModule::pulse(bool left) {
    const uint32_t now = micros();
    portENTER_CRITICAL_ISR(&lock_);
    volatile uint32_t& last = left ? lastLeftUs_ : lastRightUs_;
    if (static_cast<uint32_t>(now - last) > Config::ENCODER_DEBOUNCE_US) {
        if (left) ++leftTicks_; else ++rightTicks_;
        last = now;
    }
    portEXIT_CRITICAL_ISR(&lock_);
}
void EncoderModule::update(uint32_t now) {
    if (!Config::ENCODER_ENABLED) return;
    portENTER_CRITICAL(&lock_);
    const uint32_t left = leftTicks_, right = rightTicks_;
    portEXIT_CRITICAL(&lock_);
    tracker_.update(left, right, now);
}
void EncoderModule::context(const char* from, const char* to, int32_t route, uint8_t index,
                            uint32_t session, bool tracking, bool translating, bool reverse) {
    if (!Config::ENCODER_ENABLED) return;
    update(millis()); // tính xung cuối cùng bằng trạng thái cũ trước khi đổi chặng
    tracker_.context(from, to, route, index, session, tracking, translating, reverse);
}
