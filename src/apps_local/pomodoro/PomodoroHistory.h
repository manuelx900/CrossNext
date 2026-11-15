#pragma once

#include "../reading/ReadingStatsCore.h"
#include "PomodoroCore.h"

namespace pomodoro {
enum class Period : uint8_t { Day, Month, Year, Week };
struct DisplayOptions {
  Period accumulation = Period::Week;
  bool showAccumulation = true;
};
struct Clock {
  int day = 0;
  uint32_t second = 0;
};
struct Delta {
  int day = 0;
  uint64_t milliseconds = 0;
  uint32_t sessions = 0;
};
// Four date buckets cover ordinary midnight crossings; undated time also
// preserves study during missing clocks or prolonged storage failures.
struct Pending {
  Delta dates[4]{};
  uint64_t undatedMs = 0;
  uint32_t undatedSessions = 0;
  bool empty() const;
  // Returns the recorded date, or zero when credit must remain undated.
  int add(int day, uint64_t milliseconds, uint32_t sessions);
};
struct Stats {
  Period period = Period::Day;
  int day = 0, first = 0, last = 0, plotFirst = 0, count = 0;
  uint64_t milliseconds = 0, buckets[31]{};
  uint32_t sessions = 0, counts[31]{};
  void begin(Period selected, int selectedDay);
  void add(const Delta& delta);
  void add(const Pending& pending);
  uint64_t millisecondsWithout(const Pending* active) const;
  uint64_t bucketWithout(int index, const Pending* active) const;
};
int shiftPeriod(Period period, int day, int step, int today);
// Advances the existing timer once and accounts only time actually consumed
// by its focus phase. Clock discontinuities retain credit without a false date.
bool advanceTracked(State& state, uint32_t elapsed, Clock from, Clock to, Pending& result);
constexpr size_t kHistoryBytes = 160;
void encodeHistory(const State& state, const Pending& pending, uint32_t sequence, uint8_t* out,
                   const DisplayOptions& options = {});
bool decodeHistory(const uint8_t* data, State& state, Pending& pending, uint32_t& sequence,
                   DisplayOptions* options = nullptr);
}  // namespace pomodoro
