#pragma once
#include "MotorModule.h"
#include "core/LinePdController.h"

class LineFollowModule {
public:
    explicit LineFollowModule(MotorModule& motor) : motor_(motor) {}
    void begin();
    LineReadings readSensors() const;
    void followLine();
    void correctLeft();
    void correctRight();
    void gentleCorrectLeft();
    void gentleCorrectRight();
    bool followPd(uint8_t pattern, uint32_t now, int cap = 255);
    void resetPd() { pd_.reset(); }
    void stop();
private:
    MotorModule& motor_;
    LinePdController pd_;
};
