#pragma once
#include <algorithm>
#include <iterator>

#include "ReadingStatsCore.h"
namespace readinggoal {
inline constexpr unsigned minutes[] = {0, 5, 10, 15, 20, 30, 45, 60, 90, 120};
inline unsigned target(unsigned choice) { return choice < std::size(minutes) ? minutes[choice] : 0; }
inline bool met(uint64_t seconds, unsigned goal) { return goal && seconds >= uint64_t(goal) * 60; }
inline unsigned streak(const readingstats::Summary& summary, int day, unsigned goal) {
  if (!goal || !day) return 0;
  if (!met(summary.onDay(day), goal)) --day;
  unsigned count = 0;
  while (count < readingstats::kHistoryDays && met(summary.onDay(day - count), goal)) ++count;
  return count;
}
// Disposable daily projection. Cache misses rebuild from the authoritative history.
bool load(readingstats::Summary& summary, int day);
}  // namespace readinggoal
