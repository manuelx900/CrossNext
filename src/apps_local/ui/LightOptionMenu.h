#pragma once
#include <cstdio>

#include "LightUi.h"
namespace lightui {
struct ChoiceMenu {
  const char *title = nullptr, *headline = nullptr, *message = nullptr;
  const freeink::ui::DialogOption* options = nullptr;
  int count = 0, current = 0, focus = 0;
  freeink::ui::ActionId guard = 2, pageAction = 3;
};
struct ChoiceLayout {
  freeink::ui::Rect panel{}, rows{}, footer{};
  int first = 0, capacity = 1, pages = 1;
  int lines[3] = {2, 3, 3};
};
inline int textHeight(const fui::DrawTarget& t, const char* text, int width, int lines, bool bold = false) {
  if (!text || !*text) return 0;
  fui::TextStyle s;
  s.font = 0;
  s.bold = bold;
  s.maxLines = lines;
  int count = 0;
  fui::layoutText(t, fui::makeRect(0, 0, width, 1), text, s, [&](const char*, fui::Rect) { ++count; });
  return std::min(lines, std::max(1, count)) * t.lineHeight(0) + 12;
}
inline ChoiceLayout choiceLayout(const fui::DrawTarget& t, fui::Rect screen, const ChoiceMenu& m) {
  const int header = std::clamp(chrome::headerHeight - int(screen.y), 0, int(screen.height));
  screen.y += header;
  screen.height -= header;
  ChoiceLayout g;
  const int width = std::min(600, int(screen.width) * 4 / 5), padding = 18;
  const int row = std::max(48, int(t.lineHeight(0)) + 18);
  const char* text[] = {m.title, m.headline, m.message};
  auto headerHeight = [&]() {
    int total = 0;
    for (int n = 0; n < 3; ++n) total += textHeight(t, text[n], width - padding * 2, g.lines[n], n == 0);
    return total;
  };
  const int headerBudget = int(screen.height) - 48 - 2 * padding - row - (m.count > 1 ? 44 : 0);
  for (int n = 2; n >= 0; --n)
    while (g.lines[n] > 1 && headerHeight() > headerBudget) --g.lines[n];
  const int top = headerHeight();
  const int available = std::max(row, int(screen.height) - 48 - 2 * padding - top);
  g.capacity = std::max(1, available / row);
  if (m.count > g.capacity) g.capacity = std::max(1, (available - 44) / row);
  g.pages = (m.count + g.capacity - 1) / g.capacity;
  const int height = padding * 2 + top + std::min(m.count, g.capacity) * row + (g.pages > 1 ? 44 : 0);
  g.panel = fui::centeredRect(screen, {int16_t(width), int16_t(height)});
  g.first = std::clamp(m.focus, 0, std::max(0, m.count - 1)) / g.capacity * g.capacity;
  g.rows = fui::makeRect(g.panel.x + padding, g.panel.y + padding + top, width - 2 * padding,
                         std::min(m.count, g.capacity) * row);
  if (g.pages > 1) g.footer = fui::makeRect(g.rows.x, g.rows.bottom(), g.rows.width, 44);
  return g;
}
template <typename Frame>
void choiceMenu(Frame& frame, fui::Rect screen, const ChoiceMenu& m) {
  if (!m.options || m.count <= 0) return;
  auto& t = frame.target();
  const auto g = choiceLayout(t, screen, m);
  shade(t, screen);
  t.fill(g.panel, paper, 8);
  t.stroke(g.panel, ink, 1, 8);
  frame.hit(g.panel, m.guard, 0, fui::InputTouch);
  fui::TextStyle style;
  style.font = 0;
  style.maxLines = 2;
  int y = g.panel.y + 18;
  const char* text[] = {m.title, m.headline, m.message};
  for (int n = 0; n < 3; ++n) {
    const char* value = text[n];
    const int h = textHeight(t, value, g.rows.width, g.lines[n], n == 0);
    if (!h) continue;
    style.maxLines = g.lines[n];
    style.bold = n == 0;
    t.text(fui::makeRect(g.rows.x, y, g.rows.width, h - 8), value, style);
    y += h;
  }
  style.bold = false;
  const int height = g.rows.height / std::min(m.count, g.capacity);
  style.maxLines = 1;
  for (int n = g.first; n < std::min(m.count, g.first + g.capacity); ++n) {
    const auto r = fui::makeRect(g.rows.x, g.rows.y + (n - g.first) * height, g.rows.width, height);
    const bool selected = n == m.focus;
    if (selected) t.fill(r, ink, 4);
    style.color = selected ? fui::Color::White : fui::Color::Black;
    t.text(fui::makeRect(r.x + 10, r.y, r.width - 20, r.height), m.options[n].label, style);
    if (!selected && n + 1 != m.focus) separator(t, r);

    frame.hit(r, m.options[n].action, m.options[n].value, fui::InputTouch);
  }
  if (g.pages <= 1) return;
  char count[24];
  std::snprintf(count, sizeof(count), "%d / %d", g.first / g.capacity + 1, g.pages);
  style.color = fui::Color::Black;
  style.align = fui::TextAlign::Center;
  t.text(g.footer, count, style);
  for (int direction : {-1, 1}) {
    const int next = g.first + direction * g.capacity;
    if (next < 0 || next >= m.count) continue;
    const auto r = fui::makeRect(direction < 0 ? g.footer.x : g.footer.right() - 44, g.footer.y, 44, 44);
    const int cx = r.x + 22, cy = r.y + 22;
    t.line({int16_t(cx - direction * 4), int16_t(cy - 7)}, {int16_t(cx + direction * 4), int16_t(cy)}, 2, ink);
    t.line({int16_t(cx + direction * 4), int16_t(cy)}, {int16_t(cx - direction * 4), int16_t(cy + 7)}, 2, ink);
    frame.hit(r, m.pageAction, next, fui::InputTouch);
  }
}
}  // namespace lightui
