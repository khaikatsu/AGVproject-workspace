#pragma once
#include <cstdint>
#include <unordered_map>
#define IRAM_ATTR
#define INPUT_PULLUP 2
#define RISING 3
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL(portMUX_TYPE*) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE*) {}
namespace EncoderIo {
inline uint32_t timeUs = 0;
struct Interrupt { void (*callback)(void*); void* argument; int mode; };
inline std::unordered_map<int,Interrupt> interrupts;
inline std::unordered_map<int,int> pinModes;
inline void fire(int pin, uint32_t at) {
    timeUs = at; const auto& interrupt = interrupts.at(pin); interrupt.callback(interrupt.argument);
}
}
inline uint32_t micros() { return EncoderIo::timeUs; }
inline uint32_t millis() { return EncoderIo::timeUs / 1000; }
inline void pinMode(int pin,int mode) { EncoderIo::pinModes[pin] = mode; }
inline void attachInterruptArg(int pin,void (*callback)(void*),void* argument,int mode) {
    EncoderIo::interrupts[pin] = {callback,argument,mode};
}
