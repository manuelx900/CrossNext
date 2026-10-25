#pragma once

#include "../ui/ToyboxScreen.h"
#include "ReadingStatsCore.h"

namespace readingui {
enum : freeink::ui::ActionId {
  Tab = 1,
  Page,
  OpenBook,
  Day,
  ContinueReading,
  SelectDay,
  SelectWeek,
  SelectMonth,
  BackToCalendar,
  PeriodPage,
  Filter,
  BackToBooks,
  ChoosePeriod,
  PickerYear,
  PickerMonth,
  SelectYear,
  SelectChartDay,
  BackToData
};
struct Model {
  const readingstats::Summary* summary = nullptr;
  const readingstats::PeriodStats* periodStats = nullptr;
  readingstats::Period period = readingstats::Period::Calendar;
  readingstats::BookFilter filter = readingstats::BookFilter::Finished;
  bool weekStartsSunday = false;
  const readingstats::Book* books = nullptr;
  const readingstats::Book* detail = nullptr;
  int displayedDay = 0;
  int today = 0, month = 0, tab = 0, page = 0, count = 0, selected = 0, pageSize = readingstats::kBooksPerPage;
  int bookTotal = 0;
  int battery = -1;
  bool charging = false, showPercent = true;
  bool loading = false, failed = false, damaged = false;
  bool bookOnly = false;
  bool pickerVisible = false, periodFromPicker = false;
  int pickerYear = 2026;
};
int capacity(const freeink::ui::DeviceContext& device);
struct Work {
  freeink::ui::Rect calendarGrid;
  freeink::ui::ButtonProps button;
  freeink::ui::TabBarProps tabs;
  freeink::ui::TabItem items[4];
  freeink::ui::HeaderProps status;
  char labels[4][48]{}, buttonLabel[96]{};
};
int calendarDayAt(const Work& work, int month, bool sunday, int x, int y);
void build(toybox::Screen& screen, const Model& model, Work& work);
void duration(uint64_t seconds, char* out, size_t size);
}  // namespace readingui
