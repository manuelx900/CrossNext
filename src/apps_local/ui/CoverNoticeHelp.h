#pragma once

#include <I18n.h>

#include "PixelMenu.h"

namespace covernotice {
namespace fui = freeink::ui;
inline constexpr StrId PARAGRAPHS[] = {StrId::STR_HOME_COVER_NOTICE_BENEFIT, StrId::STR_HOME_COVER_NOTICE_DURING,
                                       StrId::STR_HOME_COVER_NOTICE_DISABLED, StrId::STR_HOME_COVER_NOTICE_ADVANTAGE};
template <typename Screen>
freeink::ui::Rect buttonRect(Screen& screen) {
  const auto full = pixelui::geometry(screen.device()).body;
  fui::TextStyle text;
  text.font = 3;
  text.maxLines = 2;
  const auto* label = tr(STR_HOME_COVER_NOTICE_WHY);
  const int width = std::min<int>(full.width, screen.target().measureText(3, label, text).width + 32);
  const auto size = fui::measureWrappedText(screen.target(), label, text, width - 24);
  const int height = std::max(44, int(size.height) + 16);
  return fui::makeRect(full.right() - width, full.bottom() - height - 4, width, height);
}
template <typename Screen>
int footerReserve(Screen& screen) {
  return buttonRect(screen).height + 16;
}
template <typename Screen>
void button(Screen& screen, fui::ActionId action) {
  const auto box = buttonRect(screen);
  fui::TextStyle text;
  text.font = 3;
  text.align = fui::TextAlign::Center;
  text.maxLines = 2;
  screen.target().fill(box, lightui::paper, lightui::radius());
  screen.target().stroke(box, lightui::ink, 1, lightui::radius());
  screen.target().text(fui::makeRect(box.x + 12, box.y + 8, box.width - 24, box.height - 16),
                       tr(STR_HOME_COVER_NOTICE_WHY), text);
  screen.frame().hit(box, action, 0, fui::InputTouch);
}
template <typename Screen>
int contentHeight(Screen& screen) {
  const auto body = screen.body();
  fui::TextStyle copy;
  copy.font = 4;
  copy.maxLines = 16;
  int height = 24;
  for (const auto key : PARAGRAPHS)
    height += fui::measureWrappedText(screen.target(), I18N.get(key), copy, body.width - 36).height + 12;
  return height;
}
template <typename Screen, typename CompactFont>
void fit(Screen& screen, CompactFont compact) {
  if (contentHeight(screen) <= screen.body().height) return;
  compact(false);
  if (contentHeight(screen) > screen.body().height) compact(true);
}
template <typename Screen>
void explain(Screen& screen) {
  const auto body = screen.body();
  fui::TextStyle copy;
  copy.font = 4;
  copy.maxLines = 16;
  const int inset = 18, gap = 12, width = body.width - 2 * inset;
  const int height = contentHeight(screen);
  screen.target().stroke(fui::makeRect(body.x, body.y, body.width, height), lightui::ink, 1, lightui::radius());
  int y = body.y + inset;
  for (const auto key : PARAGRAPHS) {
    const auto* text = I18N.get(key);
    const auto size = fui::measureWrappedText(screen.target(), text, copy, width);
    screen.target().text(fui::makeRect(body.x + inset, y, width, size.height), text, copy);
    y += size.height + gap;
  }
}
}  // namespace covernotice
