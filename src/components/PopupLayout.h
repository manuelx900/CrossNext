#pragma once

#include <FreeInkUI.h>

#include <algorithm>

namespace popupui {
namespace fui = freeink::ui;
struct Layout {
  fui::Rect body, text;
  fui::TextStyle style;
};
inline Layout layout(const fui::DrawTarget& target, fui::Rect safe, const char* message, fui::TextStyle style,
                     int marginX, int marginY, int frame, int preferredY) {
  safe = safe.inset(fui::Insets{int16_t(frame + 12), int16_t(frame + 12), int16_t(frame + 12), int16_t(frame + 12)});
  const int lineHeight = std::max(1, int(target.lineHeight(style.font)));
  const int contentWidth = std::max(1, int(safe.width) - 2 * marginX);
  style.align = fui::TextAlign::Center;
  style.maxLines = std::clamp((int(safe.height) - 2 * marginY) / lineHeight, 1, 16);
  const int width = std::min(contentWidth, std::max(1, int(target.measureText(style.font, message, style).width)));
  int lines = 0;
  fui::layoutText(target, fui::makeRect(0, 0, width, 1), message, style, [&](const char*, fui::Rect) { ++lines; });
  const int height = std::max(1, lines) * lineHeight + 2 * marginY;
  const int y = std::clamp(preferredY, int(safe.y), std::max(int(safe.y), safe.bottom() - height));
  const auto body = fui::makeRect(safe.x + (safe.width - width - 2 * marginX) / 2, y, width + 2 * marginX, height);
  return {body, body.inset(fui::Insets{int16_t(marginY), int16_t(marginX), int16_t(marginY), int16_t(marginX)}), style};
}
}  // namespace popupui
