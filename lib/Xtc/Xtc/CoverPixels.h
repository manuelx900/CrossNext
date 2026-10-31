#pragma once

#include <cstddef>
#include <cstdint>

namespace xtc {
// Source gray: 0 black, 85 dark gray, 170 light gray, 255 white.
inline uint8_t coverGray(const uint8_t* bytes, unsigned width, unsigned height, unsigned depth, unsigned x, unsigned y,
                         bool counterclockwise) {
  const unsigned sx = counterclockwise ? width - 1 - y : x, sy = counterclockwise ? x : y;
  if (sx >= width || sy >= height) return 255;
  if (depth == 1) return (bytes[sy * ((width + 7) / 8) + sx / 8] & (0x80 >> (sx % 8))) ? 255 : 0;
  const size_t colBytes = (height + 7) / 8, plane = width * colBytes, at = (width - 1 - sx) * colBytes + sy / 8;
  const unsigned shift = 7 - sy % 8;
  static constexpr uint8_t grays[] = {255, 85, 170, 0};
  return grays[(((bytes[at] >> shift) & 1) << 1) | ((bytes[at + plane] >> shift) & 1)];
}
}  // namespace xtc
