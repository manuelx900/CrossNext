#pragma once
#include "Memory.h"
class HalMemory {
 public:
  struct HeapStats {
    size_t freeBytes;
  };
  static inline size_t freePsram = 8 * 1024 * 1024;
  static HeapStats getPsramHeap() { return {freePsram}; }
  using PsramBuffer = std::unique_ptr<uint8_t[]>;
  static inline bool unavailable = false;
  static inline size_t attempts = 0;
  static PsramBuffer allocatePsram(size_t size) {
    ++attempts;
    return unavailable ? nullptr : makeUniqueNoThrow<uint8_t[]>(size);
  }
};
