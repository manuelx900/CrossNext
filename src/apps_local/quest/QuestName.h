#pragma once
#include <cstdint>
#include <cstring>

#include "../ui/UnicodeName.h"

namespace quest {
inline constexpr unsigned NAME_LIMIT = 12, NAME_BYTES = NAME_LIMIT * 2 + 1;
inline unsigned nameLength(const char* text) {
  unsigned count = 0;
  for (unsigned n = 0; n < NAME_BYTES && text[n]; ++n)
    if ((uint8_t(text[n]) & 0xc0) != 0x80) ++count;
  return count;
}
inline bool validName(const char* text, bool emptyAllowed = true) {
  return unicode_name::valid(text, NAME_BYTES, NAME_LIMIT, emptyAllowed);
}
inline bool typeName(char* text, char key) {
  if (nameLength(text) >= NAME_LIMIT) return false;
  const auto n = std::strlen(text);
  if (key == ' ') {
    if (!n || text[n - 1] == ' ') return false;
    text[n] = key;
    text[n + 1] = 0;
  } else if (key == '[') {
    if (n + 2 >= NAME_BYTES) return false;
    text[n] = char(0xc3);
    text[n + 1] = char(0x91);
    text[n + 2] = 0;
  } else if (key >= 'A' && key <= 'Z') {
    if (n + 1 >= NAME_BYTES) return false;
    text[n] = key;
    text[n + 1] = 0;
  } else {
    return false;
  }
  return true;
}
inline bool eraseName(char* text) {
  auto n = std::strlen(text);
  if (!n) return false;
  do {
    --n;
  } while (n && (uint8_t(text[n]) & 0xc0) == 0x80);
  text[n] = 0;
  return true;
}
inline bool finishName(char* text) {
  auto n = std::strlen(text);
  while (n && text[n - 1] == ' ') text[--n] = 0;
  return validName(text, false);
}
}  // namespace quest
