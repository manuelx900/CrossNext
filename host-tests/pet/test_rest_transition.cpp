#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "../../src/apps_local/pet/PetRestTransition.h"

int main() {
  using namespace petrest;
  for (auto kind : {Kind::Sleep, Kind::Wake})
    for (bool bed : {false, true})
      for (uint32_t begin : {0u, UINT32_MAX - 400u}) {
        Clock clock;
        clock.start(kind, bed);
        uint32_t now = begin;
        unsigned frames = 0, steps = 0;
        while (clock.active()) {
          const auto phase = clock.phase();
          assert(!clock.update(now + 100000));
          clock.painted(now, Phase::Idle);
          assert(!clock.update(now + 100000));
          clock.painted(now, phase);
          const unsigned duration = walking(phase) ? petdisplay::hold(650, 200) : petdisplay::hold(900, 300);
          assert(!clock.update(now + duration - 1));
          assert(clock.update(now + duration));
          steps += walking(phase);
          now += duration;
          assert(++frames <= 6);
        }
        assert(frames == (bed ? (kind == Kind::Sleep ? 5u : 6u) : 2u));
        assert(steps == (bed ? 3u : 0u));
        clock.start(kind, bed);
        clock.cancel();
        assert(!clock.active() && !clock.update(now + 90000));
      }
  pet::Profile p;
  assert(!eligible(p, Kind::Sleep));
  p.created = true;
  p.readingSeconds = pet::stageStart(pet::Stage::Juvenile);
  assert(eligible(p, Kind::Wake) && !eligible(p, Kind::Sleep));
  p.resting = true;
  assert(eligible(p, Kind::Sleep) && !eligible(p, Kind::Wake));
  p.illness = pet::Illness::Cold;
  assert(!eligible(p, Kind::Sleep));
  p.illness = pet::Illness::Healthy;
  p.lifeStatus = pet::LifeStatus::Lost;
  assert(!eligible(p, Kind::Sleep));
  bool used[96]{};
  for (bool right : {false, true})
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 1; age <= 4; ++age) {
        const auto slot = walkSlot(pet::Species(species), pet::Stage(age), right);
        for (bool secondFrame : {false, true}) {
          const auto id = artIndex(slot, secondFrame);
          assert(id >= WALK_OFFSET && id + 1 < ART_COUNT && !used[id - WALK_OFFSET]);
          used[id - WALK_OFFSET] = used[id - WALK_OFFSET + 1] = true;
        }
      }
  for (bool present : used) assert(present);
  assert(walkSlot(pet::Species::Silva, pet::Stage::Egg, false) == 24);
  assert(progress(Phase::ApproachFirst) < progress(Phase::ApproachSecond));
  assert(progress(Phase::ApproachSecond) < progress(Phase::Arrive));
  assert(progress(Phase::ReturnFirst) > progress(Phase::ReturnSecond));
  assert(progress(Phase::ReturnSecond) > progress(Phase::ReturnLast));
  std::puts("PET rest transition: painted frames, wrap, cancellation, no-bed, eligibility and 96 sprite records PASS");
}
