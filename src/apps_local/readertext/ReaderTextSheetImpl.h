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
inline void chapterNumber(int number, char* out, size_t size) {
  struct Roman {
    int value;
    const char* text;
  };
  static constexpr Roman romans[] = {{1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"},
                                     {90, "XC"},  {50, "L"},   {40, "XL"}, {10, "X"},   {9, "IX"},
                                     {5, "V"},    {4, "IV"},   {1, "I"}};
  if (number > 3999) {
    std::snprintf(out, size, "%d", number);
    return;
  }
  size_t used = 0;
  if (size) out[0] = 0;
  for (const auto& entry : romans)
    while (number >= entry.value) {
      number -= entry.value;
      for (const char* ch = entry.text; *ch && used + 1 < size; ++ch) out[used++] = *ch;
      if (size) out[used] = 0;
    }
}
inline fui::Rect sheetRect(const fui::DeviceContext& device, bool) {
  const auto safe = device.safeRect();
  const int height = safe.height * (device.width > device.height ? 94 : 76) / 100;
  return fui::makeRect(safe.x, safe.bottom() - height, safe.width, height);
}

inline constexpr int kSectionGap = 12;
template <typename Screen>
void button(Screen& screen, Work& work, fui::Rect box, const char* label, int control, bool enabled = true) {
  work.button = {};
  auto& p = work.button;
  p.styles = pixelui::rowStyles();
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
  screen.target().fill(fui::makeRect(box.x, box.bottom() - 2, box.width, 2), ink);
  const int valueStart = control == Family ? box.width / 3 : box.width / 2;
  const int labelWidth = control == Family ? valueStart - 12 : check ? box.width - 54 : box.width * 5 / 8;
  pixelui::text(screen.target(), fui::makeRect(box.x + 4, box.y, labelWidth, box.height - 12), label, pixelui::kSmall);
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
  screen.frame().hit(box, 6, control, fui::InputTouch);
}
template <typename Screen>
void stepper(Screen& screen, Work& work, fui::Rect box, const char* label, const char* value, int current, int count,
             int down, int up) {
  pixelui::text(screen.target(), fui::makeRect(box.x + 4, box.y, box.width * 2 / 3, 32), label, pixelui::kSmall);
  pixelui::text(screen.target(), fui::makeRect(box.x + box.width * 2 / 3, box.y, box.width / 3 - 4, 32), value, 3,
                fui::Color::Black, fui::TextAlign::Right);
  const int y = box.y + 36;
  button(screen, work, fui::makeRect(box.x, y, 44, 44), "-", down, current > 0);
  button(screen, work, fui::makeRect(box.right() - 44, y, 44, 44), "+", up, current < count - 1);
  const int bars = std::max(1, std::min(15, count)), width = (box.width - 112) / bars;
  for (int n = 0; n < bars; ++n) {
    const auto block = fui::makeRect(box.x + 56 + n * width, y + 14, std::max(2, width - 4), 14);
    if (n * count / bars <= current)
      screen.target().fill(block, fui::Paint::solid(fui::Color::Black));
    else
      screen.target().stroke(block, fui::Paint::solid(fui::Color::Black), 2);
  }
  screen.target().fill(fui::makeRect(box.x, box.bottom() - 2, box.width, 2), fui::Paint::solid(fui::Color::Black));
}
template <typename Screen>
void compactPager(Screen& screen, fui::Rect band, int page, int pages, fui::ActionId previous, int previousValue,
                  fui::ActionId next, int nextValue) {
  const auto g = pageui::layout(band);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  for (auto box : {g.before, g.count, g.after}) screen.target().stroke(box, ink, 2);
  for (int n = 0; n < 2; ++n) {
    const auto box = n ? g.after : g.before;
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

template <typename Screen>
void build(Screen& screen, const Model& model, Work& work) {
  const auto rect = sheetRect(screen.device(), model.fixed);
  fui::SheetProps sheet;
  sheet.anchor = fui::SheetEdge::Bottom;
  sheet.radius = 0;
  sheet.ruleWidth = 3;
  sheet.grabberHeight = sheet.grabberMargin = sheet.grabberInset = 0;
  sheet.dismissAction = 1;
  screen.sheet(sheet, rect.height);
  screen.insetContent(fui::Insets{4, 10, 8, 10});
  const auto grip = screen.takeTop(44);
  const int cx = grip.x + grip.width / 2, cy = grip.y + 20;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  screen.target().line({int16_t(cx - 16), int16_t(cy + 4)}, {int16_t(cx), int16_t(cy - 4)}, 3, ink);
  screen.target().line({int16_t(cx), int16_t(cy - 4)}, {int16_t(cx + 16), int16_t(cy + 4)}, 3, ink);
  screen.frame().hit(grip, 1, 0, fui::InputTouch);
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
    work.button.styles = pixelui::rowStyles();
    work.button.action = 1;
    work.button.label = tr(STR_DONE);
    work.button.text.font = pixelui::kBody;
    work.button.text.align = fui::TextAlign::Center;
    work.button.minTouchSize = 0;
    screen.button(work.button, close);
    const auto pager = screen.takeBottom(44, kSectionGap);
    const int capacity = fontRowsPerPage(screen.device());
    const int pages = std::max(1, (model.fontCount + capacity - 1) / capacity);
    compactPager(screen, pager, model.fontPage + 1, pages, 6, FontPrevious, 6, FontNext);
    const auto list = screen.body();
    const int first = model.fontPage * capacity;
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
      p.styles = pixelui::rowStyles();
      p.state = index == model.fontSelected ? fui::StateSelected : fui::StateNormal;
      p.action = 6;
      p.value = FontPick + index;
      p.minTouchSize = 0;
      screen.button(p, fui::makeRect(list.x, list.y + n * kFontRowHeight, list.width, kFontRowHeight));
    }
    return;
  }
  static constexpr StrId names[] = {StrId::STR_FONT, StrId::STR_LAYOUT, StrId::STR_TOOL_CONTENTS, StrId::STR_TOOL_MORE};
  int count = 0;
  static constexpr int order[] = {2, 0, 1, 3};
  for (int n : order) {
    if (model.fixed && (n == 0 || (n == 1 && !model.statusAvailable))) continue;
    if (n == 2 && !model.contentsAvailable) continue;
    pixelui::copyCaps(work.tabLabels[count], sizeof(work.tabLabels[count]), I18N.get(names[n]));
    work.tabs[count] = fui::tabItem(n, model.tab == n, true, work.tabLabels[count]);
    ++count;
  }
  pixelui::tabs(screen, work.tabs, count, 2);
  const auto done = screen.takeBottom(screen.device().width > screen.device().height ? 48 : 56, kSectionGap);
  work.button = {};
  auto& p = work.button;
  p.styles = pixelui::rowStyles();
  p.action = 1;
  pixelui::copyCaps(work.label, sizeof(work.label), tr(STR_DONE));
  p.label = work.label;
  p.text.font = pixelui::kBody;
  p.text.align = fui::TextAlign::Center;
  p.minTouchSize = 0;
  screen.button(p, done);
  if (!model.fixed && model.tab == 3 && model.view == SheetView::Style) {
    const auto heading = screen.takeTop(44, kSectionGap);
    button(screen, work, fui::makeRect(heading.x, heading.y, 48, heading.height), "", StyleBack);
    const int ax = heading.x + 14, ay = heading.y + heading.height / 2;
    screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 22), int16_t(ay)}, 3, ink);
    screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 10), int16_t(ay - 10)}, 3, ink);
    screen.target().line({int16_t(ax), int16_t(ay)}, {int16_t(ax + 10), int16_t(ay + 10)}, 3, ink);
    pixelui::text(screen.target(), fui::makeRect(heading.x + 60, heading.y, heading.width - 60, heading.height),
                  tr(STR_STYLE), pixelui::kSmall);
    const auto area = screen.body();
    const int cols = screen.device().width > screen.device().height ? 2 : 1;
    const int rows = 4 / cols;
    const int width = (area.width - (cols - 1) * kSectionGap) / cols;
    const int height = (area.height - (rows - 1) * kSectionGap) / rows;
    static constexpr StrId labels[] = {StrId::STR_FOCUS_READING, StrId::STR_HYPHENATION, StrId::STR_EMBEDDED_STYLE,
                                       StrId::STR_TEXT_AA};
    const bool checks[] = {model.focus, model.hyphenation, model.embedded, model.antialiasing};
    for (int n = 0; n < 4; ++n) {
      const auto option = fui::makeRect(area.x + n % cols * (width + kSectionGap),
                                        area.y + n / cols * (height + kSectionGap), width, height);
      screen.target().stroke(option, ink, 2);
      pixelui::copyCaps(work.label, sizeof(work.label), I18N.get(labels[n]));
      row(screen, work, option, work.label, "", Focus + n, true, checks[n]);
    }
    return;
  }
  if (model.tab == 2 || model.tab == 3) {
    const int capacity = sheetRowsPerPage(screen.device(), model.fixed, model.tab);
    const int pages = std::max(1, (model.listCount + capacity - 1) / capacity);
    const int page = std::clamp(model.listPage, 0, pages - 1);
    if (model.tab == 2 || pages > 1) {
      const auto band = screen.takeBottom(44, kSectionGap);
      compactPager(screen, band, page + 1, pages, 6, SheetPrevious, 6, SheetNext);
    }
    if (model.tab == 2) {
      const auto heading = screen.takeTop(34, kSectionGap);
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
    const int gap = kSectionGap;
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
      if (selected)
        screen.target().fill(box, ink);
      else
        screen.target().stroke(box, ink, 2);
      const auto fg = selected ? fui::Color::White : fui::Color::Black;
      const auto paint = fui::Paint::solid(fg);
      const int control = (model.tab == 2 ? ChapterPick : MorePick) + index;
      screen.frame().hit(box, 6, control, fui::InputTouch);
      if (model.tab == 2) {
        const int side = std::min(68, int(box.width) / 7);
        chapterNumber(index + 1, work.value, sizeof(work.value));
        pixelui::text(screen.target(), fui::makeRect(box.x + 4, box.y, side - 8, h), work.value, pixelui::kSmall, fg,
                      fui::TextAlign::Center);
        screen.target().fill(fui::makeRect(box.x + side, box.y + 8, 2, h - 16), paint);
        const auto title = fui::makeRect(box.x + side + 12, box.y + 6, box.width - side - 24, h - 12);
        fui::TextStyle chapterStyle;
        chapterStyle.font = 4;
        chapterStyle.color = fg;
        chapterStyle.maxLines = std::clamp(int(title.height) / screen.target().lineHeight(chapterStyle.font), 1, 2);
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
                                        &icon_check_32,       &icon_refresh_cw_32};
        const auto& mark = *icons[std::clamp(index, 0, 7)];
        const int side = std::min(36, h - 12);
        screen.target().bitmap(fui::makeRect(box.x + 16, box.y + (h - side) / 2, side, side), fui::bitmapFromIcon(mark),
                               fui::BitmapMode::Contain, paint);
        pixelui::text(screen.target(), fui::makeRect(box.x + side + 32, box.y + 8, box.width - side - 72, h - 16),
                      work.label, pixelui::kSmall);
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
      weight = box(1, controlHeight);
      size = box(2, controlHeight);
      size.y = content.y + controlHeight + controlGap;
      samples.y = size.y;
      samples.height = controlHeight;
    }
    row(screen, work, family, tr(STR_TEXT_FONT_FAMILY), model.family, Family);
    std::snprintf(work.label, sizeof(work.label), "%d pt", model.size);
    stepper(screen, work, size, tr(STR_TEXT_FONT_SIZE), work.label, model.sizeIndex, model.sizeCount, SizeDown, SizeUp);
    stepper(screen, work, weight, tr(STR_TEXT_FONT_WEIGHT),
            model.bold ? tr(STR_TEXT_WEIGHT_BOLD) : tr(STR_TEXT_WEIGHT_REGULAR), model.bold ? 1 : 0, 2, WeightDown,
            WeightUp);
    const int half = samples.width / 2;
    const int labelHeight = 22;
    const int sampleHeight = std::max(0, samples.height - labelHeight);
    for (int n = 0; n < 2; ++n) {
      const auto sample = fui::makeRect(samples.x + n * half, samples.y, half, sampleHeight);
      const auto choice = fui::makeRect(sample.x + 4, samples.y, half - 8, samples.height);
      screen.frame().hit(choice, 6, n == 0 ? WeightDown : WeightUp, fui::InputTouch);
      if (model.bold == (n == 1)) {
        screen.target().stroke(choice, ink, 2);
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
    const int h = 96;
    const int columnWidth = wide ? (content.width - kSectionGap) / 2 : content.width;
    auto spacing = fui::makeRect(content.x, content.y, columnWidth, h);
    stepper(screen, work, spacing, tr(STR_LINE_SPACING), model.spacingLabel, model.spacing, 4, SpacingDown, SpacingUp);
    std::snprintf(work.value, sizeof(work.value), "%d", model.margin);
    const auto margin = wide ? fui::makeRect(content.x + columnWidth + kSectionGap, content.y, columnWidth, h)
                             : fui::makeRect(content.x, content.y + h + kSectionGap, content.width, h);
    stepper(screen, work, margin, tr(STR_SCREEN_MARGIN), work.value, model.marginIndex, model.marginCount, MarginDown,
            MarginUp);
    const int alignmentY = content.y + (wide ? 1 : 2) * (h + kSectionGap);
    const auto a = fui::makeRect(content.x, alignmentY, content.width, 78);
    const int side = std::min(48, int(a.height) - 28);
    pixelui::text(screen.target(), fui::makeRect(a.x + 4, a.y, a.width - 3 * (side + 10) - 4, a.height - 12),
                  tr(STR_ALIGNMENT), pixelui::kSmall);
    for (int n = 0; n < 3; ++n) {
      const int align = n == 0 ? 1 : n == 1 ? 2 : 0;
      const auto r =
          fui::makeRect(a.right() - 3 * (side + 10) + n * (side + 10), a.y + (a.height - 12 - side) / 2, side, side);
      const bool active = model.alignmentIndex == align;
      if (active)
        screen.target().fill(r, ink);
      else
        screen.target().stroke(r, ink, 2);
      const auto stroke = fui::Paint::solid(active ? fui::Color::White : fui::Color::Black);
      for (int y = 0; y < 4; ++y) {
        const int w = (n == 2 || y % 2 == 0) ? side - 16 : side - 24;
        const int x = r.x + 8 + (n == 1 ? (side - 16 - w) / 2 : 0);
        screen.target().fill(fui::makeRect(x, r.y + 12 + y * 7, w, 3), stroke);
      }
      screen.frame().hit(r, 6, AlignmentLeft + n, fui::InputTouch);
    }
    screen.target().fill(fui::makeRect(a.x, a.bottom() - 1, a.width, 1), ink);
  }
}
}  // namespace readertext
