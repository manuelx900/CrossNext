#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
inline bool failPsram = false;
struct HalMemory {
  struct HeapStats {
    size_t freeBytes, totalBytes, minFreeBytes, largestBlockBytes;
  };
  struct PsramDeleter {
    void operator()(uint8_t* p) const { std::free(p); }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static PsramBuffer allocatePsram(size_t n) {
    return PsramBuffer(failPsram ? nullptr : static_cast<uint8_t*>(std::malloc(n)));
  }
  static HeapStats getDefaultHeap() { return {10000000, 10000000, 10000000, 10000000}; }
};
