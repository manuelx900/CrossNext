#pragma once
#include <cstddef>
#include <cstdint>

namespace headerband {
// Inclusive physical bounds have already been clipped to the framebuffer.
inline void invertRect(uint8_t* buffer, int stride, int left, int right, int top, int bottom) {
  const int first = left / 8, last = right / 8;
  const uint8_t firstMask = uint8_t(0xffu >> (left % 8));
  const uint8_t lastMask = uint8_t(0xffu << (7 - right % 8));
  for (int y = top; y <= bottom; ++y) {
    auto* row = buffer + size_t(y) * stride;
    if (first == last) {
      row[first] ^= uint8_t(firstMask & lastMask);
      continue;
    }
    row[first] ^= firstMask;
    for (int byte = first + 1; byte < last; ++byte) row[byte] ^= 0xffu;
    row[last] ^= lastMask;
  }
}
}  // namespace headerband
