#pragma once
#include <FreeInkUI.h>

#include <algorithm>

namespace homeui {
struct ShelfLayout {
  freeink::ui::Rect footer, grid;
  int rows, gap, cellWidth, artHeight;
};
inline ShelfLayout shelfLayout(const freeink::ui::DeviceContext& device) {
  namespace fui = freeink::ui;
  const auto safe = device.safeRect();
  const bool wide = device.width > device.height;
  const int band = safe.y + 44, footerHeight = wide ? 100 : 140;
  ShelfLayout out;
  out.footer = fui::makeRect(safe.x + 12, safe.bottom() - footerHeight, safe.width - 24, footerHeight);
  out.grid = fui::makeRect(safe.x + 12, band + 62, safe.width - 24, out.footer.y - band - 106);
  out.rows = wide ? 1 : 2;
  out.gap = wide ? 24 : 14;
  out.cellWidth = (out.grid.width - out.gap * 2) / 3;
  const int height = (out.grid.height - out.gap * (out.rows - 1)) / out.rows;
  out.artHeight = std::max(24, std::min(height - 30, out.cellWidth * 3 / 2));
  return out;
}
}  // namespace homeui
