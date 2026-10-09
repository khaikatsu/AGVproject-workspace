#pragma once
#include <Arduino.h>
#include "Types.h"

class BatteryModule {
public:
    void begin();
    void update();
    float readVoltage() const { return voltage_; }
    float getBatteryPercentage() const { return percentage_; }
    uint32_t adcMillivolts() const { return adcMv_; }
    bool calibrated() const;
private:
    static constexpr uint8_t SAMPLE_COUNT = 16;
    static constexpr uint32_t SAMPLE_SPACING_MS = Config::BATTERY_SAMPLE_INTERVAL_MS / SAMPLE_COUNT;
    uint32_t adcMv_ = 0, lastSampleMs_ = 0;
    uint32_t sampleSum_ = 0, sampleMin_ = 0, sampleMax_ = 0;
    uint8_t sampleCount_ = 0;
    float filteredAdcMv_ = UNKNOWN_VALUE;
    float voltage_ = UNKNOWN_VALUE, percentage_ = UNKNOWN_VALUE;
};
