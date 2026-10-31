#pragma once

#include <cstddef>
#include <cstring>

namespace pixelui {
// Use translated mixed case as supplied; soften display-only all-caps labels.
enum class LabelCase { Sentence, Preserve };
inline bool copyNatural(char* out, size_t size, const char* text, LabelCase casing = LabelCase::Sentence) {
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
// Preserve complete UTF-8 codepoints and uppercase Latin-1 letters.
inline bool copyCaps(char* out, const size_t size, const char* text) {
  if (!size) return false;
  if (!text) text = "";
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
    if (n + bytes >= size) break;
    if (bytes == 1)
      out[n++] = ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch;
    else {
      std::memmove(out + n, text, bytes);
      if (ch == 0xC3) {
        const auto tail = static_cast<unsigned char>(out[n + 1]);
        if (tail >= 0xA0 && tail <= 0xBE && tail != 0xB7) out[n + 1] = static_cast<char>(tail - 0x20);
      }
      n += bytes;
    }
    text += bytes;
  }
  out[n] = '\0';
  return unicode;
}
}  // namespace pixelui
