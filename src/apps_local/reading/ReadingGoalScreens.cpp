#include "ReadingGoalScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/StatisticsArt.h"
#include "../ui/ToolsDigits.h"
#include "ReadingGoal.h"

namespace readingui {
namespace fui = freeink::ui;
namespace {
void completedMark(fui::DrawTarget& target, fui::Rect box) {
  target.fill(box, fui::Paint::solid(fui::Color::Black), chrome::lightHeader ? 4 : 0);
  target.bitmap(box.inset({4, 4, 4, 4}), fui::bitmapFromIcon(extra_check_32), fui::BitmapMode::Contain,
                fui::Paint::solid(fui::Color::White));
}
void progress(toybox::Screen& s, fui::Rect r, uint64_t seconds, unsigned goal) {
  dashboard::frame(s, r, 2);
  const int fill = goal ? int(std::min(uint64_t(r.width - 8), seconds * uint64_t(r.width - 8) / (goal * 60))) : 0;
  if (fill > 0)
    s.target().fill(fui::makeRect(r.x + 4, r.y + 4, fill, r.height - 8), dashboard::ink, chrome::lightHeader ? 3 : 0);
}
void goalControls(toybox::Screen& s, fui::Rect row, unsigned minutes) {
  const int gap = 8, resetWidth = 44, backWidth = row.width / 3;
  const int goalWidth = row.width - backWidth - resetWidth - 2 * gap;
  char label[64];
  std::snprintf(label, sizeof(label), tr(STR_GOAL_TARGET), minutes);
  dashboard::button(s, fui::makeRect(row.x, row.y, goalWidth, row.height), label, GoalChoose, 0, false, nullptr, 0);
  const auto reset = fui::makeRect(row.x + goalWidth + gap, row.y, resetWidth, row.height);
  dashboard::button(s, reset, "", GoalReset);
  dashboard::icon(s, reset.inset({(int16_t)((row.height - 28) / 2), 8, (int16_t)((row.height - 28) / 2), 8}),
                  extra_rotate_cw_32);
  dashboard::button(s, fui::makeRect(row.right() - backWidth, row.y, backWidth, row.height), tr(STR_BACK), GoalBack);
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
  fui::TextStyle titleStyle;
  titleStyle.font = 1;
  titleStyle.align = fui::TextAlign::Center;
  titleStyle.maxLines = 2;
  const int titleHeight =
      std::max(44, int(fui::measureWrappedText(s.target(), tr(STR_GOAL_DAILY), titleStyle, r.width - 116).height));
  s.target().text(fui::makeRect(r.x + 58, top, r.width - 116, titleHeight), tr(STR_GOAL_DAILY), titleStyle);
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
    const int valueTop = top + titleHeight + 6, valueHeight = std::max(24, bar.y - valueTop - 8);
    const bool completed = m.summary && readinggoal::met(m.summary->onDay(m.today), goal);
    const auto number = fui::makeRect(r.x + 24, valueTop + std::max(0, (valueHeight - 44) / 2),
                                      r.width - 48 - (completed ? 36 : 0), std::min(44, valueHeight));
    char digits[16];
    std::snprintf(digits, sizeof(digits), "%u", goal);
    // Keep the same numerical scale in every script; translated units must
    // not push the entire value onto the small fallback text face.
    const char* unit = label;
    while (*unit >= '0' && *unit <= '9') ++unit;
    while (*unit == ' ') ++unit;
    fui::TextStyle unitStyle;
    unitStyle.font = 1;
    const int unitW = std::min(int(number.width) / 2, int(s.target().measureText(1, unit, unitStyle).width));
    const int digitW = std::min(int(number.width) - unitW - 12, int(std::strlen(digits)) * number.height * 3 / 4);
    const int left = number.x + (number.width - digitW - unitW - 12) / 2;
    toolsdigits::draw(s, fui::makeRect(left, number.y, digitW, number.height), digits);
    dashboard::text(s, fui::makeRect(left + digitW + 12, number.y, unitW, number.height), unit, 1);
    if (completed)
      completedMark(s.target(), fui::makeRect(number.right() + 4, number.y + (number.height - 28) / 2, 28, 28));
    progress(s, bar, m.summary ? m.summary->onDay(m.today) : 0, goal);
  } else {
    const int hintTop = top + titleHeight + 8;
    const int hintHeight = bottom - buttonH - hintTop - 8;
    if (hintHeight >= 24)
      dashboard::text(s, fui::makeRect(r.x + 12, hintTop, r.width - 24, hintHeight), tr(STR_GOAL_SET_HINT), 1);
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
    goalControls(s, fui::makeRect(r.x, r.bottom() - 52, r.width, 52), m.goalMinutes);
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
    const auto state =
        m.summary ? readinggoal::dayState(day, m.today, m.goalStartDay, seconds, m.goalMinutes, m.goalLegacyHistory)
                  : readinggoal::DayState::Untracked;
    const bool done = state == readinggoal::DayState::Complete;
    const bool partial = day == m.today && state == readinggoal::DayState::InProgress;
    if (done)
      lightui::surface(s.target(), cell, dashboard::ink);
    else if (partial) {
      lightui::surface(s.target(), cell, lightui::highlight);
      dashboard::frame(s, cell, 1);
    } else
      dashboard::frame(s, cell, 1);
    std::snprintf(label, sizeof(label), "%d", n + 1);
    const bool missed = state == readinggoal::DayState::Missed;
    dashboard::text(s, cell, label, wide ? 3 : 0, fui::TextAlign::Center, done);
    if (missed) {
      const int inset = 4, weight = std::max(2, std::min(int(cell.width), int(cell.height)) / 12);
      s.target().line({int16_t(cell.x + inset), int16_t(cell.y + inset)},
                      {int16_t(cell.right() - inset), int16_t(cell.bottom() - inset)}, weight, dashboard::ink);
      s.target().line({int16_t(cell.right() - inset), int16_t(cell.y + inset)},
                      {int16_t(cell.x + inset), int16_t(cell.bottom() - inset)}, weight, dashboard::ink);
    }
  }
  const int y = grid.bottom() + 8, legendH = wide ? 32 : 52;
  static constexpr StrId legend[] = {StrId::STR_GOAL_COMPLETE, StrId::STR_GOAL_INCOMPLETE};
  const int legendWidth = r.width * 31 / 100;
  for (int n = 0; n < 2; ++n) {
    const int x = r.x + legendWidth * n;
    const auto swatch = fui::makeRect(x + 2, y + (legendH - 16) / 2, 16, 16);
    if (n == 0) s.target().fill(swatch, dashboard::ink);
    s.target().stroke(swatch, dashboard::ink, 1);
    if (n == 1) {
      s.target().line({int16_t(swatch.x + 4), int16_t(swatch.y + 4)},
                      {int16_t(swatch.right() - 4), int16_t(swatch.bottom() - 4)}, 1, dashboard::ink);
      s.target().line({int16_t(swatch.right() - 4), int16_t(swatch.y + 4)},
                      {int16_t(swatch.x + 4), int16_t(swatch.bottom() - 4)}, 1, dashboard::ink);
    }
    dashboard::text(s, fui::makeRect(x + 24, y, legendWidth - 26, legendH), I18N.get(legend[n]), 3,
                    fui::TextAlign::Left);
  }
  const unsigned streakDays = m.summary ? readinggoal::streak(*m.summary, m.today, m.goalMinutes, m.goalStartDay) : 0;
  char daysLabel[40];
  std::snprintf(daysLabel, sizeof(daysLabel), streakDays == 1 ? tr(STR_FLASH_INTERVAL_DAY) : tr(STR_GOAL_STREAK_VALUE),
                streakDays);
  const int streakX = r.x + 2 * legendWidth + 8, streakW = r.right() - streakX;
  if (wide) {
    std::snprintf(label, sizeof(label), "%s: %s", tr(STR_SLEEP_GOAL_STREAK), daysLabel);
    dashboard::text(s, fui::makeRect(streakX, y, streakW, legendH), label, 3, fui::TextAlign::Right);
  } else {
    dashboard::text(s, fui::makeRect(streakX, y, streakW, legendH / 2), tr(STR_SLEEP_GOAL_STREAK), 3,
                    fui::TextAlign::Right);
    dashboard::text(s, fui::makeRect(streakX, y + legendH / 2, streakW, legendH / 2), daysLabel, 3,
                    fui::TextAlign::Right);
  }
  const int buttonY = y + legendH + 8, buttonH = footer - legendH - 8;
  goalControls(s, fui::makeRect(r.x, buttonY, r.width, buttonH), m.goalMinutes);
}
void goalWallpaper(toybox::Screen& s, const readingstats::Summary* summary, int day, unsigned goal, int startDay) {
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
  const unsigned days = summary ? readinggoal::streak(*summary, day, goal, startDay) : 0;
  if (days == 1)
    std::snprintf(label, sizeof(label), tr(STR_FLASH_INTERVAL_DAY), 1);
  else
    std::snprintf(label, sizeof(label), tr(STR_GOAL_STREAK_VALUE), days);
  dashboard::text(s, fui::makeRect(streak.x + 12, flameY + mark + 8, streak.width - 24, 40), label, 1);
  dashboard::text(s,
                  fui::makeRect(streak.x + 12, streak.bottom() - (wide ? 88 : 54), streak.width - 24, wide ? 64 : 44),
                  tr(STR_GOAL_STREAK), 0);
}
void goalOverlay(fui::DrawTarget& target, const fui::DeviceContext& device, const readinggoal::WallpaperData& data) {
  const auto safe = device.safeRect();
  const bool wide = device.width > device.height;
  const int margin = 16, height = wide ? 84 : 100;
  const auto card = fui::makeRect(safe.x + margin, safe.bottom() - height - margin, safe.width - margin * 2, height);
  const auto ink = fui::Paint::solid(fui::Color::Black), paper = fui::Paint::solid(fui::Color::White);
  target.fill(card, paper, chrome::lightHeader ? 8 : 0);
  target.stroke(card, ink, 2, chrome::lightHeader ? 8 : 0);
  target.fill(fui::makeRect(card.x + 6, card.y + 12, 4, card.height - 24), ink);
  fui::TextStyle style;
  style.font = 0;
  style.maxLines = 1;
  style.color = fui::Color::Black;
  char value[64];
  if (data.state != readinggoal::WallpaperState::Ready) {
    style.maxLines = 2;
    style.align = fui::TextAlign::Center;
    target.text(
        card.inset({12, 18, 12, 18}),
        data.state == readinggoal::WallpaperState::NoGoal ? tr(STR_GOAL_SET_HINT) : tr(STR_READING_STORAGE_ERROR),
        style);
    return;
  }
  const int pad = 20, row = (height - 16) / 2;
  const bool completed = readinggoal::met(data.seconds, data.goal);
  for (int n = 0; n < 2; ++n) {
    const int y = card.y + 8 + n * row;
    const int reserve = n == 0 && completed ? 30 : 0;
    if (n == 0)
      std::snprintf(value, sizeof(value), tr(STR_GOAL_PROGRESS), static_cast<unsigned long long>(data.seconds / 60),
                    data.goal);
    else if (data.streak == 1)
      std::snprintf(value, sizeof(value), tr(STR_FLASH_INTERVAL_DAY), 1);
    else
      std::snprintf(value, sizeof(value), tr(STR_GOAL_STREAK_VALUE), data.streak);
    style.align = fui::TextAlign::Right;
    const int valueWidth = target.measureText(0, value, style).width;
    const int valueX = card.right() - pad - reserve - valueWidth;
    target.text(fui::makeRect(valueX, y, valueWidth, row), value, style);
    style.align = fui::TextAlign::Left;
    target.text(fui::makeRect(card.x + pad, y, valueX - card.x - pad - 12, row),
                n ? tr(STR_SLEEP_GOAL_STREAK) : tr(STR_SLEEP_GOAL_LABEL), style);
    if (!n && completed) completedMark(target, fui::makeRect(card.right() - pad - 24, y + (row - 24) / 2, 24, 24));
  }
}
}  // namespace readingui
