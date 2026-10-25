#pragma once

#include <I18n.h>

#include <algorithm>

#include "PixelMenu.h"

namespace wordlekeys {
namespace fui = freeink::ui;
inline constexpr char kEnter = '\n', kErase = '\b';
inline constexpr const char* ROWS[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM["};
enum class Tone : uint8_t { Normal, Present, Correct };
struct Layout {
  bool spanish = false;
  int16_t top = 0, keyW = 0, keyH = 0, gap = 0, wide = 0;
  int16_t rowLeft[3]{};
};
inline int length(int row, bool spanish) { return row == 0 ? 10 : row == 1 ? 9 : spanish ? 8 : 7; }
inline fui::Rect keyRect(const Layout& k, int row, int index) {
  return fui::makeRect(k.rowLeft[row] + (row == 2 ? k.wide + k.gap : 0) + index * (k.keyW + k.gap),
                       k.top + row * (k.keyH + k.gap), k.keyW, k.keyH);
}
inline fui::Rect enterRect(const Layout& k) {
  return fui::makeRect(k.rowLeft[2], k.top + 2 * (k.keyH + k.gap), k.wide, k.keyH);
}
inline fui::Rect eraseRect(const Layout& k) {
  const auto last = keyRect(k, 2, length(2, k.spanish) - 1);
  return fui::makeRect(last.right() + k.gap, last.y, k.wide, k.keyH);
}
inline Layout keyboardLayout(const fui::Rect& safe, int16_t top) {
  Layout k;
  k.spanish = I18N.getLanguage() == Language::ES;
  k.top = top;
  k.gap = 4;
  k.keyH = 64;
  k.keyW = (safe.width - 9 * k.gap - 12) / 10;
  const int last = length(2, k.spanish);
  k.wide = (safe.width - last * k.keyW - (last + 1) * k.gap) / 2;
  for (int row = 0; row < 3; ++row) {
    const int width =
        length(row, k.spanish) * k.keyW + (length(row, k.spanish) - 1) * k.gap + (row == 2 ? 2 * (k.wide + k.gap) : 0);
    k.rowLeft[row] = safe.x + (safe.width - width) / 2;
  }
  return k;
}
inline char keyAt(const Layout& k, int x, int y) {
  if (k.keyW <= 0) return 0;
  if (enterRect(k).contains(x, y)) return kEnter;
  if (eraseRect(k).contains(x, y)) return kErase;
  for (int row = 0; row < 3; ++row)
    for (int n = 0; n < length(row, k.spanish); ++n)
      if (keyRect(k, row, n).contains(x, y)) return ROWS[row][n];
  return 0;
}
inline void draw(toybox::Screen& screen, const fui::Rect& controls, const Layout& k, fui::ActionId action,
                 const Tone* tones = nullptr) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  auto& target = screen.target();
  for (int row = 0; row < 3; ++row) {
    const int count = length(row, k.spanish);
    for (int n = 0; n < count; ++n) {
      const char key = ROWS[row][n];
      const auto tone = tones ? tones[key - 'A'] : Tone::Normal;
      const auto box = keyRect(k, row, n);
      if (tone == Tone::Correct)
        target.fill(box, ink);
      else if (tone == Tone::Present)
        target.fill(box, fui::Paint::dither(fui::Color::LightGray));
      target.stroke(box, ink, 3);
      char label[3] = {key, 0, 0};
      if (key == '[') {
        label[0] = char(0xc3);
        label[1] = char(0x91);
      }
      pixelui::text(target, box, label, pixelui::kBody, tone == Tone::Correct ? fui::Color::White : fui::Color::Black,
                    fui::TextAlign::Center);
    }
    const auto first = keyRect(k, row, 0), last = keyRect(k, row, count - 1);
    screen.frame().navigationGrid(fui::makeRect(first.x, first.y, last.right() - first.x, first.height), count, 1);
  }
  const auto enter = enterRect(k), erase = eraseRect(k);
  screen.frame().navigationGrid(enter, 1, 1);
  screen.frame().navigationGrid(erase, 1, 1);
  target.fill(enter, ink);
  pixelui::text(target, enter, tr(STR_WORDLE_OK), pixelui::kSmall, fui::Color::White, fui::TextAlign::Center);
  target.stroke(erase, ink, 2);
  const int cx = erase.x + erase.width / 2, cy = erase.y + erase.height / 2;
  target.line({int16_t(cx - 8), int16_t(cy)}, {int16_t(cx), int16_t(cy - 8)}, 2, ink);
  target.line({int16_t(cx - 8), int16_t(cy)}, {int16_t(cx), int16_t(cy + 8)}, 2, ink);
  target.stroke(fui::makeRect(cx, cy - 8, 10, 16), ink, 2);
  target.line({int16_t(cx + 2), int16_t(cy - 4)}, {int16_t(cx + 8), int16_t(cy + 4)}, 2, ink);
  target.line({int16_t(cx + 2), int16_t(cy + 4)}, {int16_t(cx + 8), int16_t(cy - 4)}, 2, ink);
  screen.frame().hit(fui::makeRect(controls.x, k.top, controls.width, 3 * k.keyH + 2 * k.gap), action, 0,
                     fui::InputTouch | fui::InputPointerOnly);
}
}  // namespace wordlekeys
