#pragma once

#include <cstddef>
#include <cstdint>

namespace picross {
// Only reuse a frame while this activity still owns the last physical paint.
class PaintState {
 public:
  static uint64_t fingerprint(const uint8_t* pixels, size_t bytes) {
    uint64_t result = 14695981039346656037ull;
    for (size_t n = 0; n < bytes; ++n) result = (result ^ pixels[n]) * 1099511628211ull;
    return result;
  }
  bool unchanged(uint64_t pixels, uint32_t paint, uint32_t meaning, uint32_t context) const {
    return valid_ && pixels_ == pixels && paint_ == paint && meaning_ == meaning && context_ == context;
  }
  void remember(uint64_t pixels, uint32_t paint, uint32_t meaning, uint32_t context) {
    pixels_ = pixels;
    paint_ = paint;
    meaning_ = meaning;
    context_ = context;
    valid_ = true;
  }
  void invalidate() { valid_ = false; }

 private:
  uint64_t pixels_ = 0;
  uint32_t paint_ = 0, meaning_ = 0, context_ = 0;
  bool valid_ = false;
};
}  // namespace picross
