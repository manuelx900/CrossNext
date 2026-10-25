#pragma once
#include <EpdFontFamily.h>
#include <Utf8.h>

#include <map>

namespace fontfallback {
inline int resolve(const std::map<int, EpdFontFamily>& fonts, const std::map<int, int>& fallbacks, int fontId,
                   const char* text, EpdFontFamily::Style style) {
  if (!text || !*text || fallbacks.empty()) return fontId;
  int resolved = fontId, edge = fontId;
  // UI fonts can redirect through a built-in face to an optional SD/TTF face.
  // The bound also makes accidental fallback cycles harmless.
  for (int hop = 0; hop < 4; ++hop) {
    const auto next = fallbacks.find(edge);
    if (next == fallbacks.end() || next->second == edge) break;
    edge = next->second;
    const auto primary = fonts.find(resolved), candidate = fonts.find(edge);
    if (primary == fonts.end() || candidate == fonts.end()) continue;
    const char* cursor = text;
    uint32_t cp;
    while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&cursor)))) {
      if (cp >= 0x80 && !primary->second.hasCodepoint(cp, style) && candidate->second.hasCodepoint(cp, style)) {
        resolved = edge;
        break;
      }
    }
  }
  return resolved;
}
}  // namespace fontfallback
