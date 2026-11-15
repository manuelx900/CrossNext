#pragma once
#include <cstdint>
#include <memory>
class HalMemory {
 public:
  struct HeapStats {
    size_t freeBytes;
  };
  static HeapStats getPsramHeap() { return {10000000}; }
  using PsramBuffer = std::unique_ptr<uint8_t[]>;
  inline static int failAfter = -1;
  inline static unsigned allocations = 0;
  static PsramBuffer allocatePsram(size_t n) {
    if (failAfter == 0) return {};
    if (failAfter > 0) --failAfter;
    ++allocations;
    return std::make_unique<uint8_t[]>(n);
  }
};
