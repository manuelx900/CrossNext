#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
class HalMemory {
 public:
  static inline bool fail = false;
  static inline size_t alive = 0;
  struct PsramDeleter {
    void operator()(uint8_t* buffer) const {
      if (buffer) {
        --alive;
        free(buffer);
      }
    }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static PsramBuffer allocatePsram(size_t bytes) {
    auto* buffer = fail ? nullptr : static_cast<uint8_t*>(malloc(bytes));
    if (buffer) ++alive;
    return PsramBuffer(buffer);
  }
};
