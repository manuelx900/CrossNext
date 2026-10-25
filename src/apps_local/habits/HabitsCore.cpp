#include "HabitsCore.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace habits {
namespace {

bool leap(const int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

int monthDays(const int year, const int month) {
  static constexpr uint8_t lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return lengths[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}

bool bit(const Habit& habit, const int index) { return (habit.history[index / 8] & (1u << (index % 8))) != 0; }

void putBit(Habit& habit, const int index, const bool done) {
  const auto mask = static_cast<uint8_t>(1u << (index % 8));
  if (done)
    habit.history[index / 8] |= mask;
  else
    habit.history[index / 8] &= static_cast<uint8_t>(~mask);
}

char folded(const char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c; }

bool sameName(const char* a, const char* b) {
  while (*a && *b && folded(*a) == folded(*b)) {
    ++a;
    ++b;
  }
  return *a == *b;
}

void write32(uint8_t*& out, const uint32_t value) {
  for (int n = 0; n < 4; ++n) *out++ = static_cast<uint8_t>(value >> (8 * n));
}

uint32_t read32(const uint8_t*& in) {
  uint32_t value = 0;
  for (int n = 0; n < 4; ++n) value |= static_cast<uint32_t>(*in++) << (8 * n);
  return value;
}

}  // namespace

bool validDay(const int day) { return day >= kMinDay && day <= kMaxDay; }

int dayIndex(const int year, const int month, const int date) {
  if (year < 2020 || year > 2099 || month < 1 || month > 12 || date < 1 || date > monthDays(year, month)) return -1;
  int result = kMinDay;
  for (int y = 2020; y < year; ++y) result += leap(y) ? 366 : 365;
  for (int m = 1; m < month; ++m) result += monthDays(year, m);
  return result + date - 1;
}

void dateOfDay(const int day, int& year, int& month, int& date) {
  year = 2020;
  month = 1;
  date = 1;
  if (!validDay(day)) return;
  int rest = day - kMinDay;
  while (rest >= (leap(year) ? 366 : 365)) {
    rest -= leap(year) ? 366 : 365;
    ++year;
  }
  while (rest >= monthDays(year, month)) {
    rest -= monthDays(year, month);
    ++month;
  }
  date = rest + 1;
}

bool parseDate(const char* text, int& day) {
  if (!text || std::strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
  for (int n = 0; n < 10; ++n) {
    if (n != 4 && n != 7 && (text[n] < '0' || text[n] > '9')) return false;
  }
  const int year = (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 + text[3] - '0';
  const int month = (text[5] - '0') * 10 + text[6] - '0';
  const int date = (text[8] - '0') * 10 + text[9] - '0';
  const int parsed = dayIndex(year, month, date);
  if (!validDay(parsed)) return false;
  day = parsed;
  return true;
}

void formatDate(const int day, char* out, const size_t size) {
  if (!validDay(day)) {
    std::snprintf(out, size, "SET DATE");
    return;
  }
  int y, m, d;
  dateOfDay(day, y, m, d);
  std::snprintf(out, size, "%04d-%02d-%02d", y, m, d);
}

int weekday(const int day) { return (day + 3) % 7; }
int firstDay(const Store& store) { return std::max<int>(kMinDay, store.latestDay - kHistoryDays + 1); }

bool checked(const Store& store, const int index, const int day) {
  if (index < 0 || index >= store.count || day < firstDay(store) || day > store.latestDay ||
      day < store.items[index].createdDay)
    return false;
  return bit(store.items[index], day - firstDay(store));
}

bool advance(Store& store, const int day) {
  if (!validDay(day)) return false;
  if (store.latestDay < 0) {
    store.latestDay = day;
    return true;
  }
  if (day < store.latestDay) return false;
  if (day == store.latestDay) return true;
  const int shift = std::max(0, day - kHistoryDays + 1 - firstDay(store));
  if (shift > 0) {
    for (int h = 0; h < store.count; ++h) {
      Habit& habit = store.items[h];
      for (int n = 0; n < std::min(shift, kHistoryDays); ++n) {
        if (bit(habit, n)) {
          ++habit.archivedTotal;
          ++habit.archivedRun;
        } else
          habit.archivedRun = 0;
        habit.archivedBest = std::max(habit.archivedBest, habit.archivedRun);
      }
      if (shift > kHistoryDays) habit.archivedRun = 0;
      for (int n = 0; n < kHistoryDays; ++n) {
        putBit(habit, n, n + shift < kHistoryDays && bit(habit, n + shift));
      }
    }
  }
  store.latestDay = day;
  return true;
}

NameResult nameResult(const Store& store, const char* name, const int except) {
  if (!name || !name[0]) return NameResult::Empty;
  size_t length = 0;
  while (name[length] && length <= kNameMax) ++length;
  if (length > kNameMax) return NameResult::TooLong;
  bool letter = false;
  for (size_t n = 0; n < length; ++n) {
    const auto c = static_cast<unsigned char>(name[n]);
    if (c < 32 || c > 126) return NameResult::Unsupported;
    if (c != ' ') letter = true;
  }
  if (!letter) return NameResult::Empty;
  for (int n = 0; n < store.count; ++n) {
    if (n != except && sameName(store.items[n].name, name)) return NameResult::Duplicate;
  }
  return NameResult::Ok;
}

NameResult add(Store& store, const char* name, const int day) {
  if (store.count >= kMaxHabits) return NameResult::Full;
  const NameResult result = nameResult(store, name);
  if (result != NameResult::Ok) return result;
  if (!advance(store, day)) return NameResult::Unsupported;
  Habit& habit = store.items[store.count++];
  habit = Habit{};
  habit.createdDay = day;
  std::snprintf(habit.name, sizeof(habit.name), "%s", name);
  return NameResult::Ok;
}

NameResult rename(Store& store, const int index, const char* name) {
  if (index < 0 || index >= store.count) return NameResult::Empty;
  const NameResult result = nameResult(store, name, index);
  if (result == NameResult::Ok) std::snprintf(store.items[index].name, sizeof(store.items[index].name), "%s", name);
  return result;
}

bool remove(Store& store, const int index) {
  if (index < 0 || index >= store.count) return false;
  for (int n = index; n + 1 < store.count; ++n) store.items[n] = store.items[n + 1];
  store.items[--store.count] = Habit{};
  return true;
}

bool toggle(Store& store, const int index, const int day, const int today) {
  if (index < 0 || index >= store.count || !validDay(today) || day > today || day < store.items[index].createdDay ||
      !advance(store, today) || day < firstDay(store))
    return false;
  const int offset = day - firstDay(store);
  putBit(store.items[index], offset, !bit(store.items[index], offset));
  return true;
}

Stats stats(const Store& store, const int index, const int today) {
  Stats out;
  if (index < 0 || index >= store.count || !validDay(today)) return out;
  const Habit& habit = store.items[index];
  out.total = habit.archivedTotal;
  out.best = habit.archivedBest;
  uint32_t run = habit.archivedRun;
  const int end = std::min<int>(today, store.latestDay);
  for (int day = firstDay(store); day <= end; ++day) {
    if (checked(store, index, day)) {
      ++out.total;
      ++run;
      out.best = std::max(out.best, run);
    } else
      run = 0;
  }
  int tail = checked(store, index, today) ? today : today - 1;
  while (tail >= firstDay(store) && checked(store, index, tail)) {
    ++out.current;
    --tail;
  }
  if (tail == firstDay(store) - 1) out.current += habit.archivedRun;
  for (int day = std::max<int>(habit.createdDay, today - 29); day <= today; ++day) {
    ++out.recentDays;
    if (checked(store, index, day)) ++out.recentDone;
  }
  return out;
}

int countOnDay(const Store& store, int day) {
  int count = 0;
  for (int n = 0; n < store.count; ++n)
    if (store.items[n].createdDay <= day) ++count;
  return count;
}
int indexOnDay(const Store& store, int day, int position) {
  if (position < 0) return -1;
  for (int n = 0; n < store.count; ++n)
    if (store.items[n].createdDay <= day && position-- == 0) return n;
  return -1;
}
int doneCount(const Store& store, const int day) {
  int done = 0;
  for (int n = 0; n < store.count; ++n)
    if (checked(store, n, day)) ++done;
  return done;
}

uint32_t checksum(const uint8_t* data, const size_t size) {
  static constexpr auto table = [] {
    std::array<uint32_t, 256> values{};
    for (uint32_t n = 0; n < values.size(); ++n) {
      uint32_t value = n;
      for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u)));
      values[n] = value;
    }
    return values;
  }();
  uint32_t crc = 0xffffffffu;
  for (size_t n = 0; n < size; ++n) crc = table[(crc ^ data[n]) & 0xffu] ^ (crc >> 8);
  return ~crc;
}

bool encode(const Store& store, const uint32_t generation, uint8_t* out, const size_t size) {
  if (size != kFileSize || store.count > kMaxHabits) return false;
  std::memset(out, 0, size);
  uint8_t* p = out;
  std::memcpy(p, "HBT1", 4);
  p += 4;
  write32(p, generation);
  write32(p, static_cast<uint32_t>(store.latestDay));
  *p++ = store.count;
  p += 3;
  for (int n = 0; n < kMaxHabits; ++n) {
    const Habit& h = store.items[n];
    std::memcpy(p, h.name, kNameMax + 1);
    p += kNameMax + 1;
    write32(p, static_cast<uint32_t>(h.createdDay));
    write32(p, h.archivedTotal);
    write32(p, h.archivedBest);
    write32(p, h.archivedRun);
    std::memcpy(p, h.history, kHistoryBytes);
    p += kHistoryBytes;
  }
  write32(p, checksum(out, size - 4));
  return true;
}

bool decode(const uint8_t* data, const size_t size, Store& store, uint32_t& generation) {
  if (size != kFileSize || std::memcmp(data, "HBT1", 4) != 0) return false;
  const uint8_t* crc = data + size - 4;
  if (read32(crc) != checksum(data, size - 4)) return false;
  const uint8_t* p = data + 4;
  const uint32_t gen = read32(p);
  const uint32_t latest = read32(p);
  const int count = *p++;
  p += 3;
  if (count > kMaxHabits || (latest != 0xffffffffu && !validDay(static_cast<int>(latest))) ||
      (count > 0 && latest == 0xffffffffu))
    return false;
  store.count = 0;
  store.latestDay = latest == 0xffffffffu ? -1 : static_cast<int32_t>(latest);
  for (int n = 0; n < kMaxHabits; ++n) {
    Habit& h = store.items[n];
    std::memcpy(h.name, p, kNameMax + 1);
    p += kNameMax + 1;
    h.createdDay = static_cast<int32_t>(read32(p));
    h.archivedTotal = read32(p);
    h.archivedBest = read32(p);
    h.archivedRun = read32(p);
    std::memcpy(h.history, p, kHistoryBytes);
    p += kHistoryBytes;
    if (n >= count) continue;
    if (h.name[kNameMax] != '\0' || !validDay(h.createdDay) || h.createdDay > store.latestDay ||
        h.archivedRun > h.archivedBest || h.archivedBest > h.archivedTotal ||
        h.archivedTotal > static_cast<uint32_t>(std::max<int>(0, firstDay(store) - h.createdDay)) ||
        nameResult(store, h.name) != NameResult::Ok)
      return false;
    for (int i = 0; i < kHistoryDays; ++i) {
      const int day = firstDay(store) + i;
      if (bit(h, i) && (day < h.createdDay || day > store.latestDay)) return false;
    }
    ++store.count;
  }
  generation = gen;
  return true;
}

int monthStart(int day) {
  int year, month, date;
  dateOfDay(day, year, month, date);
  return dayIndex(year, month, 1);
}
int shiftMonth(int day, int step) {
  int year, month, date;
  dateOfDay(day, year, month, date);
  const int ordinal = std::max(2020 * 12, std::min(2099 * 12 + 11, year * 12 + month - 1 + step));
  return dayIndex(ordinal / 12, ordinal % 12 + 1, 1);
}

}  // namespace habits
