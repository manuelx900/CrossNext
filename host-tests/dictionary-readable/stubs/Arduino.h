#pragma once
#include <cstddef>
struct TestHeap {
  size_t bytes = 4 * 1024 * 1024;
  size_t getMaxAllocHeap() const { return bytes; }
  size_t getFreeHeap() const { return bytes; }
};
inline TestHeap ESP;
inline unsigned long millis() { return 0; }
inline void delay(unsigned) {}
