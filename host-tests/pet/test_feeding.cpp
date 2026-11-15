#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "../../src/apps_local/pet/PetFeeding.h"

int main() {
  constexpr auto biteMs = petdisplay::hold(1200, 300), resultMs = petdisplay::hold(2400, 900);
  static_assert(petfeed::ART_COUNT == 582 && petsleep::ART_COUNT == 474);
  for (uint32_t start : {0u, UINT32_MAX - 500u}) {
    petfeed::Clock clock;
    assert(!clock.active() && !clock.update(start));
    clock.start();
    assert(clock.phase() == petfeed::Phase::Bite);
    assert(!clock.update(start + 100000));
    uint32_t now = start;
    for (unsigned bite = 0; bite < 3; ++bite) {
      assert(clock.phase() == petfeed::Phase::Bite && clock.foodStage() == bite);
      clock.painted(now);
      clock.painted(now + biteMs / 2);
      assert(!clock.update(now + biteMs - 1));
      assert(clock.update(now + biteMs) && clock.phase() == petfeed::Phase::Chew);
      assert(clock.foodStage() == bite + 1);
      assert(!clock.update(now + 500000));
      now += 600000;
      clock.painted(now);
      assert(clock.update(now + biteMs));
      now += biteMs;
    }
    assert(clock.phase() == petfeed::Phase::Result && clock.foodStage() == 3);
    clock.painted(now);
    assert(!clock.update(now + resultMs - 1));
    assert(clock.update(now + resultMs) && !clock.active());
    clock.start();
    clock.cancel();
    clock.painted(start);
    assert(!clock.update(start + 900000) && !clock.active());
  }
  assert(petfeed::choices(petfeed::Art::Extended) == 9);
  assert(petfeed::choices(petfeed::Art::Progressive) == 6);
  for (unsigned food = 6; food < 9; ++food)
    for (unsigned bite = 0; bite <= 3; ++bite) {
      const auto id = petfeed::progressiveFoodIndex(food, bite);
      assert(id >= petart::EXTRA_FOOD_OFFSET && id + 1 < petart::COMPANION_OFFSET);
    }
  bool ids[582]{};
  for (unsigned food = 0; food < 3; ++food)
    for (bool bitten : {false, true}) {
      const auto id = petfeed::foodIndex(food, bitten);
      assert(id >= 474 && id + 1 < 534 && !ids[id]);
      ids[id] = true;
    }
  for (unsigned food = 0; food < 6; ++food)
    for (unsigned eaten = 0; eaten <= 3; ++eaten) {
      const auto id = petfeed::progressiveFoodIndex(food, eaten);
      assert(id >= 534 && id + 1 < 582 && !ids[id]);
      ids[id] = true;
    }
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 1; age <= 4; ++age)
      for (auto phase : {petfeed::Phase::Bite, petfeed::Phase::Chew}) {
        const auto id = petfeed::petIndex(species, age, phase);
        assert(id >= 486 && id + 1 < 534 && !ids[id]);
        ids[id] = true;
      }
}
