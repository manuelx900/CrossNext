// Host-only catalogue extension: stdout contains 50 validated seeds per tier.
#include <cstdio>
#include <vector>

#include "../src/apps_local/murdle/MurdleCampaign.h"
#include "../src/apps_local/murdle/MurdleCast.h"

int main() {
  static murdle::Scratch scratch;
  static murdle::Puzzle puzzle;
  std::vector<uint32_t> seeds, fingerprints;
  seeds.reserve(400);
  fingerprints.reserve(400);
  for (int t = 0; t < murdle::kTierCount; ++t) {
    const auto tier = static_cast<murdle::Tier>(t);
    fingerprints.clear();
    const auto build = [&](uint32_t seed) {
      uint8_t cast[murdle::kMaxCats][murdle::kMaxItems]{};
      const auto shape = murdle::shapeOf(tier);
      return murdle::drawCast(seed, shape, cast) &&
             murdle::generate(tier, seed, cast, murdle::attrMasksFor(cast, shape), scratch, puzzle);
    };
    for (int i = 0; i < 50; ++i) {
      const auto seed = murdlecampaign::kSeeds[murdlecampaign::first(tier) + i];
      if (!build(seed)) return 1;
      fingerprints.push_back(murdle::fingerprint(puzzle));
      seeds.push_back(seed);
    }
    uint32_t candidate = 0x20261019u ^ (uint32_t(t) * 0x9e3779b9u);
    for (int added = 0; added < 50;) {
      candidate = candidate * 1664525u + 1013904223u;
      bool duplicate = false;
      for (int previousTier = 0; previousTier < murdle::kTierCount; ++previousTier)
        for (int i = 0; i < 50; ++i)
          duplicate |= candidate == murdlecampaign::kSeeds[previousTier * murdlecampaign::kCasesPerTier + i];
      for (auto seed : seeds) duplicate |= candidate == seed;
      if (duplicate || !build(candidate)) continue;
      const auto fingerprint = murdle::fingerprint(puzzle);
      for (auto previous : fingerprints) duplicate |= fingerprint == previous;
      if (duplicate || murdle::countSolutions(puzzle, 2, scratch) != 1) continue;
      murdle::Grid grid;
      if (murdle::deduce(puzzle, grid) < 0 || !grid.complete()) continue;
      seeds.push_back(candidate);
      fingerprints.push_back(fingerprint);
      std::printf("%d 0x%08Xu\n", t, candidate);
      ++added;
    }
  }
}
