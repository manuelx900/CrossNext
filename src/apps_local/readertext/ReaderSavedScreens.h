#pragma once
#include <I18n.h>

#include <cstdio>

#include "../ui/PageIndicator.h"
namespace readersaved {
namespace fui = freeink::ui;
inline constexpr int Previous = -1, Next = -2, Done = -3;
inline constexpr int RowHeight = 120, RowGap = 8;
struct Page {
  int first, end, capacity, number, total;
};
inline Page project(int selected, int count, int height) {
  const int capacity = std::clamp((height + RowGap) / (RowHeight + RowGap), 1, 8);
  const int page = std::clamp(selected, 0, std::max(0, count - 1)) / capacity;
  return {page * capacity, std::min(count, (page + 1) * capacity), capacity, page + 1,
          std::max(1, (count + capacity - 1) / capacity)};
}
inline int destination(int selected, int count, int capacity, int delta) {
  capacity = std::max(1, capacity);
  const int pages = std::max(1, (count + capacity - 1) / capacity);
  return std::clamp(selected / capacity + delta, 0, pages - 1) * capacity;
}
inline void slice(fui::ListProps& props, fui::ListNav& nav, const Page& page) {
  // Absolute action values survive the slice. The activity owns page changes;
  // SDK feedback still publishes real visible rows for touch/long-press routing.
  nav.followOnBuild = false;
  nav.followPending = false;
  nav.top = page.first;
  nav.visibleRows = page.capacity;
  props.topIndex = page.first;
  props.count = page.end;
  props.selectedIndex = nav.selected;
  props.nav = &nav;
}

template <typename Screen>
void begin(Screen& screen, const char* title, const char* author, int count, bool quotes) {
  pixelui::begin(screen, 0);
  const bool wide = screen.device().width > screen.device().height;
  const auto hero = screen.takeTop(wide ? 74 : 104, 12);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  screen.target().stroke(hero, ink, 2);
  fui::TextStyle text;
  text.font = 0;
  text.maxLines = wide ? 1 : 2;
  screen.target().text(fui::makeRect(hero.x + 14, hero.y + 8, hero.width - 28, wide ? 32 : 60), title, text);
  text.font = 4;
  text.maxLines = 1;
  screen.target().text(fui::makeRect(hero.x + 14, hero.bottom() - 34, hero.width - 28, 30), author, text);
  char value[64];
  std::snprintf(value, sizeof(value), quotes ? tr(STR_SR_QUOTE_COUNT) : tr(STR_SR_BOOKMARK_COUNT), count);
  pixelui::text(screen.target(), screen.takeTop(34, 12), value, 0);
}
struct Footer {
  fui::Rect done, pager, hint;
};
template <typename Screen>
Footer reserveFooter(Screen& screen) {
  Footer out;
  out.done = screen.takeBottom(50, 10);
  out.pager = screen.takeBottom(44, 14);
  out.hint = screen.device().width > screen.device().height ? fui::Rect{} : screen.takeBottom(42, 8);
  return out;
}
template <typename Screen>
void footer(Screen& screen, fui::ButtonProps& button, const Footer& layout, int page, int pages, int action) {
  button = {};
  button.label = tr(STR_DONE);
  button.text.font = pixelui::kBody;
  button.text.align = fui::TextAlign::Center;
  button.styles = pixelui::buttonStyles();
  button.action = action;
  button.value = Done;
  button.minTouchSize = 0;
  screen.button(button, layout.done);
  pageui::build(screen, layout.pager, page, pages, action, Previous, action, Next);
}
inline void rows(fui::ListProps& p) {
  pixelui::styleList(p);
  p.labelText.font = 4;
  p.labelText.maxLines = 2;
  p.subtitleText.font = 3;
  p.subtitleText.maxLines = 1;
  p.valueText.font = 4;
  p.rowHeight = RowHeight;
  p.rowGap = RowGap;
  p.sidePadding = 12;
  p.rowPaddingY = 8;
  p.iconSize = 32;
  p.rowStyles.normal.border = fui::Paint::solid(fui::Color::Black);
  p.rowStyles.normal.borderWidth = 2;
  p.partialTrailingRow = false;
}
}  // namespace readersaved
