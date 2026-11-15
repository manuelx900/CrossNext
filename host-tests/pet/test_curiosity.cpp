#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "../../src/apps_local/pet/PetCuriosity.h"

int main() {
  constexpr auto lookMs = petdisplay::hold(1800, 500), blinkMs = petdisplay::hold(1000, 300);
  static_assert(petcurious::OFFSET == 582 && petcurious::ART_COUNT == 630);
  for (uint32_t start : {0u, UINT32_MAX - 500u}) {
    petcurious::Clock clock;
    assert(!clock.active() && !clock.update(start));
    clock.start();
    assert(clock.phase() == petcurious::Phase::Look && !clock.update(start + 100000));
    clock.painted(start);
    clock.painted(start + 900);
    assert(!clock.update(start + lookMs - 1));
    assert(clock.update(start + lookMs) && clock.phase() == petcurious::Phase::Blink);
    assert(!clock.update(start + 500000));
    clock.painted(start + 600000);
    assert(!clock.update(start + 600000 + blinkMs - 1));
    assert(clock.update(start + 600000 + blinkMs) && !clock.active());
    assert(!clock.update(start + 900000));
    clock.start();
    clock.cancel();
    clock.painted(start);
    assert(!clock.update(start + 900000) && !clock.active());
  }
  bool ids[48]{};
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 1; age < 5; ++age)
      for (auto phase : {petcurious::Phase::Look, petcurious::Phase::Blink}) {
        const auto id = petcurious::artIndex(pet::Species(species), pet::Stage(age), phase);
        assert(id >= petcurious::OFFSET && id + 1 < petcurious::ART_COUNT && !ids[id - petcurious::OFFSET]);
        ids[id - petcurious::OFFSET] = ids[id + 1 - petcurious::OFFSET] = true;
      }
  for (bool used : ids) assert(used);
  pet::Profile p;
  p.created = true;
  p.readingSeconds = pet::stageStart(pet::Stage::Juvenile);
  p.saciety = p.joy = p.energy = p.hygiene = 100;
  assert(petcurious::eligible(p, pet::View::Dashboard));
  for (auto view : {pet::View::Care, pet::View::Games, pet::View::Home, pet::View::Notice})
    assert(!petcurious::eligible(p, view));
  const auto healthy = p;
  for (unsigned state = 0; state < 8; ++state) {
    p = healthy;
    switch (state) {
      case 0:
        p.readingSeconds = 0;
        break;
      case 1:
        p.resting = true;
        break;
      case 2:
        p.illness = pet::Illness::Cold;
        break;
      case 3:
        p.saciety = 29;
        break;
      case 4:
        p.energy = 19;
        break;
      case 5:
        p.hygiene = 20;
        break;
      case 6:
        p.joy = 59;
        break;
      case 7:
        p.lifeStatus = pet::LifeStatus::Lost;
        break;
    }
    assert(!petcurious::eligible(p, pet::View::Dashboard));
  }
}
