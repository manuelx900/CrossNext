#include "PomodoroHistory.h"

#include <algorithm>
#include <cstring>

namespace pomodoro {
namespace {
uint64_t sum(uint64_t a, uint64_t b) { return b > UINT64_MAX - a ? UINT64_MAX : a + b; }
uint32_t sum32(uint32_t a, uint32_t b) { return b > UINT32_MAX - a ? UINT32_MAX : a + b; }
void put(uint8_t* p, uint64_t v, int bytes) {
  for (int n = 0; n < bytes; ++n) p[n] = uint8_t(v >> (8 * n));
}
uint64_t get(const uint8_t* p, int bytes) {
  uint64_t v = 0;
  for (int n = 0; n < bytes; ++n) v |= uint64_t(p[n]) << (8 * n);
  return v;
}
bool dated(int day) { return day >= 18262 && day <= 84005; }
}  // namespace
bool Pending::empty() const {
  if (undatedMs || undatedSessions) return false;
  for (const auto& d : dates)
    if (d.milliseconds || d.sessions) return false;
  return true;
}
int Pending::add(int day, uint64_t ms, uint32_t sessions) {
  if (!ms && !sessions) return 0;
  if (dated(day)) {
    for (auto& d : dates)
      if (d.day == day || !d.day) {
        d.day = day;
        d.milliseconds = sum(d.milliseconds, ms);
        d.sessions = sum32(d.sessions, sessions);
        return day;
      }
  }
  undatedMs = sum(undatedMs, ms);
  undatedSessions = sum32(undatedSessions, sessions);
  return 0;
}
void Stats::begin(Period selected, int selectedDay) {
  *this = {};
  period = selected;
  day = selectedDay;
  if (!dated(day)) return;
  const auto date = readingstats::dateFromDay(day);
  switch (period) {
    case Period::Day:
      first = last = day;
      plotFirst = day - 6;
      count = 7;
      break;
    case Period::Month:
      first = plotFirst = readingstats::dayIndex({date.year, date.month, 1});
      count = readingstats::daysInMonth(date.year, date.month);
      last = first + count - 1;
      break;
    case Period::Week:
      first = plotFirst = readingstats::weekStart(day, false);
      last = std::min(first + 6, 84005);
      count = 7;
      break;
    case Period::Year:
      first = plotFirst = readingstats::dayIndex({date.year, 1, 1});
      last = first + (readingstats::daysInMonth(date.year, 2) == 29 ? 366 : 365) - 1;
      count = 12;
      break;
  }
}
void Stats::add(const Delta& d) {
  if (!first || !d.day) return;
  if (d.day >= first && d.day <= last) {
    milliseconds = sum(milliseconds, d.milliseconds);
    sessions = sum32(sessions, d.sessions);
  }
  int bucket = d.day - plotFirst;
  if (period == Period::Year) {
    if (d.day < first || d.day > last) return;
    bucket = readingstats::dateFromDay(d.day).month - 1;
  }
  if (bucket >= 0 && bucket < count) {
    buckets[bucket] = sum(buckets[bucket], d.milliseconds);
    counts[bucket] = sum32(counts[bucket], d.sessions);
  }
}
void Stats::add(const Pending& pending) {
  for (const auto& d : pending.dates) add(d);
}
uint64_t Stats::millisecondsWithout(const Pending* active) const {
  uint64_t value = milliseconds;
  if (active)
    for (const auto& d : active->dates)
      if (d.day && d.day >= first && d.day <= last) value -= std::min(value, d.milliseconds);
  return value;
}
uint64_t Stats::bucketWithout(int index, const Pending* active) const {
  if (index < 0 || index >= count) return 0;
  uint64_t value = buckets[index];
  if (active)
    for (const auto& d : active->dates) {
      if (!d.day) continue;
      int bucket = d.day - plotFirst;
      if (period == Period::Year) {
        if (d.day < first || d.day > last) continue;
        bucket = readingstats::dateFromDay(d.day).month - 1;
      }
      if (bucket == index) value -= std::min(value, d.milliseconds);
    }
  return value;
}
int shiftPeriod(Period period, int day, int step, int today) {
  if (!dated(day) || !dated(today) || !step) return day;
  auto date = readingstats::dateFromDay(day);
  switch (period) {
    case Period::Day:
      day += step;
      break;
    case Period::Month: {
      const int month = date.year * 12 + date.month - 1 + step;
      day = readingstats::dayIndex({month / 12, month % 12 + 1, 1});
      break;
    }
    case Period::Year:
      day = readingstats::dayIndex({date.year + step, 1, 1});
      break;
    case Period::Week:
      day += 7 * step;
      break;
  }
  return dated(day) ? day : (step < 0 ? 18262 : 84005);
}
bool advanceTracked(State& state, uint32_t elapsed, Clock from, Clock to, Pending& result) {
  result = {};
  const bool focus = state.running && state.phase == Phase::Focus;
  const uint32_t consumed = focus ? std::min(elapsed, state.remainingMs) : 0;
  const bool complete = advance(state, elapsed);
  if (!consumed) return complete;
  const uint32_t earned = complete ? 1 : 0;
  const int64_t wall = (int64_t(to.day) - from.day) * 86400 + int64_t(to.second) - from.second;
  const int64_t drift = wall * 1000 - elapsed;
  if (!dated(from.day) || !dated(to.day) || from.second >= 86400 || to.second >= 86400 || drift < -15000 ||
      drift > 15000) {
    result.add(0, consumed, earned);
    return complete;
  }
  const uint32_t beforeMidnight = (86400 - from.second) * 1000;
  const uint32_t first = std::min(consumed, beforeMidnight);
  result.add(from.day, first, earned && consumed < beforeMidnight ? 1 : 0);
  if (consumed >= beforeMidnight) result.add(from.day + 1, consumed - first, earned);
  return complete;
}
void encodeHistory(const State& state, const Pending& pending, uint32_t sequence, uint8_t* out,
                   const DisplayOptions& options) {
  std::memset(out, 0, kHistoryBytes);
  std::memcpy(out, "PHJ1", 4);
  encode(state, sequence, out + 4);
  for (int n = 0; n < 4; ++n) {
    const auto& d = pending.dates[n];
    auto* p = out + 68 + n * 16;
    put(p, d.day, 4);
    put(p + 4, d.milliseconds, 8);
    put(p + 12, d.sessions, 4);
  }
  put(out + 132, pending.undatedMs, 8);
  put(out + 140, pending.undatedSessions, 4);
  out[144] = 1;
  out[145] = uint8_t(options.accumulation);
  out[146] = options.showAccumulation;
  put(out + 156, readingstats::checksum(out, 156), 4);
}
bool decodeHistory(const uint8_t* data, State& state, Pending& pending, uint32_t& sequence, DisplayOptions* options) {
  if (std::memcmp(data, "PHJ1", 4) || get(data + 156, 4) != readingstats::checksum(data, 156) ||
      !decode(data + 4, state, sequence) || data[144] > 1 ||
      (data[144] == 1 && (data[145] > uint8_t(Period::Week) || data[146] > 1)))
    return false;
  pending = {};
  for (int n = 0; n < 4; ++n) {
    const auto* p = data + 68 + n * 16;
    auto& d = pending.dates[n];
    d.day = int(get(p, 4));
    d.milliseconds = get(p + 4, 8);
    d.sessions = uint32_t(get(p + 12, 4));
    if ((!d.day && (d.milliseconds || d.sessions)) || (d.day && !dated(d.day))) return false;
  }
  pending.undatedMs = get(data + 132, 8);
  pending.undatedSessions = uint32_t(get(data + 140, 4));
  if (options) {
    *options = {};
    if (data[144] == 1) {
      options->accumulation = static_cast<Period>(data[145]);
      options->showAccumulation = data[146] != 0;
    }
  }
  return true;
}
}  // namespace pomodoro
