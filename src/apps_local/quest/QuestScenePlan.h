#pragma once

#include <algorithm>

#include "QuestCampaign.h"
#include "QuestCampaignPacing.h"

namespace questscene {
inline unsigned sagaVersion(const quest::Profile& p, unsigned book) {
  const int completed = int(quest::completedPaths(p)) - int(book * quest::CHAPTERS_PER_BOOK);
  for (unsigned n = sizeof(quest::SAGA_MILESTONES); n-- > 1;)
    if (completed >= quest::SAGA_MILESTONES[n]) return n;
  return 0;
}

// Retain the prologue offsets and use spare offsets for pages in a saga version.
inline constexpr int OPENING_BOOKMARK = 128, OPENING_STRIDE = 16, SAGA_BOOKMARK = 208, SAGA_STRIDE = 8;
struct StoryPosition {
  int section, page;
};
inline StoryPosition decodeBookmark(unsigned value, int prologueSections, unsigned version) {
  if (value == 0) return {0, 0};
  if (prologueSections && value >= OPENING_BOOKMARK &&
      value < unsigned(OPENING_BOOKMARK + prologueSections * OPENING_STRIDE)) {
    const int offset = int(value) - OPENING_BOOKMARK;
    return {offset / OPENING_STRIDE, offset % OPENING_STRIDE};
  }
  const unsigned start = SAGA_BOOKMARK + version * SAGA_STRIDE;
  return {prologueSections, value >= start && value < start + SAGA_STRIDE ? int(value - start) : 0};
}
inline uint16_t encodeBookmark(int section, int page, int prologueSections, unsigned version) {
  if (section < prologueSections)
    return uint16_t(OPENING_BOOKMARK + section * OPENING_STRIDE + std::min(page, OPENING_STRIDE - 1));
  return uint16_t(SAGA_BOOKMARK + version * SAGA_STRIDE + std::min(page, SAGA_STRIDE - 1));
}
}  // namespace questscene
