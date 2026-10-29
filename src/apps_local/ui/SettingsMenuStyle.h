#pragma once
#include "PixelMenu.h"
namespace settingsui {
inline void styleRows(freeink::ui::ListProps& p) {
  pixelui::styleList(p, true);
  p.rowHeight = 60;
  p.rowPaddingY = 4;
  p.iconSize = 42;
  p.textGap = 16;
  p.sidePadding = 18;
  p.labelText.font = 0;
  p.labelText.bold = false;
  p.labelText.maxLines = 3;
  p.subtitleText.font = 3;
  p.subtitleText.maxLines = 3;
  p.valueText.font = 4;
  p.headerText.font = 0;
  p.headerText.bold = true;
  p.headerUnderline = false;
  p.headerRowHeight = 32;
  p.sectionGap = 8;
  p.rowStyles.normal.borderWidth = 1;
  p.trailingCentered = true;
  p.checkboxCheckmark = true;
}
}  // namespace settingsui
