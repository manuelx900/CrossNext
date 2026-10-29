#include "ReadingStatsScreens.h"

#include <I18n.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../ui/Dashboard.h"
#include "../ui/ExtraIcons.h"
#include "../ui/PageIndicator.h"
#include "../ui/PixelMenu.h"
#include "../ui/StatisticsArt.h"
#include "../ui/ToolsDigits.h"
#include "../ui/ToyboxIcons.h"

namespace readingui {
namespace {
void localizeDecimal(char* number) {
  if (I18N.getLanguage() == Language::ES)
    if (char* point = std::strchr(number, '.')) *point = ',';
}

namespace fui = freeink::ui;
struct Context {
  toybox::Screen& screen;
  Work& work;
  auto& target() { return screen.target(); }
  const auto& device() { return screen.device(); }
  auto& frame() { return screen.frame(); }
  auto body() { return screen.body(); }
  auto takeTop(int height, int gap) { return screen.takeTop(height, gap); }
  void button(const fui::ButtonProps& props, fui::Rect rect) { screen.button(props, rect); }
};
const auto black = fui::Paint::solid(fui::Color::Black);
void text(Context& s, fui::Rect r, const char* value, fui::FontId font = pixelui::kSmall,
          fui::TextAlign align = fui::TextAlign::Center, fui::Color color = fui::Color::Black, int lines = 1,
          bool scaleDown = true, bool emphasis = false) {
  if (r.empty()) return;
  char copy[192];
  pixelui::copyCaps(copy, sizeof(copy), value);
  fui::TextStyle style;
  style.font = font;
  style.bold = emphasis;
  style.align = align;
  style.color = color;
  if (lines == 1) {
    static constexpr fui::FontId smaller[] = {3, 0, 1};
    while (scaleDown && style.font < 3 && s.target().measureText(style.font, copy, style).width > r.width)
      style.font = smaller[style.font];
    style.maxLines = 1;
    pixelui::fitLabel(s.target(), copy, r.width, style);
    s.target().text(toybox::inkCentred(s.target(), r, style), copy, style);
  } else {
    while (style.font < 3 && r.height < 2 * s.target().lineHeight(style.font) &&
           s.target().measureText(style.font, copy, style).width > r.width)
      style.font = style.font == 2 ? 1 : style.font == 1 ? 0 : 3;
    style.maxLines = std::max(1, std::min(lines, r.height / s.target().lineHeight(style.font)));
    pixelui::fitLabel(s.target(), copy, r.width, style);
    s.target().text(style.maxLines == 1 ? toybox::inkCentred(s.target(), r, style) : r, copy, style);
  }
}
void button(Context& s, fui::Rect r, const char* label, fui::ActionId action, int value = 0, bool selected = false) {
  const bool back = action == BackToCalendar || action == BackToBooks || action == BackToData;
  auto& p = s.work.button;
  p = {};
  pixelui::copyCaps(s.work.buttonLabel, sizeof(s.work.buttonLabel), label);
  p.label = label && !back ? s.work.buttonLabel : nullptr;
  p.action = action;
  p.value = value;
  p.text.font =
      s.target().measureText(pixelui::kSmall, s.work.buttonLabel, p.text).width > r.width - 12 ? 3 : pixelui::kSmall;
  p.text.align = fui::TextAlign::Center;
  p.styles = pixelui::buttonStyles();
  p.minTouchSize = 0;
  p.state = selected ? fui::StateSelected : fui::StateNormal;
  s.button(p, r);
  if (back) {
    dashboard::chevron(s.target(), fui::makeRect(r.x + 8, r.y, 24, r.height), selected, true);
    text(s, fui::makeRect(r.x + 44, r.y, r.width - 54, r.height), label, 0, fui::TextAlign::Left,
         selected ? fui::Color::White : fui::Color::Black);
  }
}
void frame(Context& s, fui::Rect r, int weight = pixelui::kBorder) { s.target().stroke(r, black, weight); }
void gridFrame(Context& s, fui::Rect r, int columns, int rows) {
  frame(s, r, 2);

  for (int n = 1; n < columns; ++n) {
    const int x = r.x + r.width * n / columns;
    s.target().fill(fui::makeRect(x, r.y, 2, r.height), black);
  }
  for (int n = 1; n < rows; ++n) {
    const int y = r.y + r.height * n / rows;
    s.target().fill(fui::makeRect(r.x, y, r.width, 2), black);
  }
}
fui::Rect cell(fui::Rect r, int n, int columns, int rows) {
  const int x = r.x + (n % columns) * r.width / columns, y = r.y + (n / columns) * r.height / rows;
  return fui::makeRect(x, y, r.x + (n % columns + 1) * r.width / columns - x,
                       r.y + (n / columns + 1) * r.height / rows - y);
}
void date(int64_t epoch, char* out, size_t size) {
  const time_t t = epoch;
  tm local{};
  if (epoch <= 0) {
    std::snprintf(out, size, "--");
    return;
  }
#if defined(_WIN32)
  if (localtime_s(&local, &t) != 0) {
    std::snprintf(out, size, "--");
    return;
  }
#else
  if (!localtime_r(&t, &local)) {
    std::snprintf(out, size, "--");
    return;
  }
#endif
  dashboard::date(out, size, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}
void art(Context& s, fui::Rect r, const fui::BitmapRef& bitmap) { statsart::draw(s.screen, r, bitmap); }
void prose(Context& s, fui::Rect r, const char* value, fui::TextAlign align = fui::TextAlign::Center, int font = 4) {
  fui::TextStyle style;
  style.font = font;
  style.align = align;
  style.maxLines = std::max(1, r.height / s.target().lineHeight(font));
  s.target().text(style.maxLines == 1 ? toybox::inkCentred(s.target(), r, style) : r, value, style);
}
void number(Context& s, fui::Rect r, const char* value) {
  const int cap = s.device().width > s.device().height ? 40 : 52;
  const int h = std::min(int(r.height), cap);
  r.y += (r.height - h) / 2;
  r.height = h;
  numerals::draw(s.target(), r, value);
}
void count(Context& s, fui::Rect r, uint64_t value) {
  char label[24];
  std::snprintf(label, sizeof(label), "%llu", static_cast<unsigned long long>(value));
  number(s, r, label);
}
void arrow(Context& s, fui::Rect r, fui::ActionId action, int value) {
  frame(s, r, 2);
  dashboard::chevron(s.target(), r, false, value < 0);
  dashboard::hit(s.screen, r, action, value);
}
// Digits share an ink height; translated units sit on their baseline.
void timeValue(Context& s, fui::Rect r, uint64_t seconds, bool precise = false, int maxInkHeight = 52) {
  char first[24], second[8];
  const bool hours = seconds >= 3600;
  const bool pair = hours || (precise && seconds % 60);
  std::snprintf(first, sizeof(first), "%llu", static_cast<unsigned long long>(hours ? seconds / 3600 : seconds / 60));
  std::snprintf(second, sizeof(second), "%02llu",
                static_cast<unsigned long long>(hours ? seconds / 60 % 60 : seconds % 60));
  const char* unit1 = hours ? tr(STR_STATS_UNIT_H) : tr(STR_STATS_UNIT_MIN);
  const char* unit2 = hours ? tr(STR_STATS_UNIT_MIN) : tr(STR_STATS_UNIT_S);
  fui::TextStyle style;
  const int font = r.height >= 64 && maxInkHeight > 40 ? 1 : 0;
  style.font = font;
  const int u1 = s.target().measureText(font, unit1, style).width;
  const int u2 = pair ? s.target().measureText(font, unit2, style).width : 0;
  const int digits = int(std::strlen(first) + (pair ? std::strlen(second) : 0));
  const int gap = r.height >= 64 ? 8 : 4;
  const int h = std::max(8, std::min(std::min(int(r.height), maxInkHeight),
                                     (r.width - u1 - u2 - gap * (pair ? 3 : 1)) * 10 / std::max(1, digits * 7)));
  const int w1 = int(std::strlen(first)) * h * 7 / 10, w2 = pair ? int(std::strlen(second)) * h * 7 / 10 : 0;
  const int width = w1 + u1 + gap + (pair ? w2 + u2 + 2 * gap : 0);
  int x = r.x + (r.width - width) / 2;
  const int top = r.y + (r.height - h) / 2;
  number(s, fui::makeRect(x, top, w1, h), first);
  x += w1 + gap;
  const int unitH = s.target().lineHeight(font);
  prose(s, fui::makeRect(x, top + h - unitH + 4, u1 + 2, unitH), unit1, fui::TextAlign::Left, font);
  if (!pair) return;
  x += u1 + gap;
  number(s, fui::makeRect(x, top, w2, h), second);
  prose(s, fui::makeRect(x + w2 + gap, top + h - unitH + 4, u2 + 2, unitH), unit2, fui::TextAlign::Left, font);
}
void timePanel(Context& s, fui::Rect r, const char* label, uint64_t seconds, bool today = false,
               bool labelAbove = false) {
  frame(s, r, 2);
  const auto safe = s.device().safeRect();
  const bool wide = safe.width > safe.height;
  const int valueHeight = wide ? 40 : 52;
  if (r.height < 140) {
    const int valueH = std::max(12, int(r.height) - 38), icon = std::min(32, valueH);
    art(s, fui::makeRect(r.x + 12, r.y + 6 + (valueH - icon) / 2, icon, icon), statsart::clock);
    timeValue(s, fui::makeRect(r.x + icon + 24, r.y + 6, r.width - icon - 36, valueH), seconds, false, valueHeight);
    text(s, fui::makeRect(r.x + 8, r.bottom() - 28, r.width - 16, 24), label, 3);
    return;
  }
  if (labelAbove && wide && r.height >= 190) {
    const int groupH = 146, availableH = r.height - (today ? 34 : 0);
    const int top = r.y + (availableH - groupH) / 2;
    art(s, fui::makeRect(r.x + (r.width - 36) / 2, top, 36, 36), statsart::clock);
    text(s, fui::makeRect(r.x + 8, top + 48, r.width - 16, 30), label, 0);
    timeValue(s, fui::makeRect(r.x + 20, top + 86, r.width - 40, 60), seconds, false, valueHeight);
    if (today) text(s, fui::makeRect(r.x + 8, r.bottom() - 34, r.width - 16, 28), tr(STR_TODAY), 0);
    return;
  }
  const int icon = std::min(40, r.height / 5), pad = r.height > 150 ? 14 : 8;
  art(s, fui::makeRect(r.x + (r.width - icon) / 2, r.y + pad, icon, icon), statsart::clock);
  const int labelH = 30;
  const int top = r.y + icon + pad + (labelAbove ? labelH + 4 : 10);
  const int bottom = r.bottom() - (today || !labelAbove ? 34 : 14);
  if (labelAbove) text(s, fui::makeRect(r.x + 8, r.y + icon + pad + 6, r.width - 16, labelH), label, 0);
  timeValue(s, fui::makeRect(r.x + 20, top, r.width - 40, std::max(12, bottom - top)), seconds, false, valueHeight);
  if (!labelAbove) text(s, fui::makeRect(r.x + 8, r.bottom() - 34, r.width - 16, 28), label, 0);
  if (today) text(s, fui::makeRect(r.x + 8, r.bottom() - 34, r.width - 16, 28), tr(STR_TODAY), 0);
}
void metric(Context& s, fui::Rect r, const char* label, const char* value, const fui::BitmapRef& icon,
            bool vertical = false, bool outlined = true) {
  if (outlined) frame(s, r, 2);
  const int side = std::min(38, r.height / (vertical ? 4 : 2));
  char caps[96];
  pixelui::copyCaps(caps, sizeof(caps), label);
  fui::TextStyle probe;
  probe.font = 3;
  const int labelH = s.target().measureText(3, caps, probe).width > r.width - 12 ? 42 : 26;
  fui::Rect valueArea;
  if (!icon.data) {
    valueArea = fui::makeRect(r.x + 10, r.y + 9, r.width - 20, r.height - labelH - 18);
  } else if (vertical) {
    art(s, fui::makeRect(r.x + (r.width - side) / 2, r.y + 10, side, side), icon);
    valueArea = fui::makeRect(r.x + 10, r.y + side + 17, r.width - 20, r.height - side - labelH - 22);
  } else {
    art(s, fui::makeRect(r.x + 12, r.y + std::max(4, (r.height - labelH - side) / 2), side, side), icon);
    valueArea = fui::makeRect(r.x + side + 22, r.y + 9, r.width - side - 34, r.height - labelH - 18);
  }
  number(s, valueArea, value);
  text(s, fui::makeRect(r.x + 6, r.bottom() - labelH, r.width - 12, labelH), label, 3, fui::TextAlign::Center,
       fui::Color::Black, 2);
}
void numberMetric(Context& s, fui::Rect r, const char* label, uint64_t value, const fui::BitmapRef& icon,
                  bool vertical = false, bool outlined = true) {
  char labelValue[24];
  std::snprintf(labelValue, sizeof(labelValue), "%llu", static_cast<unsigned long long>(value));
  metric(s, r, label, labelValue, icon, vertical, outlined);
}
void timeMetric(Context& s, fui::Rect r, const char* label, uint64_t seconds, bool precise = false,
                bool outlined = true) {
  if (outlined) frame(s, r, 2);
  const int side = std::min(38, r.height / 2);
  art(s, fui::makeRect(r.x + 12, r.y + std::max(4, (r.height - 28 - side) / 2), side, side), statsart::clock);
  const auto value = fui::makeRect(r.x + side + 22, r.y + 10, r.width - side - 34, std::max(16, r.height - 45));
  timeValue(s, value, seconds, precise, s.device().width > s.device().height ? 40 : 52);
  text(s, fui::makeRect(r.x + 6, r.bottom() - 28, r.width - 12, 24), label, 3);
}
void summary(Context& s, fui::Rect r, const Model& m) {
  const auto& v = *m.summary;
  const bool wide = r.width > r.height;
  const int gap = 10;
  const auto main = fui::makeRect(r.x, r.y, wide ? r.width / 3 - gap : r.width, wide ? r.height : r.height * 24 / 100);
  timePanel(s, main, tr(STR_STATS_TOTAL_MINUTES), v.seconds, false, true);
  if (m.today >= readingstats::dayIndex({2020, 1, 1})) {
    const auto action = fui::makeRect(main.right() - 54, main.y + 6, 46, 40);
    frame(s, action, 2);
    art(s, fui::makeRect(action.x + 8, action.y + 5, 30, 30), statsart::calendar);
    dashboard::hit(s.screen, main, ChoosePeriod);
  }
  const auto stats = wide ? fui::makeRect(main.right() + gap, r.y, r.right() - main.right() - gap, r.height * 46 / 100)
                          : fui::makeRect(r.x, main.bottom() + gap, r.width, r.height * 34 / 100);
  gridFrame(s, stats, 2, 2);
  numberMetric(s, cell(stats, 0, 2, 2), tr(STR_READING_SESSIONS), v.sessions, statsart::book, !wide, false);
  if (wide)
    timeMetric(s, cell(stats, 1, 2, 2), tr(STR_READING_AVERAGE), v.sessions ? v.seconds / v.sessions : 0, false, false);
  else {
    auto box = cell(stats, 1, 2, 2);
    art(s, fui::makeRect(box.x + (box.width - 36) / 2, box.y + 10, 36, 36), statsart::clock);
    timeValue(s, fui::makeRect(box.x + 12, box.y + 54, box.width - 24, box.height - 86),
              v.sessions ? v.seconds / v.sessions : 0);
    text(s, fui::makeRect(box.x + 4, box.bottom() - 28, box.width - 8, 24), tr(STR_READING_AVERAGE), 3);
  }
  numberMetric(s, cell(stats, 2, 2, 2), tr(STR_READING_PAGES), v.pages, wide ? fui::BitmapRef{} : statsart::page, !wide,
               false);
  char speed[24];
  std::snprintf(speed, sizeof(speed), "%.1f", v.seconds ? v.pages * 60.0 / v.seconds : 0.0);
  localizeDecimal(speed);
  metric(s, cell(stats, 3, 2, 2), tr(STR_READING_SPEED), speed, statsart::speed, !wide, false);
  const auto rest = wide ? fui::makeRect(stats.x, stats.bottom() + gap, stats.width, r.bottom() - stats.bottom() - gap)
                         : fui::makeRect(r.x, stats.bottom() + gap, r.width, r.bottom() - stats.bottom() - gap);
  const int headingH = wide ? 24 : 30;
  text(s, fui::makeRect(rest.x, rest.y, rest.width, headingH), tr(STR_STATS_CONSISTENCY), 0, fui::TextAlign::Left);
  const int finishedH = wide ? 36 : 46, streakTop = wide ? 28 : 34;
  const int streakH = rest.height - (wide ? streakTop : 40) - finishedH - gap;
  const auto streak = fui::makeRect(rest.x, rest.y + streakTop, rest.width, streakH);
  gridFrame(s, streak, 3, 1);
  numberMetric(s, cell(streak, 0, 3, 1), tr(STR_READING_CURRENT_STREAK), v.currentStreak(m.today), statsart::flame,
               !wide, false);
  numberMetric(s, cell(streak, 1, 3, 1), tr(STR_READING_BEST_STREAK), v.longest, statsart::trophy, !wide, false);
  numberMetric(s, cell(streak, 2, 3, 1), tr(STR_READING_DAYS), v.daysRead(), statsart::calendar, !wide, false);
  const auto finished = fui::makeRect(rest.x, rest.bottom() - finishedH, rest.width, finishedH);
  frame(s, finished, 2);
  art(s, fui::makeRect(finished.x + 10, finished.y + 6, 34, finishedH - 12), statsart::book);
  text(s, fui::makeRect(finished.x + 56, finished.y, finished.width - 115, finishedH), tr(STR_STATS_FINISHED_BOOKS), 0,
       fui::TextAlign::Left);
  count(s, fui::makeRect(finished.right() - 46, finished.y + 6, 36, finishedH - 12), v.completed);
}
void today(Context& s, fui::Rect r, const Model& m) {
  const bool wide = r.width > r.height;
  const auto dateValue = readingstats::dateFromDay(m.displayedDay ? m.displayedDay : m.today);
  char label[64];
  dashboard::date(label, sizeof(label), dateValue.year, dateValue.month, dateValue.day);
  text(s, fui::makeRect(r.x + 58, r.y, r.width - 116, 48), m.today ? label : tr(STR_READING_CLOCK), 1);
  if (m.today) {
    arrow(s, fui::makeRect(r.x, r.y, 48, 48), Day, -1);
    arrow(s, fui::makeRect(r.right() - 48, r.y, 48, 48), Day, 1);
  }
  r.y += 60;
  r.height -= 60;
  if (m.source == readingstats::Source::Unified) {
    if (m.dailyKnown)
      timePanel(s, fui::makeRect(r.x, r.y, r.width, r.height * 2 / 3), tr(STR_READING_TOTAL),
                m.periodStats ? m.periodStats->seconds : 0);
    else
      prose(s, fui::makeRect(r.x + 12, r.y + 16, r.width - 24, r.height / 2), tr(STR_READING_SYNC_NO_DAILY));
    prose(s, fui::makeRect(r.x + 12, r.y + r.height * 2 / 3 + 8, r.width - 24, r.height / 3 - 8),
          tr(STR_READING_SYNC_DETAILS));
    return;
  }
  const auto main =
      fui::makeRect(r.x, r.y, wide ? r.width * 38 / 100 - 6 : r.width, wide ? r.height : r.height * 37 / 100);
  const uint64_t seconds = m.periodStats ? m.periodStats->seconds : 0;
  timePanel(s, main, tr(STR_STATS_READING_MINUTES), seconds, m.displayedDay == m.today, true);
  const auto rest = wide ? fui::makeRect(main.right() + 12, r.y, r.right() - main.right() - 12, r.height)
                         : fui::makeRect(r.x, main.bottom() + 10, r.width, r.bottom() - main.bottom() - 10);
  const int metricH = rest.height * 39 / 100;
  const auto band = fui::makeRect(rest.x, rest.y, rest.width, metricH);
  numberMetric(s, cell(band, 0, 3, 1), tr(STR_READING_SESSIONS), m.periodStats ? m.periodStats->sessions : 0,
               statsart::sessions, !wide);
  numberMetric(s, cell(band, 1, 3, 1), tr(STR_READING_PAGES), m.periodStats ? m.periodStats->pages : 0,
               wide ? fui::BitmapRef{} : statsart::page, !wide);
  numberMetric(s, cell(band, 2, 3, 1), tr(STR_READING_FINISHED), m.periodStats ? m.periodStats->completed : 0,
               statsart::book, !wide);
  const auto session = fui::makeRect(rest.x, band.bottom() + 10, rest.width, rest.bottom() - band.bottom() - 10);
  frame(s, session, 2);
  const int icon = wide ? 32 : std::min(56, std::max(24, session.height - 134));
  art(s, fui::makeRect(wide ? session.x + 14 : session.x + (session.width - icon) / 2, session.y + 10, icon, icon),
      statsart::sparkBook);
  text(s,
       fui::makeRect(session.x + (wide ? 58 : 10), session.y + (wide ? 6 : icon + 18), session.width - (wide ? 70 : 20),
                     28),
       seconds ? tr(STR_STATS_KEEP_READING) : tr(STR_STATS_EMPTY_TITLE), 0);
  prose(s, fui::makeRect(session.x + 12, session.y + (wide ? 44 : icon + 48), session.width - 24, 28),
        tr(STR_STATS_EMPTY_BODY));
  dashboard::button(s.screen, fui::makeRect(session.x + 10, session.bottom() - 50, session.width - 20, 40),
                    tr(STR_CONTINUE_READING), ContinueReading, 0, true);
}
constexpr StrId months[] = {StrId::STR_READING_MONTH_01, StrId::STR_READING_MONTH_02, StrId::STR_READING_MONTH_03,
                            StrId::STR_READING_MONTH_04, StrId::STR_READING_MONTH_05, StrId::STR_READING_MONTH_06,
                            StrId::STR_READING_MONTH_07, StrId::STR_READING_MONTH_08, StrId::STR_READING_MONTH_09,
                            StrId::STR_READING_MONTH_10, StrId::STR_READING_MONTH_11, StrId::STR_READING_MONTH_12};
constexpr StrId weekdays[] = {StrId::STR_READING_MON, StrId::STR_READING_TUE, StrId::STR_READING_WED,
                              StrId::STR_READING_THU, StrId::STR_READING_FRI, StrId::STR_READING_SAT,
                              StrId::STR_READING_SUN};
void bars(Context& s, fui::Rect r, const Model& m, bool monthly) {
  frame(s, r, 2);
  const int titleH = 40;
  art(s, fui::makeRect(r.x + 10, r.y + 6, 24, 24), monthly ? statsart::calendar : statsart::sessions);
  text(s, fui::makeRect(r.x + 48, r.y, r.width - (monthly ? 94 : 130), titleH),
       monthly ? tr(STR_READING_MONTHLY) : tr(STR_STATS_WEEKLY_READING), 0, fui::TextAlign::Left);
  if (!monthly)
    prose(s, fui::makeRect(r.right() - 84, r.y + 7, 32, titleH - 14), tr(STR_STATS_UNIT_MIN), fui::TextAlign::Center,
          3);
  s.target().line({r.x, int16_t(r.y + titleH)}, {r.right(), int16_t(r.y + titleH)}, 2, black);
  const auto current = readingstats::dateFromDay(m.today);
  const int currentMonth = current.year * 12 + current.month - 1;
  const int count = monthly ? 6 : 7;
  uint64_t values[8]{};
  uint64_t maximum = 1;
  for (int n = 0; n < count; ++n) {
    if (monthly) {
      const int at = currentMonth - (count - 1 - n);
      for (int d = 1; d <= readingstats::daysInMonth(at / 12, at % 12 + 1); ++d)
        values[n] += m.summary->onDay(readingstats::dayIndex({at / 12, at % 12 + 1, d})) > 0;
    } else {
      const int day = readingstats::weekStart(m.today, m.weekStartsSunday) + n;
      values[n] = m.summary->onDay(day) / 60;
    }
    maximum = std::max(maximum, values[n]);
  }
  const auto select = monthly ? SelectMonth : SelectChartDay;
  const int chosen = monthly ? readingstats::dayIndex({currentMonth / 12, currentMonth % 12 + 1, 1})
                             : readingstats::weekStart(m.today, m.weekStartsSunday);
  arrow(s, fui::makeRect(r.right() - 44, r.y, 44, titleH), monthly ? SelectMonth : SelectWeek,
        std::min(m.today, chosen));
  const int left = r.x + 42, base = r.bottom() - 28, top = r.y + titleH + 30, height = std::max(6, base - top),
            width = (r.right() - left - 10) / count;
  s.target().line({int16_t(left), int16_t(top)}, {int16_t(left), int16_t(base)}, 2, black);
  s.target().line({int16_t(left), int16_t(base)}, {int16_t(r.right() - 10), int16_t(base)}, 2, black);
  for (int n = 0; n < 3; ++n) {
    char label[16];
    std::snprintf(label, sizeof(label), "%llu", static_cast<unsigned long long>(maximum * n / 2));
    text(s, fui::makeRect(r.x + 3, base - height * n / 2 - 10, 34, 20), label, 3, fui::TextAlign::Right);
  }
  for (int n = 0; n < count; ++n) {
    const int x = left + n * width, bh = values[n] ? std::max(2, int(height * values[n] / maximum)) : 0;
    if (bh) s.target().fill(fui::makeRect(x + width / 5, base - bh, width * 3 / 5, bh), black);
    char label[24];
    const int at = currentMonth - (count - 1 - n);
    const int start = monthly ? readingstats::dayIndex({at / 12, at % 12 + 1, 1})
                              : readingstats::weekStart(m.today, m.weekStartsSunday) + n;
    if (monthly || start <= m.today)
      dashboard::hit(s.screen, fui::makeRect(x, r.y + titleH, width, r.height - titleH), select, start);
    if (monthly) {
      char full[40];
      dashboard::date(full, sizeof(full), at / 12, at % 12 + 1, 1);
      const char* begin = full + 3;
      const char* end = begin;
      while (*end && *end != ' ') ++end;
      std::snprintf(label, sizeof(label), "%.*s", int(end - begin), begin);
    } else
      std::snprintf(label, sizeof(label), "%s", I18N.get(weekdays[(n + (m.weekStartsSunday ? 6 : 0)) % 7]));
    text(s, fui::makeRect(x, base + 2, width, 24), label, 3);
    char amount[20];
    std::snprintf(amount, sizeof(amount), "%llu", static_cast<unsigned long long>(values[n]));
    text(s, fui::makeRect(x, base - bh - 22, width, 20), amount, 3);
  }
}
void periodPicker(Context& s, fui::Rect r, const Model& m) {
  button(s, fui::makeRect(r.x, r.y, r.width, 44), tr(STR_STATS_BACK_DATA), BackToData);
  r.y += 50;
  r.height -= 50;
  const bool wide = r.width > r.height;
  char label[20];
  std::snprintf(label, sizeof(label), "%d", m.pickerYear);
  arrow(s, fui::makeRect(r.x, r.y, 48, 44), PickerYear, -1);
  text(s, fui::makeRect(r.x + 56, r.y, r.width - 112, 44), label, 1);
  arrow(s, fui::makeRect(r.right() - 48, r.y, 48, 44), PickerYear, 1);
  const int cols = wide ? 4 : 3, rows = 12 / cols, gap = 8;
  const int h = std::min(76, (r.height - 134 - gap * (rows - 1)) / rows), w = (r.width - gap * (cols - 1)) / cols;
  const auto current = readingstats::dateFromDay(m.today);
  for (int n = 0; n < 12; ++n) {
    const auto box = fui::makeRect(r.x + n % cols * (w + gap), r.y + 56 + n / cols * (h + gap), w, h);
    if (m.pickerYear < current.year || n < current.month)
      button(s, box, I18N.get(months[n]), PickerMonth, n + 1);
    else {
      frame(s, box, 1);
      text(s, box, I18N.get(months[n]), 3, fui::TextAlign::Center, fui::Color::DarkGray);
    }
  }
  std::snprintf(label, sizeof(label), "%d", m.pickerYear);
  button(s, fui::makeRect(r.x, r.bottom() - 60, r.width, 60), tr(STR_STATS_WHOLE_YEAR), SelectYear);
}
void calendar(Context& s, fui::Rect r, const Model& m) {
  const bool wide = r.width > r.height;
  const auto cal = fui::makeRect(r.x, r.y, wide ? r.width / 2 - 6 : r.width, wide ? r.height : r.height * 51 / 100);
  frame(s, cal, 2);
  const int year = m.month / 12, month = m.month % 12 + 1;
  char label[48];
  std::snprintf(label, sizeof(label), "%s %d", I18N.get(months[month - 1]), year);
  text(s, fui::makeRect(cal.x + 56, cal.y + 8, cal.width - 112, 44), label, 1);
  dashboard::hit(s.screen, fui::makeRect(cal.x + 56, cal.y + 8, cal.width - 112, 44), SelectMonth,
                 std::min(m.today, readingstats::dayIndex({year, month, 1})));
  arrow(s, fui::makeRect(cal.x + 8, cal.y + 8, 44, 40), Page, -1);
  arrow(s, fui::makeRect(cal.right() - 52, cal.y + 8, 44, 40), Page, 1);
  const auto area = fui::makeRect(cal.x + 8, cal.y + 56, cal.width - 16, cal.height - 92);
  s.work.calendarGrid = area;
  dashboard::hit(s.screen, fui::makeRect(area.x, area.y + area.height / 7, area.width, area.height - area.height / 7),
                 SelectDay);
  constexpr StrId initials[] = {StrId::STR_STATS_WEEKDAY_0, StrId::STR_STATS_WEEKDAY_1, StrId::STR_STATS_WEEKDAY_2,
                                StrId::STR_STATS_WEEKDAY_3, StrId::STR_STATS_WEEKDAY_4, StrId::STR_STATS_WEEKDAY_5,
                                StrId::STR_STATS_WEEKDAY_6};
  for (int n = 0; n < 7; ++n)
    text(s, cell(area, n, 7, 7), I18N.get(initials[(n + (m.weekStartsSunday ? 6 : 0)) % 7]), 0);
  const int first = readingstats::dayIndex({year, month, 1}),
            offset = (readingstats::weekday(first) + (m.weekStartsSunday ? 1 : 0)) % 7;
  for (int d = 1; d <= readingstats::daysInMonth(year, month); ++d) {
    const auto box = cell(area, 7 + offset + d - 1, 7, 7);
    const uint32_t seconds = m.summary->onDay(first + d - 1);
    const auto inner = box.inset(fui::Insets{2, 2, 2, 2});
    const int importedOffset = m.importedAnchor - (first + d - 1);
    const bool activeImported = m.importedActive && importedOffset >= 0 &&
                                importedOffset < readingstats::kHistoryDays &&
                                (m.importedActive[importedOffset / 8] & (1u << (importedOffset % 8)));
    if (seconds || activeImported)
      s.target().fill(inner, seconds >= 1800 ? black : fui::Paint::dither(fui::Color::LightGray));
    if (first + d - 1 == m.today) {
      frame(s, box, 2);
      frame(s, inner.inset(fui::Insets{2, 2, 2, 2}), 1);
    }
    std::snprintf(label, sizeof(label), "%d", d);
    text(s, inner, label, 0, fui::TextAlign::Center, seconds >= 1800 ? fui::Color::White : fui::Color::Black);
  }
  const StrId ranges[] = {StrId::STR_STATS_NO_READING, StrId::STR_STATS_UNDER30, StrId::STR_STATS_30PLUS};
  for (int n = 0; n < 3; ++n) {
    const int w = (cal.width - 16) / 3, x = cal.x + 8 + n * w;
    const auto box = fui::makeRect(x, cal.bottom() - 26, 18, 18);
    if (n) s.target().fill(box, n == 1 ? fui::Paint::dither(fui::Color::LightGray) : black);
    frame(s, box, 1);
    prose(s, fui::makeRect(x + 24, cal.bottom() - 28, w - 26, 24), I18N.get(ranges[n]), fui::TextAlign::Left, 3);
  }
  const auto charts = wide ? fui::makeRect(cal.right() + 12, r.y, r.right() - cal.right() - 12, r.height)
                           : fui::makeRect(r.x, cal.bottom() + 12, r.width, r.bottom() - cal.bottom() - 12);
  const int half = (charts.height - 12) / 2;
  bars(s, fui::makeRect(charts.x, charts.y, charts.width, half), m, false);
  bars(s, fui::makeRect(charts.x, charts.y + half + 12, charts.width, half), m, true);
}
void periodChart(Context& s, fui::Rect r, const Model& m, bool daily) {
  frame(s, r, 2);
  text(s, fui::makeRect(r.x + 12, r.y + 4, r.width - 24, 30),
       m.period == readingstats::Period::Year ? tr(STR_STATS_MONTH_FINISHED)
       : daily                                ? tr(STR_STATS_TIME_OF_DAY)
                                              : tr(STR_STATS_DAILY_READING),
       0, fui::TextAlign::Left);
  const bool annual = m.period == readingstats::Period::Year;
  const int count = annual ? 12 : daily ? 6 : 7;
  uint64_t maximum = annual ? 2 : 60;
  for (int n = 0; n < count; ++n)
    maximum = std::max(maximum, annual  ? uint64_t(m.periodStats->annualCompleted[n])
                                : daily ? m.periodStats->hours[n]
                                        : m.periodStats->dailySeconds[n]);
  if (annual) maximum = (maximum + 1) / 2 * 2;
  const int left = r.x + (daily ? 10 : 34), base = r.bottom() - 30, height = std::max(8, r.height - 97),
            width = (r.right() - left - 10) / count;
  s.target().line({int16_t(left), int16_t(base)}, {int16_t(r.right() - 10), int16_t(base)}, 2, black);
  if (!daily) {
    s.target().line({int16_t(left), int16_t(base - height)}, {int16_t(left), int16_t(base)}, 2, black);
    for (int n = 0; n < 3; ++n) {
      char label[16];
      std::snprintf(label, sizeof(label), "%llu", static_cast<unsigned long long>(maximum * n / (annual ? 2 : 120)));
      const auto tick = fui::makeRect(r.x + 2, base - height * n / 2 - 10, 26, 20);
      if (annual)
        dashboard::digits(s.screen, tick, label);
      else
        text(s, tick, label, 3, fui::TextAlign::Right);
    }
  }
  for (int n = 0; n < count; ++n) {
    const uint64_t seconds = annual  ? m.periodStats->annualCompleted[n]
                             : daily ? m.periodStats->hours[n]
                                     : m.periodStats->dailySeconds[n];
    const int x = left + n * width, bh = seconds ? std::max(2, int(height * seconds / maximum)) : 0;
    if (bh) s.target().fill(fui::makeRect(x + width / 6, base - bh, width * 2 / 3, bh), black);
    char label[24];
    std::snprintf(label, sizeof(label), daily ? tr(STR_STATS_MINUTE_VALUE) : "%llu",
                  static_cast<unsigned long long>(annual ? seconds : seconds / 60));
    const auto value = fui::makeRect(x, base - bh - 24, width, 22);
    if (annual)
      dashboard::digits(s.screen, value, label);
    else
      text(s, value, label, 3);
    if (annual)
      std::snprintf(label, sizeof(label), "%d", n + 1);
    else if (daily)
      std::snprintf(label, sizeof(label), "%02d–%02d", n * 4, (n + 1) * 4);
    else
      std::snprintf(label, sizeof(label), "%s", I18N.get(weekdays[readingstats::weekday(m.periodStats->first + n)]));
    text(s, fui::makeRect(x, base + 3, width, 24), label, 3);
  }
}
void periodDetails(Context& s, fui::Rect r, const Model& m) {
  if (!m.periodStats) return;
  const auto& v = *m.periodStats;
  const bool wide = r.width > r.height;
  const int backWidth = wide ? r.width / 2 - 6 : r.width;
  button(s, fui::makeRect(r.x, r.y, backWidth, 44),
         m.periodFromPicker ? tr(STR_STATS_BACK_PERIODS) : tr(STR_STATS_BACK_CALENDAR), BackToCalendar);
  if (m.source == readingstats::Source::Unified) {
    r.y += 52;
    r.height -= 52;
    if (m.dailyKnown)
      timePanel(s, fui::makeRect(r.x, r.y, r.width, r.height * 2 / 3), tr(STR_READING_TOTAL), v.seconds);
    else
      prose(s, fui::makeRect(r.x + 12, r.y + 20, r.width - 24, r.height / 2), tr(STR_READING_SYNC_NO_DAILY));
    prose(s, fui::makeRect(r.x + 12, r.y + r.height * 2 / 3 + 8, r.width - 24, r.height / 3 - 8),
          tr(STR_READING_SYNC_DETAILS));
    return;
  }
  if (!wide) {
    r.y += 50;
    r.height -= 50;
  }
  const auto first = readingstats::dateFromDay(v.first), last = readingstats::dateFromDay(v.last);
  char label[80], subtitle[40]{};
  switch (m.period) {
    case readingstats::Period::Calendar:
      return;
    case readingstats::Period::Day: {
      dashboard::date(label, sizeof(label), first.year, first.month, first.day);
      constexpr StrId names[] = {StrId::STR_STATS_MONDAY,   StrId::STR_STATS_TUESDAY, StrId::STR_STATS_WEDNESDAY,
                                 StrId::STR_STATS_THURSDAY, StrId::STR_STATS_FRIDAY,  StrId::STR_STATS_SATURDAY,
                                 StrId::STR_STATS_SUNDAY};
      std::snprintf(subtitle, sizeof(subtitle), "%s", I18N.get(names[readingstats::weekday(v.first)]));
      break;
    }
    case readingstats::Period::Week: {
      char end[24];
      dashboard::date(end, sizeof(end), last.year, last.month, last.day);
      if (first.year == last.year && first.month == last.month)
        std::snprintf(label, sizeof(label), "%d – %s", first.day, end);
      else {
        char start[24];
        dashboard::date(start, sizeof(start), first.year, first.month, first.day);
        std::snprintf(label, sizeof(label), "%s - %s", start, end);
      }
      std::snprintf(subtitle, sizeof(subtitle), tr(STR_STATS_WEEK_NUMBER), readingstats::isoWeek(v.first + 3));
      break;
    }
    case readingstats::Period::Year:
      std::snprintf(label, sizeof(label), "%d", first.year);
      break;
    case readingstats::Period::Month:
      std::snprintf(label, sizeof(label), "%s %d", I18N.get(months[first.month - 1]), first.year);
      break;
  }
  const int tx = wide ? r.x + backWidth + 12 : r.x, tw = r.right() - tx,
            titleH = wide ? 56 : (m.period == readingstats::Period::Month ? 56 : 82);
  arrow(s, fui::makeRect(tx, r.y, 44, 44), PeriodPage, -1);
  arrow(s, fui::makeRect(r.right() - 44, r.y, 44, 44), PeriodPage, 1);
  text(s, fui::makeRect(tx + 52, r.y, tw - 104, 44), label, 1);
  if (*subtitle && !wide) prose(s, fui::makeRect(tx + 52, r.y + 42, tw - 104, 28), subtitle, fui::TextAlign::Center, 0);
  r.y += titleH;
  r.height -= titleH;
  const auto metrics =
      wide ? fui::makeRect(r.x, r.y, r.width / 2 - 6, r.height)
           : fui::makeRect(r.x, r.y, r.width, r.height * (m.period == readingstats::Period::Month ? 62 : 66) / 100);
  const int mainH = metrics.height * (wide ? 35 : 51) / 100, gap = 10;
  timePanel(s, fui::makeRect(metrics.x, metrics.y, metrics.width, mainH), tr(STR_READING_TIME), v.seconds);
  const auto cards = fui::makeRect(metrics.x, metrics.y + mainH + gap, metrics.width, metrics.height - mainH - gap);
  const int w = (cards.width - gap) / 2, h = (cards.height - gap) / 2;
  numberMetric(s, fui::makeRect(cards.x, cards.y, w, h), tr(STR_READING_SESSIONS), v.sessions, statsart::sessions);
  numberMetric(s, fui::makeRect(cards.x + w + gap, cards.y, w, h), tr(STR_READING_PAGES), v.pages,
               wide ? fui::BitmapRef{} : statsart::page);
  numberMetric(s, fui::makeRect(cards.x, cards.y + h + gap, w, h), tr(STR_STATS_FINISHED_BOOKS), v.completed,
               statsart::book);
  const int through = std::min(v.last, m.today);
  const int days = v.daysReadThrough(through);
  const auto fourth = fui::makeRect(cards.x + w + gap, cards.y + h + gap, w, h);
  if (m.period == readingstats::Period::Day)
    timeMetric(s, fourth, tr(STR_READING_AVERAGE), v.sessions ? v.seconds / v.sessions : 0, true);
  else {
    std::snprintf(label, sizeof(label), m.period == readingstats::Period::Week ? "%d / 7" : "%d", days);
    metric(s, fourth, tr(STR_READING_DAYS), label, statsart::calendar);
  }
  const auto rest = wide ? fui::makeRect(metrics.right() + 12, r.y, r.right() - metrics.right() - 12, r.height)
                         : fui::makeRect(r.x, metrics.bottom() + 12, r.width, r.bottom() - metrics.bottom() - 12);
  if (m.period != readingstats::Period::Month) {
    periodChart(s, rest, m, m.period == readingstats::Period::Day);
    return;
  }
  text(s, fui::makeRect(rest.x, rest.y, rest.width, 28), tr(STR_STATS_CONSISTENCY), 0, fui::TextAlign::Left);
  const int streakH = std::max(58, (rest.height - 40) * 36 / 100);
  const auto streak = fui::makeRect(rest.x, rest.y + 32, rest.width, streakH);
  frame(s, streak, 2);
  art(s, fui::makeRect(streak.x + 14, streak.y + 10, 36, 36), statsart::flame);
  count(s, fui::makeRect(streak.x + 80, streak.y + 4, 70, streak.height - 30), v.consistency.days());
  prose(s, fui::makeRect(streak.x + 165, streak.y + 5, 70, 26), tr(STR_READING_DAYS_UNIT), fui::TextAlign::Left, 0);
  text(s, fui::makeRect(streak.x + 80, streak.bottom() - 24, streak.width - 92, 22), tr(STR_STATS_STREAK_END), 3,
       fui::TextAlign::Left);
  const auto strip = fui::makeRect(rest.x, rest.y + 42 + streakH, rest.width, rest.height - 42 - streakH);
  frame(s, strip, 2);
  art(s, fui::makeRect(strip.x + 14, strip.y + 6, 28, 28), statsart::calendar);
  std::snprintf(label, sizeof(label), "%d / %d", days, std::max(0, through - v.first + 1));
  number(s, fui::makeRect(strip.x + 68, strip.y + 8, 100, 30), label);
  text(s, fui::makeRect(strip.x + 184, strip.y + 8, strip.width - 196, 32), tr(STR_STATS_DAYS_TO_DATE), 3,
       fui::TextAlign::Left);
  const int total = v.last - v.first + 1, step = (strip.width - 28) / total, side = std::min(13, step - 3),
            y = strip.bottom() - 48;
  for (int d = 0; d < total; ++d) {
    const auto dot = fui::makeRect(strip.x + 14 + d * step, y, side, 13);
    if (v.dailySeconds[d])
      s.target().fill(dot, black);
    else
      frame(s, dot, 1);
  }
  char dateValue[32];
  const auto cutoff = readingstats::dateFromDay(through);
  dashboard::date(dateValue, sizeof(dateValue), cutoff.year, cutoff.month, cutoff.day);
  std::snprintf(label, sizeof(label), tr(STR_STATS_AS_OF), dateValue);
  prose(s, fui::makeRect(strip.x + 14, strip.bottom() - 28, strip.width - 28, 24), label, fui::TextAlign::Left);
}
void bookDetails(Context& s, fui::Rect r, const Model& m) {
  const auto& b = *m.detail;
  const bool wide = r.width > r.height;
  if (!m.bookOnly) {
    button(s, fui::makeRect(r.x, r.y, wide ? 240 : r.width, 44), tr(STR_STATS_BACK_BOOKS), BackToBooks);
    r.y += 50;
    r.height -= 50;
  }
  const auto whole = r;
  if (wide) r.width = r.width * 38 / 100;
  const int side = wide ? 54 : 80, titleH = wide ? 72 : 88;
  art(s, fui::makeRect(r.x + 8, r.y + 2, side, side), statsart::openBook);
  text(s, fui::makeRect(r.x + side + 20, r.y, r.width - side - 24, titleH - 34), b.title, wide ? 0 : 1,
       fui::TextAlign::Left, fui::Color::Black, 2, true, true);
  const auto chip = fui::makeRect(r.x + side + 20, r.y + titleH - 32, std::min(136, r.width - side - 24), 28);
  frame(s, chip, 1);
  prose(s, chip, b.completed ? tr(STR_STATS_FINISHED) : tr(STR_STATS_IN_PROGRESS), fui::TextAlign::Center, 0);
  r.y += titleH;
  r.height -= titleH;
  char label[64];
  if (m.source == readingstats::Source::Unified && !b.completed)
    std::snprintf(label, sizeof(label), "--");
  else
    std::snprintf(label, sizeof(label), "%u%%", b.progressBp / 100);
  prose(s, fui::makeRect(r.x, r.y, r.width - 70, 28), tr(STR_READING_PROGRESS), fui::TextAlign::Left, 0);
  prose(s, fui::makeRect(r.right() - 70, r.y, 70, 28), label, fui::TextAlign::Right, 0);
  dashboard::bar(s.screen, fui::makeRect(r.x, r.y + 32, r.width, 20), b.progressBp, 10000);
  r.y += 60;
  r.height -= 60;
  const auto metrics = wide ? fui::makeRect(r.right() + 12, whole.y, whole.right() - r.right() - 12, whole.height)
                            : fui::makeRect(r.x, r.y, r.width, r.height * 65 / 100);
  const int mainH = metrics.height * 40 / 100, gap = 10;
  timeMetric(s, fui::makeRect(metrics.x, metrics.y, metrics.width, mainH), tr(STR_READING_TOTAL), b.seconds);
  const auto cards = fui::makeRect(metrics.x, metrics.y + mainH + gap, metrics.width, metrics.height - mainH - gap);
  const int h = (cards.height - gap) / 2, w = (cards.width - gap) / 2;
  numberMetric(s, fui::makeRect(cards.x, cards.y, w, h), tr(STR_READING_SESSIONS), b.sessions, statsart::sessions);
  timeMetric(s, fui::makeRect(cards.x + w + gap, cards.y, w, h), tr(STR_READING_AVERAGE),
             b.sessions ? b.seconds / b.sessions : 0, true);
  std::snprintf(label, sizeof(label), "%.1f", b.seconds ? b.pages * 60.0 / b.seconds : 0.0);
  localizeDecimal(label);
  metric(s, fui::makeRect(cards.x, cards.y + h + gap, w, h), tr(STR_READING_SPEED), label, statsart::book);
  const auto remaining = fui::makeRect(cards.x + w + gap, cards.y + h + gap, w, h);
  if (b.completed || b.progressBp)
    timeMetric(s, remaining, tr(STR_READING_TIME_LEFT),
               b.completed ? 0
                           : b.seconds / b.progressBp * (10000 - b.progressBp) +
                                 b.seconds % b.progressBp * (10000 - b.progressBp) / b.progressBp);
  else
    metric(s, remaining, tr(STR_READING_TIME_LEFT), "--", statsart::clock);
  const auto dates = wide ? r : fui::makeRect(r.x, metrics.bottom() + 12, r.width, r.bottom() - metrics.bottom() - 12);
  text(s, fui::makeRect(dates.x, dates.y, dates.width, 28), tr(STR_STATS_READING_PERIOD), 0, fui::TextAlign::Left);
  const int rowH = (dates.height - 36) / 2;
  gridFrame(s, fui::makeRect(dates.x, dates.y + 36, dates.width, 2 * rowH), 1, 2);
  for (int n = 0; n < 2; ++n) {
    const auto box = fui::makeRect(dates.x, dates.y + 36 + n * rowH, dates.width, rowH);
    const char* caption = n ? (b.completed ? tr(STR_STATS_FINISHED) : tr(STR_STATS_LAST_READ)) : tr(STR_STATS_STARTED);
    date(n ? (b.completed ? b.finishedEpoch : b.lastEpoch) : b.firstEpoch, label, sizeof(label));
    if (wide) {
      const bool tall = box.height >= 66;
      const int captionH = tall ? 28 : 18, dateH = tall ? 28 : 24, gap = tall ? 4 : 2;
      const int top = box.y + (box.height - captionH - gap - dateH) / 2, icon = tall ? 32 : 24;
      art(s, fui::makeRect(box.x + 12, box.y + (box.height - icon) / 2, icon, icon), statsart::calendar);
      prose(s, fui::makeRect(box.x + 56, top, box.width - 68, captionH), caption, fui::TextAlign::Left, tall ? 4 : 3);
      text(s, fui::makeRect(box.x + 56, top + captionH + gap, box.width - 68, dateH), label, 0, fui::TextAlign::Left);
    } else {
      art(s, fui::makeRect(box.x + 14, box.y + (box.height - 32) / 2, 32, 32), statsart::calendar);
      prose(s, fui::makeRect(box.x + 64, box.y + 4, box.width - 72, 22), caption, fui::TextAlign::Left);
      text(s, fui::makeRect(box.x + 64, box.y + 24, box.width - 72, box.height - 26), label, 0, fui::TextAlign::Left);
    }
  }
}
void bookMinutes(Context& s, fui::Rect r, uint64_t seconds) {
  char label[24];
  std::snprintf(label, sizeof(label), "%llu", static_cast<unsigned long long>(seconds / 60));
  const int unitH = std::min(18, int(r.height) / 2);
  const int digitH = std::min(24, std::max(1, int(r.height) - unitH - 8));
  const int y = r.y + (r.height - digitH - unitH - 2) / 2;
  number(s, fui::makeRect(r.x, y, r.width, digitH), label);
  prose(s, fui::makeRect(r.x, y + digitH + 2, r.width, unitH), tr(STR_STATS_UNIT_MIN), fui::TextAlign::Center, 3);
}
void books(Context& s, fui::Rect r, const Model& m) {
  const bool finished = m.filter == readingstats::BookFilter::Finished, wide = r.width > r.height;
  const int half = r.width / 2, filterH = wide ? 42 : 48;
  button(s, fui::makeRect(r.x, r.y, half, filterH), tr(STR_STATS_FINISHED_FILTER), Filter, 0, finished);
  button(s, fui::makeRect(r.x + half, r.y, r.width - half, filterH),
         m.source == readingstats::Source::Unified ? tr(STR_READING_SYNC_UNFINISHED) : tr(STR_LIBRARY_TAB_RECENT),
         Filter, 1, !finished);
  r.y += filterH + 14;
  r.height -= filterH + 14;
  if (!wide) {
    text(s, fui::makeRect(r.x, r.y, r.width - 48, 32),
         finished                                    ? tr(STR_STATS_FINISHED_BOOKS)
         : m.source == readingstats::Source::Unified ? tr(STR_READING_SYNC_UNFINISHED)
                                                     : tr(STR_STATS_RECENT_BOOKS),
         1, fui::TextAlign::Left);
    count(s, fui::makeRect(r.right() - 40, r.y + 2, 32, 28), m.bookTotal);
    s.target().line({r.x, int16_t(r.y + 38)}, {r.right(), int16_t(r.y + 38)}, 2, black);
    r.y += 50;
    r.height -= 50;
  }
  if (!m.count) {
    art(s, fui::makeRect(r.x + (r.width - 80) / 2, r.y + 20, 80, 80), statsart::sparkBook);
    prose(s, fui::makeRect(r.x + 12, r.y + 120, r.width - 24, 70),
          finished ? tr(STR_STATS_NO_BOOKS) : tr(STR_STATS_NO_RECENT));
    return;
  }
  const int columns = 1, rows = wide ? 3 : 5, gap = 10, footerH = 44;
  const int rowH = std::min(108, (r.height - footerH - 16) / rows), rowW = (r.width - (columns - 1) * gap) / columns;
  for (int n = 0; n < m.count; ++n) {
    const auto row =
        fui::makeRect(r.x + (n % columns) * (rowW + gap), r.y + (n / columns) * rowH, rowW, rowH - (wide ? 8 : 10));
    const auto& b = m.books[n];
    frame(s, row, 2);
    dashboard::hit(s.screen, row, OpenBook, n);
    const int side = wide ? 24 : 32, timeW = wide ? 110 : 120, divider = row.right() - timeW;
    art(s, fui::makeRect(row.x + 14, row.y + (row.height - side) / 2, side, side), statsart::closedBook);
    const int x = row.x + side + 28, width = divider - x - 10;
    const int titleY = wide ? row.y + (row.height - 38) / 2 : row.y + 8;
    text(s, fui::makeRect(x, titleY, width, wide ? 18 : row.height / 2 - 8), b.title, wide ? 3 : 0,
         fui::TextAlign::Left, fui::Color::Black, 1, false);
    char label[48], dateValue[32];
    date(finished ? b.finishedEpoch : b.lastEpoch, dateValue, sizeof(dateValue));
    if (finished)
      std::snprintf(label, sizeof(label), "%s", dateValue);
    else
      std::snprintf(label, sizeof(label), tr(STR_STATS_LAST_READ_DATE), dateValue);
    prose(s, fui::makeRect(x, wide ? titleY + 20 : row.y + row.height / 2, width, wide ? 18 : row.height / 2 - 6),
          label, fui::TextAlign::Left, 3);
    s.target().line({int16_t(divider), int16_t(row.y + 12)}, {int16_t(divider), int16_t(row.bottom() - 12)}, 2, black);
    bookMinutes(s, fui::makeRect(divider + 5, row.y, timeW - 30, row.height), b.seconds);
    const auto arrow = fui::makeRect(row.right() - 27, row.y + (row.height - 30) / 2, 22, 30);
    if (n == m.selected) s.target().fill(arrow, black);
    dashboard::chevron(s.target(), arrow, n == m.selected);
  }
  pageui::build(s, fui::makeRect(r.x, r.bottom() - footerH - 12, r.width, footerH), m.page + 1,
                std::max(1, (m.bookTotal + m.pageSize - 1) / m.pageSize), Page, -1, Page, 1);
}

}  // namespace
int calendarDayAt(const Work& work, int month, bool sunday, int x, int y) {
  const int first = readingstats::dayIndex({month / 12, month % 12 + 1, 1});
  const int offset = (readingstats::weekday(first) + (sunday ? 1 : 0)) % 7;
  for (int day = 1; day <= readingstats::daysInMonth(month / 12, month % 12 + 1); ++day) {
    const auto box = cell(work.calendarGrid, 7 + offset + day - 1, 7, 7);
    if (x >= box.x && x < box.right() && y >= box.y && y < box.bottom()) return first + day - 1;
  }
  return 0;
}
void duration(uint64_t seconds, char* out, size_t size) {
  if (seconds >= 3600)
    std::snprintf(out, size, tr(STR_READING_DURATION_HM), static_cast<unsigned long long>(seconds / 3600),
                  static_cast<unsigned long long>(seconds / 60 % 60));
  else
    std::snprintf(out, size, tr(STR_READING_DURATION_M), static_cast<unsigned long long>(seconds / 60));
}
int capacity(const fui::DeviceContext& device) {
  return device.width > device.height ? 3 : readingstats::kBooksPerPage;
}
void build(toybox::Screen& screen, const Model& m, Work& work) {
  Context s{screen, work};
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_READING_STATS_TITLE);
  header.back = s.device().hasTouch;
  header.battery = m.battery;
  header.charging = m.charging;
  header.showPercent = m.showPercent;
  if (!m.bookOnly && m.syncAvailable) {
    header.mark = fui::bitmapFromIcon(icon_live_check_24);
    header.markAction = ImportHistory;
  }
  pixelui::header(screen, header);
  backui::publish(header.back, pixelui::geometry(s.device()).back);
  if (!m.bookOnly) {
    auto& tabs = work.items;
    for (auto& item : tabs) item = {};
    tabs[0].label = tr(STR_TODAY);
    tabs[1].label = tr(STR_READING_STATS_TAB);
    tabs[2].label = tr(STR_READING_CALENDAR);
    tabs[3].label = tr(STR_READING_BOOKS);
    for (int n = 0; n < 4; ++n) {
      pixelui::copyCaps(work.labels[n], sizeof(work.labels[n]), tabs[n].label);
      tabs[n].label = work.labels[n];
      tabs[n].value = n;
      tabs[n].selected = n == m.tab;
    }
    auto& props = work.tabs;
    props = {};
    props.tabs = tabs;
    props.count = 4;
    props.action = Tab;
    props.inputMask = fui::InputTouch;
    props.text.font = 3;
    props.text.maxLines = 1;
    props.tabStyles = pixelui::rowStyles();
    props.tabInset = {};
    props.contentInset = {};
    props.minTouchSize = 0;
    const auto band = s.takeTop(50, 12);
    fui::tabBar(s.frame(), band, props);
    frame(s, band);
  }
  auto body = s.body();
  if (m.loading || m.failed) {
    text(s, body, (m.failed ? tr(STR_READING_STORAGE_ERROR) : tr(STR_READING_LOADING)), pixelui::kBody,
         fui::TextAlign::Center, fui::Color::Black, 3);
    return;
  }
  if (m.pickerVisible) {
    periodPicker(s, body, m);
    return;
  }
  if (m.detail) {
    bookDetails(s, body, m);
    return;
  }
  if ((m.tab == 2 || (m.tab == 1 && m.periodFromPicker)) && m.period != readingstats::Period::Calendar) {
    periodDetails(s, body, m);
    return;
  }
  switch (m.tab) {
    case 0:
      today(s, body, m);
      break;
    case 1:
      summary(s, body, m);
      break;
    case 2:
      if (m.today)
        calendar(s, body, m);
      else
        text(s, body, tr(STR_READING_CLOCK), pixelui::kSmall, fui::TextAlign::Center, fui::Color::Black, 3);
      break;
    case 3:
      books(s, body, m);
      break;
    default:
      break;
  }
}

void buildImport(toybox::Screen& screen, const ImportModel& m, Work& work) {
  Context s{screen, work};
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_READING_SYNC_TITLE);
  header.back = true;
  header.battery = m.battery;
  pixelui::header(s.target(), s.device(), header);
  backui::publish(true, pixelui::geometry(s.device()).back);
  const auto body = s.body();
  const int footer = m.setup ? 108 : 54;
  const auto hero = fui::makeRect(body.x, body.y, body.width, body.height - footer - 12);
  frame(s, hero, 2);
  const char* title = m.state == ImportState::Importing && m.progress ? tr(STR_READING_SYNC_PROGRESS_BUSY)
                      : m.state == ImportState::Done                  ? tr(STR_READING_SYNC_DONE)
                      : m.state == ImportState::Importing             ? tr(STR_READING_SYNC_BUSY)
                                                                      : tr(STR_READING_SOURCE_CLOUD);
  const int side = std::min(112, std::min(hero.width / 3, hero.height / 2));
  art(s, fui::makeRect(hero.x + 20, hero.y + 20, side, side), statsart::sparkBook);
  text(s, fui::makeRect(hero.x + side + 38, hero.y + 20, hero.width - side - 56, side), title, 1, fui::TextAlign::Left,
       fui::Color::Black, 2);
  const int textTop = hero.y + side + 42;
  const char* message = m.message                      ? m.message
                        : m.state == ImportState::Done ? tr(STR_READING_SYNC_SAFE)
                                                       : tr(STR_READING_SYNC_HELP);
  prose(s, fui::makeRect(hero.x + 20, textTop, hero.width - 40, std::max(32, hero.bottom() - textTop - 64)), message);
  if (m.state == ImportState::Importing || m.state == ImportState::Done) {
    char label[96];
    std::snprintf(label, sizeof(label), "%s: %lu", tr(STR_READING_BOOKS), static_cast<unsigned long>(m.books));
    text(s, fui::makeRect(hero.x + 16, hero.bottom() - 52, hero.width - 32, 36), label, 0);
  }
  if (m.setup)
    button(s, fui::makeRect(body.x, body.bottom() - 108, body.width, 44), tr(STR_READING_SYNC_ACCOUNT), ImportSetup);
  if (m.state == ImportState::Importing)
    button(s, fui::makeRect(body.x, body.bottom() - 48, body.width, 48), tr(STR_CANCEL), ImportDone);
  else if (m.state == ImportState::Done)
    button(s, fui::makeRect(body.x, body.bottom() - 48, body.width, 48), tr(STR_DONE), ImportDone, 0, true);
  else
    button(s, fui::makeRect(body.x, body.bottom() - 48, body.width, 48), tr(STR_READING_SYNC_IMPORT), ImportStart, 0,
           true);
}
}  // namespace readingui
