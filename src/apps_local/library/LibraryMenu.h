#pragma once

#include "../ui/BackButton.h"
#include "../ui/PixelMenu.h"

namespace libraryui {
namespace fui = freeink::ui;
constexpr int kFooterHeight = 44;
constexpr int gridCapacity(int width, int height) { return width > height ? 4 : 9; }

struct GridLayout {
  fui::Rect area{}, pager{};
  int columns = 3, rows = 3;
  int capacity() const { return columns * rows; }
  fui::Rect card(int index) const {
    constexpr int gap = 12;
    const int width = (area.width - (columns - 1) * gap) / columns;
    const int height = (area.height - (rows - 1) * gap) / rows;
    const int bookWidth = std::min(width, height * 2 / 3);
    return fui::makeRect(area.x + index % columns * (width + gap) + (width - bookWidth) / 2,
                         area.y + index / columns * (height + gap), bookWidth, height);
  }
};
inline GridLayout gridLayout(const fui::DeviceContext& device, fui::Rect body) {
  GridLayout out;
  if (device.width > device.height) {
    out.columns = 4;
    out.rows = 1;
  }
  out.pager = fui::makeRect(body.x, body.bottom() - 52, body.width, 48);
  body.height = std::max(1, int(body.height) - 64);
  out.area = body;
  return out;
}

struct HeaderWork {
  pixelui::Header heading{};
  fui::ButtonProps button{};
};

template <typename Screen>
void header(Screen& screen, HeaderWork& work, const char* title, bool actions, fui::BitmapRef search,
            fui::BitmapRef refresh, fui::ActionId searchAction, fui::ActionId refreshAction,
            fui::ActionId viewAction = 0, bool grid = false) {
  const auto device = screen.device();
  const auto safe = device.safeRect();
  work.heading = {};
  work.heading.title = title;
  work.heading.back = device.hasTouch;
  constexpr int buttonSize = 48, buttonGap = 24, edge = 12;
  const int actionCount = (actions ? (device.hasTouch ? 2 : 1) : 0) + (viewAction ? 1 : 0);
  work.heading.titleReserve = actionCount ? edge + actionCount * (buttonSize + buttonGap) - buttonGap : 0;
  pixelui::header(screen.target(), device, work.heading);
  backui::publish(work.heading.back, pixelui::geometry(device).back);
  const auto back = pixelui::geometry(device).back;
  const int actionY = back.y + (back.height - buttonSize) / 2;
  if (viewAction) {
    const auto box = fui::makeRect(safe.right() - edge - buttonSize, actionY, buttonSize, buttonSize);
    const auto white = fui::Paint::solid(fui::Color::White);
    if (grid) {
      for (int n = 0; n < 3; ++n) screen.target().fill(fui::makeRect(box.x + 12, box.y + 13 + n * 9, 24, 4), white);
    } else {
      for (int n = 0; n < 4; ++n)
        screen.target().stroke(fui::makeRect(box.x + 11 + n % 2 * 15, box.y + 11 + n / 2 * 15, 11, 11), white, 2);
    }
    screen.frame().hit(box, viewAction, 0, fui::InputTouch);
  }
  const int shift = viewAction ? buttonSize + buttonGap : 0;
  if (!actions) return;
  auto& button = work.button;
  button = {};
  button.styles = fui::plainStyles(fui::Paint::solid(fui::Color::White));
  button.borderEdges = fui::EdgesNone;
  button.minTouchSize = buttonSize;
  button.icon = search;
  button.action = searchAction;
  screen.button(button, fui::makeRect(safe.right() - edge - buttonSize - shift, actionY, buttonSize, buttonSize));
  if (device.hasTouch) {
    button.icon = refresh;
    button.action = refreshAction;
    screen.button(button, fui::makeRect(safe.right() - edge - 2 * buttonSize - buttonGap - shift, actionY, buttonSize,
                                        buttonSize));
  }
}

inline void styleRows(fui::ListProps& props) {
  pixelui::styleList(props, true);
  props.rowHeight = 56;
  props.iconSize = 32;
  props.labelText.font = pixelui::kSmall;
  props.rowPaddingY = 6;
  props.rowStyles.normal.borderWidth = 2;
  props.subtitleText.maxLines = 1;
}

inline void styleBookRows(fui::ListProps& props, const fui::DrawTarget& target) {
  styleRows(props);
  props.labelText.font = 4;
  props.labelText.maxLines = 2;
  props.valueText.font = 3;
  props.iconSize = 32;
  props.rowPaddingY = 4;
  props.rowHeight = std::max(int(props.iconSize), 2 * int(target.lineHeight(props.labelText.font))) + 8;
  props.sidePadding = 12;
  props.textGap = 12;
  props.rowStyles.normal.borderWidth = 1;
  props.rowStyles.selected.borderWidth = 1;
  props.rowStyles.focused.borderWidth = 1;
  props.rowStyles.active.borderWidth = 1;
  props.rowStyles.disabled.borderWidth = 1;
}

inline void format(fui::DrawTarget& target, char* text, int width, fui::TextStyle& style) {
  if (!text) return;
  for (char* p = text; *p; ++p)
    if (*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 'a' + 'A');
  pixelui::fitLabel(target, text, width, style);
}

inline void footer(fui::DrawTarget& target, const fui::DeviceContext& device, const char* position, int hints) {
  const auto safe = device.safeRect();
  const int top = safe.bottom() - hints - kFooterHeight;
  target.fill(fui::makeRect(0, top, device.width, 3), fui::Paint::solid(fui::Color::Black));
  pixelui::text(target, fui::makeRect(safe.x + 14, top + 3, safe.width - 28, kFooterHeight - 3), position,
                pixelui::kSmall, fui::Color::Black, fui::TextAlign::Right);
}
}  // namespace libraryui
