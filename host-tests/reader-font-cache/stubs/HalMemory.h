#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
class HalMemory {
 public:
  inline static bool fail = false;
  inline static unsigned allocations = 0, active = 0;
  struct PsramDeleter {
    void operator()(uint8_t* p) const {
      if (p) {
        --active;
        free(p);
      }
    }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static PsramBuffer allocatePsram(size_t n) {
    if (fail) return {};
    auto* p = static_cast<uint8_t*>(malloc(n));
    if (p) {
      ++active;
      ++allocations;
    }
    return PsramBuffer(p);
  }
};
