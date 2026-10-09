#include "BatteryModule.h"
#include "core/ControlLogic.h"
#include <cmath>

void BatteryModule::begin() {
    pinMode(Config::BATTERY_ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Config::BATTERY_ADC_PIN, ADC_11db);
    lastSampleMs_ = millis() - SAMPLE_SPACING_MS;
}
void BatteryModule::update() {
    if (static_cast<uint32_t>(millis() - lastSampleMs_) < SAMPLE_SPACING_MS) return;
    lastSampleMs_ = millis();
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
    const float measured = AGVMath::batteryPercentage(voltage_, Config::BATTERY_EMPTY_VOLTAGE,
                                                       Config::BATTERY_FULL_VOLTAGE);
    if (!std::isfinite(measured)) { percentage_ = UNKNOWN_VALUE; return; }
    if (!std::isfinite(percentage_)) percentage_ = std::round(measured);
    else if (measured >= percentage_ + 1.5f) percentage_ += 1.0f;
    else if (measured <= percentage_ - 1.5f) percentage_ -= 1.0f;
}
bool BatteryModule::calibrated() const {
    return std::isfinite(voltage_) && std::isfinite(percentage_);
}
