#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>

class HalMemory {
 public:
  inline static bool fail = false;
  inline static size_t allocations = 0, active = 0, requested = 0;
  struct PsramDeleter {
    void operator()(uint8_t* buffer) const {
      if (buffer) {
        --active;
        free(buffer);
      }
    }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static PsramBuffer allocatePsram(size_t bytes) {
    ++allocations;
    requested = bytes;
    if (fail) return {};
    auto* buffer = static_cast<uint8_t*>(malloc(bytes));
    if (buffer) ++active;
    return PsramBuffer(buffer);
  }
};
