#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/Dashboard.h"
#include "PomodoroScreens.h"
#include "TimerSettingsUi.h"

namespace pomodoroui {
namespace {
namespace fui = freeink::ui;
constexpr auto ink = fui::Paint::solid(fui::Color::Black);
constexpr StrId months[] = {StrId::STR_READING_MONTH_01, StrId::STR_READING_MONTH_02, StrId::STR_READING_MONTH_03,
                            StrId::STR_READING_MONTH_04, StrId::STR_READING_MONTH_05, StrId::STR_READING_MONTH_06,
                            StrId::STR_READING_MONTH_07, StrId::STR_READING_MONTH_08, StrId::STR_READING_MONTH_09,
                            StrId::STR_READING_MONTH_10, StrId::STR_READING_MONTH_11, StrId::STR_READING_MONTH_12};
constexpr StrId weekdays[] = {StrId::STR_UI_WEEKDAY_0_857678, StrId::STR_UI_WEEKDAY_1_B0E936,
                              StrId::STR_UI_WEEKDAY_2_68EA03, StrId::STR_UI_WEEKDAY_3_85CCD3,
                              StrId::STR_UI_WEEKDAY_4_BEC79A, StrId::STR_UI_WEEKDAY_5_A4D8CC,
                              StrId::STR_UI_WEEKDAY_6_8FB0EF};
void summary(toybox::Screen& screen, fui::Rect box, const Model& model, Work& work) {
  screen.target().stroke(box, ink, 2, lightui::radius());
  const bool wide = screen.device().width > screen.device().height;
  if (wide)
    screen.target().line({int16_t(box.x + 12), int16_t(box.y + box.height / 2)},
                         {int16_t(box.right() - 12), int16_t(box.y + box.height / 2)}, 1, ink);
  else
    screen.target().line({int16_t(box.x + box.width / 2), int16_t(box.y + 12)},
                         {int16_t(box.x + box.width / 2), int16_t(box.bottom() - 12)}, 1, ink);
  for (int n = 0; n < 2; ++n) {
    const auto cell = wide ? fui::makeRect(box.x, box.y + n * box.height / 2, box.width, box.height / 2)
                           : fui::makeRect(box.x + n * box.width / 2, box.y, box.width / 2, box.height);
    const uint64_t minutes = model.stats->millisecondsWithout(model.activeCredit) / 60000;
    if (!n)
      std::snprintf(work.label, sizeof(work.label), "%lu", static_cast<unsigned long>(model.stats->sessions));
    else if (minutes >= 60)
      std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_DURATION_HOURS),
                    unsigned(std::min(minutes / 60, uint64_t(UINT32_MAX))), unsigned(minutes % 60));
    else
      std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_DURATION_MINUTES), unsigned(minutes));
    dashboard::text(screen, fui::makeRect(cell.x + 4, cell.y + 6, cell.width - 8, cell.height / 2),
                    model.loading || model.historyFailed ? "--" : work.label, 1);
    dashboard::text(screen, fui::makeRect(cell.x + 4, cell.y + cell.height / 2, cell.width - 8, cell.height / 2 - 4),
                    n ? tr(STR_FOCUS_STUDIED) : tr(STR_POMODORO_COUNT), 3);
  }
}
fui::Paint calendarPaint(uint32_t sessions) {
  return sessions >= 4   ? ink
         : sessions >= 2 ? fui::Paint::dither(fui::Color::DarkGray)
         : sessions == 1 ? fui::Paint::dither(fui::Color::LightGray)
                         : dashboard::paper;
}
void calendar(toybox::Screen& screen, fui::Rect box, const Model& model, Work& work, fui::Rect legend) {
  const auto& stats = *model.stats;
  dashboard::text(screen, fui::makeRect(box.x + 4, box.y, box.width - 8, 32), tr(STR_POMODORO_MONTH_CHART), 0,
                  fui::TextAlign::Left);
  box.y += 38;
  box.height -= 38;
  const int offset = readingstats::weekday(stats.first), rows = (offset + stats.count + 6) / 7;
  const int height = std::max(1, (box.height - (legend.empty() ? 76 : 30)) / rows);
  work.calendarGrid = fui::makeRect(box.x, box.y + 30, box.width, height * rows);
  dashboard::hit(screen, work.calendarGrid, StatsCalendar);
  for (int n = 0; n < 7; ++n)
    dashboard::text(screen, fui::makeRect(box.x + box.width * n / 7, box.y, box.width / 7, 26), I18N.get(weekdays[n]),
                    3);
  for (int n = 0; n < stats.count; ++n) {
    const int index = n + offset, day = stats.first + n;
    const int x = box.x + box.width * (index % 7) / 7, right = box.x + box.width * (index % 7 + 1) / 7;
    const auto cell = fui::makeRect(x + 3, box.y + 30 + index / 7 * height + 3, right - x - 6, height - 6);
    lightui::surface(screen.target(), cell, calendarPaint(stats.counts[n]));
    screen.target().stroke(cell, ink, 1, lightui::radius());
    if (day == model.today)
      screen.target().line({int16_t(cell.x + 8), int16_t(cell.bottom() + 2)},
                           {int16_t(cell.right() - 8), int16_t(cell.bottom() + 2)}, 2, ink);
    std::snprintf(work.label, sizeof(work.label), "%d", n + 1);
    dashboard::text(screen, cell, work.label, 3, fui::TextAlign::Center, stats.counts[n] >= 2);
  }
  static constexpr uint32_t levels[] = {0, 1, 2, 4};
  static constexpr StrId labels[] = {StrId::STR_POMODORO_LEGEND_ZERO, StrId::STR_POMODORO_LEGEND_ONE,
                                     StrId::STR_POMODORO_LEGEND_FEW, StrId::STR_POMODORO_LEGEND_MANY};
  if (legend.empty()) legend = fui::makeRect(box.x, work.calendarGrid.bottom() + 12, box.width, 28);
  for (int n = 0; n < 4; ++n) {
    const int x = legend.x + n * legend.width / 4;
    const auto swatch = fui::makeRect(x + 4, legend.y + 4, 18, 18);
    screen.target().fill(swatch, calendarPaint(levels[n]));
    screen.target().stroke(swatch, ink, 1);
    dashboard::text(screen, fui::makeRect(x + 28, legend.y, legend.width / 4 - 30, 28), I18N.get(labels[n]), 3,
                    fui::TextAlign::Left);
  }
}
void chart(toybox::Screen& screen, fui::Rect box, const Model& model, Work& work) {
  const auto& stats = *model.stats;
  const bool year = stats.period == pomodoro::Period::Year;
  dashboard::text(screen, fui::makeRect(box.x + 4, box.y, box.width - 8, 32),
                  year ? tr(STR_POMODORO_YEAR_CHART) : tr(STR_FOCUS_TIME_PER_DAY), 0, fui::TextAlign::Left);
  const auto plot = fui::makeRect(box.x + 68, box.y + 50, box.width - 74, box.height - 88);
  uint64_t maximum = 30;
  for (int n = 0; n < stats.count; ++n)
    maximum = std::max(maximum, stats.bucketWithout(n, model.activeCredit) / 60000 + 1);
  const uint64_t unit = maximum >= 120 ? 120 : 30;
  maximum = (maximum + unit - 1) / unit * unit;
  for (int n = 0; n < 3; ++n) {
    const int y = plot.bottom() - plot.height * n / 2;
    screen.target().line({plot.x, int16_t(y)}, {plot.right(), int16_t(y)}, 1,
                         n ? fui::Paint::dither(fui::Color::LightGray) : ink);
    const bool hours = maximum >= 120;
    std::snprintf(work.label, sizeof(work.label), hours ? tr(STR_FOCUS_AXIS_HOURS) : tr(STR_FOCUS_AXIS_MINUTES),
                  unsigned(maximum * n / (hours ? 120 : 2)));
    dashboard::text(screen, fui::makeRect(box.x, y - 12, 62, 24), work.label, 3, fui::TextAlign::Right);
  }
  for (int n = 0; n < stats.count; ++n) {
    const int left = plot.x + plot.width * n / stats.count, right = plot.x + plot.width * (n + 1) / stats.count;
    const int h = int((stats.bucketWithout(n, model.activeCredit) / 60000) * plot.height / maximum);
    const int inset = year ? 7 : 10;
    if (h && !model.loading && !model.historyFailed)
      screen.target().fill(fui::makeRect(left + inset, plot.bottom() - h, std::max(2, right - left - 2 * inset), h),
                           fui::Paint::dither(fui::Color::DarkGray));
    if (year) {
      // Numeric month labels stay legible when twelve bars share a row.
      std::snprintf(work.label, sizeof(work.label), "%d", n + 1);
      dashboard::text(screen, fui::makeRect(left, plot.bottom() + 8, right - left, 26), work.label, 3);
      dashboard::hit(screen, fui::makeRect(left, plot.y, right - left, plot.height + 34), StatsMonth, n + 1);
    } else {
      dashboard::text(screen, fui::makeRect(left, plot.bottom() + 8, right - left, 26),
                      I18N.get(weekdays[readingstats::weekday(stats.plotFirst + n)]), 3);
      if (stats.plotFirst + n == stats.day)
        screen.target().line({int16_t(left + 12), int16_t(plot.bottom() + 35)},
                             {int16_t(right - 12), int16_t(plot.bottom() + 35)}, 2, ink);
      dashboard::hit(screen, fui::makeRect(left, plot.y, right - left, plot.height + 34), StatsDay, n);
    }
  }
}
}  // namespace
int calendarDay(const pomodoro::Stats& stats, freeink::ui::Rect grid, int x, int y) {
  if (grid.empty() || x < grid.x || x >= grid.right() || y < grid.y || y >= grid.bottom()) return 0;
  const int offset = readingstats::weekday(stats.first), rows = (offset + stats.count + 6) / 7;
  const int cell = (y - grid.y) * rows / grid.height * 7 + ((x - grid.x + 1) * 7 - 1) / grid.width - offset;
  return cell >= 0 && cell < stats.count ? stats.first + cell : 0;
}
void statistics(toybox::Screen& screen, freeink::ui::Rect body, const Model& model, Work& work) {
  if (!model.today || !model.stats) {
    dashboard::prose(screen, fui::makeRect(body.x + 16, body.y + 36, body.width - 32, 140), tr(STR_POMODORO_NO_DATE),
                     fui::TextAlign::Center, 4);
    return;
  }
  const auto& stats = *model.stats;
  const auto tabs = fui::makeRect(body.x + 4, body.y, body.width - 8, 48);
  static constexpr StrId labels[] = {StrId::STR_POMODORO_DAY, StrId::STR_POMODORO_MONTH, StrId::STR_POMODORO_YEAR};
  for (int n = 0; n < 3; ++n)
    dashboard::segment(screen, tabs, n, 3, I18N.get(labels[n]), StatsPeriod, n, int(stats.period) == n, 0);
  const auto nav = fui::makeRect(body.x, body.y + 60, body.width, 48);
  timerui::control(screen, fui::makeRect(nav.x, nav.y, 46, 48), "<", StatsMove, -1);
  timerui::control(screen, fui::makeRect(nav.right() - 46, nav.y, 46, 48), ">", StatsMove, 1);
  const auto date = readingstats::dateFromDay(stats.day);
  switch (stats.period) {
    case pomodoro::Period::Day:
      dashboard::date(work.label, sizeof(work.label), date.year, date.month, date.day);
      break;
    case pomodoro::Period::Month:
      std::snprintf(work.label, sizeof(work.label), "%s %d", I18N.get(months[date.month - 1]), date.year);
      break;
    case pomodoro::Period::Year:
      std::snprintf(work.label, sizeof(work.label), "%d", date.year);
      break;
    case pomodoro::Period::Week:
      dashboard::date(work.label, sizeof(work.label), date.year, date.month, date.day);
      break;
  }
  dashboard::text(screen, fui::makeRect(nav.x + 54, nav.y, nav.width - 108, nav.height), work.label, 0);
  body.y += 120;
  body.height -= 120;
  const bool wide = screen.device().width > screen.device().height;
  const auto metrics = fui::makeRect(body.x, body.y, wide ? body.width / 3 - 8 : body.width, wide ? 168 : 96);
  summary(screen, metrics, model, work);
  auto graph = wide ? fui::makeRect(metrics.right() + 16, body.y, body.right() - metrics.right() - 16, body.height - 46)
                    : fui::makeRect(body.x, metrics.bottom() + 16, body.width, body.bottom() - metrics.bottom() - 80);
  if (model.loading || model.historyFailed) {
    dashboard::prose(screen, fui::makeRect(graph.x + 8, graph.y + 24, graph.width - 16, 100),
                     model.historyFailed ? tr(STR_POMODORO_HISTORY_FAILED) : tr(STR_LOADING), fui::TextAlign::Center,
                     3);
  } else if (stats.period == pomodoro::Period::Month)
    calendar(screen, graph, model, work,
             wide ? fui::makeRect(metrics.x, metrics.bottom() + 8, metrics.width, 28) : fui::Rect{});
  else
    chart(screen, graph, model, work);
  const auto home = fui::makeRect(body.right() - 114, body.bottom() - 48, 114, 48);
  timerui::control(screen, home, tr(STR_TODAY), StatsToday);
  const char* note = stats.period == pomodoro::Period::Month                           ? tr(STR_POMODORO_CALENDAR_HINT)
                     : stats.millisecondsWithout(model.activeCredit) || stats.sessions ? tr(STR_POMODORO_COUNT)
                                                                                       : tr(STR_POMODORO_EMPTY_PERIOD);
  if (stats.period == pomodoro::Period::Day && stats.day == model.today) note = tr(STR_TODAY);
  if (stats.period == pomodoro::Period::Day && stats.day == model.today - 1) note = tr(STR_POMODORO_YESTERDAY);
  dashboard::prose(screen, fui::makeRect(body.x + 4, body.bottom() - 54, body.width - 132, 54), note,
                   fui::TextAlign::Left, 2);
}
}  // namespace pomodoroui
