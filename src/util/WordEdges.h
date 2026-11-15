#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wordedges {
struct Span {
  size_t first, end;
};
inline bool punctuation(uint32_t cp) {
  if (cp < 128) return !((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9'));
  return cp == 0xA0 || cp == 0xA1 || cp == 0xAB || cp == 0xBB || cp == 0xBF || (cp >= 0x2000 && cp <= 0x206F) ||
         (cp >= 0x3000 && cp <= 0x303F) || (cp >= 0xFF01 && cp <= 0xFF0F) || (cp >= 0xFF1A && cp <= 0xFF20) ||
         (cp >= 0xFF3B && cp <= 0xFF40) || (cp >= 0xFF5B && cp <= 0xFF65);
}
inline Span trim(const char* text) {
  if (!text) return {};
  const size_t length = std::strlen(text);
  Span span{length, length};
  for (size_t at = 0; at < length;) {
    const size_t start = at;
    const auto byte = static_cast<uint8_t>(text[at++]);
    uint32_t cp = byte;
    const int extra = byte < 0x80 ? 0 : byte < 0xE0 ? 1 : byte < 0xF0 ? 2 : 3;
    if (extra) {
      cp &= (1u << (6 - extra)) - 1;
      for (int n = 0; n < extra && at < length && (uint8_t(text[at]) & 0xC0) == 0x80; ++n)
        cp = (cp << 6) | (uint8_t(text[at++]) & 0x3F);
    }
    if (punctuation(cp)) continue;
    if (span.first == length) span.first = start;
    span.end = at;
  }
  return span;
}
}  // namespace wordedges
