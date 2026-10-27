#pragma once

#include "../ui/Dashboard.h"
#include "../ui/FitText.h"

namespace readerend {
namespace fui = freeink::ui;
constexpr fui::ActionId kHome = 1;
constexpr fui::ActionId kLastPage = 2;

struct Layout {
  fui::Rect panel{}, icon{}, title{}, home{}, back{};
};

inline Layout layout(const fui::DeviceContext& device) {
  const auto safe = device.safeRect().inset({16, 16, 16, 16});
  const int width = std::min(520, int(safe.width));
  const int height = std::min(284, int(safe.height));
  const auto panel =
      fui::makeRect(safe.x + (safe.width - width) / 2, safe.y + (safe.height - height) / 2, width, height);
  Layout out;
  out.panel = panel;
  out.back = fui::makeRect(safe.x, safe.y, 48, 48);
  out.icon = fui::makeRect(panel.x + (panel.width - 80) / 2, panel.y, 80, 80);
  out.title = fui::makeRect(panel.x, out.icon.bottom() + 16, panel.width, 80);
  out.home = fui::makeRect(panel.x, panel.bottom() - 64, panel.width, 64);
  return out;
}

inline void build(toybox::Screen& screen) {
  const auto g = layout(screen.device());
  dashboard::chevron(screen.target(), g.back, false, true);
  screen.frame().hit(g.back, kLastPage, 0, fui::InputTouch);
  if (!g.icon.empty()) dashboard::icon(screen, g.icon, extra_reading_book_64);
  char title[96];
  pixelui::copyCaps(title, sizeof(title), tr(STR_END_OF_BOOK));
  fittext::draw(screen.target(), g.title, title, 2, 2, fui::TextAlign::Center);
  dashboard::button(screen, g.home, tr(STR_EOB_HOME), kHome, 0, true, nullptr, 1);
}
}  // namespace readerend
