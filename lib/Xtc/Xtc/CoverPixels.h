#pragma once

#include <cstddef>
#include <cstdint>

namespace xtc {
struct CoverBounds {
  unsigned x = 0, y = 0, width = 0, height = 0;
};
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

// Remove only completely white outer padding; keep every non-white source pixel.
inline CoverBounds coverBounds(const uint8_t* bytes, unsigned width, unsigned height, unsigned depth,
                               bool counterclockwise) {
  CoverBounds result{0, 0, counterclockwise ? height : width, counterclockwise ? width : height};
  if (!counterclockwise) return result;
  unsigned left = result.width, top = result.height, right = 0, bottom = 0;
  for (unsigned y = 0; y < result.height; ++y)
    for (unsigned x = 0; x < result.width; ++x)
      if (coverGray(bytes, width, height, depth, x, y, true) != 255) {
        if (x < left) left = x;
        if (y < top) top = y;
        if (x > right) right = x;
        if (y > bottom) bottom = y;
      }
  if (left <= right && top <= bottom) result = {left, top, right - left + 1, bottom - top + 1};
  return result;
}
}  // namespace xtc
