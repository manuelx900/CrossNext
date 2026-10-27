#pragma once

#include "../../FreeInkUICore.h"

namespace freeink::ui {
// Fixed chrome font, independent of an app's prose or score fonts.
inline constexpr FontId HEADER_CLOCK_FONT = 5;

// Return a single-line text box only when its complete ink fits the free slot.
inline Rect headerClockRect(const DrawTarget& target, Rect slot, const char* label, TextStyle style) {
  if (!label || !*label || slot.empty()) return {};
  const auto size = target.measureText(style.font, label, style);
  const auto ink = target.inkBounds(style.font, label, style);
  if (size.width <= 0 || size.width > slot.width || ink.height > slot.height) return {};
  return makeRect(slot.right() - size.width, slot.y + (slot.height - ink.height) / 2 - ink.y, size.width,
                  target.lineHeight(style.font));
}
}  // namespace freeink::ui
