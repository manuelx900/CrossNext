#pragma once

#include <cstddef>
#include <cstring>

namespace pixelui {
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
