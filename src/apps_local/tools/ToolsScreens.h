#pragma once
#include "../ui/ToyboxScreen.h"

namespace toolsui {
enum class Kind : uint8_t { Calendar, Clock, Counter };
enum : freeink::ui::ActionId {
  Previous = 1,
  Next,
  Today,
  WeekStart,
  Appearance,
  Rotate,
  Add,
  Subtract,
  Undo,
  Reset,
  ConfirmReset,
  CancelReset,
  SetClock,
  SelectDay,
  HourFormat
};
struct Model {
  Kind kind = Kind::Calendar;
  int year = 2026, month = 1, day = 1, today = 0, hour = 0, minute = 0;
  int selected = 0;
  int64_t count = 0;
  bool twelveHour = false;
  bool rotateRight = false;
  bool sunday = false, analog = false, validTime = false, confirm = false, failed = false;
};
struct Work {
  freeink::ui::ButtonProps button;
  char label[128]{};
};
int calendarDayAt(const freeink::ui::DeviceContext&, const Model&, int x, int y);
void build(toybox::Screen&, const Model&, Work&);
}  // namespace toolsui
