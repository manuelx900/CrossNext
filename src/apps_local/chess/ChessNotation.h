#pragma once

#include <cstddef>

namespace chess {
// Saved SAN stays canonical; only the displayed piece initials are localized.
inline char spanishPiece(char piece) {
  switch (piece) {
    case 'N':
      return 'C';
    case 'B':
      return 'A';
    case 'R':
      return 'T';
    case 'Q':
      return 'D';
    case 'K':
      return 'R';
    default:
      return piece;
  }
}
inline void displaySan(const char* san, bool spanish, char* out, std::size_t size) {
  if (!size) return;
  std::size_t n = 0;
  for (; san[n] && n + 1 < size; ++n) out[n] = spanish && (n == 0 || san[n - 1] == '=') ? spanishPiece(san[n]) : san[n];
  out[n] = 0;
}
}  // namespace chess
