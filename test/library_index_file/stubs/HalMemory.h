#pragma once
#include "Memory.h"
class HalMemory {
 public:
  using PsramBuffer = std::unique_ptr<uint8_t[]>;
  static inline bool unavailable = false;
  static inline size_t attempts = 0;
  static PsramBuffer allocatePsram(size_t size) {
    ++attempts;
    return unavailable ? nullptr : makeUniqueNoThrow<uint8_t[]>(size);
  }
};
