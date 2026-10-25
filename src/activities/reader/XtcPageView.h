#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace xtcview {
struct View {
  int x = 0, y = 0, width = 0, height = 0;
  int sourceWidth = 0, sourceHeight = 0;
  int sourceX(int dx) const { return width > 1 ? int(int64_t(dx) * (sourceWidth - 1) / (width - 1)) : 0; }
  int sourceY(int dy) const { return height > 1 ? int(int64_t(dy) * (sourceHeight - 1) / (height - 1)) : 0; }
};
// Preserve the complete page and leave the status overlay outside its viewport.
inline View fit(int sourceWidth, int sourceHeight, int screenWidth, int screenHeight, int top, int right, int bottom,
                int left) {
  View view;
  const int width = screenWidth - left - right, height = screenHeight - top - bottom;
  if (sourceWidth <= 0 || sourceHeight <= 0 || width <= 0 || height <= 0) return view;
  view.sourceWidth = sourceWidth;
  view.sourceHeight = sourceHeight;
  view.width = std::min(width, sourceWidth);
  view.height = std::min(height, sourceHeight);
  if (int64_t(view.width) * sourceHeight > int64_t(view.height) * sourceWidth)
    view.width = std::max(1, int(int64_t(view.height) * sourceWidth / sourceHeight));
  else
    view.height = std::max(1, int(int64_t(view.width) * sourceHeight / sourceWidth));
  view.x = left + (width - view.width) / 2;
  view.y = top + (height - view.height) / 2;
  return view;
}
// XTC uses row-major inverted bits; XTCH stores two reversed column-major planes.
inline uint8_t pixel(const uint8_t* data, int width, int height, int depth, int x, int y) {
  if (depth == 2) {
    const size_t columnBytes = (size_t(height) + 7) / 8;
    const size_t offset = size_t(width - 1 - x) * columnBytes + y / 8;
    const unsigned shift = 7 - y % 8;
    return ((data[offset] >> shift) & 1) * 2 + ((data[size_t(width) * columnBytes + offset] >> shift) & 1);
  }
  return !((data[size_t(y) * ((size_t(width) + 7) / 8) + x / 8] >> (7 - x % 8)) & 1);
}
}  // namespace xtcview
