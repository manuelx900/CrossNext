#pragma once

#include "../../../lib/Utf8/Utf8.h"

namespace uicase {
inline uint32_t next(const unsigned char** cursor) {
  const auto* text = *cursor;
  const auto first = *text;
  const size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
  uint32_t value = first & (length == 1 ? 0x7f : length == 2 ? 0x1f : length == 3 ? 0x0f : 0x07);
  for (size_t i = 1; i < length; ++i) {
    if (!text[i] || (text[i] & 0xc0) != 0x80) {
      ++*cursor;
      return 0xfffd;
    }
    value = (value << 6) | (text[i] & 0x3f);
  }
  *cursor += length;
  return value;
}
inline uint32_t upper(const uint32_t cp, const bool turkic = false) {
  if (turkic && cp == 'i') return 0x130;
  if (cp == 0x131) return 'I';
  if (cp == 0x259) return 0x18f;
  if (cp == 0xdf) return 0x1e9e;
  if (cp == 0x1a1 || cp == 0x1b0 || cp == 0x1f5 || cp == 0x219 || cp == 0x21b) return cp - 1;
  if ((cp >= 0x460 && cp <= 0x481) || (cp >= 0x48a && cp <= 0x4bf) || (cp >= 0x4d0 && cp <= 0x52f) ||
      (cp >= 0x1e00 && cp <= 0x1e95) || (cp >= 0x1ea0 && cp <= 0x1eff))
    return cp & 1 ? cp - 1 : cp;
  return utf8SmallCapsUpper(cp);
}
}  // namespace uicase
