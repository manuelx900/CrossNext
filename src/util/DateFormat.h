#pragma once

#include <I18n.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace dateformat {
enum class Order : uint8_t { DayMonthYear, MonthDayYear, YearMonthDay };
inline constexpr uint8_t COUNT = 3;
inline Order fromStored(uint8_t value) { return value < COUNT ? Order(value) : Order::DayMonthYear; }

inline const char* label(Order order) {
  switch (order) {
    case Order::DayMonthYear:
      return tr(STR_DATE_FORMAT_DMY);
    case Order::MonthDayYear:
      return tr(STR_DATE_FORMAT_MDY);
    case Order::YearMonthDay:
      return tr(STR_DATE_FORMAT_YMD);
  }
  return tr(STR_DATE_FORMAT_DMY);
}

inline void format(char* out, size_t size, unsigned day, unsigned month, unsigned year, Order order,
                   bool compact = false) {
  const unsigned displayedYear = compact ? year % 100 : year;
  switch (order) {
    case Order::DayMonthYear:
      std::snprintf(out, size, compact ? tr(STR_DATE_VALUE_SHORT) : tr(STR_DATE_VALUE_DMY), day, month, displayedYear);
      return;
    case Order::MonthDayYear:
      std::snprintf(out, size, compact ? tr(STR_DATE_VALUE_SHORT) : tr(STR_DATE_VALUE_MDY), month, day, displayedYear);
      return;
    case Order::YearMonthDay:
      std::snprintf(out, size, compact ? tr(STR_DATE_VALUE_SHORT) : tr(STR_DATE_VALUE_YMD), displayedYear, month, day);
      return;
  }
}
}  // namespace dateformat
