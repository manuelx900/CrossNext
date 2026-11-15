#pragma once
#include "QuestCore.h"

namespace questmilestone {
inline constexpr uint32_t LEVELS[] = {50, 100, 150};
inline uint8_t reached(uint32_t level) {
  uint8_t mask = 0;
  for (unsigned n = 0; n < 3; ++n)
    if (level >= LEVELS[n]) mask |= uint8_t(1u << n);
  return mask;
}
inline int pending(const quest::Profile& p) {
  const auto available = reached(quest::level(p.xp)) & ~p.celebratedMilestones;
  for (unsigned n = 0; n < 3; ++n)
    if (available & (1u << n)) return int(n);
  return -1;
}
inline bool acknowledge(quest::Profile& p) {
  const int n = pending(p);
  if (n < 0) return false;
  p.celebratedMilestones |= uint8_t(1u << n);
  return true;
}
}  // namespace questmilestone
