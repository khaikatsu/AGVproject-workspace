#include "BatteryModule.h"
#include "core/ControlLogic.h"
#include <cmath>

void BatteryModule::begin() {
    *this = BatteryModule{};
    pinMode(Config::BATTERY_ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Config::BATTERY_ADC_PIN, ADC_11db);
    lastMotorActiveMs_ = millis();
    lastSampleMs_ = lastMotorActiveMs_ - SAMPLE_SPACING_MS;
}
void BatteryModule::update(bool motorRunning) {
    const uint32_t now = millis();
    if (motorRunning) lastMotorActiveMs_ = now;
    const bool settled = !motorRunning &&
        static_cast<uint32_t>(now - lastMotorActiveMs_) >= REST_SETTLE_MS;
    if (!settled) {
        displayBatchAtRest_ = false;
        displayFilteredAdcMv_ = UNKNOWN_VALUE;
        trendDirection_ = 0;
        trendBatches_ = 0;
    }
    if (static_cast<uint32_t>(now - lastSampleMs_) < SAMPLE_SPACING_MS) return;
    lastSampleMs_ = now;
    const uint32_t sample = analogReadMilliVolts(Config::BATTERY_ADC_PIN);
    sampleSum_ += sample;
    if (sampleCount_ == 0 || sample < sampleMin_) sampleMin_ = sample;
    if (sample > sampleMax_) sampleMax_ = sample;
    if (++sampleCount_ < SAMPLE_COUNT) return;

    // Drop one high and one low ADC reading before filtering successive batches.
    adcMv_ = (sampleSum_ - sampleMin_ - sampleMax_) / (SAMPLE_COUNT - 2);
    sampleSum_ = sampleMin_ = sampleMax_ = 0;
    sampleCount_ = 0;
    if (!std::isfinite(filteredAdcMv_)) filteredAdcMv_ = static_cast<float>(adcMv_);
    else filteredAdcMv_ += (static_cast<float>(adcMv_) - filteredAdcMv_) / 8.0f;

    voltage_ = AGVMath::batteryVoltage(static_cast<uint32_t>(std::lround(filteredAdcMv_)),
                                       Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET);
    // Voltage stays live for the existing bench interlock. Only the displayed
    // percentage uses complete batches taken after the motors have rested.
    const bool displayEligible = displayBatchAtRest_ && settled;
    displayBatchAtRest_ = settled;
    if (!displayEligible) return;
    if (!std::isfinite(displayFilteredAdcMv_)) displayFilteredAdcMv_ = static_cast<float>(adcMv_);
    else displayFilteredAdcMv_ += (static_cast<float>(adcMv_) - displayFilteredAdcMv_) / 8.0f;
    const float displayVoltage = AGVMath::batteryVoltage(
        static_cast<uint32_t>(std::lround(displayFilteredAdcMv_)),
        Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET);
    const float measured = AGVMath::batteryPercentage(displayVoltage, Config::BATTERY_EMPTY_VOLTAGE,
                                                    Config::BATTERY_FULL_VOLTAGE);
    if (!std::isfinite(measured)) {
        percentage_ = UNKNOWN_VALUE;
        trendDirection_ = 0;
        trendBatches_ = 0;
        return;
    }
    if (!std::isfinite(percentage_)) {
        percentage_ = std::round(measured);
        lastDisplayStepMs_ = now;
        return;
    }
    const float batchVoltage = AGVMath::batteryVoltage(adcMv_, Config::BATTERY_VOLTAGE_SCALE,
                                                     Config::BATTERY_VOLTAGE_OFFSET);
    const float batchPercentage = AGVMath::batteryPercentage(batchVoltage, Config::BATTERY_EMPTY_VOLTAGE,
                                                            Config::BATTERY_FULL_VOLTAGE);
    const auto directionFor = [this](float value) -> int8_t {
        if (value >= percentage_ + 1.5f || (percentage_ == 99 && value >= 99.5f)) return 1;
        if (value <= percentage_ - 1.5f || (percentage_ == 1 && value <= 0.5f)) return -1;
        return 0;
    };
    int8_t direction = directionFor(measured);
    // Confirm with the current batch too: a short dip must not keep voting
    // for a decrease just because the temporal filter is still recovering.
    if (directionFor(batchPercentage) != direction) direction = 0;
    if (!direction) {
        trendDirection_ = 0;
        trendBatches_ = 0;
        return;
    }
    if (direction != trendDirection_) {
        trendDirection_ = direction;
        trendBatches_ = 1;
    } else if (trendBatches_ < REQUIRED_TREND_BATCHES) ++trendBatches_;
    if (trendBatches_ >= REQUIRED_TREND_BATCHES &&
        static_cast<uint32_t>(now - lastDisplayStepMs_) >= DISPLAY_STEP_MS) {
        percentage_ += direction;
        lastDisplayStepMs_ = now;
        trendBatches_ = 0;
    }
}
bool BatteryModule::calibrated() const {
    return std::isfinite(voltage_) && std::isfinite(percentage_);
}
