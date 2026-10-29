#pragma once
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Dashboard.h"
#include "FitText.h"
#include "PixelMenu.h"
namespace readingui {
namespace fui = freeink::ui;
// Native GfxRendererTarget has five slots; prose reuses the display slot.
inline constexpr fui::FontId kReadingFont = 2;
inline void copyText(char* out, size_t cap, const char* value) {
  if (!cap) return;
  value = value ? value : "";
  size_t n = std::min(std::strlen(value), cap - 1);
  while (n && (static_cast<unsigned char>(value[n]) & 0xc0) == 0x80) --n;
  std::memcpy(out, value, n);
  out[n] = 0;
}
inline void text(toybox::Screen& s, fui::Rect r, const char* value, int font = 4, int lines = 1,
                 fui::TextAlign align = fui::TextAlign::Left, bool inverse = false, bool bold = false) {
  fui::TextStyle st;
  st.bold = bold;
  uint16_t tried = 0;
  st.align = align;
  st.color = inverse ? fui::Color::White : fui::Color::Black;
  for (int f : {font, font == kReadingFont ? 4 : 0, 0, 3, 4}) {
    if (tried & (1u << f)) continue;
    tried |= 1u << f;
    const int h = s.target().lineHeight(f);
    if (h <= 0 || h > r.height || h > s.target().lineHeight(font) || (font == 4 && f != 4) ||
        (font == kReadingFont && f != kReadingFont && f != 4))
      continue;
    st.font = f;
    st.maxLines = std::min(lines, std::max(1, int(r.height) / h));
    const char* at = value;
    bool fits = true;
    fui::layoutText(s.target(), r, value, st, [&](const char* run, fui::Rect) {
      while (*at == ' ' || *at == '\n') ++at;
      const size_t n = std::strlen(run);
      if (std::strncmp(at, run, n))
        fits = false;
      else
        at += n;
    });
    while (*at == ' ' || *at == '\n') ++at;
    if (fits && !*at) break;
  }
  s.target().text(r, value, st);
}
inline void caps(toybox::Screen& s, fui::Rect r, const char* value, int font = 0, int lines = 1,
                 fui::TextAlign align = fui::TextAlign::Left, bool inverse = false) {
  char out[256];
  pixelui::copyCaps(out, sizeof(out), value);
  text(s, r, out, font, lines, align, inverse);
}
inline fui::Rect begin(toybox::Screen& s, const char* title) {
  pixelui::begin(s);
  pixelui::Header h;
  h.title = title;
  h.back = s.device().hasTouch;
  pixelui::header(s, h);
  backui::publish(h.back, pixelui::geometry(s.device()).back);
  return pixelui::geometry(s.device()).body.inset(fui::Insets{4, 4, 0, 4});
}
inline void star(toybox::Screen& s, fui::Rect r, bool filled, bool inverse = false) {
  static constexpr int16_t points[10][2] = {{50, 0},  {62, 34}, {98, 36}, {70, 59}, {79, 96},
                                            {50, 75}, {20, 96}, {29, 59}, {1, 36},  {37, 34}};
  const auto ink = inverse ? dashboard::paper : dashboard::ink;
  fui::Point p[10];
  for (int n = 0; n < 10; ++n)
    p[n] = {int16_t(r.x + points[n][0] * r.width / 100), int16_t(r.y + points[n][1] * r.height / 100)};
  const fui::Point c{int16_t(r.x + r.width / 2), int16_t(r.y + r.height / 2)};
  for (int n = 0; n < 10; ++n) {
    if (filled)
      s.target().triangle(c, p[n], p[(n + 1) % 10], ink);
    else
      s.target().line(p[n], p[(n + 1) % 10], 3, ink);
  }
}
inline void book(toybox::Screen& s, fui::Rect r) {
  auto b = r.inset(fui::Insets{3, 5, 3, 5});
  s.target().stroke(b, dashboard::ink, 3);
  s.target().line({int16_t(b.x + 9), b.y}, {int16_t(b.x + 9), b.bottom()}, 3, dashboard::ink);
  s.target().line({b.x, int16_t(b.bottom() - 7)}, {b.right(), int16_t(b.bottom() - 7)}, 3, dashboard::ink);
}
inline void note(toybox::Screen& s, fui::Rect r) {
  dashboard::frame(s, r, pixelui::kBorder);
  for (int n = 0; n < 3; ++n)
    s.target().line({int16_t(r.x + 10), int16_t(r.y + 14 + n * 12)},
                    {int16_t(r.right() - 10 - (n == 2 ? r.width / 4 : 0)), int16_t(r.y + 14 + n * 12)}, 3,
                    dashboard::ink);
}
inline void sort(toybox::Screen& s, fui::Rect r) {
  const int x = r.x + 8, y = r.y + 7;
  s.target().line({int16_t(x), int16_t(y)}, {int16_t(x), int16_t(y + 25)}, 3, dashboard::ink);
  s.target().triangle({int16_t(x - 6), int16_t(y + 22)}, {int16_t(x + 6), int16_t(y + 22)},
                      {int16_t(x), int16_t(y + 30)}, dashboard::ink);
  for (int n = 0; n < 3; ++n)
    s.target().line({int16_t(x + 12), int16_t(y + n * 9)}, {int16_t(r.right() - 5 - n * 6), int16_t(y + n * 9)}, 3,
                    dashboard::ink);
}
}  // namespace readingui
