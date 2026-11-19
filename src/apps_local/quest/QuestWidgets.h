#pragma once
#include <BidiUtils.h>
#include <I18n.h>

#include <cstdlib>
#include <cstring>

#include "../ui/PixelMenu.h"
#include "../ui/ToyboxIcons.h"
#include "QuestIcons.h"
#include "QuestScreens.h"

namespace questui::widget {
namespace fui = freeink::ui;
inline constexpr auto ink = fui::Paint::solid(fui::Color::Black);
inline constexpr auto paper = fui::Paint::solid(fui::Color::White);
inline constexpr int GAP = 12, INSET = 12, HEADER_HEIGHT = toybox::kMenuHeaderVisibleHeight, RADIUS = 12, SHADOW = 3;
inline fui::Rect rect(int x, int y, int width, int height) {
  return fui::makeRect(x, y, std::max(0, width), std::max(0, height));
}
inline void panel(toybox::Screen& s, fui::Rect r) {
  s.target().fill(rect(r.x, r.y + SHADOW, r.width, r.height), ink, RADIUS);
  s.target().fill(r, paper, RADIUS);
  s.target().stroke(r, ink, 2, RADIUS);
}
inline void textRun(toybox::Screen& s, fui::Rect box, const char* source, const fui::TextAreaLine& line,
                    const fui::TextStyle& style) {
  char run[224];
  const unsigned length = std::min<unsigned>(line.len, sizeof(run) - 1);
  std::memcpy(run, source + line.start, length);
  run[length] = 0;
  s.target().text(box, run, style);
}
inline void text(toybox::Screen& s, fui::Rect r, const char* value, int font = 0, bool bold = false,
                 fui::TextAlign align = fui::TextAlign::Left, bool inverse = false, int lines = 1) {
  if (r.empty() || !value || !*value) return;
  const auto area = r;
  if (r.width > 4) r = r.inset({0, 2, 0, 2});
  fui::TextStyle style;
  style.font = font;
  style.bold = bold;
  style.align = align == fui::TextAlign::Left && BidiUtils::startsWithRtl(value) ? fui::TextAlign::Right : align;
  style.maxLines = std::max(1, lines);
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  fui::Rect bounds{};
  int boundsFont = -1;
  const auto inkBounds = [&]() -> fui::Rect {
    if (boundsFont != style.font) {
      bounds = s.target().inkBounds(style.font, value, style);
      boundsFont = style.font;
    }
    return bounds;
  };
  const auto fits = [&] {
    const auto ink = inkBounds();
    if (ink.width <= r.width && !std::strchr(value, '\n')) return ink.height <= r.height;
    if (lines <= 1) return false;
    const auto count = fui::textAreaWalk(s.target(), r.width, value, style, [](uint32_t, const fui::TextAreaLine&) {});
    return count <= unsigned(lines) && int(count) * s.target().lineHeight(style.font) <= r.height &&
           (int(count) - 1) * s.target().lineHeight(style.font) + ink.height <= r.height;
  };
  if (!fits()) {
    if (font < 3) style.font = 4;
    if (!fits()) style.font = 3;
  }
  style.maxLines =
      std::min<int>(style.maxLines, std::max(1, r.height / std::max(1, int(s.target().lineHeight(style.font)))));
  if (lines == 1 && r.height >= 2 * s.target().lineHeight(style.font) && inkBounds().width > r.width)
    style.maxLines = 2;
  const auto previousClip = s.target().clipRect();
  const auto clipped =
      rect(std::max<int>(area.x, previousClip.x), std::max<int>(area.y, previousClip.y),
           std::min<int>(area.right(), previousClip.right()) - std::max<int>(area.x, previousClip.x),
           std::min<int>(area.bottom(), previousClip.bottom()) - std::max<int>(area.y, previousClip.y));
  if (clipped.empty()) return;
  s.target().setClipRect(clipped);
  auto lineBox = r;
  if (style.maxLines == 1 && !std::strchr(value, '\n')) {
    const auto ink = inkBounds();
    lineBox.y += (r.height - ink.height) / 2 - ink.y;
    lineBox.height = s.target().lineHeight(style.font);
  } else {
    const auto ink = inkBounds();
    lineBox.y += (s.target().lineHeight(style.font) - ink.height) / 2 - ink.y;
  }
  if (style.maxLines == 1 && !std::strchr(value, '\n')) {
    s.target().text(lineBox, value, style);
  } else {
    const auto count = fui::textAreaWalk(s.target(), r.width, value, style, [](uint32_t, const fui::TextAreaLine&) {});
    if (count > unsigned(style.maxLines)) {
      s.target().text(lineBox, value, style);
    } else {
      const int height = s.target().lineHeight(style.font);
      const int top = lineBox.y + std::max(0, (lineBox.height - int(count) * height) / 2);
      style.maxLines = 1;
      fui::textAreaWalk(s.target(), r.width, value, style, [&](uint32_t n, const fui::TextAreaLine& line) {
        textRun(s, rect(r.x, top + int(n) * height, r.width, height), value, line, style);
      });
    }
  }
  s.target().setClipRect(previousClip);
}
inline void narrativeLine(toybox::Screen& s, fui::Rect box, char* line, fui::TextStyle style, bool justify) {
  size_t length = std::strlen(line);
  while (length && line[length - 1] == ' ') line[--length] = 0;
  if (!length) return;
  if (BidiUtils::startsWithRtl(line)) {
    style.align = fui::TextAlign::Right;
    s.target().text(box, line, style);
    return;
  }
  int words = 0, width = 0;
  for (char* at = line; *at;) {
    while (*at == ' ') ++at;
    if (!*at) break;
    char* end = at;
    while (*end && *end != ' ') ++end;
    const char saved = *end;
    *end = 0;
    width += s.target().measureText(style.font, at, style).width;
    ++words;
    *end = saved;
    at = end;
  }
  const int gaps = words - 1, space = s.target().measureText(style.font, " ", style).width;
  const int spare = box.width - width;
  if (!justify || gaps < 1 || spare < gaps * space || spare > gaps * space * 3) {
    s.target().text(box, line, style);
    return;
  }
  int x = box.x, gap = 0;
  for (char* at = line; *at;) {
    while (*at == ' ') ++at;
    if (!*at) break;
    char* end = at;
    while (*end && *end != ' ') ++end;
    const char saved = *end;
    *end = 0;
    const int wordWidth = s.target().measureText(style.font, at, style).width;
    s.target().text(rect(x, box.y, box.right() - x, box.height), at, style);
    x += wordWidth + spare / gaps + (gap++ < spare % gaps);
    *end = saved;
    at = end;
  }
}
inline void icon(toybox::Screen& s, fui::Rect r, const freeink::Icon& value, bool inverse = false) {
  s.target().bitmap(r, fui::bitmapFromIcon(value), fui::BitmapMode::Contain, inverse ? paper : ink);
}
inline void framedIcon(toybox::Screen& s, fui::Rect r, const freeink::Icon& value, bool inverse = false) {
  s.target().stroke(r, inverse ? paper : ink, 2, std::min(16, int(r.height) / 4));
  const int padding = std::max(4, std::min(10, int(r.width) / 7));
  icon(s, r.inset({int16_t(padding), int16_t(padding), int16_t(padding), int16_t(padding)}), value, inverse);
}
inline void diamond(toybox::Screen& s, int x, int y, int radius, bool filled = false) {
  if (filled) {
    for (int dy = -radius; dy <= radius; ++dy) {
      const int half = radius - std::abs(dy);
      s.target().fill(rect(x - half, y + dy, half * 2 + 1, 1), ink);
    }
    return;
  }
  s.target().line({int16_t(x), int16_t(y - radius)}, {int16_t(x + radius), int16_t(y)}, 2, ink);
  s.target().line({int16_t(x + radius), int16_t(y)}, {int16_t(x), int16_t(y + radius)}, 2, ink);
  s.target().line({int16_t(x), int16_t(y + radius)}, {int16_t(x - radius), int16_t(y)}, 2, ink);
  s.target().line({int16_t(x - radius), int16_t(y)}, {int16_t(x), int16_t(y - radius)}, 2, ink);
}
inline void ornamentTitle(toybox::Screen& s, fui::Rect r, const char* title, bool centred = false) {
  fui::TextStyle style;
  style.bold = true;
  int font = 0, width = s.target().measureText(font, title, style).width;
  if (width > r.width - 80) {
    font = 3;
    width = s.target().measureText(font, title, style).width;
  }
  width = std::min(width + 6, std::max(0, int(r.width) - 28));
  const int left = centred ? r.x + (r.width - width) / 2 : r.x + 2, y = r.y + r.height / 2;
  text(s, rect(left, r.y, width, r.height), title, font, true, centred ? fui::TextAlign::Center : fui::TextAlign::Left);
  const int right = left + width + 16;
  if (r.right() - right > 20) {
    if (centred) {
      diamond(s, right, y, 4);
      s.target().line({int16_t(right + 9), int16_t(y)}, {int16_t(r.right() - 5), int16_t(y)}, 2, ink);
    } else {
      s.target().line({int16_t(right), int16_t(y)}, {int16_t(r.right() - 14), int16_t(y)}, 2, ink);
      diamond(s, r.right() - 8, y, 4, true);
    }
  }
  if (centred && left - r.x > 30) {
    s.target().line({int16_t(r.x + 5), int16_t(y)}, {int16_t(left - 25), int16_t(y)}, 2, ink);
    diamond(s, left - 16, y, 4);
  }
}
inline void statusPill(toybox::Screen& s, fui::Rect r, const char* label, bool active = false,
                       const freeink::Icon* mark = nullptr, fui::FontId font = 4) {
  s.target().fill(r, active ? ink : paper, r.height / 2);
  s.target().stroke(r, ink, 2, r.height / 2);
  const int side = std::min(22, int(r.height) - 8);
  if (mark) icon(s, rect(r.x + 10, r.y + (r.height - side) / 2, side, side), *mark, active);
  const int inset = mark ? side + 18 : 6;
  text(s, rect(r.x + inset, r.y, r.width - inset - 6, r.height), label, font, true, fui::TextAlign::Center, active);
}
inline bool button(toybox::Screen& s, Work& w, fui::Rect r, const char* label, fui::ActionId id, int value = 0,
                   bool primary = false, bool enabled = true, uint16_t inputMask = fui::InputDefault) {
  panel(s, r);
  const bool inverse = enabled && s.frame().buttonSelected(id, value, primary);
  w.button = {};
  w.button.label = "";
  w.button.action = enabled ? id : fui::NO_ACTION;
  w.button.value = int16_t(value);
  w.button.inputMask = inputMask;
  w.button.minTouchSize = 24;
  w.button.styles = pixelui::buttonStyles();
  for (auto* style : {&w.button.styles.normal, &w.button.styles.selected, &w.button.styles.focused,
                      &w.button.styles.active, &w.button.styles.disabled}) {
    style->radius = RADIUS;
    style->borderWidth = 2;
  }
  w.button.state = !enabled ? fui::StateDisabled : inverse ? fui::StateSelected : fui::StateNormal;
  fui::button(s.frame(), r, w.button);
  text(s, r.inset({4, 8, 4, 8}), label, 0, primary, fui::TextAlign::Center, inverse);
  return inverse;
}
inline void progress(toybox::Screen& s, fui::Rect r, uint64_t value, uint64_t total) {
  s.target().fill(r, fui::Paint::dither(fui::Color::LightGray), r.height / 2);
  if (total && value) {
    r.width = int(r.width) * std::min<double>(1, double(value) / total);
    s.target().fill(r, ink, r.height / 2);
  }
}
inline void pagination(toybox::Screen& s, Work& w, fui::Rect band, int page, int pages) {
  const int side = std::min(44, int(band.height)), width = std::min(240, int(band.width));
  const auto row = rect(band.x + (band.width - width) / 2, band.y, width, side);
  for (int n = 0; n < 2; ++n) {
    const auto box = rect(n ? row.right() - side : row.x, row.y, side, side);
    const bool enabled = n ? page < pages : page > 1;
    const bool inverse = button(s, w, box, "", Page, n ? 1 : -1, false, enabled,
                                fui::InputDefault | (n ? fui::InputNext : fui::InputPrev));
    const int direction = n ? 1 : -1, x = box.x + side / 2, y = box.y + side / 2;
    const auto paint = inverse ? paper : ink;
    s.target().line({int16_t(x - direction * 3), int16_t(y - 7)}, {int16_t(x + direction * 4), int16_t(y)}, 2, paint);
    s.target().line({int16_t(x + direction * 4), int16_t(y)}, {int16_t(x - direction * 3), int16_t(y + 7)}, 2, paint);
  }
  char label[32];
  std::snprintf(label, sizeof(label), "%d / %d", std::clamp(page, 1, std::max(1, pages)), std::max(1, pages));
  text(s, rect(row.x + side + GAP, row.y, row.width - 2 * (side + GAP), side), label, 0, true, fui::TextAlign::Center);
}
inline void closeMark(toybox::Screen& s, fui::Rect r) {
  const auto box = r.inset({13, 13, 13, 13});
  s.target().line({box.x, box.y}, {int16_t(box.right()), int16_t(box.bottom())}, 2, ink);
  s.target().line({box.x, int16_t(box.bottom())}, {int16_t(box.right()), box.y}, 2, ink);
}
inline fui::Rect content(toybox::Screen& s) {
  const auto r = s.device().safeRect();
  return rect(r.x + 10, r.y + HEADER_HEIGHT + 10, r.width - 20, r.height - HEADER_HEIGHT - 20);
}
inline void navigation(toybox::Screen& s, const Model& m, Work& w) {
  chrome::recordPaintedHeader(0, false);
  const auto safe = s.device().safeRect();
  const bool dark = !chrome::headerIsLight();
  const auto foreground = dark ? paper : ink;
  s.target().fill(rect(0, 0, s.device().width, safe.y + HEADER_HEIGHT), dark ? ink : paper);
  const auto back = pixelui::geometry(s.device()).back;
  const int rowY = safe.y + 12;
  backui::publish(s.device().hasTouch, back);
  backui::paint(s.target(), back, dark ? fui::Color::White : fui::Color::Black);
  const bool actions = m.view != View::Setup && m.view != View::Name && m.view != View::Ending &&
                       m.view != View::ReadingReturn && m.view != View::Celebration;
  if (actions) {
    const auto box = rect(safe.right() - 62, rowY, 48, 48);
    w.button = {};
    w.button.label = "";
    w.button.action = m.view == View::Experience ? fui::NO_ACTION : OpenView;
    w.button.value = int(View::Settings);
    w.button.styles = pixelui::buttonStyles();
    for (auto* style : {&w.button.styles.normal, &w.button.styles.selected, &w.button.styles.focused,
                        &w.button.styles.active, &w.button.styles.disabled}) {
      style->background = foreground;
      style->foreground = dark ? ink : paper;
      style->border = foreground;
      style->borderWidth = 2;
      style->radius = RADIUS;
    }
    fui::button(s.frame(), box, w.button);
    icon(s, box.inset({10, 10, 10, 10}), icon_go_settings_32, !dark);
  }
  const auto& status = s.headerStatus();
  fui::TextStyle clock;
  clock.font = fui::HEADER_CLOCK_FONT;
  clock.bold = true;
  clock.align = fui::TextAlign::Right;
  clock.color = dark ? fui::Color::White : fui::Color::Black;
  const int right = safe.right() - (actions ? 72 : 14), titleX = back.right() + 4;
  const auto clockBox =
      fui::headerClockRect(s.target(), rect(titleX + 140, rowY, right - titleX - 148, 48), status.clock, clock);
  if (!clockBox.empty()) s.target().text(clockBox, status.clock, clock);
  pixelui::copyNatural(w.label, sizeof(w.label), tr(STR_READING_QUEST));
  text(s, rect(titleX, rowY, (clockBox.empty() ? right - 8 : clockBox.x - 12) - titleX, 48), w.label,
       pixelui::uppercaseLabels ? fui::HEADER_CLOCK_FONT : 0, true, fui::TextAlign::Left, dark);
  chrome::recordPaintedHeader(safe.y + HEADER_HEIGHT, !dark);
}
}  // namespace questui::widget
