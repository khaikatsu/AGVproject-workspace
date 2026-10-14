#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#define IRAM_ATTR
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define CHANGE 2
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL(portMUX_TYPE*) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE*) {}
namespace SonarIo {
inline uint32_t timeUs = 0;
struct Interrupt { void (*callback)(void*); void* argument; };
inline std::unordered_map<int,Interrupt> interrupts;
inline std::unordered_map<int,int> modes, levels;
inline std::vector<int> triggered;
inline void fire(int pin, int level, uint32_t time) {
    timeUs=time; levels[pin]=level;
    const auto interrupt=interrupts.at(pin); interrupt.callback(interrupt.argument);
}
}
inline uint32_t micros() { return SonarIo::timeUs; }
inline uint32_t millis() { return SonarIo::timeUs/1000; }
inline void delayMicroseconds(uint32_t us) { SonarIo::timeUs+=us; }
inline void pinMode(int pin,int mode) { SonarIo::modes[pin]=mode; }
inline int digitalRead(int pin) { return SonarIo::levels[pin]; }
inline void digitalWrite(int pin,int level) {
    SonarIo::levels[pin]=level;
    if (level==HIGH) SonarIo::triggered.push_back(pin);
}
inline void attachInterruptArg(int pin,void (*callback)(void*),void* argument,int) {
    SonarIo::interrupts[pin]={callback,argument};
}
