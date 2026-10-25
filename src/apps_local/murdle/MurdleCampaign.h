#pragma once
#include "MurdleCampaignSeeds.h"
#include "MurdleCore.h"
namespace murdlecampaign {
static_assert(kCaseCount == kCasesPerTier * murdle::kTierCount);
struct Progress {
  uint8_t completed[kProgressBytes]{};
  bool cleared(int index) const {
    return index >= 0 && index < kCaseCount && (completed[index / 8] & (1u << (index % 8)));
  }
  void finish(int index) {
    if (index >= 0 && index < kCaseCount) completed[index / 8] |= 1u << (index % 8);
  }
  int next() const {
    for (int n = 0; n < kCaseCount; ++n)
      if (!cleared(n)) return n;
    return kCaseCount;
  }
  int count() const {
    int total = 0;
    for (int n = 0; n < kCaseCount; ++n) total += cleared(n);
    return total;
  }
  bool unlocked(int index) const { return index >= 0 && index < kCaseCount; }
};
inline constexpr int first(murdle::Tier tier) { return static_cast<int>(tier) * kCasesPerTier; }
inline constexpr int kPagesPerTier = (kCasesPerTier + kCasesPerPage - 1) / kCasesPerPage;
inline murdle::Tier tier(int index) { return static_cast<murdle::Tier>(index / kCasesPerTier); }
}  // namespace murdlecampaign
