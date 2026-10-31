#pragma once
#include <FreeInkUI.h>

#include <algorithm>
namespace homecover {
// Fit a cached thumbnail without enlarging or clipping its source artwork.
inline freeink::ui::Rect contain(freeink::ui::Rect slot, int width, int height) {
  if (slot.empty() || width <= 0 || height <= 0) return {};
  int w = std::min(width, int(slot.width));
  int h = std::min(height, int(slot.height));
  if (int64_t(w) * height > int64_t(h) * width)
    w = std::max(1, int(int64_t(width) * h / height));
  else
    h = std::max(1, int(int64_t(height) * w / width));
  return freeink::ui::makeRect(slot.x + (slot.width - w) / 2, slot.y + (slot.height - h) / 2, w, h);
}
}  // namespace homecover
