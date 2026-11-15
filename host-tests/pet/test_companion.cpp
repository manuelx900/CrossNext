#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "../../src/apps_local/pet/PetCompanion.h"

int main() {
  using namespace petcompanion;
  for (auto kind : {Kind::Sit, Kind::Stretch, Kind::Greet, Kind::Discover, Kind::Satisfied, Kind::WakeFirst}) {
    for (uint32_t start : {0u, UINT32_MAX - 400u}) {
      Clock clock;
      clock.start(kind);
      assert(!clock.update(start + 90000));
      clock.painted(start, kind, Phase::Second);
      assert(!clock.update(start + 90000));
      clock.painted(start, kind, Phase::First);
      const auto duration = kind == Kind::Sit ? petdisplay::hold(1800, 700) : petdisplay::hold(1000, 300);
      assert(!clock.update(start + duration - 1));
      assert(clock.update(start + duration) && clock.phase() == Phase::Second);
      assert(!clock.update(start + 90000));
      clock.painted(start + duration, kind, Phase::Second);
      assert(clock.update(start + duration * 2) && !clock.active());
      assert(!clock.scheduleIdle(start + duration * 2 + 29999, true));
      assert(clock.scheduleIdle(start + duration * 2 + 30000, true));
      clock.cancel();
      assert(!clock.active() && !clock.update(start + 90000));
      assert(!clock.scheduleIdle(start + 100000, false));
    }
  }
  pet::Profile p;
  p.created = true;
  p.readingSeconds = pet::stageStart(pet::Stage::Juvenile);
  p.saciety = p.joy = p.energy = p.hygiene = 100;
  assert(calm(p) && touchKind(p, Kind::Sit) == Kind::Sit);
  p.joy = 20;
  assert(touchKind(p, Kind::Sit) == Kind::Greet);
  p.hygiene = 0;
  assert(touchKind(p, Kind::Sit) == Kind::Dirty);
  p.energy = 0;
  assert(touchKind(p, Kind::Sit) == Kind::Tired);
  p.saciety = 0;
  assert(touchKind(p, Kind::Sit) == Kind::Hungry);
  p.illness = pet::Illness::Cold;
  assert(touchKind(p, Kind::Sit) == Kind::Sick);
  const auto saved = p;
  p.resting = true;
  assert(!awake(p) && touchKind(p, Kind::Sit) == Kind::WakeFirst);
  p.resting = false;
  assert(p.saciety == saved.saciety && p.energy == saved.energy && p.joy == saved.joy);
  bool slots[36]{};
  for (auto kind : {Kind::Sit, Kind::Stretch, Kind::Greet})
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 1; age <= 4; ++age) {
        const auto slot = poseSlot(kind, pet::Species(species), pet::Stage(age));
        assert(slot < 36 && !slots[slot]);
        slots[slot] = true;
        for (auto phase : {Phase::First, Phase::Second}) {
          const auto id = artIndex(slot, phase);
          assert(id >= petart::COMPANION_OFFSET && id + 1 < petart::ART_COUNT && id % 2 == 0);
        }
      }
  assert(poseSlot(Kind::Sit, pet::Species::Silva, pet::Stage::Egg) == 36);
  assert(emotion(Kind::Greet) == Emotion::Curious && emotion(Kind::Satisfied) == Emotion::Satisfied);
  assert(emotion(Kind::Discover) == Emotion::Surprised && emotion(Kind::WakeFirst) == Emotion::None);
  std::puts("PET companion clocks, contexts, sleep priority and all 72 frame indices PASS");
}
