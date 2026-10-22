#pragma once
#include "Config.h"
#include <algorithm>

struct ForwardLineSample {
    bool authorized = false;
    uint8_t pattern = 0;
    uint32_t now = 0;
};
struct LinePdOutput { int left = 0, right = 0; };

class LinePdController {
public:
    void reset() { ready_ = false; lastError_ = 0; left_ = right_ = 0; }
    bool update(uint8_t pattern, uint32_t now, int cap, LinePdOutput& out) {
        float error;
        switch (pattern) {
            case 4: error = -2; break;
            case 6: error = -1; break;
            case 1: error = 2; break;
            case 3: error = 1; break;
            case 2: case 5: case 7: error = 0; break;
            default: reset(); out = {}; return false;
        }
        error *= Config::LINE_PD_STEERING_SIGN;
        const uint32_t elapsed = now - updatedAt_;
        if (!ready_ || elapsed >= Config::LINE_PD_CONTROL_MS) {
            // D theo buoc mau danh nghia; bo D sau STOP/ngat dai.
            const float derivative = ready_ && elapsed < Config::LINE_PD_RESET_GAP_MS
                ? (error-lastError_) * Config::LINE_PD_CONTROL_MS / elapsed : 0;
            const float correction = Config::LINE_KP * error + Config::LINE_KD * derivative;
            // Chỉ giới hạn đầu ra khi pattern còn hợp lệ. STOP/000/hazard
            // không đi qua nhánh này nên sàn PWM không thể tự cấp chuyển động.
            left_ = std::clamp(static_cast<int>(Config::LINE_PD_BASE_SPEED + Config::LEFT_SPEED_TRIM - correction),
                               Config::LINE_PD_MIN_RUNNING_PWM, Config::LINE_PD_MAX_RUNNING_PWM);
            right_ = std::clamp(static_cast<int>(Config::LINE_PD_BASE_SPEED + Config::RIGHT_SPEED_TRIM + correction),
                                Config::LINE_PD_MIN_RUNNING_PWM, Config::LINE_PD_MAX_RUNNING_PWM);
            lastError_ = error; updatedAt_ = now; ready_ = true;
        }
        cap = std::clamp(cap, 0, 255);
        out = {std::min(left_, cap), std::min(right_, cap)};
        return true;
    }
private:
    bool ready_ = false;
    float lastError_ = 0;
    uint32_t updatedAt_ = 0;
    int left_ = 0, right_ = 0;
};
