#pragma once
#include "Memory.h"
class HalMemory {
 public:
  using PsramBuffer = std::unique_ptr<uint8_t[]>;
  static PsramBuffer allocatePsram(size_t size) { return makeUniqueNoThrow<uint8_t[]>(size); }
};
