#pragma once
#include <cstddef>
inline size_t wordTestHeap = 1024 * 1024;
class HalMemory {
 public:
  struct Heap {
    size_t largestBlockBytes, freeBytes;
  };
  static Heap getDefaultHeap() { return {wordTestHeap, wordTestHeap}; }
};
