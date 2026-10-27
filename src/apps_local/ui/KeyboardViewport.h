#pragma once

#include <algorithm>
#include <limits>

namespace keyboardview {
constexpr int keyHeight(int preferred, bool landscape) { return landscape ? std::min(preferred, 44) : preferred; }
inline int visibleLines(int keyboardTop, int fieldTop, int lineHeight, int gap, bool landscape) {
  return landscape ? std::max(1, (keyboardTop - fieldTop - gap) / std::max(1, lineHeight))
                   : std::numeric_limits<int>::max();
}

// The renderer supplies its UTF-8-aware line breaker; no second wrap table is retained.
template <typename NextLine>
int firstByte(int length, int cursor, int visible, NextLine nextLine) {
  if (visible >= length) return 0;
  int first = 0, line = 0;
  while (first < length) {
    const int end = nextLine(first);
    if (end <= first || cursor < end || end == length) break;
    first = end;
    ++line;
  }
  const int skip = std::max(0, line - std::max(1, visible) + 1);
  first = 0;
  for (int n = 0; n < skip; ++n) {
    const int end = nextLine(first);
    if (end <= first || end >= length) break;
    first = end;
  }
  return first;
}
}  // namespace keyboardview
