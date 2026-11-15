#pragma once
#include <algorithm>
#include <iterator>

#include "ReadingStatsCore.h"
namespace readinggoal {
inline constexpr unsigned historyVersion = 1;
inline bool preserveLegacyHistory(unsigned version, bool savedLegacy) {
  return version < historyVersion || savedLegacy;
}
inline constexpr unsigned minutes[] = {0, 5, 10, 15, 20, 30, 45, 60, 90, 120};
inline unsigned target(unsigned choice) { return choice < std::size(minutes) ? minutes[choice] : 0; }
inline bool met(uint64_t seconds, unsigned goal) { return goal && seconds >= uint64_t(goal) * 60; }
enum class DayState : uint8_t { Untracked, InProgress, Complete, Missed };
inline DayState dayState(int day, int today, int start, uint64_t seconds, unsigned goal, bool legacyHistory = false) {
  if (!goal || !today || day > today) return DayState::Untracked;
  if ((!start || day < start) && !(legacyHistory && met(seconds, goal))) return DayState::Untracked;
  if (met(seconds, goal)) return DayState::Complete;
  return day < today ? DayState::Missed : DayState::InProgress;
}
enum class WallpaperState : uint8_t { NoGoal, Ready, Unavailable };
struct WallpaperData {
  uint64_t seconds = 0;
  unsigned goal = 0, streak = 0;
  WallpaperState state = WallpaperState::NoGoal;
};
inline unsigned streak(const readingstats::Summary& summary, int day, unsigned goal, int startDay = 0) {
  if (!goal || !day || (startDay && day < startDay)) return 0;
  if (!met(summary.onDay(day), goal)) --day;
  unsigned count = 0;
  while (count < readingstats::kHistoryDays && (!startDay || day - int(count) >= startDay) &&
         met(summary.onDay(day - count), goal))
    ++count;
  return count;
}
// Disposable daily projection. Cache misses rebuild from the authoritative history.
bool load(readingstats::Summary& summary, int day);
}  // namespace readinggoal
