#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
inline bool testPsramFailure = false;
inline size_t testLargestPsram = 0;
class HalMemory {
 public:
  struct HeapStats {
    size_t freeBytes, totalBytes, minFreeBytes, largestBlockBytes;
  };
  struct PsramDeleter {
    void operator()(uint8_t* p) const { std::free(p); }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static PsramBuffer allocatePsram(size_t n) {
    testLargestPsram = std::max(n, testLargestPsram);
    return PsramBuffer(testPsramFailure ? nullptr : static_cast<uint8_t*>(std::malloc(n)));
  }
  static HeapStats getDefaultHeap() { return {}; }
  static HeapStats getPsramHeap() { return {}; }
};
