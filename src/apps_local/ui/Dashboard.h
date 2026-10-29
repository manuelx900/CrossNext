#pragma once

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "ExtraIcons.h"
#include "NumberTypography.h"
#include "PixelMenu.h"
#include "ToyboxScreen.h"

namespace dashboard {
namespace fui = freeink::ui;
inline void date(char* out, size_t size, int year, int month, int day) {
  static constexpr StrId months[] = {StrId::STR_UI_JAN_521AD2, StrId::STR_UI_FEB_F719E9, StrId::STR_UI_MAR_A8D9B8,
                                     StrId::STR_UI_APR_72402E, StrId::STR_UI_MAY_18AE53, StrId::STR_UI_JUN_F998A6,
                                     StrId::STR_UI_JUL_7DA032, StrId::STR_UI_AUG_FE905B, StrId::STR_UI_SEP_492C5F,
                                     StrId::STR_UI_OCT_AA5816, StrId::STR_UI_NOV_F62CE2, StrId::STR_UI_DEC_CA3A45};
  if (year < 2020 || month < 1 || month > 12 || day < 1 || day > 31) {
    std::snprintf(out, size, "-- --- ----");
    return;
  }
  std::snprintf(out, size, "%02d %s %04d", day, I18N.get(months[month - 1]), year);
}
inline constexpr auto ink = fui::Paint::solid(fui::Color::Black);
inline constexpr auto paper = fui::Paint::solid(fui::Color::White);
inline void chevron(fui::DrawTarget& target, fui::Rect rect, bool inverse = false, bool previous = false) {
  const auto paint = inverse ? paper : ink;
  const int cx = rect.x + rect.width / 2, cy = rect.y + rect.height / 2, direction = previous ? -1 : 1;
  target.line({int16_t(cx - direction * 4), int16_t(cy - 9)}, {int16_t(cx + direction * 5), int16_t(cy)}, 3, paint);
  target.line({int16_t(cx + direction * 5), int16_t(cy)}, {int16_t(cx - direction * 4), int16_t(cy + 9)}, 3, paint);
}
inline void action(fui::DrawTarget& target, fui::Rect rect, const char* label, const freeink::Icon* mark = nullptr,
                   bool primary = false, int font = pixelui::kBody) {
  target.fill(rect, primary ? ink : paper);
  target.stroke(rect, ink, 2);
  const int side = std::min(48, int(rect.height) - 24);
  if (mark)
    target.bitmap(fui::makeRect(rect.x + 18, rect.y + (rect.height - side) / 2, side, side), fui::bitmapFromIcon(*mark),
                  fui::BitmapMode::Contain, primary ? paper : ink);
  const int left = mark ? side + 36 : 18;
  pixelui::text(target, fui::makeRect(rect.x + left, rect.y + 8, rect.width - left - 46, rect.height - 16), label, font,
                primary ? fui::Color::White : fui::Color::Black);
  chevron(target, fui::makeRect(rect.right() - 40, rect.y, 28, rect.height), primary);
}
inline void action(toybox::Screen& screen, fui::Rect rect, const char* label, fui::ActionId id,
                   const freeink::Icon* mark = nullptr, bool primary = false, int value = 0) {
  primary = screen.frame().buttonSelected(id, value, primary);
  action(screen.target(), rect, label, mark, primary);
  screen.frame().hit(rect, id, value, fui::InputTouch);
}
inline void text(toybox::Screen& s, fui::Rect r, const char* value, int font = pixelui::kSmall,
                 fui::TextAlign align = fui::TextAlign::Center, bool inverse = false) {
  pixelui::text(s.target(), r, value, font, inverse ? fui::Color::White : fui::Color::Black, align);
}
inline void prose(toybox::Screen& screen, fui::Rect rect, const char* value,
                  fui::TextAlign align = fui::TextAlign::Center, int lines = 1) {
  fui::TextStyle style;
  style.font = 4;
  style.align = align;
  style.maxLines = lines;
  rect.height = std::max(int(rect.height), int(screen.target().lineHeight(4)) * lines);
  screen.target().text(rect, value, style);
}
inline void frame(toybox::Screen& s, fui::Rect r, int weight = pixelui::kBorder) { s.target().stroke(r, ink, weight); }
inline void icon(toybox::Screen& s, fui::Rect r, const freeink::Icon& value, bool inverse = false) {
  s.target().bitmap(r, fui::bitmapFromIcon(value), fui::BitmapMode::Contain, inverse ? paper : ink);
}
inline void hit(toybox::Screen& s, fui::Rect r, fui::ActionId action, int value = 0) {
  s.frame().hit(r, action, value, fui::InputTouch);
}
inline void button(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, int value = 0,
                   bool filled = false, const freeink::Icon* mark = nullptr, int font = pixelui::kSmall) {
  filled = s.frame().buttonSelected(action, value, filled);
  if (filled)
    s.target().fill(r, ink);
  else
    frame(s, r);
  hit(s, r, action, value);
  if (mark) {
    const int side = std::min(36, int(r.height) - 12);
    icon(s, fui::makeRect(r.x + 12, r.y + (r.height - side) / 2, side, side), *mark, filled);
    r.x += side + 24;
    r.width -= side + 36;
  }
  text(s, r.inset(fui::Insets{0, 8, 0, 8}), label, font, fui::TextAlign::Center, filled);
}
inline void bar(toybox::Screen& s, fui::Rect r, uint64_t value, uint64_t total) {
  frame(s, r, 2);
  const int width = total ? int(uint64_t(std::max(0, r.width - 6)) * std::min(value, total) / total) : 0;
  if (width) s.target().fill(fui::makeRect(r.x + 3, r.y + 3, width, r.height - 6), ink);
}
inline void check(toybox::Screen& s, fui::Rect r, bool checked) {
  if (checked) {
    s.target().fill(r, ink);
    icon(s, r.inset(fui::Insets{5, 5, 5, 5}), extra_check_32, true);
  } else
    frame(s, r, pixelui::kBorder);
}
inline void digits(toybox::Screen& s, fui::Rect r, const char* value, bool inverse = false) {
  numerals::draw(s.target(), r, value, inverse ? fui::Color::White : fui::Color::Black);
}
inline void metric(toybox::Screen& s, fui::Rect r, const char* label, const char* value,
                   const freeink::Icon* mark = nullptr) {
  frame(s, r);
  const int side = std::min(40, int(r.height) / 4);
  if (mark) icon(s, fui::makeRect(r.x + (r.width - side) / 2, r.y + 12, side, side), *mark);
  const int top = r.y + (mark ? side + 20 : 8), labelH = std::min(48, int(r.height) / 3);
  digits(s, fui::makeRect(r.x + 10, top, r.width - 20, std::max(7, r.bottom() - labelH - top - 8)), value);
  char copy[96];
  pixelui::copyCaps(copy, sizeof(copy), label);
  fui::TextStyle style;
  style.font = 3;
  style.align = fui::TextAlign::Center;
  style.maxLines = 2;
  s.target().text(fui::makeRect(r.x + 6, r.bottom() - labelH, r.width - 12, labelH), copy, style);
}
}  // namespace dashboard
