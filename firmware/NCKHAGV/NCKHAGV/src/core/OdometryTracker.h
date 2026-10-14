#pragma once
#include "Types.h"
#include <cstring>

// Một OUT mỗi bánh chỉ đo số xung. Chiều/chặng lấy từ navigator, không từ encoder.
// Chỉ cộng quãng đường khi bám line tiến/lùi; quay tại chỗ không làm tiến marker.
class OdometryTracker {
public:
    explicit OdometryTracker(float pulsesPer100Cm = Config::ENCODER_PULSES_PER_100CM)
        : cmPerTick_(100.0f / pulsesPer100Cm) {}
    void begin(uint32_t left, uint32_t right, uint32_t now) {
        left_ = left; right_ = right; now_ = windowAt_ = now;
    }
    void update(uint32_t left, uint32_t right, uint32_t now) {
        const uint32_t dl = left - left_, dr = right - right_; // chịu wrap bộ đếm
        left_ = left; right_ = right; now_ = now;
        if (sample_.tracking && sample_.translating) {
            const float cm = (static_cast<double>(dl) + dr) * 0.5 * cmPerTick_;
            sample_.distanceCm += cm;
            windowCm_ += cm;
        }
        const uint32_t elapsed = now - windowAt_;
        if (elapsed >= Config::ODOMETRY_INTERVAL_MS) {
            sample_.speedCmS = sample_.translating ? windowCm_ * 1000.0f / elapsed : 0;
            windowCm_ = 0; windowAt_ = now;
        }
    }
    void context(const char* from, const char* to, int32_t route, uint8_t index,
                 uint32_t session, bool tracking, bool translating, bool reverse) {
        if (sample_.localSession != session || sample_.routeCmdId != route || sample_.pathIndex != index ||
            std::strcmp(sample_.fromNode, from) || std::strcmp(sample_.toNode, to)) {
            sample_.distanceCm = sample_.speedCmS = windowCm_ = 0;
            windowAt_ = now_;
        }
        if (!translating || translating != sample_.translating) {
            sample_.speedCmS = windowCm_ = 0; windowAt_ = now_;
        }
        std::strncpy(sample_.fromNode, from, sizeof(sample_.fromNode) - 1);
        std::strncpy(sample_.toNode, to, sizeof(sample_.toNode) - 1);
        sample_.localSession = session; sample_.routeCmdId = route; sample_.pathIndex = index;
        sample_.tracking = tracking; sample_.translating = tracking && translating;
        sample_.reverse = tracking && reverse;
        if (!tracking) sample_.distanceCm = sample_.speedCmS = 0;
    }
    OdometrySnapshot snapshot(uint32_t seq) const {
        auto result = sample_;
        result.seq = seq; result.uptimeMs = now_; result.leftTicks = left_; result.rightTicks = right_;
        return result;
    }
private:
    float cmPerTick_, windowCm_ = 0;
    uint32_t left_ = 0, right_ = 0, now_ = 0, windowAt_ = 0;
    OdometrySnapshot sample_{};
};
