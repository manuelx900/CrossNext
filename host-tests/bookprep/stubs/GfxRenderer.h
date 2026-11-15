#pragma once
#include <cstdint>
inline unsigned long testMillis = 0;
inline unsigned long millis() { return testMillis++; }
class GfxRenderer {
 public:
  struct FrameBufferLoan {
    explicit FrameBufferLoan(GfxRenderer&) {}
  };
};
