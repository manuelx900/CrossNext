#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "../../src/apps_local/hearts/HeartsBrain.h"
#include "../../src/apps_local/home/HomeMenuCore.h"
int main() {
  homemenu::Config home;
  home.visible = 1u << int(homemenu::Id::Settings);
  for (const auto* title : {"JAIPUR", "SLITHERLINK", "HASHI", "SNAKE"})
    assert(homemenu::addShortcut(home, 0, title) != homemenu::Id::Count);
  uint8_t encoded[homemenu::kSaveBytes];
  homemenu::encode(home, 12, encoded);
  homemenu::Config restored;
  uint32_t generation = 0;
  assert(homemenu::decode(encoded, restored, generation));
  assert(generation == 12 && homemenu::visibleCount(restored) == 1);
  for (const auto* title : {"JAIPUR", "SLITHERLINK", "HASHI", "SNAKE"})
    assert(homemenu::findShortcut(restored, 0, title) == homemenu::Id::Count);
  assert(homemenu::addShortcut(restored, 0, "OTHELLO") != homemenu::Id::Count);
  assert(homemenu::findShortcut(restored, 0, "OTHELLO") != homemenu::Id::Count);
  for (int limit : {50, 100}) {
    hearts::Game g;
    g.total[0] = 49;
    g.taken[0] = 1;
    hearts::scoreHand(g, limit);
    assert(g.total[0] == 50);
    assert(g.phase == (limit == 50 ? hearts::Phase::GameOver : hearts::Phase::HandOver));
  }
  unsigned turns = 0;
  for (uint32_t seed = 1; seed <= 80; ++seed) {
    hearts::Game game;
    uint32_t rng = seed;
    hearts::newGame(game, rng);
    for (int n = 0; n < 4; ++n) {
      hearts::Observation obs;
      hearts::observe(game, static_cast<hearts::Seat>(n), obs);
      uint8_t pass[3];
      hearts::decidePass(obs, hearts::Skill::Normal, rng, pass);
      assert(hearts::setPass(game, static_cast<hearts::Seat>(n), pass, 3));
    }
    hearts::commitPass(game);
    while (game.phase != hearts::Phase::HandOver && game.phase != hearts::Phase::GameOver) {
      if (game.phase == hearts::Phase::TrickTaken) {
        hearts::sweepTrick(game, 50);
        continue;
      }
      hearts::Observation obs;
      hearts::observe(game, game.turn, obs);
      auto card = hearts::decidePlay(obs, hearts::Skill::Normal, rng);
      assert(hearts::isLegalPlay(game, game.turn, card));
      assert(hearts::playCard(game, card));
      ++turns;
    }
    assert(hearts::isConsistent(game));
  }
  std::printf("Both score limits and %u legal Normal-policy plays passed.\n", turns);
}
