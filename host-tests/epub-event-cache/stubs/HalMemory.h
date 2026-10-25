#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
class HalMemory {
 public:
  inline static bool fail = false, absent = false;
  inline static int active = 0;
  struct Stats {
    size_t totalBytes;
  };
  struct PsramDeleter {
    void operator()(uint8_t* p) const {
      if (p) {
        --active;
        free(p);
      }
    }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static Stats getPsramHeap() { return {absent ? 0U : 8388608U}; }
  static PsramBuffer allocatePsram(size_t n) {
    if (fail) return {};
    auto* p = static_cast<uint8_t*>(malloc(n));
    if (p) ++active;
    return PsramBuffer(p);
  }
};
