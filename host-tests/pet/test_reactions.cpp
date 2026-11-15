#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "../../src/apps_local/pet/PetReaction.h"

int main() {
  static_assert(petreact::LEGACY_OFFSET == 630 && petreact::LEGACY_ART_COUNT == 822);
  static_assert(petreact::OFFSET == 822 && petreact::ART_COUNT == 1014);
  bool used[petreact::ART_COUNT - petreact::OFFSET]{};
  for (auto kind : {petreact::Kind::Petting, petreact::Kind::Cleaning, petreact::Kind::Healing, petreact::Kind::Game})
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 1; age <= 4; ++age)
        for (auto phase : {petreact::Phase::First, petreact::Phase::Second}) {
          const auto mask = petreact::artIndex(kind, pet::Species(species), pet::Stage(age), phase);
          const auto legacy =
              petreact::artIndex(kind, pet::Species(species), pet::Stage(age), phase, petreact::Art::Legacy);
          assert(mask - legacy == petreact::ART_COUNT - petreact::LEGACY_ART_COUNT);
          assert(mask >= petreact::OFFSET && mask + 1 < petreact::ART_COUNT);
          const auto at = mask - petreact::OFFSET;
          assert(!used[at] && !used[at + 1]);
          used[at] = used[at + 1] = true;
        }
  for (bool value : used) assert(value);
  assert(petreact::artIndex(petreact::Kind::Growth, pet::Species::Silva, pet::Stage::Baby, petreact::Phase::First) ==
         petreact::ART_COUNT);
  assert(petreact::artIndex(petreact::Kind::Petting, pet::Species::Silva, pet::Stage::Egg, petreact::Phase::First) ==
         petreact::ART_COUNT);
  for (uint32_t start : {0u, UINT32_MAX - 500u}) {
    for (auto kind : {petreact::Kind::Petting, petreact::Kind::Cleaning, petreact::Kind::Healing, petreact::Kind::Game,
                      petreact::Kind::Growth}) {
      petreact::Clock clock;
      clock.start(kind);
      assert(clock.active() && !clock.update(start + 100000));
      clock.painted(start, petreact::Kind::None, clock.phase());
      assert(!clock.update(start + 100000));
      clock.painted(start, kind, petreact::Phase::Result);
      assert(!clock.update(start + 100000));
      auto now = start;
      unsigned frames = 0;
      while (clock.active()) {
        const auto phase = clock.phase();
        if (kind == petreact::Kind::Growth)
          assert(petreact::silhouette(phase) == (phase != petreact::Phase::Reveal && phase != petreact::Phase::Result));
        clock.painted(now, kind, phase);
        clock.painted(now + 500, kind, phase);
        assert(!clock.update(now + petdisplay::hold(700, 199)));
        assert(clock.update(now + 10000));
        now += 10000;
        ++frames;
        if (clock.active()) assert(!clock.update(now + 100000));
        assert(frames <= 8);
      }
      assert(frames == (kind == petreact::Kind::Growth ? 8u : 3u));
      assert(clock.kind() == petreact::Kind::None && !clock.update(now + 100000));
      clock.start(kind);
      clock.cancel();
      clock.painted(now, kind, petreact::Phase::First);
      assert(!clock.active() && !clock.update(now + 100000));
    }
  }
  pet::Profile p;
  p.readingSeconds = pet::stageStart(pet::Stage::Adult);
  p.noticedStage = pet::Stage::Newborn;
  assert(petreact::growthStage(p, petreact::Phase::Idle) == pet::Stage::Newborn);
  assert(petreact::growthStage(p, petreact::Phase::BeforeAgain) == pet::Stage::Newborn);
  assert(petreact::growthStage(p, petreact::Phase::AfterAgain) == pet::Stage::Adult);
  assert(petreact::silhouette(petreact::Phase::AfterAgain));
  assert(!petreact::silhouette(petreact::Phase::Reveal));
}
