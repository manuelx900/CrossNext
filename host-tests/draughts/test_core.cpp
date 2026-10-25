#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../src/apps_local/checkers/DraughtsCore.h"
using namespace draughts;
static Game game;
static Generator legal;
static void generate() {
  legal.begin(game);
  int chunks = 0;
  while (!legal.tick(64)) assert(++chunks < 1000000);
}
static void empty(Rules rules) {
  start(game, rules);
  std::memset(game.cells, 0, sizeof(game.cells));
}
int main() {
  for (int rule = 0; rule < 4; ++rule) {
    start(game, Rules(rule));
    int pieces[2]{};
    for (auto p : game.cells)
      if (p) ++pieces[owner(p)];
    assert(game.size == (rule == 0 ? 10 : 8));
    assert(pieces[0] == (rule == 0 ? 20 : 12));
    assert(pieces[0] == pieces[1]);
    generate();
    assert(legal.count() == (rule == 0 ? 9 : 7));
  }
  // A man captures backwards only in international and Brazilian draughts.
  for (auto rule : {Rules::Brazilian, Rules::Spanish, Rules::English}) {
    empty(rule);
    game.cells[26] = 1;
    game.cells[35] = 2;
    generate();
    assert(legal.permits(26, 44) == (rule == Rules::Brazilian));
  }
  // Majority is compulsory except in English; equal counts prefer kings in Spanish.
  for (auto rule : {Rules::Brazilian, Rules::Spanish, Rules::English}) {
    empty(rule);
    game.cells[42] = 1;
    game.cells[33] = 2;
    game.cells[17] = 2;
    game.cells[46] = 1;
    game.cells[37] = 4;
    generate();
    assert(legal.permits(42, 24));
    assert(legal.permits(46, 28) == (rule == Rules::English));
    assert(play(game, legal, 42, 24));
    assert(game.turn == 0 && game.forced == 24);
    generate();
    assert(legal.permits(24, 10));
    assert(play(game, legal, 24, 10));
    assert(game.turn == 1 && !game.cells[33] && !game.cells[17]);
  }
  empty(Rules::Spanish);
  game.cells[42] = 1;
  game.cells[33] = 2;
  game.cells[46] = 1;
  game.cells[37] = 4;
  generate();
  assert(!legal.permits(42, 24));
  assert(legal.permits(46, 28));
  // Flying kings have every clear landing; English kings only move one step.
  empty(Rules::Brazilian);
  game.cells[42] = 3;
  generate();
  assert(legal.permits(42, 7));
  empty(Rules::English);
  game.cells[42] = 3;
  generate();
  assert(!legal.permits(42, 7));
  // Taken pieces remain blockers until the whole capture has ended.
  empty(Rules::Brazilian);
  game.cells[42] = 3;
  game.cells[35] = 2 | kTaken;
  game.forced = 42;
  generate();
  assert(!legal.permits(42, 28));
  // Brazilian men pass through the far row as men if a backward capture remains.
  empty(Rules::Brazilian);
  game.cells[17] = 1;
  game.cells[10] = 2;
  game.cells[12] = 2;
  generate();
  assert(play(game, legal, 17, 3));
  assert(game.turn == 0 && game.cells[3] == 1);
  generate();
  assert(play(game, legal, 3, 21));
  assert(game.turn == 1 && game.cells[21] == 1);
  // English crowning ends the turn even if a newly crowned king could jump back.
  empty(Rules::English);
  game.cells[17] = 1;
  game.cells[10] = 2;
  game.cells[12] = 2;
  generate();
  assert(play(game, legal, 17, 3));
  assert(game.turn == 1 && game.cells[3] == 3 && game.cells[12] == 2);
  // Special endgame deadlines survive a material reduction: no fresh sixteen turns.
  empty(Rules::International);
  game.cells[90] = 3;
  game.cells[9] = 4;
  game.cells[25] = 4;
  game.endLimit = 16;
  game.endRemaining[0] = 3;
  game.endRemaining[1] = 4;
  generate();
  assert(play(game, legal, 90, 81));
  assert(game.endLimit == 5 && game.endRemaining[0] == 2 && game.endRemaining[1] == 4);
  // Two remaining turns are counted for both sides, not awarded afresh after a king move.
  empty(Rules::Brazilian);
  game.cells[56] = 3;
  game.cells[7] = 4;
  game.endLimit = 5;
  game.endRemaining[0] = 1;
  game.endRemaining[1] = 0;
  generate();
  assert(play(game, legal, 56, 49));
  assert(game.draw);
  // Exercise every variant under deterministic play; every accepted prefix is legal.
  uint32_t rng = 19;
  int plies = 0;
  for (int rule = 0; rule < 4; ++rule)
    for (int round = 0; round < 12; ++round) {
      start(game, Rules(rule));
      for (int turn = 0; turn < 700; ++turn) {
        generate();
        if (!legal.count() || game.draw) break;
        const int index = choose(game, legal, round % 3, rng);
        assert(index >= 0 && index < legal.count());
        const auto m = legal.at(index).move;
        assert(play(game, legal, m.from, m.to));
        ++plies;
        int pieces[2]{};
        for (auto p : game.cells)
          if (p) ++pieces[owner(p)];
        assert(pieces[0] <= 20 && pieces[1] <= 20);
      }
    }
  std::printf(
      "Draughts: four variants, mandatory majority/quality, chains, flying kings and %d legal steps; workspace=%zu "
      "bytes\n",
      plies, sizeof(legal));
}
