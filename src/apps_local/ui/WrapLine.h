#pragma once

#include <FreeInkUI.h>

#include <cstddef>
#include <cstring>

namespace pagedtext {
struct WrappedLine {
  unsigned end = 0, after = 0;
};
// Bisection measures complete UTF-8 prefixes instead of every growing prefix.
// The caller owns and reuses the line buffer; an oversized glyph still advances.
inline WrappedLine wrapLine(const freeink::ui::DrawTarget& target, const char* text, unsigned start, int width,
                            freeink::ui::TextStyle style, char* line, size_t capacity) {
  const auto advance = [&](unsigned at) {
    const auto c = static_cast<unsigned char>(text[at]);
    unsigned bytes = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
    unsigned valid = 1;
    while (valid < bytes && text[at + valid] && (static_cast<unsigned char>(text[at + valid]) & 0xc0) == 0x80) ++valid;
    return at + valid;
  };
  unsigned stop = start, count = 0;
  while (text[stop] && text[stop] != '\n') {
    const unsigned next = advance(stop);
    if (next - start >= capacity) break;
    stop = next;
    ++count;
  }
  unsigned low = 0, high = count;
  while (low < high) {
    const unsigned mid = low + (high - low + 1) / 2;
    unsigned end = start;
    for (unsigned n = 0; n < mid; ++n) end = advance(end);
    std::memcpy(line, text + start, end - start);
    line[end - start] = 0;
    if (target.measureText(style.font, line, style).width <= width)
      low = mid;
    else
      high = mid - 1;
  }
  unsigned end = start, space = start;
  for (unsigned n = 0; n < low; ++n) {
    if (text[end] == ' ') space = end;
    end = advance(end);
  }
  if (end == start && text[end] && text[end] != '\n') end = advance(end);
  unsigned after = end;
  if (text[end] && text[end] != '\n' && space > start) {
    end = space;
    after = space + 1;
  } else if (text[end] == '\n')
    after = end + 1;
  while (text[after] == ' ') ++after;
  std::memcpy(line, text + start, end - start);
  line[end - start] = 0;
  return {end, after};
}
}  // namespace pagedtext
