#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "../../src/apps_local/pet/PetCompanion.h"
#include "../../src/apps_local/pet/PetFeeding.h"
#include "../../src/apps_local/pet/PetReaction.h"
#include "../../src/apps_local/pet/PetRestTransition.h"

int main() {
  uint32_t now = 0;
  unsigned frames = 0;
  petfeed::Clock feeding;
  feeding.start();
  while (feeding.active()) {
    ++frames;
    feeding.painted(now);
    while (!feeding.update(++now)) assert(now < 20000);
  }
  assert(frames == 7 && now == (petdisplay::NATIVE_GRAYSCALE ? 9600u : 2700u));
  std::printf("PET meal: %u ms dwell, %u phases\n", now, frames);
  for (auto kind : {petreact::Kind::Petting, petreact::Kind::Growth}) {
    now = frames = 0;
    petreact::Clock reaction;
    reaction.start(kind);
    while (reaction.active()) {
      ++frames;
      reaction.painted(now, kind, reaction.phase());
      while (!reaction.update(++now)) assert(now < 20000);
    }
    const bool growth = kind == petreact::Kind::Growth;
    assert(frames == (growth ? 8u : 3u));
    assert(now == (petdisplay::NATIVE_GRAYSCALE ? (growth ? 9200u : 4800u) : (growth ? 3100u : 1500u)));
  }
  now = frames = 0;
  petcompanion::Clock greeting;
  greeting.start(petcompanion::Kind::Greet);
  while (greeting.active()) {
    ++frames;
    greeting.painted(now, greeting.kind(), greeting.phase());
    while (!greeting.update(++now)) assert(now < 20000);
  }
  assert(frames == 2 && now == (petdisplay::NATIVE_GRAYSCALE ? 2000u : 600u));
  std::puts("PET action dwell budgets and complete visual phase counts PASS (panel time excluded)");
}
