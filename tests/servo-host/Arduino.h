#pragma once
#include <cstdint>
#include <unordered_map>
namespace ServoIo {
inline uint32_t timeMs=0;
inline std::unordered_map<int,int> pins,pwm,hz,bits,writes;
}
inline uint32_t millis(){return ServoIo::timeMs;}
inline void ledcSetup(int channel,int hz,int bits){ServoIo::hz[channel]=hz;ServoIo::bits[channel]=bits;}
inline void ledcAttachPin(int pin,int channel){ServoIo::pins[pin]=channel;}
inline void ledcWrite(int channel,int duty){ServoIo::pwm[channel]=duty;++ServoIo::writes[channel];}
