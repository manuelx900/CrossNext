#pragma once

#include <I18n.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../../lib/GfxRenderer/ChromeRegion.h"
#include "../ui/BackButton.h"
#include "../ui/PixelMenu.h"
#include "../ui/SettingsArt.h"

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
            fui::ActionId viewAction = 0, bool grid = false, fui::ActionId menuAction = 0) {
  const auto device = screen.device();
  const auto safe = device.safeRect();
  work.heading = {};
  work.heading.title = title;
  work.heading.back = device.hasTouch;
  const int buttonSize = chrome::lightHeader ? 44 : 48;
  constexpr int edge = 12;
  const int buttonGap = chrome::lightHeader ? 0 : menuAction ? 8 : 24;
  const int actionCount = (actions ? (device.hasTouch ? 2 : 1) : 0) + (viewAction ? 1 : 0) + (menuAction ? 1 : 0);
  work.heading.titleReserve = actionCount ? edge + actionCount * (buttonSize + buttonGap) - buttonGap : 0;
  pixelui::header(screen.target(), device, work.heading);
  backui::publish(work.heading.back, pixelui::geometry(device).back);
  const auto back = pixelui::geometry(device).back;
  const int actionY = back.y + (back.height - buttonSize) / 2;
  const int menuShift = menuAction ? buttonSize + buttonGap : 0;
  if (menuAction) {
    const auto box = fui::makeRect(safe.right() - edge - buttonSize, actionY, buttonSize, buttonSize);
    for (int n = 0; n < 3; ++n)
      screen.target().fill(fui::makeRect(box.x + 21, box.y + 10 + n * 11, 5, 5), fui::Paint::solid(fui::Color::White));
    screen.frame().hit(box, menuAction, -3, fui::InputTouch);
  }
  if (viewAction) {
    const auto box = fui::makeRect(safe.right() - edge - buttonSize - menuShift, actionY, buttonSize, buttonSize);
    const auto white = fui::Paint::solid(fui::Color::White);
    if (grid) {
      for (int n = 0; n < 3; ++n) screen.target().fill(fui::makeRect(box.x + 12, box.y + 13 + n * 9, 24, 4), white);
    } else {
      for (int n = 0; n < 4; ++n)
        screen.target().stroke(fui::makeRect(box.x + 11 + n % 2 * 15, box.y + 11 + n / 2 * 15, 11, 11), white, 2);
    }
    screen.frame().hit(box, viewAction, 0, fui::InputTouch);
  }
  const int shift = (viewAction ? buttonSize + buttonGap : 0) + menuShift;
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
  props.labelText.bold = false;
  props.headerText.font = 4;
  props.headerText.bold = false;
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

inline void lightRows(fui::ListProps& props) {
  props.rowHeight = 76;
  props.rowPaddingY = 10;
  props.iconSize = 28;
  props.labelText.maxLines = 2;
  props.labelText.bold = false;
  props.valueText.font = 3;
  props.sidePadding = 12;
  props.rowStyles.normal.borderWidth = 0;
  props.rowStyles.normal.background = fui::Paint::solid(fui::Color::White);
  props.rowStyles.normal.foreground = fui::Paint::solid(fui::Color::Black);
  props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
  props.separatorPaint = fui::Paint::dither(fui::Color::DarkGray);
  props.selectionMarker = fui::SelectionMarker::Underline;
  props.markerThickness = 1;
}
inline void fitPortraitRows(fui::ListProps& props, const fui::DeviceContext& device, fui::Rect viewport) {
  if (!chrome::lightHeader || device.width >= device.height) return;
  props.rowHeight = std::max(1, int(viewport.height) / 10);
  props.rowPaddingY = 0;
}
inline void fileSize(char* out, size_t size, uint64_t bytes) {
  if (bytes == UINT64_MAX) {
    std::snprintf(out, size, "—");
    return;
  }
  if (bytes < 1024)
    std::snprintf(out, size, tr(STR_BROWSER_SIZE_B), static_cast<unsigned long long>(bytes));
  else if (bytes < 1024 * 1024)
    std::snprintf(out, size, tr(STR_BROWSER_SIZE_KB), static_cast<unsigned long long>(bytes / 1024));
  else {
    const uint64_t unit = bytes < 1024ULL * 1024 * 1024 ? 1024ULL * 1024 : 1024ULL * 1024 * 1024;
    std::snprintf(out, size, unit == 1024ULL * 1024 ? tr(STR_BROWSER_SIZE_MB) : tr(STR_BROWSER_SIZE_GB),
                  static_cast<unsigned long long>(bytes / unit), unsigned(bytes % unit * 10 / unit));
  }
}
inline void footerPath(fui::DrawTarget& target, const fui::TextStyle& style, const char* path, int width, char* out,
                       size_t bytes) {
  const int prefix = target.measureText(style.font, "...", style).width;
  const char* suffix = path;
  if (target.measureText(style.font, path, style).width <= width && std::strlen(path) < bytes) {
    std::snprintf(out, bytes, "%s", path);
    return;
  }
  while (const char* slash = std::strchr(suffix + (*suffix != '\0'), '/')) {
    if (!slash[1]) break;
    suffix = slash;
    if (std::strlen(suffix) + 3 < bytes && target.measureText(style.font, suffix, style).width + prefix <= width) break;
    ++suffix;
  }
  while (*suffix &&
         (std::strlen(suffix) + 3 >= bytes || target.measureText(style.font, suffix, style).width + prefix > width)) {
    ++suffix;
    while ((*suffix & 0xc0) == 0x80) ++suffix;
  }
  std::snprintf(out, bytes, "...%s", suffix);
}

template <typename Screen>
void statusFooter(Screen& screen, const char* path, const char* count) {
  const auto safe = screen.device().safeRect();
  const auto band = fui::makeRect(0, safe.bottom() - kFooterHeight, screen.device().width, kFooterHeight);
  screen.target().fill(fui::makeRect(0, band.y, band.width, 2),
                       chrome::lightHeader ? lightui::rule : fui::Paint::solid(fui::Color::Black));
  fui::TextStyle style;
  style.font = 0;
  style.maxLines = 1;
  const int countWidth =
      std::min(int(safe.width) / 2, int(screen.target().measureText(style.font, count, style).width));
  const auto total = fui::makeRect(safe.right() - 14 - countWidth, band.y + 4, countWidth, band.height - 4);
  const auto location =
      fui::makeRect(safe.x + 14, band.y + 4, std::max(0, int(total.x) - safe.x - 30), band.height - 4);
  char label[160];
  footerPath(screen.target(), style, path, location.width, label, sizeof(label));
  screen.target().text(toybox::inkCentred(screen.target(), location, style), label, style);
  style.align = fui::TextAlign::Right;
  screen.target().text(toybox::inkCentred(screen.target(), total, style), count, style);
}

template <typename Screen>
void countFooter(Screen& screen, const char* count) {
  const auto safe = screen.device().safeRect();
  const int top = safe.bottom() - kFooterHeight;
  screen.target().fill(fui::makeRect(0, top, screen.device().width, 1),
                       chrome::lightHeader ? lightui::rule : fui::Paint::solid(fui::Color::Black));
  pixelui::text(screen.target(), fui::makeRect(safe.x + 14, top + 4, safe.width - 28, kFooterHeight - 4), count, 0,
                fui::Color::Black, fui::TextAlign::Right);
}

template <typename Screen>
void listFooter(Screen& screen, const char* count, fui::ActionId action, int previousValue, int nextValue,
                bool previous, bool next) {
  const auto safe = screen.device().safeRect();
  const auto band = fui::makeRect(0, safe.bottom() - kFooterHeight, screen.device().width, kFooterHeight);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  screen.target().fill(fui::makeRect(0, band.y, band.width, 2), fui::Paint::dither(fui::Color::DarkGray));
  fui::TextStyle style;
  style.font = 0;
  style.maxLines = 1;
  screen.target().text(fui::makeRect(safe.x + 14, band.y + 4, safe.width - 140, band.height - 4), count, style);
  for (int n = 0; n < 2; ++n) {
    const auto r = fui::makeRect(safe.right() - 112 + n * 56, band.y + 2, 48, band.height - 2);
    const int cx = r.x + r.width / 2, cy = r.y + r.height / 2, direction = n ? 1 : -1;
    const auto paint = (n ? next : previous) ? ink : fui::Paint::dither(fui::Color::LightGray);
    screen.target().line({int16_t(cx - 7), int16_t(cy - direction * 4)}, {int16_t(cx), int16_t(cy + direction * 4)}, 2,
                         paint);
    screen.target().line({int16_t(cx), int16_t(cy + direction * 4)}, {int16_t(cx + 7), int16_t(cy - direction * 4)}, 2,
                         paint);
    if (n ? next : previous) screen.frame().hit(r, action, n ? nextValue : previousValue, fui::InputTouch);
  }
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
