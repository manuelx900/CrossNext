#pragma once
#include <EpdFontFamily.h>
#include <Utf8.h>

#include <map>

namespace fontfallback {
inline int resolve(const std::map<int, EpdFontFamily>& fonts, const std::map<int, int>& fallbacks, int fontId,
                   const char* text, EpdFontFamily::Style style, const std::map<int, int>* languageFallbacks = nullptr,
                   void* context = nullptr, bool (*load)(void*, int) = nullptr) {
  if (!text || !*text || (fallbacks.empty() && (!languageFallbacks || languageFallbacks->empty()))) return fontId;
  const auto base = fonts.find(fontId);
  if (base == fonts.end()) return fontId;
  const char* scan = text;
  bool needsFallback = false;
  uint32_t codepoint;
  while ((codepoint = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&scan)))) {
    if (codepoint >= 0x80 && !base->second.hasCodepoint(codepoint, style)) {
      needsFallback = true;
      break;
    }
  }
  if (!needsFallback) return fontId;
  int resolved = fontId;
  const bool hasLanguageFonts = languageFallbacks && !languageFallbacks->empty();
  // Language resources take priority, but their catalog subset must not hide
  // the selected reader family's wider coverage. Both routes are bounded.
  for (int route = hasLanguageFonts ? 0 : 1; route < 2; ++route) {
    int edge = fontId;
    for (int hop = 0; hop < 4; ++hop) {
      const auto language = hasLanguageFonts ? languageFallbacks->find(edge) : fallbacks.end();
      int next = 0;
      if (route == 0 && language != languageFallbacks->end()) {
        next = language->second;
      } else {
        const auto found = fallbacks.find(edge);
        if (found == fallbacks.end()) break;
        next = found->second;
      }
      if (next == edge) break;
      edge = next;
      if (fonts.find(edge) == fonts.end() && load) load(context, edge);
      const auto primary = fonts.find(resolved), candidate = fonts.find(edge);
      if (primary == fonts.end() || candidate == fonts.end()) continue;
      const char* cursor = text;
      bool improves = false, losesCoverage = false;
      uint32_t cp;
      while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&cursor)))) {
        if (cp < 0x80) continue;
        const bool covered = primary->second.hasCodepoint(cp, style);
        const bool available = candidate->second.hasCodepoint(cp, style);
        improves |= !covered && available;
        losesCoverage |= covered && !available;
      }
      if (improves && !losesCoverage) resolved = edge;
    }
  }
  return resolved;
}
}  // namespace fontfallback
