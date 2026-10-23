#pragma once
#include <Arduino.h>
#include "Types.h"

class BatteryModule {
public:
    void begin();
    void update(bool motorRunning = false);
    float readVoltage() const { return voltage_; }
    float getBatteryPercentage() const { return percentage_; }
    uint32_t adcMillivolts() const { return adcMv_; }
    bool calibrated() const;
private:
    static constexpr uint8_t SAMPLE_COUNT = 16;
    static constexpr uint32_t SAMPLE_SPACING_MS = Config::BATTERY_SAMPLE_INTERVAL_MS / SAMPLE_COUNT;
    static constexpr uint32_t REST_SETTLE_MS = 3000;
    static constexpr uint32_t DISPLAY_STEP_MS = 5000;
    static constexpr uint8_t REQUIRED_TREND_BATCHES = 3;
    uint32_t adcMv_ = 0, lastSampleMs_ = 0;
    uint32_t lastMotorActiveMs_ = 0, lastDisplayStepMs_ = 0;
    uint32_t sampleSum_ = 0, sampleMin_ = 0, sampleMax_ = 0;
    uint8_t sampleCount_ = 0, trendBatches_ = 0;
    int8_t trendDirection_ = 0;
    bool displayBatchAtRest_ = false;
    float filteredAdcMv_ = UNKNOWN_VALUE;
    float displayFilteredAdcMv_ = UNKNOWN_VALUE;
    float voltage_ = UNKNOWN_VALUE, percentage_ = UNKNOWN_VALUE;
};
