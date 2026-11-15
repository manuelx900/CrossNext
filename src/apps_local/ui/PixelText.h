#pragma once

#include <cstddef>
#include <cstring>

#include "UnicodeCase.h"

namespace pixelui {
inline bool uppercaseLabels = false;
inline bool turkicCase = false;
inline bool preserveTranslatedCase = false;
inline bool copyCaps(char* out, size_t size, const char* text);
// Use translated mixed case as supplied; soften display-only all-caps labels.
enum class LabelCase { Sentence, Preserve, Verbatim };
inline bool copyNatural(char* out, size_t size, const char* text, LabelCase casing = LabelCase::Sentence) {
  if (uppercaseLabels && casing != LabelCase::Verbatim) return copyCaps(out, size, text);
  if (preserveTranslatedCase) casing = LabelCase::Verbatim;
  if (!size) return false;
  if (!text) text = "";
  bool mixed = false;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
    if ((*p >= 'a' && *p <= 'z') || (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7)) mixed = true;
  size_t n = 0;
  bool first = true, unicode = false;
  while (*text && n + 1 < size) {
    const auto ch = static_cast<unsigned char>(*text);
    const size_t bytes = ch < 0x80 ? 1 : ch < 0xE0 ? 2 : ch < 0xF0 ? 3 : 4;
    bool complete = n + bytes < size;
    for (size_t i = 1; complete && i < bytes; ++i)
      complete = text[i] && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80;
    if (!complete) break;
    std::memmove(out + n, text, bytes);
    if (casing == LabelCase::Sentence && !mixed && !first) {
      if (bytes == 1 && ch >= 'A' && ch <= 'Z')
        out[n] = char(ch + 'a' - 'A');
      else if (bytes == 2 && ch == 0xC3) {
        const auto tail = static_cast<unsigned char>(out[n + 1]);
        if (tail >= 0x80 && tail <= 0x9E && tail != 0x97) out[n + 1] = char(tail + 0x20);
      }
    }
    if (ch != ' ' && ch != '\t') first = false;
    unicode |= bytes > 1;
    n += bytes;
    text += bytes;
  }
  out[n] = '\0';
  return unicode;
}
// Keep complete UTF-8 codepoints, including when case conversion changes their byte count.
inline bool copyCaps(char* out, const size_t size, const char* text) {
  if (!size) return false;
  if (!text) text = "";
  if (out == text) {
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    size_t length = 0;
    while (*cursor) {
      const auto* start = cursor;
      const auto upper = uicase::upper(uicase::next(&cursor), turkicCase);
      const size_t bytes = upper < 0x80 ? 1 : upper < 0x800 ? 2 : upper < 0x10000 ? 3 : 4;
      if (length + bytes >= size) {
        cursor = start;
        break;
      }
      length += bytes;
    }
    out[cursor - reinterpret_cast<const unsigned char*>(out)] = 0;
    // Shrink first so expansions cannot overwrite unconverted bytes or exceed capacity.
    for (bool growing : {false, true}) {
      char* at = out;
      while (*at) {
        const auto* next = reinterpret_cast<const unsigned char*>(at);
        const auto upper = uicase::upper(uicase::next(&next), turkicCase);
        const size_t before = next - reinterpret_cast<const unsigned char*>(at);
        const size_t after = upper < 0x80 ? 1 : upper < 0x800 ? 2 : upper < 0x10000 ? 3 : 4;
        if ((after > before) == growing) {
          if (after != before) std::memmove(at + after, next, std::strlen(reinterpret_cast<const char*>(next)) + 1);
          utf8AppendCodepoint(at, upper);
          at += after;
        } else
          at += before;
      }
    }
    for (const auto* at = reinterpret_cast<const unsigned char*>(out); *at; ++at)
      if (*at >= 0x80) return true;
    return false;
  }
  size_t n = 0;
  bool unicode = false;
  while (*text && n + 1 < size) {
    const auto ch = static_cast<unsigned char>(*text);
    const size_t bytes = ch < 0x80 ? 1 : (ch < 0xE0 ? 2 : (ch < 0xF0 ? 3 : 4));
    if (bytes > 1) {
      unicode = true;
      bool complete = true;
      for (size_t i = 1; i < bytes; ++i) {
        if (!text[i] || (static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
          complete = false;
          break;
        }
      }
      if (!complete) {
        ++text;
        continue;
      }
    }
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    const auto upper = uicase::upper(uicase::next(&cursor), turkicCase);
    const size_t length = upper < 0x80 ? 1 : upper < 0x800 ? 2 : upper < 0x10000 ? 3 : 4;
    if (n + length >= size) break;
    unicode |= length > 1;
    utf8AppendCodepoint(out + n, upper);
    n += length;
    text += bytes;
  }
  out[n] = '\0';
  return unicode;
}
}  // namespace pixelui
