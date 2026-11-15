#pragma once
#include <FreeInkUIIcon.h>

#include <algorithm>
#include <cstdio>

#include "../../components/icons/listIcons.h"
#include "../../components/icons/readerToolbarIcons.h"
#include "../ui/ExtraIcons.h"
#include "../ui/PageIndicator.h"
#include "../ui/ToyboxTokens.h"
namespace readertext {
namespace fui = freeink::ui;
inline void panelLabel(char* out, size_t length, const char* label) {
  if (chrome::lightHeader)
    pixelui::copyNatural(out, length, label);
  else
    pixelui::copyCaps(out, length, label);
}
inline fui::Rect sheetRect(const fui::DeviceContext& device, bool, int minTop) {
  const auto safe = device.safeRect();
  const int percent = device.width > device.height ? 94 : 76;
  const int height = std::max(1, std::min(safe.height * percent / 100, int(safe.bottom()) - minTop));
  return fui::makeRect(safe.x, safe.bottom() - height, safe.width, height);
}

inline constexpr int kSectionGap = 12;
template <typename Screen>
void button(Screen& screen, Work& work, fui::Rect box, const char* label, int control, bool enabled = true) {
  work.button = {};
  auto& p = work.button;
  p.styles = pixelui::buttonStyles();
  p.action = enabled ? 6 : fui::NO_ACTION;
  p.value = control;
  p.label = label;
  p.text.font = pixelui::kSmall;
  p.text.align = fui::TextAlign::Center;
  p.minTouchSize = 0;
  p.state = enabled ? fui::StateNormal : fui::StateDisabled;
  screen.button(p, box);
}
template <typename Screen>
void row(Screen& screen, Work&, fui::Rect box, const char* label, const char* value, int control, bool check = false,
         bool checked = false) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  if (chrome::lightHeader)
    screen.target().stroke(box, ink, 1, lightui::kControlRadius);
  else
    screen.target().fill(
        fui::makeRect(box.x, box.bottom() - (chrome::lightHeader ? 1 : 2), box.width, chrome::lightHeader ? 1 : 2),
        chrome::lightHeader ? lightui::rule : ink);
  const int valueStart = control == Family ? box.width / 3 : box.width / 2;
  const int labelWidth = control == Family ? valueStart - 12 : check ? box.width - 54 : box.width * 5 / 8;
  pixelui::text(screen.target(),
                fui::makeRect(box.x + (chrome::lightHeader ? 12 : 4), box.y, labelWidth - 8, box.height - 12), label,
                pixelui::kSmall);
  if (check) {
    const auto square = fui::makeRect(box.right() - 34, box.y + (box.height - 12 - 26) / 2, 26, 26);
    screen.target().stroke(square, ink, 2);
    if (checked) {
      screen.target().fill(square, ink);
      const auto tick = fui::Paint::solid(fui::Color::White);
      screen.target().line({int16_t(square.x + 5), int16_t(square.y + 13)},
                           {int16_t(square.x + 11), int16_t(square.y + 19)}, 3, tick);
      screen.target().line({int16_t(square.x + 11), int16_t(square.y + 19)},
                           {int16_t(square.x + 22), int16_t(square.y + 6)}, 3, tick);
    }
  } else {
    const auto valueBox = fui::makeRect(box.x + valueStart, box.y, box.width - valueStart - 8, box.height - 12);
    if (control == Family) {
      fui::TextStyle style;
      style.font = 4;
      style.align = fui::TextAlign::Right;
      style.maxLines = std::clamp(int(valueBox.height) / screen.target().lineHeight(style.font), 1, 2);
      screen.target().text(valueBox, value, style);
    } else {
      pixelui::text(screen.target(), valueBox, value, 3, fui::Color::Black, fui::TextAlign::Right);
    }
  }
  if (fui::hasState(screen.frame().stateFor(6, control), fui::StateFocused)) screen.target().stroke(box, ink, 3);
  screen.frame().hit(box, 6, control, fui::InputTouch);
}
template <typename Screen>
void styleToggle(Screen& screen, fui::Rect box, const char* label, int control, bool checked, bool outlined = true) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const int radius = chrome::lightHeader ? lightui::kControlRadius : 0;
  if (outlined)
    screen.target().stroke(box, ink, chrome::lightHeader ? 1 : 2, radius);
  else
    screen.target().fill(fui::makeRect(box.x, box.bottom() - 2, box.width, 2), ink);
  const auto square = fui::makeRect(box.right() - 42, box.y + (box.height - 26) / 2, 26, 26);
  screen.target().stroke(square, ink, 2, radius);
  if (checked) {
    screen.target().fill(square, ink, radius);
    const auto white = fui::Paint::solid(fui::Color::White);
    screen.target().line({int16_t(square.x + 5), int16_t(square.y + 13)},
                         {int16_t(square.x + 11), int16_t(square.y + 19)}, 3, white);
    screen.target().line({int16_t(square.x + 11), int16_t(square.y + 19)},
                         {int16_t(square.x + 22), int16_t(square.y + 6)}, 3, white);
  }
  fui::TextStyle style;
  style.font = pixelui::kSmall;
  style.maxLines = 2;
  const int textInset = control == Paragraphs ? (chrome::lightHeader ? 14 : 4) : 16;
  const bool defaults = control == CopyDefaults;
  const auto info = fui::makeRect(box.right() - 88, box.y, 44, box.height);
  const auto text = fui::makeRect(box.x + textInset, box.y + 8, (defaults ? info.x : square.x) - box.x - textInset - 12,
                                  box.height - 16);
  screen.target().text(toybox::inkCentred(screen.target(), text, style), label, style);
  if (fui::hasState(screen.frame().stateFor(6, control), fui::StateFocused))
    screen.target().stroke(box, ink, 3, radius);
  if (defaults) {
    const auto circle = fui::makeRect(info.x + 9, info.y + (info.height - 26) / 2, 26, 26);
    screen.target().stroke(circle, ink, 2, 13);
    screen.target().fill(fui::makeRect(circle.x + 12, circle.y + 5, 3, 10), ink);
    screen.target().fill(fui::makeRect(circle.x + 12, circle.y + 18, 3, 3), ink);
    screen.frame().hit(fui::makeRect(box.x, box.y, info.x - box.x, box.height), 6, control, fui::InputTouch);
    screen.frame().hit(fui::makeRect(box.right() - 44, box.y, 44, box.height), 6, control, fui::InputTouch);
    screen.frame().hit(info, 6, DefaultsInfo, fui::InputTouch);
  } else {
    screen.frame().hit(box, 6, control, fui::InputTouch);
  }
}
template <typename Screen>
void stepper(Screen& screen, Work& work, fui::Rect box, const char* label, const char* value, int current, int count,
             int down, int up) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const int pad = chrome::lightHeader ? 10 : 0;
  if (box.height < 80) {
    const int y = box.y + (box.height - 44) / 2;
    button(screen, work, fui::makeRect(box.x, y, 44, 44), "-", down, current > 0);
    button(screen, work, fui::makeRect(box.right() - 44, y, 44, 44), "+", up, current < count - 1);
    pixelui::text(screen.target(), fui::makeRect(box.x + 50, box.y, box.width - 100, box.height / 2), label, 3,
                  fui::Color::Black, fui::TextAlign::Center);
    pixelui::text(screen.target(), fui::makeRect(box.x + 50, box.y + box.height / 2, box.width - 100, box.height / 2),
                  value, 3, fui::Color::Black, fui::TextAlign::Center);
    return;
  }
  pixelui::text(screen.target(), fui::makeRect(box.x + pad + 4, box.y + (pad ? 4 : 0), box.width * 2 / 3 - pad, 28),
                label, pixelui::kSmall);
  pixelui::text(screen.target(),
                fui::makeRect(box.x + box.width * 2 / 3, box.y + (pad ? 4 : 0), box.width / 3 - pad - 4, 28), value, 3,
                fui::Color::Black, fui::TextAlign::Right);
  const int bottomPadding = std::max(chrome::lightHeader ? 8 : 6, std::min(16, int(box.height) - 80));
  const int y = box.bottom() - 44 - bottomPadding;
  button(screen, work, fui::makeRect(box.x + pad, y, 44, 44), "-", down, current > 0);
  button(screen, work, fui::makeRect(box.right() - 44 - pad, y, 44, 44), "+", up, current < count - 1);
  const int bars = std::max(1, std::min(15, count)), width = (box.width - 112 - pad * 2) / bars;
  for (int n = 0; n < bars; ++n) {
    const auto block = fui::makeRect(box.x + pad + 56 + n * width, y + 14, std::max(2, width - 4), 14);
    if (n * count / bars <= current)
      screen.target().fill(block, chrome::lightHeader ? lightui::selected : fui::Paint::solid(fui::Color::Black));
    else
      screen.target().stroke(block, fui::Paint::solid(fui::Color::Black), chrome::lightHeader ? 1 : 2);
  }
  if (chrome::lightHeader)
    screen.target().stroke(box, ink, 1, lightui::kControlRadius);
  else
    screen.target().fill(fui::makeRect(box.x, box.bottom() - 2, box.width, 2), fui::Paint::solid(fui::Color::Black));
}
template <typename Screen>
void alignment(Screen& screen, Work& work, fui::Rect box, int current, const char* value) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const bool compact = box.height < 100;
  const int optionGap = compact ? 6 : 8;
  const int side = compact ? 44 : std::min({72, (box.width - 20 - 4 * optionGap) / 5, box.height - 52});
  const int optionTop = compact ? box.y + (box.height - side) / 2 : box.y + 40 + (box.height - 40 - side) / 2;
  const int groupWidth = 5 * side + 4 * optionGap;
  pixelui::text(
      screen.target(),
      fui::makeRect(box.x + 10, box.y + 4, compact ? box.width - 264 : box.width / 2, compact ? box.height - 8 : 30),
      tr(STR_ALIGNMENT), pixelui::kSmall);
  if (!compact)
    pixelui::text(screen.target(), fui::makeRect(box.x + box.width / 2, box.y + 4, box.width / 2 - 10, 34), value,
                  pixelui::kSmall, fui::Color::Black, fui::TextAlign::Right);
  static constexpr int actions[] = {AlignmentJustify, AlignmentLeft, AlignmentCenter, AlignmentRight, AlignmentBook};
  const int start = compact ? box.right() - groupWidth - 6 : box.x + (box.width - groupWidth) / 2;
  for (int n = 0; n < 5; ++n) {
    const auto option = fui::makeRect(start + n * (side + optionGap), optionTop, side, side);
    button(screen, work, option, "", actions[n]);
    if (n == current) screen.target().fill(option, ink, chrome::lightHeader ? lightui::kControlRadius : 0);
    const auto color = fui::Paint::solid(n == current ? fui::Color::White : fui::Color::Black);
    if (n == 4) {
      screen.target().bitmap(option.inset({8, 8, 8, 8}), fui::bitmapFromIcon(icon_book_32), fui::BitmapMode::Contain,
                             color);
    } else {
      const int step = std::max(7, side * 7 / 44);
      const int stroke = side >= 64 ? 4 : 3;
      const int glyphTop = (side - 3 * step - stroke) / 2;
      for (int line = 0; line < 4; ++line) {
        const int width = n == 0 || line % 2 == 0 ? side - 18 : side - 26;
        const int x = option.x + (n == 2 ? (side - width) / 2 : n == 3 ? side - 9 - width : 9);
        screen.target().fill(fui::makeRect(x, option.y + glyphTop + line * step, width, stroke), color);
      }
    }
  }
  screen.target().stroke(box, ink, chrome::lightHeader ? 1 : 2, chrome::lightHeader ? lightui::kControlRadius : 0);
}
template <typename Screen>
void compactPager(Screen& screen, fui::Rect band, int page, int pages, fui::ActionId previous, int previousValue,
                  fui::ActionId next, int nextValue) {
  const auto g = pageui::layout(band);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  if (!chrome::lightHeader)
    for (auto box : {g.before, g.count, g.after}) screen.target().stroke(box, ink, 2);
  for (int n = 0; n < 2; ++n) {
    const auto box = n ? g.after : g.before;
    if (fui::hasState(screen.frame().stateFor(n ? next : previous, n ? nextValue : previousValue), fui::StateFocused))
      screen.target().stroke(box, ink, 3);
    const int x = box.x + box.width / 2, y = box.y + box.height / 2, sign = n ? 1 : -1;
    screen.target().line({int16_t(x - sign * 3), int16_t(y - 6)}, {int16_t(x + sign * 3), int16_t(y)}, 2, ink);
    screen.target().line({int16_t(x + sign * 3), int16_t(y)}, {int16_t(x - sign * 3), int16_t(y + 6)}, 2, ink);
  }
  char label[24];
  std::snprintf(label, sizeof(label), "%d / %d", page, pages);
  pixelui::text(screen.target(), g.count, label, pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
  if (page > 1) screen.frame().hit(g.before, previous, previousValue, fui::InputTouch);
  if (page < pages) screen.frame().hit(g.after, next, nextValue, fui::InputTouch);
}

inline int layoutCapacity(const fui::DeviceContext& device, int minTop = 0, bool standalone = false) {
  const bool wide = device.width > device.height;
  const int grip = standalone && wide ? 0 : chrome::lightHeader ? (wide ? 24 : 36) : wide ? 32 : 44;
  const int tabs = chrome::lightHeader || wide ? 44 : 52;
  const int tabGap = wide ? 8 : chrome::lightHeader ? 12 : 10;
  const int done = chrome::lightHeader || wide ? 44 : 56;
  const int available =
      sheetRect(device, false, minTop).height - 12 - grip - tabs - tabGap - done - (wide ? 8 : 12) - 44 - 8;
  // The first advanced control is a compact checkbox row, not a stepper.
  if (!wide && available >= 56 + 3 * (88 + 8)) return 4;
  const int rows = std::max(1, (available + 8) / 92);
  return std::min(4, rows * (wide ? 2 : 1));
}
inline int layoutPageCount(const fui::DeviceContext& device, int minTop = 0, bool standalone = false) {
  const int capacity = layoutCapacity(device, minTop, standalone);
  return (3 + capacity - 1) / capacity + (4 + capacity - 1) / capacity;
}
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work) {
  const bool compactLandscape = screen.device().width > screen.device().height;
  const int sectionGap = compactLandscape ? 8 : kSectionGap;
  const auto rect = sheetRect(screen.device(), model.fixed, model.minTop);
  fui::SheetProps sheet;
  sheet.anchor = fui::SheetEdge::Bottom;
  sheet.radius = chrome::lightHeader ? 18 : 0;
  sheet.ruleWidth = chrome::lightHeader ? 1 : 3;
  sheet.grabberHeight = sheet.grabberMargin = sheet.grabberInset = 0;
  sheet.dismissAction = 1;
  screen.sheet(sheet, rect.height);
  screen.insetContent(fui::Insets{4, 10, 8, 10});
  const auto grip = screen.takeTop(model.standalone && compactLandscape ? 0
                                   : chrome::lightHeader                ? (compactLandscape ? 24 : 36)
                                   : compactLandscape                   ? 32
                                                                        : 44);
  const int cx = grip.x + grip.width / 2, cy = grip.y + grip.height / 2;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  if (chrome::lightHeader)
    screen.target().fill(fui::makeRect(cx - 28, cy - 2, 56, 4), ink, 2);
  else {
    screen.target().line({int16_t(cx - 16), int16_t(cy + 4)}, {int16_t(cx), int16_t(cy - 4)}, 3, ink);
    screen.target().line({int16_t(cx), int16_t(cy - 4)}, {int16_t(cx + 16), int16_t(cy + 4)}, 3, ink);
  }
  if (!grip.empty()) screen.frame().hit(grip, 1, 0, fui::InputTouch);
  if (model.view == SheetView::Families && !model.fixed) {
    const auto heading = screen.takeTop(52, kSectionGap);
    button(screen, work, fui::makeRect(heading.x, heading.y, 48, heading.height), "", FontBack);
    const int ax = heading.x + 14, ay = heading.y + heading.height / 2;
    screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 22), int16_t(ay)}, 3, ink);
    screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 10), int16_t(ay - 10)}, 3, ink);
    screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 10), int16_t(ay + 10)}, 3, ink);
    pixelui::text(screen.target(), fui::makeRect(heading.x + 60, heading.y, heading.width - 60, heading.height),
                  tr(STR_TEXT_FONT_FAMILY), pixelui::kSmall);
    const auto close = screen.takeBottom(52, kSectionGap);
    work.button = {};
    work.button.styles = pixelui::buttonStyles();
    work.button.action = 1;
    work.button.label = tr(STR_DONE);
    work.button.text.font = pixelui::kBody;
    work.button.text.align = fui::TextAlign::Center;
    work.button.minTouchSize = 0;
    screen.button(work.button, close);
    const auto pager = screen.takeBottom(44, kSectionGap);
    const int capacity = fontRowsPerPage(screen.device(), model.minTop);
    const int pages = std::max(1, (model.fontCount + capacity - 1) / capacity);
    compactPager(screen, pager, model.fontPage + 1, pages, 6, FontPrevious, 6, FontNext);
    const auto list = screen.body();
    const int first = model.fontPage * capacity;
    const int columns = fontColumns(screen.device());
    const int gap = columns > 1 ? 10 : 0;
    const int width = (list.width - gap) / columns;
    for (int n = 0; n < capacity && first + n < model.fontCount; ++n) {
      const int index = first + n;
      work.button = {};
      auto& p = work.button;
      std::snprintf(work.label, sizeof(work.label), "%s",
                    model.fontLabel ? model.fontLabel(model.fontContext, index) : "");
      p.label = work.label;
      p.text.font = 4;
      p.text.align = fui::TextAlign::Left;
      p.padding = fui::Insets{2, 14, 2, 14};
      p.styles = chrome::lightHeader ? lightui::controlStyles() : pixelui::rowStyles();
      p.state = index == model.fontSelected ? fui::StateSelected : fui::StateNormal;
      p.action = 6;
      p.value = FontPick + index;
      p.minTouchSize = 0;
      const auto row = fui::makeRect(list.x + n % columns * (width + gap), list.y + n / columns * fontRowHeight(),
                                     width, fontRowHeight() - (chrome::lightHeader ? 6 : 0));
      if (chrome::lightHeader) p.padding.right = 42;
      screen.button(p, row);
    }
    return;
  }
  static constexpr StrId names[] = {StrId::STR_FONT, StrId::STR_LAYOUT, StrId::STR_TOOL_CONTENTS, StrId::STR_TOOL_MORE};
  int count = 0;
  static constexpr int order[] = {2, 0, 1, 3};
  for (int n : order) {
    if (model.fixed && (n == 0 || (n == 1 && !model.statusAvailable))) continue;
    if (n == 2 && !model.contentsAvailable) continue;
    panelLabel(work.tabLabels[count], sizeof(work.tabLabels[count]), I18N.get(names[n]));
    work.tabs[count] = fui::tabItem(n, model.tab == n, true, work.tabLabels[count]);
    ++count;
  }
  pixelui::tabs(screen, work.tabs, count, 2, 3,
                chrome::lightHeader ? 44
                : compactLandscape  ? 44
                                    : 52,
                chrome::lightHeader ? (compactLandscape ? 8 : 12)
                : compactLandscape  ? 8
                                    : 10);
  const auto done = screen.takeBottom(chrome::lightHeader || compactLandscape          ? 44
                                      : screen.device().width > screen.device().height ? 48
                                                                                       : 56,
                                      sectionGap);
  work.button = {};
  auto& p = work.button;
  p.styles = pixelui::buttonStyles();
  p.action = 1;
  panelLabel(work.label, sizeof(work.label), tr(STR_DONE));
  p.label = work.label;
  p.text.font = chrome::lightHeader || compactLandscape ? pixelui::kSmall : pixelui::kBody;
  p.text.align = fui::TextAlign::Center;
  p.minTouchSize = 0;
  screen.button(p, done);
  if (!model.fixed && model.tab == 3 && model.view == SheetView::Style) {
    if (model.standalone) {
      const auto copy = screen.takeBottom(64, sectionGap);
      styleToggle(screen, copy, tr(STR_TEXT_COPY_DEFAULTS), CopyDefaults, model.useDefaults);
    } else {
      const auto heading = screen.takeTop(44, kSectionGap);
      button(screen, work, fui::makeRect(heading.x, heading.y, 48, heading.height), "", StyleBack);
      const int ax = heading.x + 14, ay = heading.y + heading.height / 2;
      screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 22), int16_t(ay)}, 3, ink);
      screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 10), int16_t(ay - 10)}, 3, ink);
      screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 10), int16_t(ay + 10)}, 3, ink);
      pixelui::text(screen.target(), fui::makeRect(heading.x + 60, heading.y, heading.width - 60, heading.height),
                    tr(STR_STYLE), pixelui::kSmall);
    }
    const auto area = screen.body();
    const int cols = screen.device().width > screen.device().height ? 2 : 1;
    const int rows = 4 / cols;
    const int width = (area.width - (cols - 1) * kSectionGap) / cols;
    const int height = std::min(88, (area.height - (rows - 1) * kSectionGap) / rows);
    static constexpr StrId labels[] = {StrId::STR_FOCUS_READING, StrId::STR_HYPHENATION, StrId::STR_EMBEDDED_STYLE,
                                       StrId::STR_TEXT_AA};
    const bool checks[] = {model.focus, model.hyphenation, model.embedded, model.antialiasing};
    for (int n = 0; n < 4; ++n) {
      const auto option = fui::makeRect(area.x + n % cols * (width + kSectionGap),
                                        area.y + n / cols * (height + kSectionGap), width, height);
      panelLabel(work.label, sizeof(work.label), I18N.get(labels[n]));
      styleToggle(screen, option, work.label, Focus + n, checks[n]);
    }
    return;
  }
  if (model.tab == 2 || model.tab == 3) {
    const int capacity = sheetRowsPerPage(screen.device(), model.fixed, model.tab);
    const int pages = std::max(1, (model.listCount + capacity - 1) / capacity);
    const int page = std::clamp(model.listPage, 0, pages - 1);
    if (model.tab == 2 || pages > 1) {
      const auto band = screen.takeBottom(44, sectionGap);
      const int allWidth = model.tab == 2 ? 72 : 0;
      compactPager(screen, band, page + 1, pages, 6, SheetPrevious, 6, SheetNext);
      if (allWidth)
        button(screen, work, fui::makeRect(band.right() - allWidth, band.y, allWidth, band.height),
               tr(STR_READER_ALL_CHAPTERS), AllChapters);
    }
    if (model.tab == 2) {
      const auto heading = screen.takeTop(compactLandscape ? 28 : 34, sectionGap);
      pixelui::text(screen.target(), fui::makeRect(heading.x, heading.y, heading.width * 2 / 3, heading.height),
                    tr(STR_SELECT_CHAPTER), pixelui::kSmall);
      std::snprintf(work.label, sizeof(work.label), tr(STR_READER_CHAPTERS), model.listCount);
      pixelui::text(screen.target(),
                    fui::makeRect(heading.x + heading.width * 2 / 3, heading.y, heading.width / 3, heading.height),
                    work.label, 3, fui::Color::Black, fui::TextAlign::Right);
    }
    const auto area = screen.body();
    if (!model.listCount) pixelui::text(screen.target(), area, tr(STR_NO_CHAPTERS), 0);
    const int cols = model.tab == 3 && screen.device().width > screen.device().height ? 2 : 1;
    const int rows = (capacity + cols - 1) / cols;
    const int gap = sectionGap;
    const int h = model.fixed && model.tab == 3 ? 70 : std::max(1, (int(area.height) - (rows - 1) * gap) / rows);
    for (int n = 0; n < capacity && page * capacity + n < model.listCount; ++n) {
      const int index = page * capacity + n;
      work.label[0] = work.value[0] = 0;
      if (model.listLabel) model.listLabel(model.listContext, index, work.label, sizeof(work.label));
      if (model.listValue) model.listValue(model.listContext, index, work.value, sizeof(work.value));
      const int colWidth = (area.width - (cols - 1) * 12) / cols;
      const auto box =
          fui::makeRect(area.x + (n % cols) * (colWidth + 12), area.y + (n / cols) * (h + gap), colWidth, h);
      const bool selected = model.tab == 2 && index == model.currentChapter;
      if (chrome::lightHeader) {
        screen.target().stroke(box, ink, 1, lightui::kControlRadius);
        if (selected) screen.target().fill(box, lightui::selected, 4);
      } else if (selected)
        screen.target().fill(box, ink);
      else
        screen.target().stroke(box, ink, 2);
      const auto fg = selected ? fui::Color::White : fui::Color::Black;
      const auto paint = fui::Paint::solid(fg);
      const int control = (model.tab == 2 ? ChapterPick : MorePick) + index;
      screen.frame().hit(box, 6, control, fui::InputTouch);
      if (model.tab == 2) {
        fui::TextStyle chapterStyle;
        chapterStyle.font = 4;
        chapterStyle.color = fg;
        if (chrome::lightHeader && h < 2 * screen.target().lineHeight(chapterStyle.font)) chapterStyle.font = 0;
        const int padding = std::min(chrome::lightHeader ? 8 : 10,
                                     std::max(0, (h - 2 * screen.target().lineHeight(chapterStyle.font)) / 2));
        const auto title = fui::makeRect(box.x + 12, box.y + padding, box.width - 24, h - 2 * padding);
        chapterStyle.maxLines = 2;
        int count = 0;
        fui::layoutText(screen.target(), title, work.label, chapterStyle, [&](const char*, fui::Rect) { ++count; });
        const int height = std::min(int(title.height), count * int(screen.target().lineHeight(chapterStyle.font)));
        screen.target().text(fui::makeRect(title.x, title.y + (title.height - height) / 2, title.width, height),
                             work.label, chapterStyle);
      } else if (model.fixed) {
        const int half = (h - 12) / 2;
        pixelui::text(screen.target(), fui::makeRect(box.x + 4, box.y, box.width - 8, half), work.label,
                      pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
        pixelui::text(screen.target(), fui::makeRect(box.x + 4, box.y + half, box.width - 8, h - half - 12), work.value,
                      pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
      } else {
        const freeink::Icon* icons[] = {&icon_reader_text_24, &extra_carnivoro_search_32,
                                        &icon_bookmark_32,    &icon_book_32,
                                        &icon_file_32,        &extra_chart_64,
                                        &icon_check_32,       &extra_dictionary_32,
                                        &icon_refresh_cw_32};
        const auto& mark = *icons[std::clamp(index, 0, 8)];
        const int side = std::min(36, h - 12);
        screen.target().bitmap(fui::makeRect(box.x + 16, box.y + (h - side) / 2, side, side), fui::bitmapFromIcon(mark),
                               fui::BitmapMode::Contain, paint);
        fui::TextStyle labelStyle;
        labelStyle.font = pixelui::kSmall;
        labelStyle.maxLines = chrome::lightHeader ? 2 : 1;
        const auto title = fui::makeRect(box.x + side + 28, box.y + 4, box.width - side - 64, h - 8);
        pixelui::fitLabel(screen.target(), work.label, title.width, labelStyle);
        screen.target().text(toybox::inkCentred(screen.target(), title, labelStyle), work.label, labelStyle);
        const int cx = box.right() - 20, cy = box.y + h / 2;
        screen.target().line({int16_t(cx - 4), int16_t(cy - 7)}, {int16_t(cx + 3), int16_t(cy)}, 3, ink);
        screen.target().line({int16_t(cx + 3), int16_t(cy)}, {int16_t(cx - 4), int16_t(cy + 7)}, 3, ink);
      }
    }
    return;
  }
  const auto content = screen.body();
  const bool wide = screen.device().width > screen.device().height;
  const auto box = [&](int index, int height) {
    if (wide)
      return fui::makeRect(content.x + (index % 2) * (content.width + 12) / 2, content.y + (index / 2) * 94,
                           (content.width - 12) / 2, height);
    return fui::makeRect(content.x, content.y + index * (70 + kSectionGap), content.width, height);
  };
  if (model.fixed) {
    if (model.tab == 1 && model.statusAvailable) {
      row(screen, work, box(0, 70), tr(STR_XTC_STATUS_BAR), model.placement, Placement);
      row(screen, work, box(1, 70), tr(STR_BOOK_PROGRESS_PERCENTAGE), "", Percentage, true, model.percentage);
    } else {
      row(screen, work, box(0, 70), tr(STR_NIGHT_MODE), "", Night, true, model.night);
      row(screen, work, box(1, 70), tr(STR_REFRESH_FREQ), model.refresh, Refresh);
    }
  } else if (model.tab == 0) {
    auto family = box(0, wide ? 70 : 56);
    const int controlHeight = 96, controlGap = 16;
    auto size = box(1, controlHeight);
    auto weight = box(2, controlHeight);
    auto samples = box(3, 80);
    if (!wide) {
      size.y = content.y + 66;
      weight.y = size.bottom() + controlGap;
      const int sampleY = weight.bottom() + controlGap;
      samples = fui::makeRect(content.x, sampleY, content.width, std::max(0, int(content.bottom()) - sampleY));
    } else {
      family = fui::makeRect(content.x, content.y, content.width, 48);
      size = fui::makeRect(content.x, content.y + 56, (content.width - 12) / 2, controlHeight);
      weight = fui::makeRect(size.right() + 12, size.y, size.width, controlHeight);
    }
    row(screen, work, family, tr(STR_TEXT_FONT_FAMILY), model.family, Family);
    std::snprintf(work.label, sizeof(work.label), "%d pt", model.size);
    stepper(screen, work, size, tr(STR_TEXT_FONT_SIZE), work.label, model.sizeIndex, model.sizeCount, SizeDown, SizeUp);
    stepper(screen, work, weight, tr(STR_TEXT_FONT_WEIGHT),
            model.bold ? tr(STR_TEXT_WEIGHT_BOLD) : tr(STR_TEXT_WEIGHT_REGULAR), model.bold ? 1 : 0, 2, WeightDown,
            WeightUp);
    if (wide) return;
    const int half = samples.width / 2;
    const int labelHeight = 22;
    if (samples.height < 44) return;
    const int sampleHeight = std::max(0, samples.height - labelHeight);
    for (int n = 0; n < 2; ++n) {
      const auto sample = fui::makeRect(samples.x + n * half, samples.y, half, sampleHeight);
      const auto choice = fui::makeRect(sample.x + 4, samples.y, half - 8, samples.height);
      screen.frame().hit(choice, 6, n == 0 ? WeightDown : WeightUp, fui::InputTouch);
      if (model.bold == (n == 1)) {
        screen.target().stroke(choice, ink, 2, chrome::lightHeader ? lightui::kControlRadius : 0);
      }
      fui::TextStyle style;
      style.font = pixelui::kTitle;
      style.bold = n == 1;
      style.align = fui::TextAlign::Center;
      style.maxLines = 1;
      screen.target().text(sample, "Aa", style);
      pixelui::text(screen.target(), fui::makeRect(sample.x, sample.bottom(), half, labelHeight),
                    n == 0 ? tr(STR_TEXT_WEIGHT_REGULAR) : tr(STR_TEXT_WEIGHT_BOLD), 3, fui::Color::Black,
                    fui::TextAlign::Center);
    }
  } else if (model.tab == 1) {
    const int pagerHeight = 44, gap = 8;
    const int capacity = layoutCapacity(screen.device(), model.minTop, model.standalone);
    const int firstPages = (3 + capacity - 1) / capacity;
    const int pages = layoutPageCount(screen.device(), model.minTop, model.standalone);
    const int page = std::clamp(model.layoutPage, 0, pages - 1);
    const bool advanced = page >= firstPages;
    const int first = (advanced ? page - firstPages : page) * capacity;
    const int count = std::min(capacity, (advanced ? 4 : 3) - first);
    const int columns = wide && count > 1 ? 2 : 1;
    const int rows = (count + columns - 1) / columns;
    const bool compactParagraph = advanced && first == 0 && !wide;
    const int paragraphHeight = 56;
    const int height =
        compactParagraph && count > 1
            ? std::min(104,
                       std::max(1, (int(content.height) - pagerHeight - gap * rows - paragraphHeight) / (rows - 1)))
            : std::min(96, std::max(1, (int(content.height) - pagerHeight - gap * rows) / rows));
    const int width = (content.width - (columns - 1) * gap) / columns;
    const auto pager = fui::makeRect(content.x, content.bottom() - pagerHeight, content.width, pagerHeight);
    compactPager(screen, pager, page + 1, pages, 6, LayoutPrevious, 6, LayoutNext);
    for (int n = 0; n < count; ++n) {
      const int y = compactParagraph && n > 0 ? content.y + paragraphHeight + gap + (n - 1) * (height + gap)
                                              : content.y + n / columns * (height + gap);
      auto control = fui::makeRect(content.x + n % columns * (width + gap), y, width,
                                   compactParagraph && n == 0 ? paragraphHeight : height);
      static constexpr int advancedOrder[] = {5, 6, 3, 4};
      switch (advanced ? advancedOrder[first + n] : first + n) {
        case 0:
          stepper(screen, work, control, tr(STR_LINE_SPACING), model.spacingLabel, model.spacing, model.spacingCount,
                  SpacingDown, SpacingUp);
          break;
        case 1:
          std::snprintf(work.value, sizeof(work.value), "%d", model.margin);
          stepper(screen, work, control, tr(STR_SCREEN_MARGIN), work.value, model.marginIndex, model.marginCount,
                  MarginDown, MarginUp);
          break;
        case 2:
          if (n + 1 == count) {
            if (n % columns == 0) control.width = content.width;
            control.height = std::max(int(control.height), int(pager.y - gap - control.y));
          }
          alignment(screen, work, control, model.alignmentIndex, model.alignment);
          break;
        case 3:
          std::snprintf(work.value, sizeof(work.value), "%d%%", model.wordSpacing);
          stepper(screen, work, control, tr(STR_WORD_SPACING), work.value, (model.wordSpacing - 50) / 25, 7, WordDown,
                  WordUp);
          break;
        case 4:
          std::snprintf(work.value, sizeof(work.value), "%+d", model.characterSpacing - 2);
          stepper(screen, work, control, tr(STR_CHARACTER_SPACING), work.value, model.characterSpacing, 5,
                  CharacterDown, CharacterUp);
          break;
        case 5:
          styleToggle(screen, control, tr(STR_EXTRA_SPACING), Paragraphs, model.paragraphs, chrome::lightHeader);
          break;
        case 6:
          std::snprintf(work.value, sizeof(work.value), "%d", model.indent);
          stepper(screen, work, control, tr(STR_PARAGRAPH_INDENTATION), model.indent ? work.value : tr(STR_STATE_OFF),
                  model.indent, 6, IndentDown, IndentUp);
          break;
      }
    }
  }
}
}  // namespace readertext
