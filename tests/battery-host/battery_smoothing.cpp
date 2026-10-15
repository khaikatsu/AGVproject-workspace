#include "sensors/BatteryModule.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Fixture {
    BatteryModule battery;
    float previous = UNKNOWN_VALUE;
    uint32_t lastChange = 0;
    explicit Fixture(uint32_t start = 0) {
        BatteryIo::now = start;
        BatteryIo::reads = 0;
        BatteryIo::outliers = false;
        battery.begin();
    }
    void run(uint32_t duration, uint32_t mv, bool moving = false) {
        BatteryIo::adcMv = mv;
        for (uint32_t i = 0; i < duration; ++i) {
            ++BatteryIo::now;
            const auto reads = BatteryIo::reads;
            battery.update(moving);
            check(BatteryIo::reads - reads <= 1, "ADC sampling must not block in a loop");
            const float current = battery.getBatteryPercentage();
            if (!std::isfinite(current)) continue;
            check(current >= 0 && current <= 100, "Percentage outside 0..100");
            if (!std::isfinite(previous)) lastChange = BatteryIo::now;
            else if (current != previous) {
                check(std::abs(current - previous) == 1, "Display jumped by more than 1%");
                check(static_cast<uint32_t>(BatteryIo::now - lastChange) >= 5000,
                      "Display changed sooner than five seconds");
                lastChange = BatteryIo::now;
            }
            previous = current;
        }
    }
    float percent() const { return battery.getBatteryPercentage(); }
};
int main() {
    // Startup, trimmed mean and isolated extreme samples.
    Fixture noise;
    noise.run(3000, 1560);
    check(!std::isfinite(noise.percent()), "Startup must wait for a resting batch");
    check(std::isfinite(noise.battery.readVoltage()), "Voltage must update during settling");
    noise.run(1000, 1560);
    check(noise.percent() == 70, "Initial resting estimate is incorrect");
    BatteryIo::outliers = true;
    noise.run(12000, 1560);
    check(noise.battery.adcMillivolts() == 1560 && noise.percent() == 70,
          "Trimmed mean failed to reject outliers");
    BatteryIo::outliers = false;
    noise.run(400, 1400);
    noise.run(10000, 1560);
    check(noise.percent() == 70, "Short voltage dip changed the display");

    // Motor sag must not enter the display filter; voltage interlock stays live.
    Fixture motor;
    motor.run(4000, 1560);
    motor.run(20000, 1360, true);
    check(motor.percent() == 70, "Motor sag changed displayed percentage");
    check(motor.battery.readVoltage() < 7.0f, "Motor sag was hidden from voltage interlock");
    motor.run(2999, 1560);
    check(motor.percent() == 70, "Display changed before motor rest elapsed");
    motor.run(10000, 1560);
    check(motor.percent() == 70, "Loaded samples leaked into resting display filter");
    // A brief stop during a route must not release the display hold.
    motor.run(1000, 1360, true);
    motor.run(2000, 1360);
    motor.run(1000, 1360, true);
    check(motor.percent() == 70, "Brief motor stop released display hold");

    // First eligible batch plus only one more cannot satisfy a three-batch trend.
    motor.run(4000, 1480);
    check(motor.percent() == 70, "Fewer than three resting batches changed display");
    motor.run(20000, 1480);
    check(motor.percent() < 70 && motor.percent() >= 65, "Persistent decline did not move slowly");
    const float low = motor.percent();
    motor.run(30000, 1640);
    check(motor.percent() > low, "Resting increase/charge recovery stayed locked");

    // Endpoints remain reachable despite the 1.5-point hysteresis.
    Fixture full;
    full.run(4000, 1676);
    check(full.percent() == 99, "Full endpoint fixture failed");
    full.run(20000, 1680);
    check(full.percent() == 100, "Display cannot reach 100%");
    Fixture empty;
    empty.run(4000, 1284);
    check(empty.percent() == 1, "Empty endpoint fixture failed");
    empty.run(20000, 1280);
    check(empty.percent() == 0, "Display cannot reach 0%");

    Fixture wrap(UINT32_MAX - 6000);
    wrap.run(4000, 1560);
    wrap.run(20000, 1480);
    check(wrap.percent() < 70, "Clock wrap stopped display updates");
    wrap.battery.begin();
    check(!wrap.battery.calibrated(), "Reinitialization retained stale state");
    std::cout << "PASS battery smoothing: noise, motor sag/rest, trend, rate, recovery, endpoints, clock wrap\n";
}
