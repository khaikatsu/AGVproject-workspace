#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
using byte=unsigned char;using boolean=bool;
inline unsigned long millis(){return 100;}inline void yield(){}
inline uint8_t pgm_read_byte_near(const uint8_t* p){return *p;}
class Print {public:virtual ~Print()=default;virtual size_t write(uint8_t)=0;
virtual size_t write(const uint8_t* data,size_t size){for(size_t i=0;i<size;++i)write(data[i]);return size;}};
