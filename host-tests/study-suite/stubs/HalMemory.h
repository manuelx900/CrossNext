#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
class HalMemory {
 public:
  struct PsramDeleter {
    void operator()(uint8_t* p) const { std::free(p); }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], PsramDeleter>;
  static PsramBuffer allocatePsram(size_t n) { return PsramBuffer(static_cast<uint8_t*>(std::malloc(n))); }
};
