#pragma once
#include "../../components/icons/listIcons.h"
#include "../ui/Dashboard.h"
#include "../ui/NavigationIcons.h"
#include "../ui/PageIndicator.h"
namespace collectionui {
namespace fui = freeink::ui;
inline constexpr int ListHeight = 84, ListPitch = 94;
struct Grid {
  fui::Rect area{}, pager{};
  int columns = 3, rows = 2;
  int capacity() const { return columns * rows; }
  fui::Rect card(int slot) const {
    constexpr int gap = 16;
    const int w = (area.width - (columns - 1) * gap) / columns;
    const int h = (area.height - (rows - 1) * gap) / rows;
    return fui::makeRect(area.x + slot % columns * (w + gap), area.y + slot / columns * (h + gap), w, h);
  }
};
inline Grid layout(const fui::DeviceContext& device, fui::Rect body) {
  Grid grid;
  grid.columns = 3;
  grid.rows = device.width > device.height ? 1 : 2;

  grid.pager = fui::makeRect(body.x, body.bottom() - 48, body.width, 44);
  body.height -= 60;
  grid.area = body;
  return grid;
}
inline void star(fui::DrawTarget& target, fui::Rect box) {
  static constexpr int8_t x[] = {0, 3, 11, 5, 7, 0, -7, -5, -11, -3};
  static constexpr int8_t y[] = {-11, -4, -3, 2, 10, 6, 10, 2, -3, -4};
  const int cx = box.x + box.width / 2, cy = box.y + box.height / 2;
  for (int n = 0; n < 10; ++n) {
    fui::Point a{int16_t(cx + x[n]), int16_t(cy + y[n])};
    fui::Point b{int16_t(cx + x[(n + 1) % 10]), int16_t(cy + y[(n + 1) % 10])};
    target.triangle({int16_t(cx), int16_t(cy)}, a, b, dashboard::ink);
  }
}
inline void countLabel(toybox::Screen& screen, fui::Rect area, const char* count) {
  if (count && *count) pixelui::text(screen.target(), area, count, 3);
}
inline void options(toybox::Screen& screen, fui::Rect box, fui::ActionId action, int row) {
  for (int n = -1; n <= 1; ++n)
    screen.target().fill(fui::makeRect(box.x + box.width / 2 - 3, box.y + box.height / 2 + n * 10 - 3, 6, 6),
                         dashboard::ink, 3);
  screen.frame().hit(box, action, row, fui::InputTouch);
}
inline void card(toybox::Screen& screen, fui::Rect r, const char* name, const char* count, bool favorite,
                 fui::ActionId open, int row, fui::ActionId menu) {
  dashboard::frame(screen, r);
  dashboard::icon(screen, fui::makeRect(r.x + 12, r.y + 12, 32, 32), navicons::Collections32);
  if (favorite) star(screen.target(), fui::makeRect(r.right() - 48, r.y, 48, 48));
  fui::TextStyle style;
  style.font = 4;
  style.maxLines = std::clamp((int(r.height) - 102) / int(screen.target().lineHeight(4)), 1, 3);
  const auto title = fui::makeRect(r.x + 12, r.y + 52, r.width - 24, std::max(28, int(r.height) - 102));
  screen.target().text(title, name, style);
  if (count && *count) {
    dashboard::icon(screen, fui::makeRect(r.x + 8, r.bottom() - 34, 20, 20), navicons::BookOpen24);
    fui::TextStyle number;
    number.font = 3;
    const auto value = fui::makeRect(r.x + 32, r.bottom() - 44, r.width - 32 - (menu ? 48 : 8), 40);
    screen.target().text(toybox::inkCentred(screen.target(), value, number), count, number);
  }
  if (menu) {
    screen.frame().hit(fui::makeRect(r.x, r.y, r.width, r.height - 48), open, row,
                       fui::InputTouch | fui::InputLongPress);
    screen.frame().hit(fui::makeRect(r.x, r.bottom() - 48, r.width - 48, 48), open, row,
                       fui::InputTouch | fui::InputLongPress);
    options(screen, fui::makeRect(r.right() - 48, r.bottom() - 48, 48, 48), menu, row);
  } else
    screen.frame().hit(r, open, row, fui::InputTouch | fui::InputLongPress);
}
inline void row(toybox::Screen& screen, fui::Rect r, const char* name, const char* count, bool favorite,
                fui::ActionId open, int index, fui::ActionId menu) {
  dashboard::frame(screen, r);
  dashboard::icon(screen, fui::makeRect(r.x + 12, r.y + (r.height - 32) / 2, 32, 32), navicons::Collections32);
  fui::TextStyle text;
  text.font = 4;
  text.maxLines = 1;
  screen.target().text(fui::makeRect(r.x + 56, r.y + 8, r.width - 112, 34), name, text);
  countLabel(screen, fui::makeRect(r.x + 56, r.bottom() - 34, r.width - 112, 26), count);
  if (favorite) star(screen.target(), fui::makeRect(r.right() - 44, r.y, 40, 40));
  screen.frame().hit(fui::makeRect(r.x, r.y, r.width - 48, r.height), open, index,
                     fui::InputTouch | fui::InputLongPress);
  if (menu) options(screen, fui::makeRect(r.right() - 48, r.bottom() - 44, 48, 44), menu, index);
}
inline constexpr int EditorPadding = 4;
inline int editorRowHeight(const fui::DrawTarget& target) {
  return std::max(48, 2 * int(target.lineHeight(4)) + 2 * EditorPadding);
}
struct EditorViewport {
  fui::Rect rows;
  int first, count, height;
};
inline EditorViewport editorViewport(toybox::Screen& screen, fui::ListNav& nav, int count) {
  const auto body = screen.body();
  const int height = editorRowHeight(screen.target());
  const int capacity = std::clamp(int(body.height) / height, 1, 10);
  fui::ListProps props;
  nav.syncToProps(body, height, 0, count, props);
  const int first = props.topIndex, visible = std::min(capacity, std::max(0, count - first));
  nav.drawnCount = count;
  nav.onListRendered(first, visible, props.selectedIndex >= first && props.selectedIndex < first + visible);
  return {fui::makeRect(body.x, body.y, body.width - (count > capacity ? 8 : 0), visible * height), first, visible,
          height};
}
inline void editorRow(toybox::Screen& screen, fui::Rect r, const char* name, bool directory, bool checked, bool partial,
                      bool last, fui::ActionId open, fui::ActionId toggle, int index, const char* extension = "",
                      bool boldDirectories = false) {
  if (chrome::lightHeader) {
    if (!last) screen.target().fill(fui::makeRect(r.x, r.bottom() - 1, r.width, 1), lightui::rule);
  } else {
    if (!last)
      screen.target().fill(fui::makeRect(r.x, r.bottom() - pixelui::kBorder, r.width, pixelui::kBorder),
                           dashboard::ink);
  }
  const auto check = fui::makeRect(r.x + 10, r.y + (r.height - 28) / 2, 28, 28);
  const int radius = chrome::lightHeader ? 4 : 0;
  if (checked || partial) {
    screen.target().fill(check, dashboard::ink, radius);
    if (partial)
      screen.target().fill(fui::makeRect(check.x + 6, check.y + 13, 16, 3), dashboard::paper);
    else {
      screen.target().line({int16_t(check.x + 5), int16_t(check.y + 14)},
                           {int16_t(check.x + 11), int16_t(check.y + 20)}, 3, dashboard::paper);
      screen.target().line({int16_t(check.x + 11), int16_t(check.y + 20)},
                           {int16_t(check.x + 23), int16_t(check.y + 7)}, 3, dashboard::paper);
    }
  } else
    screen.target().stroke(check, dashboard::ink, pixelui::kBorder, radius);
  screen.frame().hit(fui::makeRect(r.x, r.y, 48, r.height), toggle, index, fui::InputTouch);
  int left = 52;
  if (directory) {
    dashboard::icon(screen, fui::makeRect(r.x + left, r.y + (r.height - 28) / 2, 28, 28), navicons::Folder32);
    left += 40;
  }
  fui::TextStyle text;
  text.font = 4;
  text.maxLines = 2;
  text.bold = directory && boldDirectories && !chrome::lightHeader;
  fui::TextStyle suffix;
  suffix.font = 3;
  const int suffixWidth =
      directory || !extension || !*extension ? 0 : screen.target().measureText(suffix.font, extension, suffix).width;
  const int nameWidth = std::max(1, r.width - left - 12 - (suffixWidth ? suffixWidth + 16 : 0));
  const auto measured = fui::measureWrappedText(screen.target(), name, text, nameWidth);
  const int textHeight = std::min(int(measured.height), r.height - 2 * EditorPadding);
  screen.target().text(fui::makeRect(r.x + left, r.y + (r.height - textHeight) / 2, nameWidth, textHeight), name, text);
  if (suffixWidth)
    screen.target().text(toybox::inkCentred(screen.target(),
                                            fui::makeRect(r.right() - 12 - suffixWidth, r.y + EditorPadding,
                                                          suffixWidth, r.height - 2 * EditorPadding),
                                            suffix),
                         extension, suffix);
  screen.frame().hit(fui::makeRect(r.x + 48, r.y, r.width - 48, r.height), open, index, fui::InputTouch);
}
}  // namespace collectionui
