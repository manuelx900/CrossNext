#pragma once
#include <FreeInkUI.h>

#include <algorithm>

#include "../../components/PopupLayout.h"

namespace libraryui {
inline void loadingNotice(freeink::ui::DrawTarget& target, freeink::ui::Rect area, const char* label) {
  namespace fui = freeink::ui;
  if (area.width < 80 || area.height < 48) return;
  fui::TextStyle text;
  text.font = 4;
  text.color = fui::Color::White;
  text.maxLines = 2;
  text.align = fui::TextAlign::Center;
  const int paddingX = 18, paddingY = 12;
  const int width =
      std::min(int(area.width) - 32, int(target.measureText(text.font, label, text).width) + 2 * paddingX);
  const int lines = std::clamp(
      int(fui::textAreaWalk(target, width - 2 * paddingX, label, text, [](uint32_t, const fui::TextAreaLine&) {})), 1,
      2);
  const int lineHeight = target.lineHeight(text.font);
  const int height = std::min(int(area.height) - 4, lines * lineHeight + 2 * paddingY);
  const int y = area.y + (area.height - height) / 2;
  const auto box = fui::makeRect(area.x + (area.width - width) / 2, y, width, height);
  target.fill(box, fui::Paint::solid(fui::Color::Black));
  const popupui::Layout popup{box.inset(fui::Insets{4, 4, 4, 4}), {}, text};
  popupui::paintChrome(target, popup, 2, 0);
  target.text(box.inset({int16_t(paddingY), int16_t(paddingX), int16_t(paddingY), int16_t(paddingX)}), label, text);
}
}  // namespace libraryui
