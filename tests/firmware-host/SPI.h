#pragma once
#include <array>
inline std::array<int,4> hostSpiPins{};
struct HostSPI {void begin(int sck,int miso,int mosi,int ss) {hostSpiPins={sck,miso,mosi,ss};}};
inline HostSPI SPI;
