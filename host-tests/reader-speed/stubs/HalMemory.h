#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
class HalMemory {
 public:
  inline static bool fail = false, psram = true;
  inline static unsigned active = 0;
  struct HeapStats {
    size_t freeBytes, totalBytes, minFreeBytes, largestBlockBytes;
  };
  struct Delete {
    void operator()(uint8_t* bytes) const {
      if (bytes) {
        --active;
        std::free(bytes);
      }
    }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], Delete>;
  static PsramBuffer allocatePsram(size_t bytes) {
    if (fail || !psram) return {};
    auto* data = static_cast<uint8_t*>(std::malloc(bytes));
    if (data) ++active;
    return PsramBuffer(data);
  }
  static HeapStats getPsramHeap() { return {4 * 1024 * 1024, psram ? 8 * 1024 * 1024u : 0, 0, 4 * 1024 * 1024}; }
};
