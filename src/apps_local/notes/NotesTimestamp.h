#pragma once
#include <cstdint>
namespace notes {
struct ModifiedDate {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
};
// HalFile returns packed FAT calendar fields, already in local wall time.
inline bool decodeModified(uint32_t stamp, ModifiedDate& out) {
  out = {};
  if (!stamp) return false;
  const uint16_t date = stamp >> 16, time = stamp;
  const int year = 1980 + (date >> 9), month = (date >> 5) & 15, day = date & 31;
  const int hour = time >> 11, minute = (time >> 5) & 63;
  static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 2020 || month < 1 || month > 12 || day < 1 || hour > 23 || minute > 59) return false;
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (day > days[month - 1] + (month == 2 && leap)) return false;
  out = {year, month, day, hour, minute};
  return true;
}
}  // namespace notes
