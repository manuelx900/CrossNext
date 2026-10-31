#pragma once
#include "LibraryMenu.h"
#include "LibraryPreferences.h"
namespace libraryui {
inline constexpr int kPickSort = -4, kReverseSort = -5;
template <typename Screen>
void filterTabs(Screen& screen, int active, fui::ActionId action, fui::ActionId reverseAction = 0,
                bool descending = true) {
  const auto body = screen.body();
  const int height = screen.device().width > screen.device().height ? 44 : 52;
  fui::TabItem tabs[] = {fui::tabItem(0, active == 0, true, tr(STR_LIBRARY_TAB_RECENT)),
                         fui::tabItem(1, active == 1, true, tr(STR_LIBRARY_FILTER))};
  pixelui::tabs(screen, tabs, 2, reverseAction ? fui::NO_ACTION : action, 4, height, 8);
  if (!reverseAction) return;
  for (int n = 0; n < 2; ++n) {
    const auto tab = fui::makeRect(body.x + body.width * n / 2, body.y, body.width / 2, height);
    const int arrow = n == active ? 44 : 0;
    screen.frame().hit(fui::makeRect(tab.x, tab.y, tab.width - arrow, tab.height), action, n, fui::InputTouch);
    if (!arrow) continue;
    const auto hit = fui::makeRect(tab.right() - arrow, tab.y, arrow, height);
    screen.frame().hit(hit, reverseAction, kReverseSort, fui::InputTouch);
    const int x = hit.x + hit.width / 2, y = hit.y + hit.height / 2, direction = descending ? 1 : -1;
    const auto ink = fui::Paint::solid(fui::Color::White);
    screen.target().line({int16_t(x - 5), int16_t(y - direction * 3)}, {int16_t(x), int16_t(y + direction * 3)}, 2,
                         ink);
    screen.target().line({int16_t(x), int16_t(y + direction * 3)}, {int16_t(x + 5), int16_t(y - direction * 3)}, 2,
                         ink);
  }
}

template <typename Screen>
void classicTabs(Screen& screen, int active, uint8_t directions, fui::ActionId action) {
  static constexpr StrId labels[] = {StrId::STR_LIBRARY_TAB_RECENT, StrId::STR_LIBRARY_TAB_TITLE,
                                     StrId::STR_LIBRARY_TAB_AUTHOR};
  fui::TabItem tabs[3];
  for (int n = 0; n < 3; ++n) {
    tabs[n] = fui::tabItem(n, n == active, true, I18N.get(labels[n]));
    tabs[n].indicator = n != active              ? fui::TabIndicator::None
                        : directions & (1u << n) ? fui::TabIndicator::Down
                                                 : fui::TabIndicator::Up;
  }
  pixelui::tabs(screen, tabs, 3, action, 4, screen.device().width > screen.device().height ? 44 : 52, 8);
}
template <typename Screen>
void sortBar(Screen& screen, const char* label, fui::ActionId action, bool descending, bool focused) {
  const auto box = screen.takeTop(screen.device().width > screen.device().height ? 48 : 56, 8);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const int side = 44;
  fui::TextStyle style;
  style.font = 4;
  style.maxLines = 1;
  style.bold = true;
  const int prefix = std::min(int(box.width) / 2,
                              int(screen.target().measureText(style.font, tr(STR_LIBRARY_SORT_BY), style).width) + 20);
  screen.target().text(toybox::inkCentred(screen.target(), fui::makeRect(box.x + 8, box.y, prefix, box.height), style),
                       tr(STR_LIBRARY_SORT_BY), style);
  style.bold = false;
  const auto value = fui::makeRect(box.x + prefix + 8, box.y, box.width - prefix - side - 16, box.height);
  screen.target().text(toybox::inkCentred(screen.target(), value, style), label, style);
  const auto button = fui::makeRect(box.right() - side, box.y, side, box.height);
  const int x = button.x + 12, y = button.y + button.height / 2;
  const int direction = descending ? 1 : -1;
  screen.target().line({int16_t(x), int16_t(y - 12)}, {int16_t(x), int16_t(y + 12)}, 2, ink);
  screen.target().line({int16_t(x - 5), int16_t(y + direction * 7)}, {int16_t(x), int16_t(y + direction * 12)}, 2, ink);
  screen.target().line({int16_t(x + 5), int16_t(y + direction * 7)}, {int16_t(x), int16_t(y + direction * 12)}, 2, ink);
  for (int n = 0; n < 3; ++n) screen.target().fill(fui::makeRect(x + 7, y - 10 + 7 * n, 16 - n * 4, 2), ink);
  if (!chrome::lightHeader) {
    screen.target().stroke(box, ink, pixelui::kBorder);
    screen.target().fill(fui::makeRect(button.x, button.y, pixelui::kBorder, button.height), ink);
  }
  if (chrome::lightHeader || focused)
    screen.target().fill(fui::makeRect(box.x, box.bottom() - (focused ? 2 : 1), box.width, focused ? 2 : 1), ink);
  screen.frame().hit(fui::makeRect(box.x, box.y, box.width - side, box.height), action, kPickSort, fui::InputTouch);
  screen.frame().hit(button, action, kReverseSort, fui::InputTouch);
}
inline void bookRows(fui::ListProps& props, const fui::DrawTarget& target, bool expanded, bool grouped = false) {
  styleBookRows(props, target);
  if (chrome::lightHeader) lightRows(props);
  props.labelText.maxLines = 1;
  props.rowHeight = expanded ? 80 : 56;
  props.subtitleText.font = 3;
  props.subtitleText.bold = false;
  props.subtitleText.maxLines = 1;
  props.rowPaddingY = 8;
  if (grouped) {
    props.headerText.font = chrome::lightHeader ? 3 : 4;
    props.headerText.bold = false;
    props.headerRowHeight = target.lineHeight(props.headerText.font) + 8;
    props.sectionGap = 12;
    if (!expanded) {
      props.labelText.maxLines = 2;
      props.rowHeight = std::max(32, 2 * int(target.lineHeight(props.labelText.font))) + 8;
      props.rowPaddingY = 4;
    }
  }
}
inline void filterCards(fui::ListProps& props, const fui::DrawTarget& target) {
  bookRows(props, target, false);
  props.rowGap = 0;
  props.rowHeight = 64;
  props.rowPaddingY = 8;
  props.labelText.font = 0;
  props.labelText.maxLines = 2;
  props.rowStyles.normal.borderWidth = 0;
  props.rowStyles.normal.radius = chrome::lightHeader ? 6 : 0;
  props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
  props.separatorPaint = fui::Paint::solid(fui::Color::Transparent);
}

template <typename Screen>
void filterList(Screen& screen, fui::ListProps& props) {
  if (chrome::lightHeader) {
    props.separatorPaint = lightui::rule;
    fui::list(screen.frame(), screen.body(), props);
    return;
  }
  pixelui::list(screen, props);
  const auto band = screen.body(), rows = pixelui::rowArea(band, props);
  int height = 0;
  for (int row = props.topIndex; row < props.count; ++row) {
    const int slot = row - props.itemsWindowFirst;
    if (slot < 0 || slot >= props.itemsWindowCount) break;
    const int next =
        fui::measureListRow(screen.target(), screen.frame().assets(), rows.width, props, props.items[slot]).height;
    if (height + next > band.height) break;
    if (height)
      screen.target().fill(fui::makeRect(rows.x, rows.y + height, rows.width, pixelui::kBorder),
                           fui::Paint::solid(fui::Color::Black));
    height += next;
  }
}

inline void preferenceRows(fui::ListProps& props, fui::ListItem* rows, Preferences p) {
  static constexpr StrId names[] = {StrId::STR_LIBRARY_METADATA,    StrId::STR_LIBRARY_LIST_VIEW,
                                    StrId::STR_LIBRARY_RECENT_VIEW, StrId::STR_LIBRARY_SHOW_FINISHED,
                                    StrId::STR_LIBRARY_SERIES,      StrId::STR_LIBRARY_GENRE,
                                    StrId::STR_FORMAT_EPUB,         StrId::STR_FORMAT_XTC,
                                    StrId::STR_FORMAT_TXT,          StrId::STR_FORMAT_MD};
  for (int n = 0; n < int(Preference::Count); ++n) {
    rows[n] = {};
    rows[n].label = I18N.get(names[n]);
    rows[n].actionValue = n;
    rows[n].toggle = n != 1 && n != 2;
    if (n == 0)
      rows[n].toggleChecked = p.metadata;
    else if (n >= 3)
      rows[n].toggleChecked = n < 6 ? (p.display & (1 << (n - 3))) != 0 : (p.formats & (1 << (n - 6))) != 0;
  }
  rows[1].value = p.expanded ? tr(STR_LIBRARY_EXPANDED) : tr(STR_LIBRARY_COMPACT);
  rows[2].value = p.grid ? tr(STR_LIBRARY_GRID_VIEW) : tr(STR_LIBRARY_LIST);
  rows[3].sectionHeading = tr(STR_CAT_DISPLAY);
  rows[6].sectionHeading = tr(STR_LIBRARY_SHOW_FILES);
  props.items = rows;
  props.count = int(Preference::Count);
  pixelui::styleList(props, true);
  props.labelText.font = 4;
  props.labelText.bold = false;
  props.labelText.maxLines = 1;
  props.valueText.font = 4;
  props.headerText.font = 0;
  props.headerText.bold = true;
  props.headerUnderline = true;
  props.headerRowHeight = 32;
  props.rowHeight = 48;
  props.rowPaddingY = 5;
  props.sidePadding = 10;
  props.sectionGap = 8;
  props.toggleWidth = 24;
  props.toggleCheckbox = true;
  props.checkboxCheckmark = true;
  props.rowStyles.normal.borderWidth = chrome::lightHeader ? 0 : 1;
  props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
  props.selectionMarker = fui::SelectionMarker::Underline;
  props.markerThickness = 1;
}
}  // namespace libraryui
