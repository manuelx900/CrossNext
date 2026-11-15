#pragma once

namespace wordsui {
// Compact display copy; the saved dictionary entry remains unchanged.
inline void compactDefinition(char* text) {
  char* out = text;
  bool gap = false, line = false;
  for (const char* in = text; *in; ++in) {
    const unsigned char c = static_cast<unsigned char>(*in);
    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
      if (out != text) gap = true;
      line |= c == '\r' || c == '\n';
      continue;
    }
    if (gap && out != text) *out++ = line ? '\n' : ' ';
    *out++ = *in;
    gap = line = false;
  }
  *out = 0;
}
}  // namespace wordsui
