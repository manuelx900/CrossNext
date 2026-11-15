#pragma once

#include "../ui/FitText.h"
#include "PetActionSurface.h"
#include "PetSymbols.h"

namespace petui::resultcards {
using namespace widget;
struct Metric {
  symbols::Kind mark = symbols::Kind::Health;
  const char* label = "";
  unsigned before = 0, after = 0;
  const char* value = nullptr;
};
inline void copy(toybox::Screen& s, fui::Rect box, const char* value, int font, bool bold = false,
                 fui::TextAlign align = fui::TextAlign::Left) {
  fui::TextStyle style;
  style.font = font;
  style.bold = bold;
  style.maxLines = 1;
  style.align = align;
  fittext::draw(s.target(), box, value, style, 2);
}
inline void metric(toybox::Screen& s, Work& w, fui::Rect box, const Metric& data) {
  actionsurface::panel(s, box);
  symbols::draw(s, rect(box.x + 8, box.y + 8, 24, 24), data.mark);
  copy(s, rect(box.x + 40, box.y + 5, box.width - 48, 22), data.label, 3, true);
  const auto row = rect(box.x + 12, box.y + 36, box.width - 24, box.height - 48);
  if (data.value) {
    copy(s, row, data.value, 0, true, fui::TextAlign::Center);
    return;
  }
  char after[24];
  std::snprintf(w.label, sizeof(w.label), "%u", data.before);
  const char* before = w.label;
  std::snprintf(after, sizeof(after), "%u / 100", data.after);
  const auto transition = rect(row.x, row.y, row.width, row.height - 12);
  const int bridge = row.width < 180 ? 26 : 38;
  fui::TextStyle style;
  style.bold = true;
  int font = 3, beforeWidth = 0, afterWidth = 0;
  for (int candidate : {1, 0, 4, 3}) {
    style.font = candidate;
    beforeWidth = s.target().measureText(candidate, before, style).width;
    afterWidth = s.target().measureText(candidate, after, style).width;
    font = candidate;
    if (beforeWidth + afterWidth + bridge <= row.width) break;
  }
  const int x = transition.x + (transition.width - beforeWidth - afterWidth - bridge) / 2;
  copy(s, rect(x, transition.y, beforeWidth, transition.height), before, font, true);
  copy(s, rect(x + beforeWidth + bridge, transition.y, afterWidth, transition.height), after, font, true);
  const int left = x + beforeWidth + (bridge - 18) / 2, right = left + 18, y = transition.y + transition.height / 2;
  s.target().line({int16_t(left), int16_t(y)}, {int16_t(right), int16_t(y)}, 2, ink);
  s.target().line({int16_t(right - 5), int16_t(y - 5)}, {int16_t(right), int16_t(y)}, 2, ink);
  s.target().line({int16_t(right), int16_t(y)}, {int16_t(right - 5), int16_t(y + 5)}, 2, ink);
  const auto track = rect(row.x, row.bottom() - 6, row.width, 5);
  s.target().fill(track, fui::Paint::dither(fui::Color::LightGray), 2);
  s.target().fill(rect(track.x, track.y, track.width * std::min(100u, data.after) / 100, track.height), ink, 2);
}
}  // namespace petui::resultcards
