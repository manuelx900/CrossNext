#pragma once
#include <FreeInkUICore.h>

#include <algorithm>
namespace freeink {
namespace ui {
template <typename Draw>
void forEachRoundedStrokeBand(Rect rect, uint8_t width, uint8_t radius, uint8_t corners, Draw draw) {
  if (rect.empty() || !width) return;
  const auto inner = rect.inset({int16_t(width), int16_t(width), int16_t(width), int16_t(width)});
  const auto insetAt = [corners](Rect box, int r, int y, bool left) {
    r = std::min({r, int(box.width) / 2, int(box.height) / 2});
    const bool top = y < box.y + r, bottom = y >= box.bottom() - r;
    const int corner = top ? (left ? CornerTopLeft : CornerTopRight) : (left ? CornerBottomLeft : CornerBottomRight);
    if (!r || (!top && !bottom) || !(corners & corner)) return 0;
    const int dy = top ? box.y + r - 1 - y : y - (box.bottom() - r);
    int x = r;
    while (x > 0 && x * x + dy * dy > r * r) --x;
    return r - x;
  };
  for (int y = rect.y; y < rect.bottom(); ++y) {
    const int left = rect.x + insetAt(rect, radius, y, true), right = rect.right() - insetAt(rect, radius, y, false);
    if (inner.empty() || y < inner.y || y >= inner.bottom()) {
      draw(makeRect(left, y, right - left, 1));
      continue;
    }
    const int r = std::max(0, int(radius) - int(width));
    const int a = std::min(right, inner.x + insetAt(inner, r, y, true));
    const int b = std::max(a, inner.right() - insetAt(inner, r, y, false));
    if (a > left) draw(makeRect(left, y, a - left, 1));
    if (right > b) draw(makeRect(b, y, right - b, 1));
  }
}
}  // namespace ui
}  // namespace freeink
