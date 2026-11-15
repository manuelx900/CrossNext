#include "FocusScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../pomodoro/PomodoroScreens.h"
#include "../pomodoro/TimerSettingsUi.h"
#include "../ui/Dashboard.h"
#include "../ui/ToyboxIcons.h"

namespace focusui {
namespace fui = freeink::ui;
namespace {
void card(toybox::Screen& screen, fui::Rect r) {
  screen.target().stroke(r, dashboard::ink, 2, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
}
void button(toybox::Screen& screen, fui::Rect box, const char* label, fui::ActionId action, int value = 0,
            bool primary = false) {
  timerui::control(screen, box, label, action, value, primary);
}
void label(toybox::Screen& screen, fui::Rect box, const char* text, int font = pixelui::kBody,
           fui::TextAlign align = fui::TextAlign::Center) {
  pixelui::text(screen.target(), box, text, font, fui::Color::Black, align);
}
void pager(toybox::Screen& screen, fui::Rect body, int page, int pages, Work& work) {
  if (pages <= 1) return;
  const int width = std::min(260, int(body.width));
  const auto band = fui::makeRect(body.x + (body.width - width) / 2, body.bottom() - 42, width, 42);
  button(screen, fui::makeRect(band.x, band.y, 44, band.height), "<", Page, -1);
  std::snprintf(work.label, sizeof(work.label), "%d / %d", page + 1, pages);
  label(screen, fui::makeRect(band.x + 52, band.y, width - 104, band.height), work.label, pixelui::kSmall);
  button(screen, fui::makeRect(band.right() - 44, band.y, 44, band.height), ">", Page, 1);
}
void copy(toybox::Screen& screen, fui::Rect r, const char* value, bool strong = false, int lines = 1,
          fui::TextAlign align = fui::TextAlign::Left) {
  fui::TextStyle style;
  style.font = strong ? pixelui::kSmall : 3;
  style.bold = strong;
  style.maxLines = lines;
  style.align = align;
  if (lines == 1 && screen.target().measureText(style.font, value, style).width > r.width) style.font = 3;
  screen.target().text(toybox::inkCentred(screen.target(), r, style), value, style);
}
bool control(toybox::Screen& screen, fui::Rect box, fui::ActionId action) {
  const bool selected = screen.frame().buttonSelected(action, 0, false);
  if (selected)
    screen.target().fill(box, dashboard::ink, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
  else
    card(screen, box);
  dashboard::hit(screen, box, action);
  return selected;
}
void pencil(toybox::Screen& screen, fui::Rect box, bool inverse) {
  const auto ink = inverse ? dashboard::paper : dashboard::ink;
  const int x = box.x + 12, y = box.y + 10;
  screen.target().line({int16_t(x + 2), int16_t(y + 16)}, {int16_t(x + 15), int16_t(y + 3)}, 3, ink);
  screen.target().line({int16_t(x + 6), int16_t(y + 20)}, {int16_t(x + 19), int16_t(y + 7)}, 3, ink);
  screen.target().line({int16_t(x + 15), int16_t(y + 3)}, {int16_t(x + 19), int16_t(y + 7)}, 3, ink);
  screen.target().line({int16_t(x + 2), int16_t(y + 16)}, {int16_t(x), int16_t(y + 22)}, 2, ink);
  screen.target().line({int16_t(x), int16_t(y + 22)}, {int16_t(x + 6), int16_t(y + 20)}, 2, ink);
}
void editControl(toybox::Screen& screen, fui::Rect box, fui::ActionId action) {
  pencil(screen, box, control(screen, box, action));
}
void addControl(toybox::Screen& screen, fui::Rect box) {
  const bool selected = control(screen, box, Add);
  const auto ink = selected ? dashboard::paper : dashboard::ink;
  fui::TextStyle style;
  style.font = 3;
  style.bold = true;
  style.color = selected ? fui::Color::White : fui::Color::Black;
  const int textWidth = screen.target().measureText(style.font, tr(STR_FOCUS_ADD), style).width;
  const int left = box.x + (box.width - textWidth - 28) / 2;
  const int x = left + 8, y = box.y + box.height / 2;
  screen.target().line({int16_t(x - 8), int16_t(y)}, {int16_t(x + 8), int16_t(y)}, 3, ink);
  screen.target().line({int16_t(x), int16_t(y - 8)}, {int16_t(x), int16_t(y + 8)}, 3, ink);
  const auto text = fui::makeRect(left + 28, box.y, textWidth, box.height);
  screen.target().text(toybox::inkCentred(screen.target(), text, style), tr(STR_FOCUS_ADD), style);
}
struct Sections {
  fui::Rect identity{}, summary{}, body{};
};
Sections sections(fui::Rect body, bool wide, View view) {
  Sections out;
  const bool project = view == View::Tasks;
  if (project) {
    out.identity = fui::makeRect(body.x, body.y, wide ? body.width / 3 : body.width, wide ? 72 : 68);
    if (!wide) {
      body.y += 84;
      body.height -= 84;
    }
  }
  if (project || view == View::Today || view == View::AllTasks) {
    const int inset = project && wide ? body.width / 3 + 12 : 0;
    out.summary = fui::makeRect(body.x + inset, body.y, body.width - inset, wide ? 78 : 100);
    const int advance = out.summary.height + 18;
    body.y += advance;
    body.height -= advance;
  }
  out.body = body;
  return out;
}
fui::Rect contentBody(const fui::DeviceContext& device, const Model& model) {
  auto body = pixelui::geometry(device).body;
  if (model.saveFailed || model.message) {
    body.y += 38;
    body.height -= 38;
  }
  return body;
}
int capacity(fui::Rect body, bool wide, bool projects) {
  const int available = body.height - 66 - (projects ? 64 : 48) - 50;
  const int row = projects ? (wide ? 90 : 96) : (wide ? 64 : 82);
  return std::clamp(available / row, 1, wide ? 2 : 5);
}
bool taskMatches(const focus::Task& task, const Model& model, Filter filter) {
  return focus::belongsTo(task, taskList(model.view), model.project) && (filter == Filter::All || task.done);
}
void folder(toybox::Screen& screen, fui::Rect r) {
  const int x = r.x + 1, y = r.y + 1, right = r.right() - 2, bottom = r.bottom() - 2;
  const int tab = x + r.width * 2 / 5, shoulder = y + 8;
  const int corner = chrome::lightHeader ? 2 : 0;
  const auto edge = [&screen](int x1, int y1, int x2, int y2) {
    screen.target().line({int16_t(x1), int16_t(y1)}, {int16_t(x2), int16_t(y2)}, 3, dashboard::ink);
  };
  edge(x + corner, y, tab, y);
  edge(tab, y, tab + 5, shoulder);
  edge(tab + 5, shoulder, right - corner, shoulder);
  edge(right - corner, shoulder, right, shoulder + corner);
  edge(right, shoulder + corner, right, bottom - corner);
  edge(right, bottom - corner, right - corner, bottom);
  edge(right - corner, bottom, x + corner, bottom);
  edge(x + corner, bottom, x, bottom - corner);
  edge(x, bottom - corner, x, y + corner);
  edge(x, y + corner, x + corner, y);
}
int taskTitle(toybox::Screen& screen, fui::Rect area, const char* name, int minimumHeight, bool editable = false) {
  fui::TextStyle style;
  style.font = pixelui::kBody;
  style.bold = true;
  style.align = fui::TextAlign::Center;
  style.maxLines = 2;
  const int height = std::max(minimumHeight, 2 * int(screen.target().lineHeight(style.font)));
  if (editable) {
    card(screen, fui::makeRect(area.x, area.y, area.width, height));
    style.align = fui::TextAlign::Left;
    editControl(screen, fui::makeRect(area.right() - 56, area.y + (height - 44) / 2, 44, 44), Rename);
  }
  screen.target().text(fui::makeRect(area.x + 12, area.y, area.width - (editable ? 84 : 24), height), name, style);
  return height;
}
void progress(toybox::Screen& screen, fui::Rect r, unsigned done, unsigned planned) {
  const int radius = chrome::lightHeader ? r.height / 2 : 0;
  screen.target().fill(r, fui::Paint::dither(fui::Color::LightGray), radius);
  if (planned && done)
    screen.target().fill(fui::makeRect(r.x, r.y, int(uint64_t(r.width) * std::min(done, planned) / planned), r.height),
                         dashboard::ink, radius);
}
void taskRow(toybox::Screen& screen, fui::Rect row, const focus::Task& task, int index, Work& work) {
  dashboard::hit(screen, row, Open, index);
  const auto check = fui::makeRect(row.x + 16, row.y + (row.height - 28) / 2, 28, 28);
  dashboard::check(screen, check, task.done);
  dashboard::hit(screen, check.inset({-8, -8, -8, -8}), CompleteAt, index);
  const int titleX = row.x + 64, titleWidth = row.width - 172;
  const int titleY = row.y + std::max(2, (row.height - 58) / 2);
  copy(screen, fui::makeRect(titleX, titleY, titleWidth, 32), task.name, true);
  if (task.done) {
    fui::TextStyle style;
    style.font = pixelui::kSmall;
    style.bold = true;
    const int width = std::min(titleWidth, int(screen.target().measureText(style.font, task.name, style).width));
    screen.target().line({int16_t(titleX), int16_t(titleY + 17)}, {int16_t(titleX + width), int16_t(titleY + 17)}, 1,
                         fui::Paint::dither(fui::Color::DarkGray));
  }
  std::snprintf(work.label, sizeof(work.label), "%u / %u", task.completed, task.planned);
  copy(screen, fui::makeRect(row.right() - 98, row.y + 4, 64, row.height - 8), work.label, false, 1,
       fui::TextAlign::Right);
  for (unsigned dot = 0; dot < std::min(6u, unsigned(task.planned)); ++dot) {
    const auto circle = fui::makeRect(titleX + dot * 17, titleY + 40, 10, 10);
    if (dot < task.completed)
      screen.target().fill(circle, dashboard::ink, 5);
    else
      screen.target().stroke(circle, dashboard::ink, 1, 5);
  }
  dashboard::chevron(screen.target(), fui::makeRect(row.right() - 29, row.y + 3, 20, row.height - 6));
}
void duration(char* out, size_t size, unsigned minutes) {
  if (minutes >= 60)
    std::snprintf(out, size, tr(STR_FOCUS_DURATION_HOURS), minutes / 60, minutes % 60);
  else
    std::snprintf(out, size, tr(STR_FOCUS_DURATION_MINUTES), minutes);
}
void studyDuration(char* out, size_t size, uint32_t seconds) {
  if (seconds && seconds < 60)
    std::snprintf(out, size, tr(STR_FOCUS_DURATION_SECONDS), unsigned(seconds));
  else
    duration(out, size, seconds / 60);
}
void taskStatistics(toybox::Screen& screen, fui::Rect body, const Model& model, Work& work) {
  const auto& task = model.state->tasks[model.task];
  const bool wide = body.width > body.height;
  const int nameHeight = wide ? 32 : 48;
  copy(screen, fui::makeRect(body.x + 8, body.y, body.width - 16, nameHeight), task.name, true);
  const auto summary = fui::makeRect(body.x, body.y + nameHeight + 8, body.width, wide ? 74 : 100);
  card(screen, summary);
  static constexpr StrId names[] = {StrId::STR_FOCUS_STUDIED, StrId::STR_FOCUS_SESSIONS, StrId::STR_FOCUS_SKIPPED};
  for (int n = 0; n < 3; ++n) {
    const auto cell = lightui::segmentRect(summary, n, 3);
    if (n)
      screen.target().line({cell.x, int16_t(cell.y + 12)}, {cell.x, int16_t(cell.bottom() - 12)}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    if (n == 0)
      studyDuration(work.label, sizeof(work.label), task.study.seconds);
    else
      std::snprintf(work.label, sizeof(work.label), "%u", n == 1 ? task.completed : task.study.skipped);
    copy(screen, fui::makeRect(cell.x + 6, cell.y + (wide ? 8 : 20), cell.width - 12, 32), work.label, true, 1,
         fui::TextAlign::Center);
    copy(screen, fui::makeRect(cell.x + 6, cell.y + (wide ? 40 : 56), cell.width - 12, 28), I18N.get(names[n]), false,
         1, fui::TextAlign::Center);
  }
  const int titleY = summary.bottom() + (wide ? 10 : 24);
  copy(screen, fui::makeRect(body.x + 4, titleY, body.width - 8, 32), tr(STR_FOCUS_TODAY_HOURLY), true);
  const auto plot = fui::makeRect(body.x + 72, titleY + 48, body.width - 86,
                                  std::max(44, int(body.bottom()) - titleY - (wide ? 122 : 180)));
  unsigned maximum = 3600;
  const bool today = model.day && task.study.day == model.day;
  if (today)
    for (const auto value : task.study.hours) maximum = std::max(maximum, unsigned(value));
  const unsigned ceiling = ((maximum + 3599) / 3600) * 3600;
  for (const unsigned tick : {0u, ceiling / 2, ceiling}) {
    const int y = plot.bottom() - int(uint64_t(plot.height) * tick / ceiling);
    for (int x = plot.x; x < plot.right(); x += 8)
      screen.target().line({int16_t(x), int16_t(y)}, {int16_t(std::min(x + 3, int(plot.right()))), int16_t(y)}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    if (tick && tick % 3600 == 0)
      std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_AXIS_HOURS), tick / 3600);
    else if (tick)
      std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_AXIS_MINUTES), tick / 60);
    else
      std::snprintf(work.label, sizeof(work.label), "0");
    copy(screen, fui::makeRect(body.x, y - 12, 62, 24), work.label, false, 1, fui::TextAlign::Right);
  }
  for (int hour = 0; hour < 24; ++hour) {
    const auto cell = lightui::segmentRect(plot, hour, 24);
    const unsigned seconds = today ? task.study.hours[hour] : 0;
    if (seconds) {
      const int height = std::max(1, int(uint64_t(plot.height) * seconds / ceiling));
      const auto bar = fui::makeRect(cell.x + 2, plot.bottom() - height, std::max(1, int(cell.width) - 4), height);
      screen.target().fill(bar, fui::Paint::dither(fui::Color::LightGray));
      screen.target().stroke(bar, dashboard::ink, 1);
    }
    if (hour % 6 == 0 || hour == 23) {
      std::snprintf(work.label, sizeof(work.label), "%02d", hour);
      copy(screen, fui::makeRect(cell.x - 8, plot.bottom() + 8, cell.width + 16, 28), work.label, false, 1,
           fui::TextAlign::Center);
    }
  }
  screen.target().line({plot.x, plot.bottom()}, {plot.right(), plot.bottom()}, 2, dashboard::ink);
  copy(screen, fui::makeRect(body.x + 8, plot.bottom() + 48, body.width - 16, wide ? 28 : 72),
       task.study.incomplete ? tr(STR_FOCUS_LEGACY_STUDY) : tr(STR_FOCUS_ONLY_THIS_FOCUS), false, wide ? 1 : 3);
}
const char* cleanupName(focus::CompletedScope scope) {
  switch (scope) {
    case focus::CompletedScope::Today:
      return tr(STR_TODAY);
    case focus::CompletedScope::Projects:
      return tr(STR_FOCUS_PROJECTS);
    case focus::CompletedScope::Tasks:
      return tr(STR_FOCUS_ALL_TASKS);
    case focus::CompletedScope::All:
      return tr(STR_FOCUS_CLEANUP_ALL);
  }
  return "";
}
void statistics(toybox::Screen& screen, fui::Rect body, const Model& model, Work& work) {
  const auto& state = *model.state;
  const bool wide = body.width > body.height;
  const int bottom = body.bottom();
  unsigned sessions = 0, activeDays = 0;
  uint32_t milliseconds = 0, maximum = 0;
  for (unsigned offset = 0; offset < 7; ++offset) {
    const auto* day = focus::onDay(state, model.day > offset ? model.day - offset : 0);
    if (!day) continue;
    sessions += day->sessions;
    milliseconds += day->milliseconds;
    maximum = std::max(maximum, day->milliseconds);
    activeDays += day->milliseconds != 0;
  }
  const auto period = fui::makeRect(body.x, body.y, body.width, wide ? 30 : 40);
  dashboard::icon(screen, fui::makeRect(period.x + 6, period.y + 3, 28, 28), extra_calendar_32);
  copy(screen, fui::makeRect(period.x + 46, period.y, period.width - 52, period.height), tr(STR_FOCUS_LAST_SEVEN),
       true);
  body.y += period.height + 12;
  const auto summary = fui::makeRect(body.x, body.y, body.width, wide ? 88 : 116);
  card(screen, summary);
  const freeink::Icon* marks[] = {&extra_clock_32, &extra_clock_32, &extra_chart_32};
  const StrId names[] = {StrId::STR_FOCUS_SESSIONS, StrId::STR_FOCUS_TIME_SHORT, StrId::STR_FOCUS_CONSISTENCY};
  for (int n = 0; n < 3; ++n) {
    const auto cell = lightui::segmentRect(summary, n, 3);
    if (n)
      screen.target().line({cell.x, int16_t(cell.y + 16)}, {cell.x, int16_t(cell.bottom() - 16)}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    const int size = wide ? 22 : 28;
    dashboard::icon(screen, fui::makeRect(cell.x + (cell.width - size) / 2, cell.y + 10, size, size), *marks[n]);
    if (n == 0)
      std::snprintf(work.label, sizeof(work.label), "%u", sessions);
    else if (n == 1)
      duration(work.label, sizeof(work.label), milliseconds / 60000);
    else
      std::snprintf(work.label, sizeof(work.label), "%u %%", activeDays * 100 / 7);
    const int valueY = cell.y + (wide ? 33 : 48);
    copy(screen, fui::makeRect(cell.x + 5, valueY, cell.width - 10, wide ? 22 : 30), work.label, true, 1,
         fui::TextAlign::Center);
    copy(screen, fui::makeRect(cell.x + 4, valueY + (wide ? 22 : 30), cell.width - 8, 28), I18N.get(names[n]), false, 1,
         fui::TextAlign::Center);
  }
  const int titleY = summary.bottom() + (wide ? 10 : 22);
  copy(screen, fui::makeRect(body.x + 4, titleY, body.width - 8, 32), tr(STR_FOCUS_TIME_PER_DAY), true);
  const int chartY = titleY + 40;
  const int noteHeight = wide ? 72 : 88, noteGap = wide ? 46 : 66;
  const int plotBottom = std::min(chartY + (wide ? 80 : 234), bottom - noteHeight - noteGap - 8);
  const auto plot = fui::makeRect(body.x + 64, chartY + 12, body.width - 74, std::max(32, plotBottom - chartY - 12));
  const unsigned maxMinutes = (maximum + 59999) / 60000;
  const unsigned ceiling = maxMinutes <= 120 ? 120 : maxMinutes <= 240 ? 240 : ((maxMinutes + 239) / 240) * 240;
  for (unsigned tick : {0u, ceiling / 4, ceiling / 2, ceiling}) {
    if (plot.height < 100 && tick && tick != ceiling) continue;
    const int y = plot.bottom() - int(uint64_t(plot.height) * tick / ceiling);
    for (int x = plot.x; x < plot.right(); x += 8)
      screen.target().line({int16_t(x), int16_t(y)}, {int16_t(std::min(x + 3, int(plot.right()))), int16_t(y)}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    if (tick >= 60 && tick % 60 == 0)
      std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_AXIS_HOURS), tick / 60);
    else
      std::snprintf(work.label, sizeof(work.label), tick ? tr(STR_FOCUS_AXIS_MINUTES) : "%u", tick);
    copy(screen, fui::makeRect(body.x, y - 12, 54, 24), work.label, false, 1, fui::TextAlign::Right);
  }
  static constexpr StrId weekdays[] = {
      StrId::STR_STATS_WEEKDAY_0, StrId::STR_STATS_WEEKDAY_1, StrId::STR_STATS_WEEKDAY_2, StrId::STR_STATS_WEEKDAY_3,
      StrId::STR_STATS_WEEKDAY_4, StrId::STR_STATS_WEEKDAY_5, StrId::STR_STATS_WEEKDAY_6};
  for (unsigned n = 0; n < 7; ++n) {
    const uint32_t date = model.day > 6 - n ? model.day - (6 - n) : 0;
    const auto* day = focus::onDay(state, date);
    const auto cell = lightui::segmentRect(plot, n, 7);
    const int height = day && day->milliseconds
                           ? std::max(1, int(uint64_t(plot.height) * day->milliseconds / (uint64_t(ceiling) * 60000)))
                           : 0;
    if (height) {
      const int barWidth = std::min(34, int(cell.width) - 14);
      const auto bar = fui::makeRect(cell.x + (cell.width - barWidth) / 2, plot.bottom() - height, barWidth, height);
      screen.target().fill(bar, fui::Paint::dither(fui::Color::LightGray));
      screen.target().stroke(bar, dashboard::ink, 1);
    }
    copy(screen, fui::makeRect(cell.x, plot.bottom() + 8, cell.width, 28),
         date ? I18N.get(weekdays[(date + 3) % 7]) : "-", true, 1, fui::TextAlign::Center);
    if (n == 6)
      screen.target().line({int16_t(cell.x + cell.width / 2 - 7), int16_t(plot.bottom() + 37)},
                           {int16_t(cell.x + cell.width / 2 + 7), int16_t(plot.bottom() + 37)}, 2, dashboard::ink);
  }
  screen.target().line({plot.x, plot.bottom()}, {plot.right(), plot.bottom()}, 1, dashboard::ink);
  const auto note = fui::makeRect(body.x + 4, plot.bottom() + noteGap, body.width - 8, noteHeight);
  card(screen, note);
  dashboard::icon(screen, fui::makeRect(note.x + 16, note.y + (note.height - 32) / 2, 32, 32), extra_trophy_32);
  copy(screen, fui::makeRect(note.x + 64, note.y + 8, note.width - 76, 30),
       activeDays ? tr(STR_FOCUS_GOOD_RHYTHM) : tr(STR_FOCUS_FIRST_SESSION), true);
  std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_ACTIVE_DAYS), activeDays);
  copy(screen, fui::makeRect(note.x + 64, note.y + (wide ? 38 : 42), note.width - 76, wide ? 28 : 38),
       activeDays ? work.label : tr(STR_FOCUS_START_BUILDING), false, wide ? 1 : 2);
}
void settings(toybox::Screen& screen, fui::Rect body, const focus::State& state, Work& work) {
  const bool wide = body.width > body.height;
  const auto group = timerui::durations(screen, body, state.timer.config, Change, work.label, sizeof(work.label));
  const int sectionY = group.bottom() + (wide ? 10 : 24);
  copy(screen, fui::makeRect(body.x + 4, sectionY, body.width - 8, wide ? 24 : 30), tr(STR_FOCUS_COMPLETED_TASKS),
       true);
  const auto clear = fui::makeRect(body.x, sectionY + (wide ? 30 : 40), body.width, wide ? 60 : 76);
  card(screen, clear);
  const int iconX = clear.x + 14, iconY = clear.y + (clear.height - 32) / 2;
  screen.target().stroke(fui::makeRect(iconX + 6, iconY + 10, 22, 22), dashboard::ink, 2, chrome::lightHeader ? 2 : 0);
  screen.target().stroke(fui::makeRect(iconX + 12, iconY + 3, 10, 6), dashboard::ink, 2);
  screen.target().line({int16_t(iconX + 3), int16_t(iconY + 9)}, {int16_t(iconX + 31), int16_t(iconY + 9)}, 2,
                       dashboard::ink);
  for (int x : {iconX + 13, iconX + 21})
    screen.target().line({int16_t(x), int16_t(iconY + 16)}, {int16_t(x), int16_t(iconY + 26)}, 2, dashboard::ink);
  copy(screen, fui::makeRect(clear.x + 58, clear.y + (wide ? 3 : 7), clear.width - 98, 30),
       tr(STR_FOCUS_CLEAR_COMPLETED), true);
  copy(screen, fui::makeRect(clear.x + 58, clear.y + (wide ? 31 : 41), clear.width - 98, 24),
       tr(STR_FOCUS_RETAIN_TOTALS));
  dashboard::chevron(screen.target(), fui::makeRect(clear.right() - 30, clear.y, 20, clear.height));
  dashboard::hit(screen, clear, ClearCompleted);
  const int inset = wide ? body.width / 4 : 32;
  const auto done = fui::makeRect(body.x + inset, body.bottom() - 52, body.width - 2 * inset, 52);
  screen.target().fill(done, dashboard::ink, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
  fui::TextStyle style;
  style.font = pixelui::kSmall;
  style.color = fui::Color::White;
  style.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(screen.target(), done, style), tr(STR_FOCUS_DONE), style);
  dashboard::hit(screen, done, Done);
}
}  // namespace
int pageSize(const fui::DeviceContext& device, const Model& model) {
  const bool wide = device.width > device.height;
  return capacity(sections(contentBody(device, model), wide, model.view).body, wide, model.view == View::Projects);
}
bool projectMatches(const focus::State& state, int index, Filter filter) {
  if (index < 0 || index >= focus::kProjects || !state.projects[index].used) return false;
  switch (filter) {
    case Filter::All:
      return true;
    case Filter::Completed: {
      const auto totals = focus::totals(state, focus::TaskList::Project, index);
      return totals.tasks && totals.done == totals.tasks;
    }
  }
  return false;
}
int count(const Model& model) {
  int total = 0;
  if (model.view == View::Projects) {
    for (int n = 0; n < focus::kProjects; ++n) total += projectMatches(*model.state, n, model.filter);
  } else {
    for (const auto& task : model.state->tasks) total += taskMatches(task, model, model.filter);
  }
  return total;
}
void build(toybox::Screen& screen, const Model& model, Work& work) {
  const auto& state = *model.state;
  const bool timerView = model.view == View::Timer || model.view == View::ConfirmTimerChange;
  pixelui::Header header;
  header.title = model.view == View::Today                                    ? tr(STR_TODAY)
                 : model.view == View::Settings                               ? tr(STR_SETTINGS_TITLE)
                 : timerView                                                  ? tr(STR_FOCUS_CONCENTRATION)
                 : model.view == View::Detail                                 ? tr(STR_SETTINGS_TITLE)
                 : model.view == View::Projects                               ? tr(STR_FOCUS_PROJECTS)
                 : model.view == View::AllTasks                               ? tr(STR_FOCUS_ALL_TASKS)
                 : model.view == View::Stats || model.view == View::TaskStats ? tr(STR_FOCUS_SUMMARY)
                 : model.view == View::CleanupScope                           ? tr(STR_FOCUS_COMPLETED_TASKS)
                 : model.view == View::Menu                                   ? tr(STR_FOCUS_APP)
                 : model.view == View::Tasks && model.project >= 0            ? state.projects[model.project].name
                                                                              : tr(STR_FOCUS_APP);
  header.back = true;
  if (model.view == View::Menu || model.view == View::AllTasks || model.view == View::Today ||
      model.view == View::Projects || model.view == View::Stats || model.view == View::Tasks || timerView ||
      model.view == View::TaskStats) {
    header.mark = fui::bitmapFromIcon(icon_go_settings_32);
    header.markAction = model.view == View::Tasks                    ? EditProject
                        : timerView || model.view == View::TaskStats ? EditTask
                                                                     : Settings;
  }
  if (timerView) {
    header.secondaryMark = fui::bitmapFromIcon(extra_chart_32);
    header.secondaryMarkAction = OpenTaskStats;
  }
  pixelui::header(screen, header);
  backui::publish(true, pixelui::geometry(screen.device()).back);
  auto body = pixelui::geometry(screen.device()).body;
  const bool wide = body.width > body.height;
  if (model.saveFailed || model.message) {
    label(screen, fui::makeRect(body.x, body.y, body.width, 34),
          model.message ? model.message : tr(STR_WORDLE_SAVE_FAILED), pixelui::kSmall);
    body.y += 38;
    body.height -= 38;
  }
  if (model.view == View::Menu) {
    const StrId names[] = {StrId::STR_TODAY, StrId::STR_FOCUS_PROJECTS, StrId::STR_FOCUS_ALL_TASKS,
                           StrId::STR_FOCUS_SUMMARY};
    const freeink::Icon* icons[] = {&extra_calendar_32, &icon_apps_32, &extra_check_32, &extra_chart_32};
    unsigned counts[4]{};
    for (const auto& project : state.projects) counts[1] += project.used;
    for (const auto& task : state.tasks) {
      counts[0] += focus::belongsTo(task, focus::TaskList::Today) && !task.done;
      counts[2] += focus::belongsTo(task, focus::TaskList::Tasks) && !task.done;
    }
    const auto area = body.inset({0, 8, 0, 8});
    const bool grid = wide;
    const int menuX = area.x, menuY = area.y, menuWidth = area.width;
    const int rowHeight = wide ? 64 : 76;
    for (int n = 0; n < 4; ++n) {
      const auto row =
          fui::makeRect(menuX + (grid ? (n % 2) * (menuWidth + 12) / 2 : 0), menuY + (grid ? n / 2 : n) * rowHeight,
                        grid ? (menuWidth - 12) / 2 : menuWidth, rowHeight - 12);
      card(screen, row);
      const auto iconBox = fui::makeRect(row.x + 14, row.y + (row.height - 28) / 2, 28, 28);
      if (n == 1) {
        for (int layer = 0; layer < 3; ++layer) {
          const int x = iconBox.x + 14, y = iconBox.y + 5 + layer * 7;
          screen.target().line({int16_t(x - 13), int16_t(y)}, {int16_t(x), int16_t(y - 5)}, 2, dashboard::ink);
          screen.target().line({int16_t(x), int16_t(y - 5)}, {int16_t(x + 13), int16_t(y)}, 2, dashboard::ink);
          screen.target().line({int16_t(x + 13), int16_t(y)}, {int16_t(x), int16_t(y + 6)}, 2, dashboard::ink);
          screen.target().line({int16_t(x), int16_t(y + 6)}, {int16_t(x - 13), int16_t(y)}, 2, dashboard::ink);
        }
      } else {
        if (n == 2) screen.target().stroke(iconBox, dashboard::ink, 2, chrome::lightHeader ? 3 : 0);
        dashboard::icon(screen, iconBox.inset(n == 2 ? fui::Insets{3, 3, 3, 3} : fui::Insets{}), *icons[n]);
      }
      copy(screen, fui::makeRect(row.x + 60, row.y, row.width - (grid ? 74 : 142), row.height), I18N.get(names[n]),
           true);
      std::snprintf(work.label, sizeof(work.label), "%u", counts[n]);
      if (n < 3 && !grid)
        copy(screen, fui::makeRect(row.right() - 78, row.y, 42, row.height), work.label, false, 1,
             fui::TextAlign::Right);
      if (!grid) dashboard::chevron(screen.target(), fui::makeRect(row.right() - 28, row.y, 20, row.height));
      dashboard::hit(screen, row, Overview, n == 2 ? 3 : n == 3 ? 2 : n);
    }
    const int startWidth = wide ? area.width / 2 : area.width - 64;
    const auto start = fui::makeRect(area.x + (area.width - startWidth) / 2, body.bottom() - 68, startWidth, 60);
    screen.target().fill(start, dashboard::ink, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
    fui::TextStyle startStyle;
    startStyle.font = pixelui::kSmall;
    startStyle.color = fui::Color::White;
    const int textWidth = screen.target().measureText(startStyle.font, tr(STR_FOCUS_START_FOCUS), startStyle).width;
    const int contentX = start.x + (start.width - textWidth - 38) / 2;
    const auto black = fui::Paint::solid(fui::Color::White);
    screen.target().triangle({int16_t(contentX), int16_t(start.y + 18)}, {int16_t(contentX), int16_t(start.y + 42)},
                             {int16_t(contentX + 19), int16_t(start.y + 30)}, black);
    const auto startText = fui::makeRect(contentX + 38, start.y, textWidth + 4, start.height);
    screen.target().text(toybox::inkCentred(screen.target(), startText, startStyle), tr(STR_FOCUS_START_FOCUS),
                         startStyle);
    dashboard::hit(screen, start, QuickStart, -1);
    return;
  }
  const bool projectView = model.view == View::Tasks;
  const auto areas = sections(body, wide, model.view);
  if (projectView) {
    const auto row = areas.identity;
    card(screen, row);
    folder(screen, fui::makeRect(row.x + 18, row.y + (row.height - 36) / 2, 38, 36));
    copy(screen, fui::makeRect(row.x + 76, row.y + 3, row.width - 144, row.height - 6),
         state.projects[model.project].name, true);
    editControl(screen, fui::makeRect(row.right() - 56, row.y + (row.height - 44) / 2, 44, 44), EditProject);
  }
  if (model.view == View::Today || model.view == View::AllTasks || projectView) {
    const auto summary = areas.summary;
    card(screen, summary);
    const auto* day = focus::onDay(state, model.day);
    const auto totals = focus::totals(state, taskList(model.view), model.project);
    const bool today = model.view == View::Today;
    const unsigned completed = today ? (day ? day->todaySessions : 0) : totals.sessions;
    const unsigned planned = totals.planned, done = totals.done, tasks = totals.tasks;
    const StrId names[] = {StrId::STR_FOCUS_SESSIONS, today ? StrId::STR_FOCUS_TIME : StrId::STR_FOCUS_PROGRESS,
                           StrId::STR_FOCUS_ALL_TASKS};
    for (int n = 0; n < 3; ++n) {
      const auto cell = lightui::segmentRect(summary, n, 3);
      const freeink::Icon* marks[] = {&extra_clock_32, today ? &extra_clock_32 : &extra_chart_32, &extra_check_32};
      const int iconSize = wide ? 20 : 28;
      const auto iconBox =
          fui::makeRect(cell.x + (cell.width - iconSize) / 2, cell.y + (wide ? 8 : 12), iconSize, iconSize);
      dashboard::icon(screen, iconBox, *marks[n]);
      if (n == 2) screen.target().stroke(iconBox, dashboard::ink, 2, chrome::lightHeader ? 2 : 0);
      if (n)
        screen.target().line({cell.x, int16_t(cell.y + 10)}, {cell.x, int16_t(cell.bottom() - 10)}, 1,
                             fui::Paint::dither(fui::Color::DarkGray));
      if (n == 0)
        std::snprintf(work.label, sizeof(work.label), "%u / %u", completed, planned);
      else if (n == 1) {
        if (!today)
          std::snprintf(work.label, sizeof(work.label), "%u %%",
                        planned ? unsigned(uint64_t(100) * std::min(completed, planned) / planned) : 0);
        else
          std::snprintf(work.label, sizeof(work.label), tr(STR_SLEEP_TIMER_VALUE_FORMAT),
                        unsigned(day ? day->todayMilliseconds / 60000 : 0));
      } else if (!today)
        std::snprintf(work.label, sizeof(work.label), "%u / %u", done, tasks);
      else
        std::snprintf(work.label, sizeof(work.label), "%u", tasks - done);
      const int valueY = wide ? cell.y + 34 : cell.y + 43;
      copy(screen, fui::makeRect(cell.x + 4, valueY, cell.width - 8, wide ? 22 : 28), work.label, true, 1,
           fui::TextAlign::Center);
      copy(screen, fui::makeRect(cell.x + 4, valueY + (wide ? 22 : 28), cell.width - 8, wide ? 20 : 24),
           I18N.get(names[n]), false, 1, fui::TextAlign::Center);
    }
  }
  body = areas.body;
  if (model.view == View::Stats) {
    statistics(screen, body, model, work);
    return;
  }
  if (model.view == View::Today || model.view == View::AllTasks || model.view == View::Projects || projectView) {
    const bool projects = model.view == View::Projects;
    const int total = count(model), capacity = pageSize(screen.device(), model);
    const int pages = std::max(1, (total + capacity - 1) / capacity);
    const int page = std::clamp(model.page, 0, pages - 1);
    const int addHeight = projects ? 64 : 0, pageHeight = 50;
    int top = body.y;
    {
      const auto tabs = fui::makeRect(body.x + 4, top, body.width - 8, 48);
      const StrId titles[] = {StrId::STR_FOCUS_PROJECTS_ALL, StrId::STR_FOCUS_PROJECTS_COMPLETED};
      unsigned counts[2]{};
      if (projects) {
        for (int n = 0; n < focus::kProjects; ++n) {
          counts[0] += projectMatches(state, n, Filter::All);
          counts[1] += projectMatches(state, n, Filter::Completed);
        }
      } else {
        for (const auto& task : state.tasks) {
          counts[0] += taskMatches(task, model, Filter::All);
          counts[1] += taskMatches(task, model, Filter::Completed);
        }
      }
      for (int n = 0; n < 2; ++n) {
        const auto cell = lightui::segmentRect(tabs, n, 2);
        const bool selected = screen.frame().buttonSelected(SetFilter, n, model.filter == static_cast<Filter>(n));
        screen.target().fill(cell, selected ? dashboard::ink : dashboard::paper,
                             chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
        std::snprintf(work.label, sizeof(work.label), "%s (%u)", I18N.get(titles[n]), counts[n]);
        fui::TextStyle style;
        style.font = pixelui::kSmall;
        style.align = fui::TextAlign::Center;
        style.color = selected ? fui::Color::White : fui::Color::Black;
        if (screen.target().measureText(style.font, work.label, style).width > cell.width - 12) style.font = 3;
        screen.target().text(toybox::inkCentred(screen.target(), cell, style), work.label, style);
        dashboard::hit(screen, cell, SetFilter, n);
      }
      card(screen, tabs);
      top += 66;
    }
    if (!projects) {
      std::snprintf(work.label, sizeof(work.label), "%s (%d)",
                    model.filter == Filter::Completed ? tr(STR_FOCUS_COMPLETED_TASKS)
                    : model.view == View::Today       ? tr(STR_FOCUS_TODAY_TASKS)
                                                      : tr(STR_FOCUS_ALL_TASKS),
                    total);
      copy(screen, fui::makeRect(body.x + 6, top, body.width - 142, 44), work.label, true);
      addControl(screen, fui::makeRect(body.right() - 126, top, 126, 44));
      top += 48;
    }
    const int doneSpace = 0;
    const int rowHeight = std::min(projects ? (wide ? 90 : 96) : (wide ? 64 : 82),
                                   (body.bottom() - addHeight - pageHeight - doneSpace - top) / capacity);
    int visible = 0, found = 0;
    const int onPage = std::min(capacity, std::max(0, total - page * capacity));
    const auto group = fui::makeRect(body.x, top, body.width, onPage * rowHeight);
    if (!projects && onPage) card(screen, group);
    for (int n = 0; n < (projects ? focus::kProjects : focus::kTasks); ++n) {
      const bool used =
          projects ? projectMatches(state, n, model.filter) : taskMatches(state.tasks[n], model, model.filter);
      if (!used || found++ < page * capacity) continue;
      if (visible == capacity) break;
      const auto row = fui::makeRect(body.x, top + visible * rowHeight, body.width, rowHeight - (projects ? 12 : 0));
      if (projects)
        card(screen, row);
      else if (visible)
        screen.target().line({int16_t(row.x + 1), row.y}, {int16_t(row.right() - 1), row.y}, 1,
                             fui::Paint::dither(fui::Color::DarkGray));
      if (!projects) {
        taskRow(screen, row, state.tasks[n], n, work);
        ++visible;
        continue;
      }
      dashboard::hit(screen, row, Open, n);
      unsigned completed = 0, planned = 0, tasks = 0;
      if (projects) {
        folder(screen, fui::makeRect(row.x + 14, row.y + 10, 36, 34));
        const auto totals = focus::totals(state, focus::TaskList::Project, n);
        completed = totals.sessions;
        planned = totals.planned;
        tasks = totals.tasks;
      }
      const int titleX = row.x + 62, titleWidth = wide ? row.width / 2 - 76 : row.width - 108;
      copy(screen, fui::makeRect(titleX, row.y + 4, titleWidth, 28),
           projects ? state.projects[n].name : state.tasks[n].name, true);
      if (projects) {
        std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_PROJECT_STATS), tasks, completed);
        const int metaX = wide ? row.x + row.width / 2 : titleX;
        copy(screen, fui::makeRect(metaX, row.y + (wide ? 6 : 32), row.right() - metaX - 42, 24), work.label);
        const auto bar = fui::makeRect(titleX, row.bottom() - 18, row.width - 166, 8);
        progress(screen, bar, completed, planned);
        std::snprintf(work.label, sizeof(work.label), "%u %%",
                      planned ? unsigned(uint64_t(100) * std::min(completed, planned) / planned) : 0);
        copy(screen, fui::makeRect(row.right() - 90, row.bottom() - 34, 72, 28), work.label, false, 1,
             fui::TextAlign::Right);
      }
      dashboard::chevron(screen.target(), fui::makeRect(row.right() - 29, row.y + 8, 20, 32));
      ++visible;
    }
    if (!total)
      copy(screen, fui::makeRect(body.x + 12, top + 12, body.width - 24, 76),
           projects ? (model.filter == Filter::Completed ? tr(STR_FOCUS_NO_COMPLETED_PROJECTS)
                                                         : tr(STR_FOCUS_EMPTY_PROJECTS))
           : model.filter == Filter::Completed ? tr(STR_FOCUS_NO_COMPLETED)
                                               : tr(STR_FOCUS_NO_PENDING),
           false, 3);
    if (pages > 1) {
      auto pageBody = body;
      if (projects) pageBody.height -= 68;
      pager(screen, pageBody, page, pages, work);
    }
    if (projects)
      button(screen, fui::makeRect(body.x, body.bottom() - 54, body.width, 54), tr(STR_FOCUS_ADD_PROJECT), Add, 0,
             true);
    return;
  }
  if (model.view == View::ConfirmDelete) {
    if (model.deleteTarget == DeleteTarget::Completed)
      std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_CLEAR_SCOPE_CONFIRM), cleanupName(model.cleanupScope));
    copy(screen, fui::makeRect(body.x + 12, body.y + 24, body.width - 24, std::min(160, int(body.height) - 100)),
         model.deleteTarget == DeleteTarget::Completed ? work.label
         : model.deleteTarget == DeleteTarget::Project ? tr(STR_FOCUS_DELETE_PROJECT_CONFIRM)
                                                       : tr(STR_FOCUS_DELETE_TASK_CONFIRM),
         false, 5, fui::TextAlign::Center);
    const int width = (body.width - 12) / 2;
    button(screen, fui::makeRect(body.x, body.bottom() - 64, width, 60), tr(STR_WORDLE_CANCEL), Back);
    button(screen, fui::makeRect(body.x + width + 12, body.bottom() - 64, width, 60), tr(STR_FOCUS_DELETE),
           ConfirmDelete, 0, true);
    return;
  }
  if (model.view == View::CleanupScope) {
    copy(screen, fui::makeRect(body.x + 6, body.y, body.width - 12, wide ? 44 : 64), tr(STR_FOCUS_CLEANUP_WHERE), true,
         2);
    const int top = body.y + (wide ? 72 : 106), gap = 12;
    const int height = std::min(wide ? 56 : 72, (int(body.bottom()) - top - 3 * gap) / 4);
    for (int n = 0; n < 4; ++n) {
      const auto scope = static_cast<focus::CompletedScope>(n);
      std::snprintf(work.label, sizeof(work.label), "%s (%d)", cleanupName(scope), focus::completedCount(state, scope));
      button(screen, fui::makeRect(body.x, top + n * (height + gap), body.width, height), work.label, ChooseCleanup, n);
    }
    return;
  }
  if (model.view == View::Settings) {
    settings(screen, body, state, work);
    return;
  }
  if (model.view == View::ProjectActions) {
    button(screen, fui::makeRect(body.x, body.y + 20, body.width, 64), tr(STR_FOCUS_RENAME), Rename);
    button(screen, fui::makeRect(body.x, body.y + 96, body.width, 64), tr(STR_FOCUS_DELETE), Delete);
    return;
  }
  if (model.task < 0 || model.task >= focus::kTasks) return;
  const auto& task = state.tasks[model.task];
  if (model.view == View::TaskStats) {
    taskStatistics(screen, body, model, work);
    return;
  }
  if (model.view == View::Detail) {
    const int titleHeight = taskTitle(screen, body, task.name, 68);
    const int height = wide ? 48 : 60, top = body.y + titleHeight + 12;
    button(screen, fui::makeRect(body.x, top, body.width, height), tr(STR_FOCUS_TIMER_SETTINGS), Settings);
    button(screen, fui::makeRect(body.x, top + height + 12, body.width, height), tr(STR_FOCUS_RENAME), Rename);
    button(screen, fui::makeRect(body.x, top + 2 * (height + 12), body.width, height),
           task.done ? tr(STR_FOCUS_REOPEN) : tr(STR_FOCUS_COMPLETE), Complete);
    button(screen, fui::makeRect(body.x, top + 3 * (height + 12), body.width, height), tr(STR_FOCUS_DELETE), Delete);
    return;
  }
  const int dialWidth = wide ? std::min(int(body.height), int(body.width) * 43 / 100) : body.width;
  const auto info =
      wide ? fui::makeRect(body.x + dialWidth + 20, body.y, body.width - dialWidth - 20, body.height) : body;
  const int titleHeight = taskTitle(screen, info, task.name, wide ? 68 : 76, true);
  const int badgesY = info.y + titleHeight + 8;
  const auto sessions = fui::makeRect(info.x, badgesY, info.width, 60);
  card(screen, sessions);
  const auto minus = fui::makeRect(sessions.x + 8, sessions.y + 8, 44, 44);
  const auto plus = fui::makeRect(sessions.right() - 52, sessions.y + 8, 44, 44);
  button(screen, minus, "-", Planned, -1);
  button(screen, plus, "+", Planned, 1);
  fui::TextStyle counterStyle;
  counterStyle.font = pixelui::kSmall;
  counterStyle.bold = true;
  const int counterWidth = screen.target().measureText(counterStyle.font, "1000 / 99", counterStyle).width + 4;
  const int dotsX = sessions.x + 64, dotsWidth = sessions.width - counterWidth - 128;
  copy(screen, fui::makeRect(dotsX, sessions.y + 2, dotsWidth, 26), tr(STR_FOCUS_SESSION));
  const int capacity = std::clamp((dotsWidth - 36) / 18, 1, 12);
  const int visible = std::min(capacity, int(task.planned));
  const int first = std::clamp(int(task.completed) - visible + 1, 0, int(task.planned) - visible);
  if (first) copy(screen, fui::makeRect(dotsX, sessions.y + 30, 18, 24), "…");
  for (int n = 0; n < visible; ++n) {
    const auto dot = fui::makeRect(dotsX + 18 + n * 18, sessions.y + 36, 11, 11);
    if (first + n < task.completed)
      screen.target().fill(dot, dashboard::ink, 6);
    else
      screen.target().stroke(dot, dashboard::ink, 1, 6);
  }
  if (first + visible < task.planned)
    copy(screen, fui::makeRect(dotsX + 18 + visible * 18, sessions.y + 30, 18, 24), "…");
  std::snprintf(work.label, sizeof(work.label), "%u / %u", unsigned(task.completed) + 1, task.planned);
  copy(screen, fui::makeRect(sessions.right() - counterWidth - 58, sessions.y + 8, counterWidth, 44), work.label, true,
       1, fui::TextAlign::Center);

  const int footerY = info.bottom() - (wide ? 112 : 166);
  const int timelineHeight = std::max(wide ? 52 : 56, 2 * int(screen.target().lineHeight(pixelui::kSmall)) + 8);
  const auto timeline = fui::makeRect(info.x, sessions.bottom() + 12, info.width, timelineHeight);
  const int stages = state.timer.config.rounds * 2;
  const int visibleStages = std::min(3, stages);
  const int firstStage = std::clamp(int(state.timer.stage) - 1, 0, stages - visibleStages);
  screen.target().fill(timeline, dashboard::paper, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
  for (int n = 0; n < visibleStages; ++n) {
    const int index = firstStage + n;
    const auto phase = pomodoro::phaseAt(state.timer.config, index);
    std::snprintf(work.label, sizeof(work.label), tr(STR_POMODORO_STAGE_FORMAT), index / 2 + 1,
                  phase == pomodoro::Phase::Focus       ? tr(STR_POMODORO_STAGE_FOCUS)
                  : phase == pomodoro::Phase::LongBreak ? tr(STR_POMODORO_STAGE_LONG)
                                                        : tr(STR_POMODORO_STAGE_BREAK),
                  static_cast<unsigned long>(pomodoro::duration(state.timer.config, phase) / 60000));
    const bool selected = index == state.timer.stage;
    const auto cell = lightui::segmentRect(timeline, n, visibleStages);
    if (selected)
      screen.target().fill(cell.inset({2, 2, 2, 2}), dashboard::ink, chrome::lightHeader ? lightui::kControlRadius : 0);
    if (n && !selected && index - 1 != state.timer.stage)
      screen.target().line({cell.x, int16_t(cell.y + 10)}, {cell.x, int16_t(cell.bottom() - 10)}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    fui::TextStyle style;
    style.font = pixelui::kSmall;
    style.align = fui::TextAlign::Center;
    style.maxLines = 1;
    style.color = selected ? fui::Color::White : fui::Color::Black;
    const int line = screen.target().lineHeight(style.font);
    const int top = cell.y + (cell.height - 2 * line) / 2;
    if (char* duration = std::strchr(work.label, '\n')) {
      *duration++ = '\0';
      screen.target().text(fui::makeRect(cell.x + 6, top, cell.width - 12, line), work.label, style);
      screen.target().text(fui::makeRect(cell.x + 6, top + line, cell.width - 12, line), duration, style);
    }
  }
  card(screen, timeline);
  const auto dialArea =
      wide ? fui::makeRect(body.x, body.y, dialWidth, body.height)
           : fui::makeRect(body.x, timeline.bottom() + 12, body.width, footerY - timeline.bottom() - 24);
  const uint32_t seconds = (state.timer.remainingMs + 999) / 1000;
  std::snprintf(work.label, sizeof(work.label), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60),
                static_cast<unsigned long>(seconds % 60));
  const int totalsHeight = 62;
  auto clockArea = dialArea;
  clockArea.height = std::max(32, int(clockArea.height) - totalsHeight - 12);
  pomodoroui::drawCountdown(screen.target(), clockArea.inset({12, 8, 12, 8}), work.label, wide ? 120 : 132);
  const auto totals =
      fui::makeRect(dialArea.x + 8, dialArea.bottom() - totalsHeight, dialArea.width - 16, totalsHeight);
  for (int n = 0; n < 2; ++n) {
    const auto cell = lightui::segmentRect(totals, n, 2);
    copy(screen, fui::makeRect(cell.x, cell.y, cell.width, 26), n ? tr(STR_FOCUS_PLANNED_TIME) : tr(STR_FOCUS_STUDIED),
         false, 1, fui::TextAlign::Center);
    if (n)
      duration(work.label, sizeof(work.label), task.planned * state.timer.config.focus);
    else
      studyDuration(work.label, sizeof(work.label), task.study.seconds);
    copy(screen, fui::makeRect(cell.x, cell.y + 28, cell.width, 32), work.label, true, 1, fui::TextAlign::Center);
  }
  const int half = (info.width - 12) / 2;
  const auto toggle = fui::makeRect(info.x, footerY, wide ? half : info.width, 52);
  screen.target().fill(toggle, dashboard::ink, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
  const char* toggleLabel = state.timer.running ? tr(STR_UI_PAUSE_9935DF) : tr(STR_FOCUS_START);
  fui::TextStyle toggleStyle;
  toggleStyle.font = pixelui::kSmall;
  const int labelWidth = std::min(int(toggle.width) - 72,
                                  int(screen.target().measureText(toggleStyle.font, toggleLabel, toggleStyle).width));
  const int iconX = toggle.x + (toggle.width - labelWidth - 40) / 2;
  if (state.timer.running) {
    screen.target().fill(fui::makeRect(iconX, toggle.y + 17, 7, 22), dashboard::paper);
    screen.target().fill(fui::makeRect(iconX + 12, toggle.y + 17, 7, 22), dashboard::paper);
  } else {
    screen.target().triangle({int16_t(iconX), int16_t(toggle.y + 16)}, {int16_t(iconX), int16_t(toggle.y + 40)},
                             {int16_t(iconX + 20), int16_t(toggle.y + 28)}, dashboard::paper);
  }
  toggleStyle.color = fui::Color::White;
  const auto toggleText = fui::makeRect(iconX + 36, toggle.y, labelWidth + 4, toggle.height);
  screen.target().text(toybox::inkCentred(screen.target(), toggleText, toggleStyle), toggleLabel, toggleStyle);
  dashboard::hit(screen, toggle, Toggle);
  button(screen, fui::makeRect(info.x, footerY + 64, half, 44), tr(STR_UI_RESTART_69ED00), Reset);
  button(screen, fui::makeRect(info.x + half + 12, footerY + 64, half, 44), tr(STR_UI_SKIP_431DE8), Skip);
  button(screen,
         wide ? fui::makeRect(info.x + half + 12, footerY, half, 52)
              : fui::makeRect(info.x, footerY + 120, info.width, 44),
         tr(STR_FOCUS_COMPLETE_BUTTON), Complete);
}
}  // namespace focusui
