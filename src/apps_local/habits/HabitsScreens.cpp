#include "HabitsScreens.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/CompactArt.h"
#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/PageIndicator.h"
#include "../ui/ToyboxIcons.h"

namespace habitsui {
namespace {
int summaryHeight(const fui::DeviceContext& device) { return device.width > device.height ? 64 : 112; }

fui::TextStyle style(const fui::FontId font, const fui::TextAlign align = fui::TextAlign::Left,
                     const fui::Color color = fui::Color::Black) {
  fui::TextStyle out;
  out.font = font;
  out.align = align;
  out.color = color;
  return out;
}

void text(toybox::Screen& screen, const fui::Rect& rect, const char* label, const fui::FontId font = toybox::kUiFont,
          const fui::TextAlign align = fui::TextAlign::Left, const fui::Color color = fui::Color::Black) {
  screen.target().text(toybox::inkCentred(screen.target(), rect, style(font, align, color)), label,
                       style(font, align, color));
}

void button(toybox::Screen& screen, const fui::Rect& rect, const char* label, const fui::ActionId action,
            const int value = 0, const bool filled = false) {
  dashboard::button(screen, rect, label, action, value, filled);
}
void hit(toybox::Screen& screen, const fui::Rect& rect, const fui::ActionId action, const int value = 0) {
  dashboard::hit(screen, rect, action, value);
}
void chrome(toybox::Screen& screen, const char* title, const char* actionLabel = nullptr,
            const fui::ActionId action = fui::NO_ACTION, const freeink::Icon* icon = nullptr) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.back = screen.device().hasTouch;
  header.titleReserve = actionLabel ? 100 : icon ? 56 : 0;
  pixelui::header(screen.target(), screen.device(), header);
  const auto g = pixelui::geometry(screen.device());
  backui::publish(header.back, g.back);
  if (action && (actionLabel || icon)) {
    const int width = actionLabel ? 92 : 48;
    const auto box = fui::makeRect(screen.device().safeRect().right() - width - 12, g.header.bottom() - 60, width, 48);
    lightui::surface(screen.target(), box, dashboard::paper);
    if (chrome::lightHeader) lightui::outline(screen.target(), box, dashboard::ink);
    if (icon)
      dashboard::icon(screen, box.inset(fui::Insets{8, 8, 8, 8}), *icon);
    else
      dashboard::text(screen, box, actionLabel, 0);
    hit(screen, box, action);
  }
}
int habitIndex(const char* name) {
  static constexpr const char* identifiers[][2] = {{"Exercise", "Ejercicio"},
                                                   {"Eat mindfully", "Comer sano"},
                                                   {"Drink water", "Beber agua"},
                                                   {"Read", "Leer"},
                                                   {"Sleep on time", "Dormir a tiempo"}};
  for (int n = 0; n < 5; ++n)
    for (const char* id : identifiers[n])
      if (!std::strcmp(name, id)) return n;
  return -1;
}
const freeink::Icon& habitIcon(const char* name) {
  static constexpr const freeink::Icon* icons[] = {&extra_dumbbell_32, &extra_dashboard_apple_32, &extra_drop_32,
                                                   &extra_reading_book_32, &extra_moon_32};
  const int n = habitIndex(name);
  return n < 0 ? extra_list_32 : *icons[n];
}
const fui::BitmapRef* habitArt(const char* name) {
  static constexpr const fui::BitmapRef* art[] = {&compactart::HabitExercise, &compactart::HabitApple,
                                                  &compactart::HabitWater, &compactart::HabitRead,
                                                  &compactart::HabitMoon};
  const int n = habitIndex(name);
  return n < 0 ? nullptr : art[n];
}
void drawHabit(toybox::Screen& screen, fui::Rect rect, const char* name, bool inverse = false) {
  if (const auto* art = habitArt(name))
    screen.target().bitmap(rect, *art, fui::BitmapMode::Contain, inverse ? dashboard::paper : dashboard::ink);
  else
    dashboard::icon(screen, rect, habitIcon(name), inverse);
}

fui::Rect contentBand(const fui::DeviceContext& device) {
  return pixelui::geometry(device, 12).body.inset(fui::Insets{8, 4, 0, 4});
}
int habitPitch(const fui::DeviceContext& device) {
  const int rows = device.width > device.height ? 3 : 6;
  return std::max(64, (contentBand(device).height - summaryHeight(device) - 56) / rows);
}
void pages(toybox::Screen& screen, const fui::Rect& footer, int page, int count) {
  pageui::build(screen, footer, page + 1, count, ActionPage, -1, ActionPage, 1);
}
fui::Rect calendarPanel(const fui::DeviceContext& device) {
  const auto body = contentBand(device);
  const bool wide = body.width > body.height;
  return wide ? fui::makeRect(body.x + body.width * 2 / 5 + 16, body.y, body.width * 3 / 5 - 16, body.height - 106)
              : fui::makeRect(body.x, body.y + 220, body.width, body.height - 326);
}
fui::Rect calendarGrid(const fui::DeviceContext& device) {
  const auto panel = calendarPanel(device);
  return fui::makeRect(panel.x + 8, panel.y + 72, panel.width - 16, panel.height - 110);
}
int calendarRows(int first) {
  const int next = habits::shiftMonth(first, 1);
  const int days = next > first ? next - first : 31;
  return (habits::weekday(first) + days + 6) / 7;
}
bool editable(const DetailModel& model, int day) {
  return model.store && model.index >= 0 && model.index < model.store->count && habits::validDay(day) &&
         day >= model.store->items[model.index].createdDay && day >= habits::firstDay(*model.store) &&
         day <= model.today;
}

}  // namespace

int todayCapacity(const fui::DeviceContext& device) {
  const int rows = std::max(1, (contentBand(device).height - summaryHeight(device) - 48) / habitPitch(device));
  return std::min(6, rows * (device.width > device.height ? 2 : 1));
}
int presetCapacity(const fui::DeviceContext&) { return 6; }
void buildToday(toybox::Screen& screen, const TodayModel& model) {
  chrome(screen, tr(STR_UI_HABITS_630758), tr(STR_UI_NEW_27037C), ActionAdd);
  const auto body = contentBand(screen.device());
  const bool wide = body.width > body.height;
  const int day = model.displayedDay ? model.displayedDay : model.today;
  char label[96];
  int year, month, date;
  habits::dateOfDay(day, year, month, date);
  dashboard::date(label, sizeof(label), year, month, date);
  const int count = model.store ? habits::countOnDay(*model.store, day) : 0,
            done = model.store ? habits::doneCount(*model.store, day) : 0;
  const int arrow = 44, todayW = wide ? 104 : 104, progressW = wide ? 208 : body.width;
  const int navRight = wide ? body.right() - progressW - 8 : body.right();
  const auto previous = fui::makeRect(body.x, body.y, arrow, 48),
             next = fui::makeRect(navRight - todayW - arrow - 8, body.y, arrow, 48);
  button(screen, previous, "", ActionBrowseDay, -1);
  dashboard::chevron(screen.target(), previous, false, true);
  button(screen, fui::makeRect(previous.right() + 8, body.y, next.x - previous.right() - 16, 48), label, ActionDate);
  button(screen, next, "", ActionBrowseDay, 1);
  dashboard::chevron(screen.target(), next);
  button(screen, fui::makeRect(navRight - todayW, body.y, todayW, 48), tr(STR_TODAY), ActionBrowseDay, 0, true);
  const auto progress =
      wide ? fui::makeRect(navRight + 8, body.y, progressW, 48) : fui::makeRect(body.x, body.y + 60, body.width, 40);
  dashboard::frame(screen, progress, 2);
  const auto progressLabel = fui::makeRect(progress.x + 10, progress.y, progress.width - 104, progress.height);
  if (wide && day != model.today) {
    pixelui::copyCaps(label, sizeof(label), tr(STR_HABITS_DAILY_PROGRESS));
    fittext::draw(screen.target(), progressLabel, label, 3, 2);
  } else {
    dashboard::text(screen, progressLabel,
                    wide                 ? tr(STR_TODAY)
                    : day == model.today ? tr(STR_HABITS_TODAY_PROGRESS)
                                         : tr(STR_HABITS_DAILY_PROGRESS),
                    3, fui::TextAlign::Left);
  }
  std::snprintf(label, sizeof(label), "%d / %d", done, count);
  dashboard::digits(screen, fui::makeRect(progress.right() - 88, progress.y + 10, 78, progress.height - 20), label);
  const int capacity = todayCapacity(screen.device()), first = model.page * capacity;
  if (!count) {
    dashboard::text(screen, fui::makeRect(body.x, body.y + summaryHeight(screen.device()) + 8, body.width, 64),
                    tr(STR_UI_ONE_SMALL_STEP_D0FCF7), 0);
    dashboard::action(screen, fui::makeRect(body.x, body.bottom() - 58, body.width, 58),
                      tr(STR_UI_ADD_YOUR_FIRST_HABIT_0DC70E), ActionAdd, &extra_reading_book_32, true);
    return;
  }
  for (int n = 0; n < capacity && first + n < count; ++n) {
    const int index = habits::indexOnDay(*model.store, day, first + n);
    const auto& habit = model.store->items[index];
    const auto stat = habits::stats(*model.store, index, day);
    const bool checked = habits::checked(*model.store, index, day);
    const int columns = wide ? 2 : 1, width = (body.width - (columns - 1) * 12) / columns;
    const auto row = fui::makeRect(body.x + n % columns * (width + 12),
                                   body.y + summaryHeight(screen.device()) + n / columns * habitPitch(screen.device()),
                                   width, habitPitch(screen.device()) - 10);
    const bool focused = index == model.focused;
    if (focused) lightui::surface(screen.target(), row, dashboard::ink);
    dashboard::frame(screen, row, 2);
    hit(screen, fui::makeRect(row.x, row.y, 56, row.height), ActionToggleToday, index);
    hit(screen, fui::makeRect(row.x + 56, row.y, row.width - 56, row.height), ActionOpen, index);
    const auto check = fui::makeRect(row.x + 12, row.y + (row.height - 34) / 2, 34, 34);
    if (focused) screen.target().fill(check, dashboard::paper);
    dashboard::check(screen, check, checked);
    drawHabit(screen, fui::makeRect(row.x + 68, row.y + (row.height - 36) / 2, 36, 36), habit.name, focused);
    dashboard::text(screen, fui::makeRect(row.x + 118, row.y + 4, row.width - 158, 34), habit.name, 1,
                    fui::TextAlign::Left, focused);
    std::snprintf(label, sizeof(label), (stat.current == 1 ? tr(STR_HABITS_STREAK_DAY) : tr(STR_HABITS_STREAK_DAYS)),
                  unsigned(stat.current));
    dashboard::text(screen, fui::makeRect(row.x + 118, row.y + 38, row.width - 158, screen.target().lineHeight(4)),
                    label, 4, fui::TextAlign::Left, focused);
    dashboard::chevron(screen.target(), fui::makeRect(row.right() - 36, row.y, 26, row.height), focused);
  }
  pages(screen, fui::makeRect(body.x, body.bottom() - 44, body.width, 44), model.page,
        (count + capacity - 1) / capacity);
}
void buildPresets(toybox::Screen& screen, int) {
  chrome(screen, tr(STR_UI_NEW_HABIT_558B6A));
  const auto body = contentBand(screen.device());
  const bool wide = body.width > body.height;
  dashboard::text(screen, fui::makeRect(body.x, body.y + 4, body.width, 50), tr(STR_HABITS_CHOOSE), 2,
                  fui::TextAlign::Left);
  dashboard::prose(screen, fui::makeRect(body.x, body.y + 56, body.width, 34), tr(STR_HABITS_CHOOSE_HELP),
                   fui::TextAlign::Left);
  const int gap = 12, columns = wide ? 2 : 1, rows = wide ? 3 : 6;
  const int width = (body.width - (columns - 1) * gap) / columns;
  const int height = std::min(78, (body.height - 128 - (rows - 1) * gap) / rows);
  for (int n = 0; n < 6; ++n) {
    const auto box =
        fui::makeRect(body.x + n / rows * (width + gap), body.y + 98 + n % rows * (height + gap), width, height);
    const char* label = n < 5 ? kPresets[n].get() : "";
    if (n < 5) {
      dashboard::frame(screen, box, 2);
      drawHabit(screen, fui::makeRect(box.x + 18, box.y + (box.height - 48) / 2, 48, 48), label);
      dashboard::text(screen, fui::makeRect(box.x + 88, box.y, box.width - 130, box.height), label, 1,
                      fui::TextAlign::Left);
      dashboard::chevron(screen.target(), fui::makeRect(box.right() - 36, box.y, 26, box.height));
      hit(screen, box, ActionPreset, n);
    } else {
      screen.target().fill(box, dashboard::ink);
      dashboard::icon(screen, fui::makeRect(box.x + 18, box.y + (box.height - 38) / 2, 38, 38), extra_dashboard_add_32,
                      true);
      pixelui::text(screen.target(), fui::makeRect(box.x + 84, box.y, box.width - 100, box.height),
                    tr(STR_HABITS_CUSTOM_CARD), 1, fui::Color::White, fui::TextAlign::Left);
      hit(screen, box, ActionCustom, kPresetCount);
    }
  }
  fui::TextStyle footer;
  footer.font = 4;
  footer.align = fui::TextAlign::Center;
  const char* repeat = tr(STR_HABITS_REPEAT_HELP);
  if (screen.target().measureText(footer.font, repeat, footer).width > body.width) footer.font = 3;
  const int footerHeight = std::max(24, int(screen.target().lineHeight(footer.font)));
  screen.target().text(fui::makeRect(body.x, body.bottom() - footerHeight, body.width, footerHeight), repeat, footer);
}
int calendarDayAt(const fui::DeviceContext& device, const DetailModel& model, int x, int y) {
  const auto grid = calendarGrid(device);
  if (x < grid.x || x >= grid.right() || y < grid.y || y >= grid.bottom()) return -1;
  const int first = habits::monthStart(model.historyStart), start = first - habits::weekday(first);
  const int day = start + (y - grid.y) * calendarRows(first) / grid.height * 7 + (x - grid.x) * 7 / grid.width;
  int year, month, date, fy, fm, fd;
  habits::dateOfDay(day, year, month, date);
  habits::dateOfDay(first, fy, fm, fd);
  return month == fm && year == fy && editable(model, day) ? day : -1;
}
void buildDetail(toybox::Screen& screen, const DetailModel& model) {
  if (!model.store || model.index < 0 || model.index >= model.store->count) return;
  chrome(screen, tr(STR_UI_PROGRESS_DBC52E), nullptr, ActionMenu, &icon_go_settings_32);
  const auto& habit = model.store->items[model.index];
  const auto stat = habits::stats(*model.store, model.index, model.today);
  const auto body = contentBand(screen.device());
  const bool wide = body.width > body.height;
  const auto info = fui::makeRect(body.x, body.y, wide ? body.width * 2 / 5 : body.width, 212);
  dashboard::icon(screen, fui::makeRect(info.x + 4, info.y, 52, 52), habitIcon(habit.name));
  dashboard::text(screen, fui::makeRect(info.x + 70, info.y, info.width - 70, 34), habit.name, 1, fui::TextAlign::Left);
  dashboard::prose(screen, fui::makeRect(info.x + 70, info.y + 35, info.width - 70, 24), tr(STR_HABITS_STATISTICS),
                   fui::TextAlign::Left);
  static constexpr StrId names[] = {StrId::STR_HABITS_CURRENT, StrId::STR_HABITS_BEST, StrId::STR_HABITS_CHECK_INS,
                                    StrId::STR_HABITS_RECENT_PERCENT};
  const uint32_t values[] = {stat.current, stat.best, stat.total,
                             stat.recentDays ? uint32_t(stat.recentDone * 100 / stat.recentDays) : 0};
  char value[64];
  const int metricH = wide ? 94 : 70;
  const auto metrics = fui::makeRect(info.x, info.y + 70, info.width, 2 * metricH);
  if (chrome::lightHeader) {
    dashboard::frame(screen, metrics, 2);
    screen.target().fill(fui::makeRect(metrics.x + metrics.width / 2, metrics.y, 1, metrics.height), lightui::rule);
    screen.target().fill(fui::makeRect(metrics.x, metrics.y + metricH, metrics.width, 1), lightui::rule);
  }
  for (int n = 0; n < 4; ++n) {
    const auto rect =
        fui::makeRect(info.x + n % 2 * info.width / 2, info.y + 70 + n / 2 * metricH, info.width / 2, metricH);
    if (!chrome::lightHeader) dashboard::frame(screen, rect, 2);
    std::snprintf(value, sizeof(value),
                  n < 2    ? (values[n] == 1 ? tr(STR_HABITS_ONE_DAY) : tr(STR_HABITS_MANY_DAYS))
                  : n == 3 ? "%u%%"
                           : "%u",
                  unsigned(values[n]));
    dashboard::text(screen, fui::makeRect(rect.x + 6, rect.y + 3, rect.width - 12, 36), value, 1);
    dashboard::prose(screen, fui::makeRect(rect.x + 6, rect.y + (wide ? 34 : 40), rect.width - 12, wide ? 56 : 28),
                     I18N.get(names[n]), fui::TextAlign::Center, wide ? 2 : 1);
  }
  const auto panel = calendarPanel(screen.device()), grid = calendarGrid(screen.device());
  dashboard::frame(screen, panel, 2);
  const int first = habits::monthStart(model.historyStart), start = first - habits::weekday(first);
  int year, month, date;
  habits::dateOfDay(first, year, month, date);
  static constexpr StrId months[] = {
      StrId::STR_READING_MONTH_01, StrId::STR_READING_MONTH_02, StrId::STR_READING_MONTH_03,
      StrId::STR_READING_MONTH_04, StrId::STR_READING_MONTH_05, StrId::STR_READING_MONTH_06,
      StrId::STR_READING_MONTH_07, StrId::STR_READING_MONTH_08, StrId::STR_READING_MONTH_09,
      StrId::STR_READING_MONTH_10, StrId::STR_READING_MONTH_11, StrId::STR_READING_MONTH_12};
  std::snprintf(value, sizeof(value), "%s %d", I18N.get(months[month - 1]), year);
  dashboard::text(screen, fui::makeRect(panel.x + 54, panel.y + 4, panel.width - 108, 36), value, 0);
  for (int side = 0; side < 2; ++side) {
    const auto arrow = fui::makeRect(side ? panel.right() - 48 : panel.x + 8, panel.y + 4, 44, 44);
    button(screen, arrow, "", ActionHistory, side ? 1 : -1);
    dashboard::chevron(screen.target(), arrow, false, !side);
  }
  static constexpr StrId weekdays[] = {StrId::STR_READING_MON, StrId::STR_READING_TUE, StrId::STR_READING_WED,
                                       StrId::STR_READING_THU, StrId::STR_READING_FRI, StrId::STR_READING_SAT,
                                       StrId::STR_READING_SUN};
  for (int n = 0; n < 7; ++n)
    dashboard::text(screen, fui::makeRect(grid.x + n * grid.width / 7, panel.y + 44, grid.width / 7, 26),
                    I18N.get(weekdays[n]), 3);
  for (int n = 0; n < calendarRows(first) * 7; ++n) {
    const int day = start + n;
    int y, m, d;
    habits::dateOfDay(day, y, m, d);
    const int x = grid.x + n % 7 * grid.width / 7, right = grid.x + (n % 7 + 1) * grid.width / 7;
    const int top = grid.y + n / 7 * grid.height / calendarRows(first),
              bottom = grid.y + (n / 7 + 1) * grid.height / calendarRows(first);
    const auto cell = fui::makeRect(x + 2, top + 2, right - x - 4, bottom - top - 4);
    dashboard::frame(screen, cell, 1);
    if (!habits::validDay(day) || m != month || y != year) continue;
    const bool checked = habits::checked(*model.store, model.index, day);
    if (checked) screen.target().fill(cell, dashboard::ink);
    std::snprintf(value, sizeof(value), "%d", d);
    dashboard::text(screen, cell, value, wide ? 3 : 0, fui::TextAlign::Center, checked);
    if (day == model.today || day == model.selectedDay)
      screen.target().stroke(cell.inset(fui::Insets{3, 3, 3, 3}), checked ? dashboard::paper : dashboard::ink, 2);
  }
  screen.frame().hit(grid, ActionCalendar, 0, fui::InputTouch | fui::InputPointerOnly);
  screen.frame().navigationGrid(grid, 7, calendarRows(first));
  const char* legends[] = {tr(STR_HABITS_LEGEND_DONE), tr(STR_HABITS_NO_RECORD), tr(STR_TODAY)};
  for (int n = 0; n < 3; ++n) {
    const int x = panel.x + 8 + n * (panel.width - 16) / 3;
    const auto mark = fui::makeRect(x, panel.bottom() - 28, 16, 16);
    dashboard::frame(screen, mark, 1);
    if (n == 0) screen.target().fill(mark, dashboard::ink);
    if (n == 2) screen.target().stroke(mark.inset(fui::Insets{3, 3, 3, 3}), dashboard::ink, 2);
    dashboard::prose(screen, fui::makeRect(x + 22, panel.bottom() - 32, (panel.width - 16) / 3 - 26, 24), legends[n],
                     fui::TextAlign::Left);
  }
  const bool checked = habits::checked(*model.store, model.index, model.selectedDay);
  dashboard::date(value, sizeof(value), year, month, date);
  habits::dateOfDay(model.selectedDay, year, month, date);
  dashboard::date(value, sizeof(value), year, month, date);
  const int x = wide ? panel.x : body.x, width = wide ? panel.width : body.width;
  const auto status = fui::makeRect(x, body.bottom() - 96, width, 36);
  dashboard::frame(screen, status, 2);
  dashboard::text(screen, fui::makeRect(status.x + 8, status.y, status.width * 3 / 5 - 8, status.height), value, 0,
                  fui::TextAlign::Left);
  dashboard::prose(screen,
                   fui::makeRect(status.x + status.width * 3 / 5, status.y, status.width * 2 / 5 - 8, status.height),
                   checked ? tr(STR_HABITS_DONE) : tr(STR_TASK_PENDING));
  dashboard::action(screen, fui::makeRect(x, body.bottom() - 52, width, 52),
                    checked ? tr(STR_HABITS_UNMARK_DAY) : tr(STR_UI_MARK_AS_DONE_D3D12F), ActionToggleDay,
                    &extra_check_32, true);
}

void buildMenu(toybox::Screen& screen, const char* name) {
  chrome(screen, tr(STR_UI_HABIT_9DB13B), tr(STR_UI_BACK_587EAC), ActionBack);
  const fui::Rect body = contentBand(screen.device());
  text(screen, fui::makeRect(body.x, body.y, body.width, 44), name);
  button(screen, fui::makeRect(body.x, body.y + 68, body.width, toybox::kRowHeight), tr(STR_UI_RENAME_HABIT_3E99C7),
         ActionRename);
  button(screen,
         fui::makeRect(body.x, body.y + 68 + toybox::kRowHeight + toybox::kGutter, body.width, toybox::kRowHeight),
         tr(STR_UI_DELETE_HABIT_2E8C90), ActionDelete);
}

void buildDelete(toybox::Screen& screen, const char* name) {
  chrome(screen, tr(STR_UI_DELETE_HABIT_F0ADF9), tr(STR_WORDLE_CANCEL), ActionBack);
  const fui::Rect body = contentBand(screen.device());
  text(screen, fui::makeRect(body.x, body.y, body.width, 44), name);
  text(screen, fui::makeRect(body.x, body.y + 60, body.width, 32), tr(STR_UI_ITS_ENTIRE_HISTORY_WILL_BE_DELETED_B5ED3B),
       toybox::kSmallFont);
  text(screen, fui::makeRect(body.x, body.y + 96, body.width, 32), tr(STR_UI_THIS_CANNOT_BE_UNDONE_AD2526),
       toybox::kSmallFont);
  button(screen, fui::makeRect(body.x, body.bottom() - toybox::kPillHeight, body.width, toybox::kPillHeight),
         tr(STR_UI_DELETE_HABIT_F0ADF9), ActionConfirmDelete, 0, true);
}

void buildMessage(toybox::Screen& screen, const char* title, const char* message, const fui::ActionId action,
                  const char* label) {
  chrome(screen, title, tr(STR_UI_BACK_587EAC), ActionBack);
  const fui::Rect body = contentBand(screen.device());
  fui::TextStyle textStyle = style(toybox::kSmallFont);
  textStyle.maxLines = 6;
  screen.target().text(fui::makeRect(body.x, body.y + 24, body.width, body.height - 100), message, textStyle);
  button(screen, fui::makeRect(body.x, body.bottom() - toybox::kPillHeight, body.width, toybox::kPillHeight), label,
         action, 0, true);
}

}  // namespace habitsui
