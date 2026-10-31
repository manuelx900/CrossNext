#pragma once
#include "../../../lib/GfxRenderer/ChromeRegion.h"
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
  p.valueText.font = 0;
  p.headerText.font = 0;
  p.headerText.bold = true;
  p.headerUnderline = false;
  p.headerRowHeight = 32;
  p.sectionGap = 8;
  p.rowStyles.normal.borderWidth = 1;
  p.trailingCentered = true;
  p.checkboxCheckmark = true;
  if (chrome::lightHeader) {
    p.labelText.font = 0;
    p.labelText.maxLines = 2;
    p.subtitleText.font = 4;
    p.subtitleText.maxLines = 2;
    p.valueText.font = 0;
    p.rowHeight = 70;
    p.rowPaddingY = 10;
    p.headerText.bold = true;
    p.separatorPaint = freeink::ui::Paint::dither(freeink::ui::Color::DarkGray);
    p.rowStyles.normal.borderWidth = 0;
    p.rowStyles.normal.background = freeink::ui::Paint::solid(freeink::ui::Color::White);
    p.rowStyles.normal.foreground = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    p.rowStyles.selected = p.rowStyles.focused = p.rowStyles.active = p.rowStyles.normal;
    p.selectionMarker = freeink::ui::SelectionMarker::Underline;
    p.markerThickness = 1;
    p.markerPaint = lightui::rule;
  }
}
}  // namespace settingsui
