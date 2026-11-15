#pragma once

#include <cstddef>
#include <cstdint>

namespace name_units {
// Combining marks supported by the name fields do not consume another letter.
constexpr bool mark(uint32_t cp) {
  return (cp >= 0x300 && cp <= 0x36f) || (cp >= 0x591 && cp <= 0x5bd) || cp == 0x5bf || (cp >= 0x5c1 && cp <= 0x5c2) ||
         (cp >= 0x5c4 && cp <= 0x5c5) || cp == 0x5c7 || (cp >= 0x610 && cp <= 0x61a) || (cp >= 0x64b && cp <= 0x65f) ||
         cp == 0x670 || cp == 0xe31 || cp == 0xe33 || (cp >= 0xe34 && cp <= 0xe3a) || (cp >= 0xe47 && cp <= 0xe4e) ||
         cp == 0x3099 || cp == 0x309a;
}

struct Measure {
  unsigned count = 0;
  size_t last = 0;
};

// Input is already validated UTF-8. The byte bound also protects restored fields.
inline Measure measure(const char* text, size_t capacity) {
  Measure result;
  if (!text) return result;
  for (size_t at = 0; at < capacity && text[at];) {
    const size_t start = at;
    const uint8_t first = uint8_t(text[at++]);
    unsigned extra = first < 0x80 ? 0 : first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
    uint32_t cp = extra ? first & (0x7f >> extra) : first;
    while (extra--) {
      if (at >= capacity || !text[at] || (uint8_t(text[at]) & 0xc0) != 0x80) return result;
      cp = cp << 6 | (uint8_t(text[at++]) & 0x3f);
    }
    if (!mark(cp)) {
      ++result.count;
      result.last = start;
    }
  }
  return result;
}
}  // namespace name_units
