#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstdint>

namespace homecover {
inline int edgeInset(int x, freeink::ui::Rect slot, int taper) {
  const int cut = taper ? std::max(3, int(slot.height) / 16) : 0;
  return taper < 0 ? cut * (slot.width - 1 - x) / std::max(1, int(slot.width) - 1)
                   : cut * x / std::max(1, int(slot.width) - 1);
}
template <typename Clear, typename Draw>
void finishTaper(freeink::ui::Rect slot, int taper, Clear clear, Draw draw) {
  if (!taper || slot.empty()) return;
  int start = 0;
  while (start < slot.width) {
    const int inset = edgeInset(start, slot, taper);
    int end = start + 1;
    while (end < slot.width && edgeInset(end, slot, taper) == inset) ++end;
    if (inset) {
      clear(slot.x + start, slot.y, end - start, inset);
      clear(slot.x + start, slot.bottom() - inset, end - start, inset);
    }
    draw(slot.x + start, slot.y + inset, end - start, 1);
    draw(slot.x + start, slot.bottom() - 1 - inset, end - start, 1);
    start = end;
  }
  const int outside = taper < 0 ? 0 : slot.width - 1, inset = edgeInset(outside, slot, taper);
  draw(slot.x + outside, slot.y + inset, 1, slot.height - 2 * inset);
}
template <typename White, typename Draw>
void paintRowWith(White white, int sourceX, int cropWidth, int cropHeight, int sourceLine, freeink::ui::Rect slot,
                  int taper, Draw draw) {
  int runStart = -1, runTop = 0, runBottom = 0;
  const auto flush = [&](int end) {
    if (runStart >= 0) draw(slot.x + runStart, slot.y + runTop, end - runStart, runBottom - runTop);
    runStart = -1;
  };
  if (!taper) {
    runTop = sourceLine * slot.height / cropHeight;
    runBottom = (sourceLine + 1) * slot.height / cropHeight;
    if (runBottom <= runTop) return;
    for (int x = 0; x < slot.width; ++x) {
      const int sx = sourceX + (cropWidth == slot.width ? x : x * cropWidth / slot.width);
      if (white(sx))
        flush(x);
      else if (runStart < 0)
        runStart = x;
    }
    flush(slot.width);
    return;
  }
  for (int x = 0; x < slot.width; ++x) {
    const int sx = sourceX + x * cropWidth / slot.width;
    if (white(sx)) {
      flush(x);
      continue;
    }
    const int inset = edgeInset(x, slot, taper);
    const int height = slot.height - inset * 2;
    const int top = inset + sourceLine * height / cropHeight;
    const int bottom = inset + (sourceLine + 1) * height / cropHeight;
    if (bottom <= top) {
      flush(x);
      continue;
    }
    if (runStart >= 0 && top == runTop && bottom == runBottom) continue;
    flush(x);
    runStart = x;
    runTop = top;
    runBottom = bottom;
  }
  flush(slot.width);
}

template <typename Draw>
void paintRow(const uint8_t* row, int sourceX, int cropWidth, int cropHeight, int sourceLine, freeink::ui::Rect slot,
              int taper, Draw draw) {
  paintRowWith([row](int x) { return ((row[x / 4] >> (6 - x % 4 * 2)) & 3) == 3; }, sourceX, cropWidth, cropHeight,
               sourceLine, slot, taper, draw);
}

template <typename Draw>
void paint1BitRow(const uint8_t* row, int sourceX, int cropWidth, int cropHeight, int sourceLine,
                  freeink::ui::Rect slot, int taper, Draw draw) {
  paintRowWith([row](int x) { return (row[x / 8] & (0x80 >> (x % 8))) != 0; }, sourceX, cropWidth, cropHeight,
               sourceLine, slot, taper, draw);
}
}  // namespace homecover
