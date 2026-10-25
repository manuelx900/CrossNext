#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "../../src/apps_local/pet/PetPendingTap.h"
#include "../../src/apps_local/pet/PetSleepAnimation.h"

int main() {
  petsleep::Clock clock;
  assert(!clock.update(100, false));
  assert(clock.update(100, true));
  assert(clock.step() == 0);
  assert(!clock.update(1599, true));
  assert(clock.update(1600, true) && clock.step() == 1);
  assert(clock.update(3100, true) && clock.step() == 2);
  assert(clock.update(4600, true) && clock.step() == 3);
  assert(clock.update(6100, true) && clock.step() == 0);
  assert(!clock.update(12100, true));
  assert(!clock.update(12101, false) && clock.step() == 0);
  assert(clock.update(UINT32_MAX - 100, true));
  assert(!clock.update(1398, true));
  assert(clock.update(1399, true) && clock.step() == 1);
  assert(petsleep::frame(0) == 0 && petsleep::frame(1) == 1 && petsleep::frame(2) == 2 && petsleep::frame(3) == 1);

  pet::Profile p;
  p.created = true;
  p.readingSeconds = pet::stageStart(pet::Stage::Newborn);
  p.resting = true;
  assert(petsleep::enabled(p, pet::View::Home));
  assert(petsleep::enabled(p, pet::View::Dashboard));
  assert(petsleep::enabled(p, pet::View::Reward));
  assert(!petsleep::enabled(p, pet::View::Care));
  p.resting = false;
  assert(!petsleep::enabled(p, pet::View::Home));
  p.resting = true;
  p.illness = pet::Illness::Cold;
  assert(!petsleep::enabled(p, pet::View::Home));
  p.illness = pet::Illness::Healthy;
  p.lifeStatus = pet::LifeStatus::Lost;
  assert(!petsleep::enabled(p, pet::View::Home));
  p.lifeStatus = pet::LifeStatus::Retired;
  assert(!petsleep::enabled(p, pet::View::Home));
  p.lifeStatus = pet::LifeStatus::Active;
  p.readingSeconds = 0;
  assert(!petsleep::enabled(p, pet::View::Home));
  p.readingSeconds = pet::stageStart(pet::Stage::Adult);
  p.species = pet::Species::Ember;
  assert(petsleep::artIndex(p, 2) == 70 && petsleep::artIndex(p, 2) + 1 < petsleep::ART_COUNT);
  assert(petsleep::headIndex(p) + 1 == petsleep::BODY_OFFSET - 1);
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 1; age <= 4; ++age) {
      p.species = pet::Species(species);
      p.readingSeconds = pet::stageStart(pet::Stage(age));
      for (unsigned mood = 0; mood < 5; ++mood)
        for (unsigned step = 0; step < 4; ++step) {
          const unsigned index = petsleep::bodyIndex(p.species, pet::Stage(age), petsleep::Pose(mood), step);
          assert(index >= petsleep::BODY_OFFSET && index % 2 == 0 && index + 1 < petsleep::ART_COUNT);
          if (mood < 4) assert(index == petsleep::graphicIndex(3 + species * 16 + (age - 1) * 4 + mood, step));
        }
      for (bool resting : {false, true})
        for (const auto illness : {pet::Illness::Healthy, pet::Illness::Cold, pet::Illness::Malaise}) {
          p.resting = resting;
          p.illness = illness;
          for (const auto view : {pet::View::Dashboard, pet::View::Home, pet::View::Reward, pet::View::Evolution})
            assert(petsleep::breathing(p, view));
          for (const auto view : {pet::View::Care, pet::View::Album, pet::View::Games, pet::View::Settings,
                                  pet::View::Death, pet::View::Memory})
            assert(!petsleep::breathing(p, view));
        }
    }
  for (const auto status : {pet::LifeStatus::Lost, pet::LifeStatus::Retired}) {
    p.lifeStatus = status;
    assert(!petsleep::breathing(p, pet::View::Home) && !petsleep::breathing(p, pet::View::Dashboard));
  }
  p.lifeStatus = pet::LifeStatus::Active;
  p.readingSeconds = 0;
  assert(petsleep::breathing(p, pet::View::Home));
  assert(!petsleep::moodVisible(p, pet::View::Home));
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned step = 0; step < 4; ++step) {
      const auto id = petsleep::eggIndex(species, step);
      assert(id >= petsleep::EGG_OFFSET && id % 2 == 0 && id + 1 < petsleep::ART_COUNT);
    }
  petinput::PendingTap tap;
  int x = 0, y = 0;
  assert(!tap.take(1, x, y));
  // Release arrives while rendering owns the lock; the main loop handles it afterward.
  tap.capture(240, 110, 4);
  assert(tap.take(4, x, y) && x == 240 && y == 110);
  assert(!tap.take(4, x, y));
  // A release against the previously visible screen must not target the new screen.
  tap.capture(240, 760, 4);
  assert(!tap.take(5, x, y));
  tap.capture(40, 70, 5);
  tap.capture(200, 300, 5);
  assert(tap.take(5, x, y) && x == 40 && y == 70);
  tap.capture(10, 10, 5);
  tap.clear();
  assert(!tap.take(5, x, y));
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 1; age <= 4; ++age) {
      pet::Profile p;
      p.created = true;
      p.species = pet::Species(species);
      p.readingSeconds = pet::stageStart(pet::Stage(age));
      for (unsigned step = 0; step < 4; ++step) {
        p.resting = true;
        assert(petsleep::fullBodyIndex(p, step) == petsleep::artIndex(p, step));
        p.resting = false;
        p.joy = 90;
        assert(petsleep::bodyPose(p) == petsleep::Pose::Happy);
        p.saciety = 10;
        assert(petsleep::bodyPose(p) == petsleep::Pose::Hungry);
        p.saciety = 70;
        p.energy = 10;
        assert(petsleep::bodyPose(p) == petsleep::Pose::Tired);
        p.illness = pet::Illness::Malaise;
        assert(petsleep::bodyPose(p) == petsleep::Pose::Sick);
        p.illness = pet::Illness::Healthy;
        p.energy = 100;
      }
    }
  std::puts("PET animation: PASS (timing, catch-up, wrap, all ages/poses, slot bounds, pending tap)");
}
