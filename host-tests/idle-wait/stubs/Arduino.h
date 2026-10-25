#pragma once
#include <cstdint>
#include <initializer_list>
#define IRAM_ATTR
#define ARDUINO_ISR_FLAG 0
inline uint32_t mockDelay = 0;
inline unsigned mockYield = 0, mockErrors = 0;
inline void delay(uint32_t value) { mockDelay = value; }
inline void yield() { ++mockYield; }
