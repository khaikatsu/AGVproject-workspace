#pragma once
#include <cstdint>
using byte = unsigned char;
extern uint32_t testNow;
inline uint32_t millis() { return testNow; }
struct FakeSerial { template<typename... Args> void printf(const char*,Args...) {} void println(const char*) {} };
inline FakeSerial Serial;
