#pragma once
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <utility>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
namespace MotorIo {
inline std::unordered_map<int,int> pwm, pins;
inline std::vector<std::pair<int,int>> writes;
}
inline void digitalWrite(int pin,int value) { MotorIo::pins[pin]=value; }
inline int digitalRead(int pin) { return MotorIo::pins[pin]; }
inline void pinMode(int,int) {}
inline void ledcSetup(int,int,int) {}
inline void ledcAttachPin(int,int) {}
inline void ledcWrite(int channel,int value) { MotorIo::pwm[channel]=value;MotorIo::writes.emplace_back(channel,value); }
template<class T> T constrain(T value,T low,T high) { return std::min(high,std::max(low,value)); }
