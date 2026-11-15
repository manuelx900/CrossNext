#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstring>
namespace fittext {
inline bool complete(const freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* copy,
                     freeink::ui::TextStyle style, int* usedLines = nullptr) {
  const char* next = copy;
  bool matches = true;
  int count = 0;
  freeink::ui::layoutText(target, box, copy, style, [&](const char* run, freeink::ui::Rect) {
    ++count;
    while (*next == ' ' || *next == '\n') ++next;
    const size_t length = std::strlen(run);
    if (std::strncmp(next, run, length) == 0)
      next += length;
    else
      matches = false;
  });
  while (*next == ' ' || *next == '\n') ++next;
  const int areaLines =
      freeink::ui::textAreaWalk(target, box.width, copy, style, [](uint32_t, const freeink::ui::TextAreaLine&) {});
  count = std::max(count, areaLines);
  if (usedLines) *usedLines = count;
  return matches && !*next && count <= std::max(1, int(style.maxLines));
}

inline bool fit(const freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* copy,
                freeink::ui::TextStyle& style, int lines = 2) {
  if (!copy || box.empty()) return false;
  const int initial = style.font;
  const int ceiling = target.lineHeight(initial);
  const int candidates[] = {initial, 0, 1, 2, 4, 3};
  for (int candidate : candidates) {
    const int height = target.lineHeight(candidate);
    if (height <= 0 || height > ceiling) continue;
    style.font = candidate;
    if (!std::strchr(copy, '\n') && target.measureText(candidate, copy, style).width <= box.width &&
        target.inkBounds(candidate, copy, style).height <= box.height) {
      style.maxLines = 1;
      return true;
    }
    style.maxLines = std::min(16, std::min(lines, std::max(1, int(box.height) / height)));
    if (style.maxLines == 1 && target.inkBounds(candidate, copy, style).height > box.height) continue;
    int usedLines = 0;
    if (complete(target, box, copy, style, &usedLines)) {
      style.maxLines = std::max(1, usedLines);
      return true;
    }
  }
  return false;
}

inline void drawRuns(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* copy,
                     freeink::ui::TextStyle style) {
  auto lineStyle = style;
  lineStyle.maxLines = 1;
  lineStyle.align = freeink::ui::TextAlign::Left;
  freeink::ui::layoutText(target, box, copy, style,
                          [&](const char* run, freeink::ui::Rect line) { target.text(line, run, lineStyle); });
}

inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* copy, freeink::ui::TextStyle style,
                 int lines = 2) {
  if (box.empty() || !copy) return;
  const bool fitted = fit(target, box, copy, style, lines);
  if (style.maxLines == 1) {
    const auto bounds = target.inkBounds(style.font, copy, style);
    box.y += (box.height - bounds.height) / 2 - bounds.y;
    box.height = target.lineHeight(style.font);
    target.text(box, copy, style);
    return;
  }
  if (fitted && std::strchr(copy, '\n'))
    drawRuns(target, box, copy, style);
  else
    target.text(box, copy, style);
}

inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* copy, int font = 3, int lines = 10,
                 freeink::ui::TextAlign align = freeink::ui::TextAlign::Left,
                 freeink::ui::Color color = freeink::ui::Color::Black) {
  if (box.empty() || !copy) return;
  freeink::ui::TextStyle style;
  style.font = font;
  style.maxLines = 1;
  style.align = align;
  style.color = color;
  if (fit(target, box, copy, style, lines))
    drawRuns(target, box, copy, style);
  else
    target.text(box, copy, style);
}
}  // namespace fittext
