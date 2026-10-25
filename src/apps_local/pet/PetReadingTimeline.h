#pragma once

#include <algorithm>

#include "PetState.h"

namespace pet {
// The journal is append-ordered. A clock reversal protects the save instead of inferring a death.
class ReadingTimeline {
 public:
  void begin(const Profile& p, int64_t now) {
    *this = {};
    anchor = cursor = p.lastReadAt;
    checkpoint = p.baselineSeconds + p.readingSeconds;
    through = now;
    living = p.created && p.lifeStatus == LifeStatus::Active;
    legacy = living && anchor < 1577836800;
    grace = living && !legacy && p.mode != Mode::Relaxed ? absenceGrace(p.mode) : 0;
  }
  void add(int64_t start, uint32_t seconds, int32_t localDay = 0, uint32_t secondOfDay = 0) {
    const auto previousTotal = total;
    total = seconds > UINT64_MAX - total ? UINT64_MAX : total + seconds;
    if (!living) return;
    if (start < 1577836800 || start > through || seconds > uint64_t(through - start)) {
      if (!legacy) ordered = false;
      return;
    }
    if (!legacy) {
      const auto end = start + seconds;
      if (end > anchor) {
        if (previousStart && start < previousStart) ordered = false;
        previousStart = start;
        if (lostAt) return;
        if (grace && start >= cursor && start - cursor >= grace) {
          lostAt = cursor + grace;
          return;
        }
        cursor = std::max(cursor, end);
      }
    }
    if (!ordered || lostAt || localDay <= 0 || secondOfDay >= 86400) return;
    uint32_t skip = uint32_t(std::min<uint64_t>(seconds, checkpoint > previousTotal ? checkpoint - previousTotal : 0));
    while (seconds) {
      const auto part = std::min(seconds, 86400 - secondOfDay);
      const unsigned slot = unsigned(localDay) % REWARD_DAYS;
      if (rewardDays[slot] != localDay) {
        rewardDays[slot] = localDay;
        daySeconds[slot] = 0;
      }
      const auto ignored = std::min(skip, part);
      const auto after = uint32_t(std::min<uint64_t>(UINT32_MAX, uint64_t(daySeconds[slot]) + part));
      const auto before = after - std::min(after, part - ignored);
      foodCredit += dailyFood(after) - dailyFood(before);
      medicineCredit += dailyMedicine(after) - dailyMedicine(before);
      daySeconds[slot] = after;
      skip -= ignored;
      seconds -= part;
      if (localDay == INT32_MAX) break;
      ++localDay;
      secondOfDay = 0;
    }
  }
  void finish(int64_t now = 0) {
    through = std::max(through, now);
    if (!lostAt && grace && through >= cursor && through - cursor >= grace) lostAt = cursor + grace;
  }
  bool valid() const { return ordered; }
  bool migration() const { return legacy; }
  int64_t endedAt() const { return lostAt; }
  void applyProvisions(Profile& draft, const Profile& saved) const {
    draft.earnedFood = saved.earnedFood + std::min(UINT64_MAX / 300 - saved.earnedFood, foodCredit);
    draft.earnedMedicine = saved.earnedMedicine + std::min(UINT64_MAX / 900 - saved.earnedMedicine, medicineCredit);
  }

 private:
  int64_t anchor = 0, cursor = 0, previousStart = 0, through = 0, grace = 0, lostAt = 0;
  uint64_t total = 0, checkpoint = 0, foodCredit = 0, medicineCredit = 0;
  // UTC-12 to UTC+14 can revisit either of the last two local dates without reversing UTC.
  static constexpr unsigned REWARD_DAYS = 3;
  int32_t rewardDays[REWARD_DAYS]{};
  uint32_t daySeconds[REWARD_DAYS]{};
  bool living = false, legacy = false, ordered = true;
};
}  // namespace pet
