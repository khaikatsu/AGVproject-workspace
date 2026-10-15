#pragma once
#include <cstdint>

// Host ADC/clock adapter; exercises the real BatteryModule without a board.
#define INPUT 0
#define ADC_11db 3
namespace BatteryIo {
inline uint32_t now = 0, adcMv = 1560, reads = 0;
inline bool outliers = false;
}
inline uint32_t millis() { return BatteryIo::now; }
inline void pinMode(int, int) {}
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline uint32_t analogReadMilliVolts(int) {
    const auto index = BatteryIo::reads++ % 16;
    if (BatteryIo::outliers && index == 0) return 0;
    if (BatteryIo::outliers && index == 15) return 3000;
    return BatteryIo::adcMv;
}
