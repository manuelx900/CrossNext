#pragma once
#include <cstdint>
#include <string>

// GCC's 32-bit string hash used by the X4 Pro toolchain, independent of libc++.
inline uint32_t deviceStringHash(const std::string& text) {
  constexpr uint32_t m = 0x5bd1e995U;
  uint32_t h = 0xc70f6907U ^ uint32_t(text.size());
  const auto* p = reinterpret_cast<const uint8_t*>(text.data());
  size_t n = text.size();
  while (n >= 4) {
    uint32_t k = p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    k *= m;
    k ^= k >> 24;
    h = (h * m) ^ (k * m);
    p += 4;
    n -= 4;
  }
  if (n >= 3) h ^= uint32_t(p[2]) << 16;
  if (n >= 2) h ^= uint32_t(p[1]) << 8;
  if (n >= 1) {
    h ^= p[0];
    h *= m;
  }
  h ^= h >> 13;
  h *= m;
  return h ^ (h >> 15);
}
