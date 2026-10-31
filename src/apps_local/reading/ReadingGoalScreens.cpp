#include "ReadingGoalScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/Dashboard.h"
#include "../ui/StatisticsArt.h"
#include "../ui/ToolsDigits.h"
#include "ReadingGoal.h"

namespace readingui {
namespace fui = freeink::ui;
namespace {
void progress(toybox::Screen& s, fui::Rect r, uint64_t seconds, unsigned goal) {
  dashboard::frame(s, r, 2);
  const int fill = goal ? int(std::min(uint64_t(r.width - 8), seconds * uint64_t(r.width - 8) / (goal * 60))) : 0;
  if (fill > 0)
    s.target().fill(fui::makeRect(r.x + 4, r.y + 4, fill, r.height - 8), dashboard::ink, chrome::lightHeader ? 3 : 0);
}
void value(char* label, size_t size, uint64_t seconds, unsigned goal) {
  std::snprintf(label, size, tr(STR_GOAL_PROGRESS), static_cast<unsigned long long>(seconds / 60), goal);
}
}  // namespace
void goalCard(toybox::Screen& s, fui::Rect r, const Model& m) {
  dashboard::frame(s, r);
  const unsigned goal = m.goalMinutes;
  const int icon = 36, top = r.y + 8;
  s.target().bitmap(fui::makeRect(r.x + 14, top + 4, icon, icon), statsart::sparkBook, fui::BitmapMode::Contain,
                    dashboard::ink);
  dashboard::text(s, fui::makeRect(r.x + 58, top, r.width - 116, 44), tr(STR_GOAL_DAILY), 1);
  if (goal) {
    const auto calendar = fui::makeRect(r.right() - 54, top, 44, 44);
    dashboard::button(s, calendar, "", GoalCalendar);
    dashboard::icon(s, calendar.inset(fui::Insets{6, 6, 6, 6}), extra_calendar_32);
  } else {
    dashboard::hit(s, fui::makeRect(r.x + 58, top, r.width - 116, 44), GoalChoose);
  }
  const int bottom = r.bottom() - 12, buttonH = 40;
  if (goal) {
    char label[64];
    std::snprintf(label, sizeof(label), tr(STR_STATS_MINUTE_VALUE), static_cast<unsigned long long>(goal));
    const auto bar = fui::makeRect(r.x + 12, bottom - buttonH - 22, r.width - 24, 12);
    const int valueTop = top + 50, valueHeight = std::max(24, bar.y - valueTop - 8);
    const auto number = fui::makeRect(r.x + 24, valueTop + std::max(0, (valueHeight - 44) / 2), r.width - 48,
                                      std::min(44, valueHeight));
    bool supported = true;
    for (char* p = label; *p; ++p) {
      if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
      supported &= numerals::glyph(static_cast<unsigned char>(*p)) != nullptr;
    }
    if (supported)
      toolsdigits::draw(s, number, label);
    else
      dashboard::text(s, number, label, 1);
    progress(s, bar, m.summary ? m.summary->onDay(m.today) : 0, goal);
  } else {
    dashboard::text(s, fui::makeRect(r.x + 12, top + 52, r.width - 24, std::max(24, bottom - buttonH - top - 60)),
                    tr(STR_GOAL_SET_HINT), 1);
  }
  dashboard::button(s, fui::makeRect(r.x + 10, bottom - buttonH, r.width - 20, buttonH),
                    goal ? tr(STR_CONTINUE_READING) : tr(STR_GOAL_SET), goal ? ContinueReading : GoalChoose, 0, true);
}
void goalCalendar(toybox::Screen& s, fui::Rect r, const Model& m) {
  const bool wide = r.width > r.height;
  if (!m.today) {
    const int messageHeight = 3 * s.target().lineHeight(4);
    dashboard::prose(s, fui::makeRect(r.x + 12, r.y + (r.height - 64 - messageHeight) / 2, r.width - 24, messageHeight),
                     tr(STR_READING_CLOCK), fui::TextAlign::Center, 3);
    char target[64];
    std::snprintf(target, sizeof(target), tr(STR_GOAL_TARGET), m.goalMinutes);
    dashboard::button(s, fui::makeRect(r.x, r.bottom() - 52, r.width * 2 / 3 - 5, 52), target, GoalChoose);
    dashboard::button(s, fui::makeRect(r.x + r.width * 2 / 3 + 5, r.bottom() - 52, r.width / 3 - 5, 52), tr(STR_BACK),
                      GoalBack);
    return;
  }
  const int head = wide ? 42 : 50;
  const auto date = readingstats::dateFromDay(readingstats::dayIndex({m.month / 12, m.month % 12 + 1, 1}));
  char label[80];
  dashboard::date(label, sizeof(label), date.year, date.month, 1);
  dashboard::button(s, fui::makeRect(r.x, r.y, 40, head), "<", GoalMonth, -1);
  dashboard::text(s, fui::makeRect(r.x + 48, r.y, r.width - 96, head), label, 1);
  dashboard::button(s, fui::makeRect(r.right() - 40, r.y, 40, head), ">", GoalMonth, 1);
  const int footer = wide ? 86 : 112;
  const auto grid = fui::makeRect(r.x, r.y + head + 8, r.width, r.height - head - footer - 16);
  dashboard::frame(s, grid);
  static constexpr StrId days[] = {StrId::STR_READING_MON, StrId::STR_READING_TUE, StrId::STR_READING_WED,
                                   StrId::STR_READING_THU, StrId::STR_READING_FRI, StrId::STR_READING_SAT,
                                   StrId::STR_READING_SUN};
  const int first = readingstats::dayIndex({date.year, date.month, 1});
  const int offset = (readingstats::weekday(first) + (m.weekStartsSunday ? 1 : 0)) % 7;
  const int count = readingstats::daysInMonth(date.year, date.month);
  for (int col = 0; col < 7; ++col) {
    const int x = grid.x + col * grid.width / 7;
    dashboard::text(s, fui::makeRect(x, grid.y, grid.width / 7, grid.height / 7),
                    I18N.get(days[(col + (m.weekStartsSunday ? 6 : 0)) % 7]), 3);
  }
  for (int n = 0; n < count; ++n) {
    const int index = offset + n, col = index % 7, row = index / 7 + 1, day = first + n;
    const auto cell = fui::makeRect(grid.x + col * grid.width / 7 + 3, grid.y + row * grid.height / 7 + 2,
                                    grid.width / 7 - 6, grid.height / 7 - 4);
    const uint64_t seconds = day <= m.today && m.summary ? m.summary->onDay(day) : 0;
    const bool done = readinggoal::met(seconds, m.goalMinutes);
    const bool partial = m.goalMinutes && seconds > 0 && !done;
    if (done)
      lightui::surface(s.target(), cell, dashboard::ink);
    else if (partial) {
      lightui::surface(s.target(), cell, lightui::highlight);
      dashboard::frame(s, cell, 1);
    } else
      dashboard::frame(s, cell, 1);
    std::snprintf(label, sizeof(label), "%d", n + 1);
    dashboard::text(s, cell, label, wide ? 3 : 0, fui::TextAlign::Center, done);
  }
  const int y = grid.bottom() + 8, legendH = wide ? 32 : 52;
  static constexpr StrId legend[] = {StrId::STR_GOAL_COMPLETE, StrId::STR_GOAL_PARTIAL, StrId::STR_GOAL_INCOMPLETE};
  for (int n = 0; n < 3; ++n) {
    const int x = r.x + r.width * n / 3, width = r.width / 3;
    const auto swatch = fui::makeRect(x + 2, y + (legendH - 16) / 2, 16, 16);
    if (n < 2) s.target().fill(swatch, n == 0 ? dashboard::ink : lightui::highlight);
    s.target().stroke(swatch, dashboard::ink, 1);
    dashboard::text(s, fui::makeRect(x + 24, y, width - 26, legendH), I18N.get(legend[n]), 3, fui::TextAlign::Left);
  }
  std::snprintf(label, sizeof(label), tr(STR_GOAL_TARGET), m.goalMinutes);
  const int buttonY = y + legendH + 8, buttonH = footer - legendH - 8;
  dashboard::button(s, fui::makeRect(r.x, buttonY, r.width * 2 / 3 - 5, buttonH), label, GoalChoose, 0, false, nullptr,
                    0);
  dashboard::button(s, fui::makeRect(r.x + r.width * 2 / 3 + 5, buttonY, r.width / 3 - 5, buttonH), tr(STR_BACK),
                    GoalBack);
}
void goalWallpaper(toybox::Screen& s, const readingstats::Summary* summary, int day, unsigned goal) {
  pixelui::begin(s);
  const auto safe = s.device().safeRect();
  auto r = s.body();
  const bool wide = r.width > r.height;
  r = r.inset(fui::Insets{24, 16, 24, 16});
  r.y = safe.y + (safe.height - r.height) / 2;
  const auto d = readingstats::dateFromDay(day);
  char label[96];
  dashboard::date(label, sizeof(label), d.year, d.month, d.day);
  dashboard::text(s, fui::makeRect(r.x, r.y, r.width, 40), label, 1);
  r.y += 56;
  r.height -= 56;
  const int gap = 16, left = wide ? r.width * 2 / 3 - 8 : r.width, mainH = wide ? r.height : r.height * 65 / 100;
  const auto main = fui::makeRect(r.x, r.y, left, mainH);
  dashboard::frame(s, main);
  const int icon = wide ? 40 : 64, iconY = main.y + (wide ? 12 : 20);
  s.target().bitmap(fui::makeRect(main.x + (main.width - icon) / 2, iconY, icon, icon), statsart::sparkBook,
                    fui::BitmapMode::Contain, dashboard::ink);
  const int titleY = iconY + icon + 12;
  dashboard::text(s, fui::makeRect(main.x + 12, titleY, main.width - 24, 38), tr(STR_GOAL_DAILY), 1);
  if (summary) {
    const auto seconds = summary->onDay(day);
    if (goal)
      value(label, sizeof(label), seconds, goal);
    else
      std::snprintf(label, sizeof(label), tr(STR_READING_DURATION_M), static_cast<unsigned long long>(seconds / 60));
    const int valueY = titleY + 46;
    dashboard::text(s, fui::makeRect(main.x + 16, valueY, main.width - 32, 36), label, 0);
    std::snprintf(label, sizeof(label), "%llu", static_cast<unsigned long long>(seconds / 60));
    toolsdigits::counter(s, fui::makeRect(main.x + 24, valueY + 44, main.width - 48, main.bottom() - 80 - valueY - 44),
                         label, wide ? 48 : 64);
    if (goal) progress(s, fui::makeRect(main.x + 24, main.bottom() - 66, main.width - 48, 18), seconds, goal);
    dashboard::text(s, fui::makeRect(main.x + 12, main.bottom() - 40, main.width - 24, 28),
                    goal ? (readinggoal::met(seconds, goal) ? tr(STR_GOAL_REACHED) : tr(STR_GOAL_KEEP_READING))
                         : tr(STR_GOAL_SET_HINT),
                    0);
  } else
    dashboard::text(s, fui::makeRect(main.x + 12, main.y + main.height / 2, main.width - 24, 80),
                    tr(STR_READING_STORAGE_ERROR), 0);
  const auto streak = wide ? fui::makeRect(main.right() + gap, r.y, r.right() - main.right() - gap, r.height)
                           : fui::makeRect(r.x, main.bottom() + gap, r.width, r.bottom() - main.bottom() - gap);
  dashboard::frame(s, streak);
  const int mark = wide ? 72 : 64, flameY = streak.y + (wide ? 28 : 14);
  s.target().bitmap(fui::makeRect(streak.x + (streak.width - mark) / 2, flameY, mark, mark), statsart::flame,
                    fui::BitmapMode::Contain, dashboard::ink);
  const unsigned days = summary ? readinggoal::streak(*summary, day, goal) : 0;
  if (days == 1)
    std::snprintf(label, sizeof(label), tr(STR_FLASH_INTERVAL_DAY), 1);
  else
    std::snprintf(label, sizeof(label), tr(STR_GOAL_STREAK_VALUE), days);
  dashboard::text(s, fui::makeRect(streak.x + 12, flameY + mark + 8, streak.width - 24, 40), label, 1);
  dashboard::text(s,
                  fui::makeRect(streak.x + 12, streak.bottom() - (wide ? 88 : 54), streak.width - 24, wide ? 64 : 44),
                  tr(STR_GOAL_STREAK), 0);
}
}  // namespace readingui
