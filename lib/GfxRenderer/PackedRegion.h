#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Packed physical pixels, independent of the rectangle's bit alignment.
namespace packedregion {
inline size_t bytes(int width, int height) { return width > 0 && height > 0 ? size_t((width + 7) / 8) * height : 0; }
inline void read(const uint8_t* frame, size_t stride, int x, int y, int width, int height, uint8_t* out) {
  const size_t pitch = (width + 7) / 8;
  const unsigned shift = x & 7;
  for (int row = 0; row < height; ++row) {
    const auto* src = frame + size_t(y + row) * stride + x / 8;
    auto* dst = out + size_t(row) * pitch;
    for (size_t n = 0; n < pitch; ++n) {
      const int remaining = width - int(n * 8);
      const uint8_t next = shift && remaining > 8 - int(shift) ? src[n + 1] : 0xff;
      dst[n] = shift ? uint8_t((src[n] << shift) | (next >> (8 - shift))) : src[n];
    }
    if (width & 7) dst[pitch - 1] |= uint8_t(0xff >> (width & 7));
  }
}
inline void write(uint8_t* frame, size_t stride, int x, int y, int width, int height, const uint8_t* in) {
  const size_t pitch = (width + 7) / 8;
  const unsigned shift = x & 7;
  for (int row = 0; row < height; ++row) {
    auto* dst = frame + size_t(y + row) * stride + x / 8;
    const auto* src = in + size_t(row) * pitch;
    if (!shift && !(width & 7)) {
      std::memcpy(dst, src, pitch);
      continue;
    }
    for (size_t n = 0; n < pitch; ++n) {
      const int count = width - int(n * 8) < 8 ? width - int(n * 8) : 8;
      const uint16_t mask = (uint16_t(uint8_t(0xff << (8 - count))) << 8) >> shift;
      const uint16_t value = (uint16_t(src[n]) << 8) >> shift;
      dst[n] = uint8_t((dst[n] & ~(mask >> 8)) | ((value & mask) >> 8));
      if (shift + count > 8) dst[n + 1] = uint8_t((dst[n + 1] & ~mask) | (value & mask));
    }
  }
}
}  // namespace packedregion
