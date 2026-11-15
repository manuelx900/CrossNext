#pragma once
#include "Memory.h"
class HalMemory {
 public:
  struct Stats {
    size_t totalBytes = 8388608, freeBytes = 8388608, largestBlockBytes = 8388608;
  };
  static Stats getPsramHeap() { return {}; }
  static Stats getInternalHeap() { return {}; }
  using PsramBuffer = std::unique_ptr<uint8_t[]>;
  static PsramBuffer allocatePsram(size_t n) { return makeUniqueNoThrow<uint8_t[]>(n); }
};
