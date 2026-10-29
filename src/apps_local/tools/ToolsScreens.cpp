#include "ToolsScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../reading/ReadingStatsCore.h"
#include "../ui/Dashboard.h"
#include "../ui/PixelMenu.h"
#include "../ui/ToolsDigits.h"
#include "../ui/ToyboxIcons.h"

namespace toolsui {
namespace fui = freeink::ui;
namespace {
constexpr auto ink = fui::Paint::solid(fui::Color::Black);
void text(toybox::Screen& s, fui::Rect r, const char* label, int font = 1) {
  pixelui::text(s.target(), r, label, font, fui::Color::Black, fui::TextAlign::Center);
}
void button(toybox::Screen& s, Work& w, fui::Rect r, const char* label, fui::ActionId action, bool selected = false) {
  pixelui::copyCaps(w.label, sizeof(w.label), label);
  w.button = {};
  w.button.label = w.label;
  fui::TextStyle probe;
  probe.font = 0;
  w.button.text.font = s.target().measureText(0, w.label, probe).width > r.width - 16 ? 3 : 0;
  w.button.text.align = fui::TextAlign::Center;
  w.button.styles = pixelui::buttonStyles();
  w.button.action = action;
  w.button.minTouchSize = 0;
  w.button.state = selected ? fui::StateSelected : fui::StateNormal;
  s.button(w.button, r);
}
constexpr StrId months[] = {StrId::STR_READING_MONTH_01, StrId::STR_READING_MONTH_02, StrId::STR_READING_MONTH_03,
                            StrId::STR_READING_MONTH_04, StrId::STR_READING_MONTH_05, StrId::STR_READING_MONTH_06,
                            StrId::STR_READING_MONTH_07, StrId::STR_READING_MONTH_08, StrId::STR_READING_MONTH_09,
                            StrId::STR_READING_MONTH_10, StrId::STR_READING_MONTH_11, StrId::STR_READING_MONTH_12};
constexpr StrId weekdays[] = {StrId::STR_STATS_MONDAY,   StrId::STR_STATS_TUESDAY, StrId::STR_STATS_WEDNESDAY,
                              StrId::STR_STATS_THURSDAY, StrId::STR_STATS_FRIDAY,  StrId::STR_STATS_SATURDAY,
                              StrId::STR_STATS_SUNDAY};
void caps(toybox::Screen& s, Work& w, fui::Rect r, const char* label, int font = 1) {
  pixelui::copyCaps(w.label, sizeof(w.label), label);
  text(s, r, w.label, font);
}
void control(toybox::Screen& s, Work& w, fui::Rect r, const char* value, fui::ActionId action, int argument = 0,
             bool primary = false, const freeink::Icon* icon = nullptr) {
  pixelui::copyCaps(w.label, sizeof(w.label), value);
  dashboard::button(s, r, w.label, action, argument, primary, icon);
}
struct CalendarGrid {
  fui::Rect rect;
  int offset, days, rows;
};
CalendarGrid calendarGrid(fui::Rect body, const Model& m) {
  const bool wide = body.width > body.height;
  const int navH = wide ? 42 : 58, gap = 10, rotateH = wide ? 40 : 56;
  const int footer = wide ? 2 * rotateH + 2 * gap : 3 * rotateH + 3 * gap;
  const int first = readingstats::dayIndex({m.year, m.month, 1});
  const int offset = (readingstats::weekday(first) + (m.sunday ? 1 : 0)) % 7;
  const int days = readingstats::daysInMonth(m.year, m.month);
  return {fui::makeRect(body.x, body.y + navH + gap, body.width, body.height - navH - gap - footer), offset, days,
          1 + (offset + days + 6) / 7};
}
void calendar(toybox::Screen& s, const Model& m, Work& w, fui::Rect body) {
  const bool wide = body.width > body.height;
  const int navH = wide ? 42 : 58, gap = 10, rotateH = wide ? 40 : 56;
  std::snprintf(w.label, sizeof(w.label), "%s %d", I18N.get(months[m.month - 1]), m.year);
  caps(s, w, fui::makeRect(body.x + navH + 8, body.y, body.width - 2 * navH - 16, navH), w.label);
  button(s, w, fui::makeRect(body.x, body.y, navH, navH), "<", Previous);
  button(s, w, fui::makeRect(body.right() - navH, body.y, navH, navH), ">", Next);
  const int first = readingstats::dayIndex({m.year, m.month, 1});
  const auto layout = calendarGrid(body, m);
  const int offset = layout.offset, days = layout.days, rows = layout.rows;
  const auto grid = layout.rect;
  static constexpr StrId names[] = {StrId::STR_READING_MON, StrId::STR_READING_TUE, StrId::STR_READING_WED,
                                    StrId::STR_READING_THU, StrId::STR_READING_FRI, StrId::STR_READING_SAT,
                                    StrId::STR_READING_SUN};
  for (int col = 0; col < 7; ++col) {
    const int x = grid.x + col * grid.width / 7, right = grid.x + (col + 1) * grid.width / 7;
    caps(s, w, fui::makeRect(x, grid.y, right - x, grid.height / rows), I18N.get(names[(col + (m.sunday ? 6 : 0)) % 7]),
         0);
  }
  for (int d = 1; d <= days; ++d) {
    const int n = offset + d - 1, col = n % 7, row = 1 + n / 7;
    const int x = grid.x + col * grid.width / 7, y = grid.y + row * grid.height / rows;
    const auto cell =
        fui::makeRect(x, y, grid.x + (col + 1) * grid.width / 7 - x, grid.y + (row + 1) * grid.height / rows - y);
    const bool selected = first + d - 1 == m.selected;
    if (selected) s.target().fill(cell, ink);
    std::snprintf(w.label, sizeof(w.label), "%d", d);
    pixelui::text(s.target(), cell, w.label, 1, selected ? fui::Color::White : fui::Color::Black,
                  fui::TextAlign::Center);
  }
  for (int col = 1; col < 7; ++col)
    s.target().line({int16_t(grid.x + col * grid.width / 7), grid.y},
                    {int16_t(grid.x + col * grid.width / 7), grid.bottom()}, 2, ink);
  for (int row = 1; row < rows; ++row)
    s.target().line({grid.x, int16_t(grid.y + row * grid.height / rows)},
                    {grid.right(), int16_t(grid.y + row * grid.height / rows)}, 2, ink);
  s.target().stroke(grid, ink, pixelui::kBorder);
  s.frame().hit(grid, SelectDay, 0, fui::InputTouch | fui::InputPointerOnly);
  const int dayTop = grid.y + grid.height / rows;
  s.frame().navigationGrid(fui::makeRect(grid.x, dayTop, grid.width, grid.bottom() - dayTop), 7, rows - 1);
  const auto selected = readingstats::dateFromDay(m.selected);
  char date[128];
  std::snprintf(date, sizeof(date), tr(STR_TOOLS_SELECTED_DATE), I18N.get(weekdays[readingstats::weekday(m.selected)]),
                selected.day, I18N.get(months[selected.month - 1]));
  const int y = grid.bottom() + gap, half = (body.width - gap) / 2;
  if (wide) {
    const int side = body.width * 22 / 100;
    button(s, w, fui::makeRect(body.x, y, side, rotateH), tr(STR_TODAY), Today);
    text(s, fui::makeRect(body.x + side + gap, y, body.width - 2 * side - 2 * gap, rotateH), date, 0);
    button(s, w, fui::makeRect(body.right() - side, y, side, rotateH),
           m.sunday ? tr(STR_WEEK_SUNDAY) : tr(STR_WEEK_MONDAY), WeekStart);
  } else {
    text(s, fui::makeRect(body.x, y, body.width, rotateH), date, 0);
    button(s, w, fui::makeRect(body.x, y + rotateH + gap, half, rotateH), tr(STR_TODAY), Today);
    button(s, w, fui::makeRect(body.x + half + gap, y + rotateH + gap, half, rotateH),
           m.sunday ? tr(STR_WEEK_SUNDAY) : tr(STR_WEEK_MONDAY), WeekStart);
  }
  control(s, w, fui::makeRect(body.x, body.bottom() - rotateH, body.width, rotateH), tr(STR_QUICK_ROTATE), Rotate, 0,
          false, m.rotateRight ? &extra_rotate_cw_32 : &extra_rotate_ccw_32);
}
void face(toybox::Screen& s, fui::Rect box, const Model& m) {
  const int size = std::min(int(box.width), int(box.height)), cx = box.x + box.width / 2, cy = box.y + box.height / 2,
            r = size / 2;
  toybox::ring(s, cx, cy, r, 3, fui::Color::Black, fui::Color::White);
  for (int n = 0; n < 60; ++n) {
    const float a = n * 3.14159265f / 30;
    const int outer = r - 8, inner = outer - (n % 5 ? std::max(5, size / 40) : size / 18);
    s.target().line({int16_t(cx + std::sin(a) * inner), int16_t(cy - std::cos(a) * inner)},
                    {int16_t(cx + std::sin(a) * outer), int16_t(cy - std::cos(a) * outer)}, n % 5 ? 1 : 3, ink);
  }
  const int n = size / 7, inset = size / 10;
  toolsdigits::draw(s, fui::makeRect(cx - n / 2, cy - r + inset, n, n), "12");
  toolsdigits::draw(s, fui::makeRect(cx + r - inset - n, cy - n / 2, n, n), "3");
  toolsdigits::draw(s, fui::makeRect(cx - n / 2, cy + r - inset - n, n, n), "6");
  toolsdigits::draw(s, fui::makeRect(cx - r + inset, cy - n / 2, n, n), "9");
  const float angles[] = {float((m.hour % 12) * 60 + m.minute) * 3.14159265f / 360, float(m.minute) * 3.14159265f / 30};
  for (int n = 0; n < 2; ++n)
    s.target().line({int16_t(cx), int16_t(cy)},
                    {int16_t(cx + std::sin(angles[n]) * size * (n ? .40f : .25f)),
                     int16_t(cy - std::cos(angles[n]) * size * (n ? .40f : .25f))},
                    n ? 4 : 7, ink);
  toybox::disc(s, cx, cy, std::max(5, size / 40), fui::Color::Black);
}
void clock(toybox::Screen& s, const Model& m, Work& w, fui::Rect body) {
  const bool wide = body.width > body.height;
  const int footer = wide ? 58 : 62;
  const auto content = fui::makeRect(body.x, body.y, body.width, body.height - footer - 18);
  const int weekday = readingstats::weekday(readingstats::dayIndex({m.year, m.month, std::max(1, m.day)}));
  char time[16], date[64];
  std::snprintf(time, sizeof(time), "%02d:%02d", m.twelveHour ? ((m.hour + 11) % 12 + 1) : m.hour, m.minute);
  dashboard::date(date, sizeof(date), m.year, m.month, m.day);
  fui::Rect info = content;
  if (m.analog) {
    if (wide) {
      const int side = std::min(int(content.height), int(content.width) * 48 / 100);
      face(s, fui::makeRect(content.x, content.y, side, content.height), m);
      info = fui::makeRect(content.x + side + 16, content.y, content.width - side - 16, content.height);
      const int valueH = std::min(104, info.height / 2), top = info.y + (info.height - valueH - 76) / 2;
      toolsdigits::draw(s, fui::makeRect(info.x + 12, top, info.width - 24, valueH), time);
      caps(s, w, fui::makeRect(info.x, top + valueH + 8, info.width, 32), I18N.get(weekdays[weekday]));
      text(s, fui::makeRect(info.x, top + valueH + 44, info.width, 32), m.validTime ? date : tr(STR_READING_CLOCK), 0);
    } else {
      const int offset = std::min(18, std::max(0, int(content.height) - int(content.width) - 110));
      const int side = std::min(int(content.width), int(content.height) - 100 - offset);
      face(s, fui::makeRect(content.x, content.y + offset, content.width, side), m);
      toolsdigits::draw(s, fui::makeRect(content.x, content.y + side + offset + 12, content.width, 50), time);
      std::snprintf(w.label, sizeof(w.label), "%s · %s", I18N.get(weekdays[weekday]),
                    m.validTime ? date : tr(STR_READING_CLOCK));
      caps(s, w, fui::makeRect(content.x, content.y + side + offset + 66, content.width, 34), w.label, 0);
    }
  } else {
    const int y = content.y + (wide ? 30 : content.height / 5);
    caps(s, w, fui::makeRect(content.x, y - 20, content.width, 44), I18N.get(weekdays[weekday]));
    toolsdigits::draw(s,
                      fui::makeRect(content.x + 8, y + 50 - (wide ? 8 : 0), content.width - 16,
                                    content.height * (wide ? 48 : 40) / 100),
                      time);
    text(s,
         fui::makeRect(content.x, content.y + content.height * (wide ? 73 : 76) / 100, content.width, wide ? 32 : 40),
         m.validTime ? date : tr(STR_READING_CLOCK), 0);
  }
  if (!m.analog) {
    const auto chip = fui::makeRect(content.x + (content.width - 136) / 2, content.bottom() - 42, 136, 34);
    button(s, w, chip, m.twelveHour ? (m.hour < 12 ? tr(STR_TOOLS_12_AM) : tr(STR_TOOLS_12_PM)) : tr(STR_TOOLS_24H),
           HourFormat);
  }
  const int half = (body.width - 12) / 2, y = body.bottom() - footer;
  control(s, w, fui::makeRect(body.x, y, half, footer), m.analog ? tr(STR_CLOCK_DIGITAL) : tr(STR_CLOCK_ANALOG),
          Appearance, 0, false, m.analog ? &extra_cards_32 : &extra_clock_32);
  control(s, w, fui::makeRect(body.x + half + 12, y, half, footer), tr(STR_QUICK_ROTATE), Rotate, 0, false,
          m.rotateRight ? &extra_rotate_cw_32 : &extra_rotate_ccw_32);
}
void counter(toybox::Screen& s, const Model& m, Work& w, fui::Rect body) {
  const bool wide = body.width > body.height;
  const auto number = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 8, body.height)
                           : fui::makeRect(body.x, body.y + 38, body.width, body.height * 40 / 100);
  text(s, fui::makeRect(number.x, number.y, number.width, 42), tr(STR_COUNTER_CURRENT), 0);
  const auto display = fui::makeRect(number.x, number.y + 48, number.width, number.height - 48);
  dashboard::frame(s, display, pixelui::kBorder);
  std::snprintf(w.label, sizeof(w.label), "%lld", static_cast<long long>(m.count));
  toolsdigits::counter(s, display.inset(fui::Insets{24, 20, 24, 20}), w.label, wide ? 112 : 120);
  const auto controls =
      wide ? fui::makeRect(number.right() + 16, body.y, body.right() - number.right() - 16, body.height)
           : fui::makeRect(body.x, number.bottom() + 16, body.width, body.bottom() - number.bottom() - 16);
  const int gap = 12, h = (controls.height - 2 * gap) / 3, half = (controls.width - gap) / 2;
  control(s, w, fui::makeRect(controls.x, controls.y, controls.width, h), tr(STR_TOOLS_ADD), Add, 0, true);
  const int plusY = controls.y + h / 2;
  s.target().fill(fui::makeRect(controls.x + 28, plusY - 3, 40, 6), dashboard::paper);
  s.target().fill(fui::makeRect(controls.x + 45, plusY - 20, 6, 40), dashboard::paper);
  pixelui::text(s.target(), fui::makeRect(controls.right() - 60, controls.y, 48, h), "+1", 1, fui::Color::White,
                fui::TextAlign::Right);
  control(s, w, fui::makeRect(controls.x, controls.y + h + gap, half, h), tr(STR_TOOLS_SUBTRACT), Subtract);
  control(s, w, fui::makeRect(controls.x + half + gap, controls.y + h + gap, half, h), tr(STR_UNDO), Undo, 0, false,
          &extra_dashboard_undo_32);
  control(s, w, fui::makeRect(controls.x, controls.y + 2 * (h + gap), controls.width, h), tr(STR_COUNTER_RESET), Reset,
          0, false, &extra_rotate_cw_32);
}

}  // namespace
int calendarDayAt(const fui::DeviceContext& device, const Model& m, int x, int y) {
  auto body = pixelui::geometry(device).body;
  if (m.failed) {
    body.y += 50;
    body.height -= 50;
  }
  const auto layout = calendarGrid(body, m);
  const auto grid = layout.rect;
  if (x < grid.x || x >= grid.right() || y < grid.y || y >= grid.bottom()) return 0;
  const int col = ((x - grid.x + 1) * 7 - 1) / grid.width, row = ((y - grid.y + 1) * layout.rows - 1) / grid.height;
  if (!row) return 0;
  const int day = (row - 1) * 7 + col - layout.offset + 1;
  return day >= 1 && day <= layout.days ? day : 0;
}
void build(toybox::Screen& s, const Model& m, Work& w) {
  pixelui::begin(s);
  pixelui::Header header;
  header.back = true;
  header.title = m.kind == Kind::Calendar ? tr(STR_CALENDAR_APP)
                 : m.kind == Kind::Clock  ? tr(STR_CLOCK)
                                          : tr(STR_COUNTER_APP);
  pixelui::header(s, header);
  backui::publish(true, pixelui::geometry(s.device()).back);
  auto body = s.body();
  if (m.failed) {
    text(s, fui::makeRect(body.x, body.y, body.width, 46), tr(STR_TOOLS_SAVE_FAILED), 3);
    body.y += 50;
    body.height -= 50;
  }
  switch (m.kind) {
    case Kind::Calendar:
      calendar(s, m, w, body);
      break;
    case Kind::Clock:
      clock(s, m, w, body);
      break;
    case Kind::Counter:
      if (m.confirm) {
        text(s, fui::makeRect(body.x, body.y, body.width, 110), tr(STR_COUNTER_CONFIRM), 0);
        button(s, w, fui::makeRect(body.x, body.y + 140, body.width, 64), tr(STR_RESET), ConfirmReset);
        button(s, w, fui::makeRect(body.x, body.y + 216, body.width, 64), tr(STR_CANCEL), CancelReset);
        break;
      }
      counter(s, m, w, body);
      break;
  }
}
}  // namespace toolsui
