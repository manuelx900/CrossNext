#include <cstdio>

#include "../../src/apps_local/murdle/MurdleCampaign.h"
#include "../../src/apps_local/murdle/MurdleCast.h"
#include "../../src/apps_local/murdle/MurdleSave.h"
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  static murdle::Scratch scratch;
  static murdle::Puzzle puzzle;
  uint8_t cast[murdle::kMaxCats][murdle::kMaxItems]{};
  murdlesave::State state{};
  state.seed = murdlecampaign::kSeeds[0];
  state.campaignCase = 0;
  const auto tier = murdlecampaign::tier(0);
  state.tier = state.caseTier = static_cast<uint8_t>(tier);
  if (!murdle::drawCast(state.seed, murdle::shapeOf(tier), cast) ||
      !murdle::generate(tier, state.seed, cast, murdle::attrMasksFor(cast, murdle::shapeOf(tier)), scratch, puzzle))
    return 2;
  state.fingerprint = murdle::fingerprint(puzzle);
  state.caseNumber = 1;
  state.hasCase = 1;
  uint8_t bytes[murdlesave::kBytes];
  murdlesave::encode(state, bytes);
  murdlesave::State roundtrip{};
  if (!murdlesave::decode(bytes, sizeof(bytes), roundtrip)) return 3;
  FILE* out = std::fopen(argv[1], "wb");
  if (!out) return 4;
  std::fwrite(bytes, 1, sizeof(bytes), out);
  std::fclose(out);
  std::printf("Murdle campaign case 1: save v%u, %zu bytes, fingerprint %u\n", murdlesave::kVersion, sizeof(bytes),
              state.fingerprint);
}
