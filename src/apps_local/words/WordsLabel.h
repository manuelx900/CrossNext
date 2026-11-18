#pragma once

#include <cstring>

#include "../ui/UnicodeCase.h"
#include "Vocabulary.h"

namespace wordsui {
// Presentation only: dictionary headwords, record IDs and review history stay
// canonical. One UTF-8 initial is replaced without allocating a string.
inline void wordLabel(char* out, size_t capacity, const char* word, const char* language = "") {
  vocabulary::copy(out, capacity, word);
  if (!*word) return;
  auto* next = reinterpret_cast<const unsigned char*>(word);
  const auto initial = uicase::next(&next);
  if (initial == 0xfffd) return;
  const char first = language[0], second = first ? language[1] : 0;
  const bool turkic = (((first == 't' || first == 'T') && (second == 'r' || second == 'R')) ||
                       ((first == 'a' || first == 'A') && (second == 'z' || second == 'Z'))) &&
                      (language[2] == 0 || language[2] == '-' || language[2] == '_');
  const auto upper = uicase::upper(initial, turkic);
  if (upper == initial) return;
  char encoded[4];
  const size_t length = utf8AppendCodepoint(encoded, upper) - encoded;
  const auto* rest = reinterpret_cast<const char*>(next);
  const size_t remaining = std::strlen(rest);
  if (length + remaining >= capacity) return;
  std::memmove(out + length, rest, remaining + 1);
  std::memcpy(out, encoded, length);
}
}  // namespace wordsui
