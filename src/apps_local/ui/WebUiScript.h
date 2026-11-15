#pragma once

#include <I18nPackKeys.h>
#include <Memory.h>

#include <cstring>

#include "WebUiTerms.h"

namespace webui {
inline size_t jsonLength(const char* text) {
  size_t length = 2;
  for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
    length += *p < 32 ? 6 : (*p == '"' || *p == '\\') ? 2 : 1;
  return length;
}
inline char* writeJson(char* out, const char* text) {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  *out++ = '"';
  for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
    if (*p < 32) {
      std::memcpy(out, "\\u00", 4);
      out += 4;
      *out++ = HEX_DIGITS[*p >> 4];
      *out++ = HEX_DIGITS[*p & 15];
    } else {
      if (*p == '"' || *p == '\\') *out++ = '\\';
      *out++ = *p;
    }
  }
  *out++ = '"';
  return out;
}
struct Script {
  std::unique_ptr<char[]> data;
  size_t size = 0;
};
inline Script script() {
  static constexpr char PREFIX[] = "window.CrossNextUi={";
  static constexpr char LOCALE[] = "};window.CrossNextLocale=";
  const auto selected = I18N.getLanguage();
  const char* language = LANGUAGE_TAGS[static_cast<size_t>(selected)];
  size_t length = sizeof(PREFIX) - 1 + sizeof(LOCALE) - 1 + jsonLength(language) + 2;
  for (const auto& term : TERMS) length += jsonLength(term.source) + jsonLength(I18N.get(term.id)) + 2;
  if (length > 128 * 1024) return {};
  // One bounded HTTP response; its translated size is unknown at compile time and cannot fit the task stack.
  auto buffer = makeUniqueNoThrow<char[]>(length + 1);
  if (!buffer) return {};
  char* out = buffer.get();
  std::memcpy(out, PREFIX, sizeof(PREFIX) - 1);
  out += sizeof(PREFIX) - 1;
  const char* end = buffer.get() + length;
  const auto append = [&](const char* text) {
    if (jsonLength(text) > static_cast<size_t>(end - out)) return false;
    out = writeJson(out, text);
    return true;
  };
  bool first = true;
  for (const auto& term : TERMS) {
    if (!first) {
      if (out == end) return {};
      *out++ = ',';
    }
    first = false;
    if (!append(term.source)) return {};
    if (out == end) return {};
    *out++ = ':';
    if (!append(I18N.get(term.id))) return {};
  }
  if (I18N.getLanguage() != selected || size_t(end - out) < sizeof(LOCALE) - 1 + jsonLength(language) + 1) return {};
  std::memcpy(out, LOCALE, sizeof(LOCALE) - 1);
  out += sizeof(LOCALE) - 1;
  out = writeJson(out, language);
  *out++ = ';';
  *out = 0;
  const size_t size = out - buffer.get();
  return {std::move(buffer), size};
}
}  // namespace webui
