#pragma once
// Adapter host cho test logic sketch; khong thay the Arduino/ESP32 runtime.
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <deque>
using byte = unsigned char;
inline uint32_t hostMillis = 0;
inline uint32_t millis() {return hostMillis;}
struct HostSerial {
    std::deque<char> input;
    void begin(int) {}
    int available() {return static_cast<int>(input.size());}
    int read() {char c=input.front();input.pop_front();return c;}
    void println(const char* value="") {std::fprintf(stderr,"%s\n",value);}
    void printf(const char* format,...) {va_list args;va_start(args,format);std::vfprintf(stderr,format,args);va_end(args);}
};
inline HostSerial Serial;
struct HostESP {uint64_t getEfuseMac() {return 0x010203040506ULL;}};
inline HostESP ESP;

#if defined(AGV_DRIVER_BENCH)
#include <algorithm>
inline uint32_t hostMicros = 0;
inline int hostPins[40] = {};
inline int hostDuty[40] = {};
inline bool hostPwmAttached[40] = {};
inline uint32_t micros() { return hostMicros; }
inline void delayMicroseconds(uint32_t amount) { hostMicros += amount; }
inline int digitalRead(int pin) { return hostPins[pin]; }
inline void digitalWrite(int pin, int level) { hostPins[pin] = level; }
inline void pinMode(int, int) {}
inline bool ledcAttach(int pin, uint32_t, int) { hostPwmAttached[pin] = true; return true; }
inline bool ledcWrite(int pin, int value) { hostDuty[pin] = value; return hostPwmAttached[pin]; }
inline void attachInterrupt(int, void (*)(), int) {}
inline int digitalPinToInterrupt(int pin) { return pin; }
inline long map(long x,long inMin,long inMax,long outMin,long outMax) {
  return (x-inMin)*(outMax-outMin)/(inMax-inMin)+outMin;
}
template <typename T> inline T constrain(T x,T low,T high) { return std::min(std::max(x,low),high); }
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define portENTER_CRITICAL_ISR(mux) ((void)(mux))
#define portEXIT_CRITICAL_ISR(mux) ((void)(mux))
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define CHANGE 2
#endif
