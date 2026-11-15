#pragma once

#include <cstddef>
#include <cstdint>

#include "NameUnits.h"

namespace unicode_name {
inline bool requiresNewFormat(const char* text) {
  for (const unsigned char* at = reinterpret_cast<const unsigned char*>(text); *at; ++at)
    if (!(*at >= 'A' && *at <= 'Z') && *at != ' ') return true;
  return false;
}
// Keep the existing byte budget/save layout. Reject malformed UTF-8 and hidden
// controls rather than accepting a partial character at the end of the field.
inline bool valid(const char* text, size_t capacity, unsigned limit, bool emptyAllowed) {
  if (!text || !capacity) return false;
  size_t at = 0;
  unsigned count = 0;
  bool space = true;
  while (at < capacity && text[at]) {
    const uint8_t first = uint8_t(text[at++]);
    uint32_t cp = first;
    unsigned extra = 0;
    if (first >= 0xc2 && first <= 0xdf) {
      cp &= 0x1f;
      extra = 1;
    } else if (first >= 0xe0 && first <= 0xef) {
      cp &= 0x0f;
      extra = 2;
    } else if (first >= 0xf0 && first <= 0xf4) {
      cp &= 7;
      extra = 3;
    } else if (first >= 0x80)
      return false;
    for (unsigned n = 0; n < extra; ++n) {
      if (at >= capacity || (uint8_t(text[at]) & 0xc0) != 0x80) return false;
      cp = cp << 6 | (uint8_t(text[at++]) & 0x3f);
    }
    if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff) || (!name_units::mark(cp) && ++count > limit))
      return false;
    if (cp == ' ') {
      if (space) return false;
      space = true;
    } else {
      if (cp < 0x80 && !((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9') ||
                         cp == '-' || cp == '\''))
        return false;
      if ((cp >= 0x80 && cp <= 0x9f) || cp == 0xa0 || (cp >= 0x2000 && cp <= 0x206f) || cp == 0xfeff) return false;
      const bool supported = cp < 0x250 || cp == 0x2bc || (cp >= 0x300 && cp <= 0x36f) ||
                             (cp >= 0x400 && cp <= 0x6ff) || (cp >= 0xe01 && cp <= 0xe5b) ||
                             (cp >= 0x1e00 && cp <= 0x1eff) || (cp >= 0x3041 && cp <= 0x30ff) ||
                             (cp >= 0x3400 && cp <= 0x9fff);
      if (!supported || (space && name_units::mark(cp))) return false;
      space = false;
    }
  }
  return at < capacity && (count ? !space : emptyAllowed);
}
}  // namespace unicode_name
