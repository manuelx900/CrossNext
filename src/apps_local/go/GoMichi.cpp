#include "GoMichi.h"

#include "GoEngine.h"
#include "michi/MichiBridge.h"

namespace gomichi {
namespace {

// Our position as the bridge wants it: one byte a point, row major, and the
// values are already `go::kEmpty` / `go::kBlack` / `go::kWhite` by construction.
// The game holds its board two bits a point so that a thirteen by thirteen
// position fits a packet; michi wants bytes.
void flatten(const go::Game& game, uint8_t out[go::kMaxPoints]) {
  const int points = game.points();
  for (int i = 0; i < points; ++i) out[i] = game.at(i);
}

}  // namespace

Settings settingsFor(const go::Level level) {
  // Simulations only. The handicap, the komi and which colour the player takes
  // are three SEPARATE settings and none of them is strength: a level that
  // silently spotted you stones made "easy" mean two things at once.
  //
  // The counts are where the engine was MEASURED, not guessed: 300 seeded
  // games a level against GNU Go 3.8 at --level 1, nine by nine, area scoring,
  // komi 7.5. See docs/apps/go.md for the ladder and for why an unseeded match
  // of the same size measured nothing at all. The budgets are what those counts
  // are expected to cost on this chip, which is roughly twenty-six times slower
  // than the laptop they were measured on, and they exist because that
  // multiplier is an estimate and the five second ceiling is not negotiable.
  switch (level) {
    case go::Level::Easy:
      return Settings{60, 1200};
    case go::Level::Medium:
      // 36% against GNU Go 3.8 at --level 1, which is level with michi-c2's own
      // build at this count.
      return Settings{500, 2500};
    case go::Level::Hard:
      // 72% against that same opponent.
      return Settings{1500, 4000};
    case go::Level::Count_:
      break;
  }
  return Settings{500, 2500};
}

int chooseMove(const go::Game& game, const go::Level level, uint32_t& seed, const Clock clock) {
  // michi's generator is a global that starts at 1 and that nothing else here
  // sets, so an unseeded engine answers a given position the same way forever:
  // every game from a cold boot was the SAME game. The seed the caller keeps is
  // michi's seed, advanced once a move by michi's own generator so the next move
  // is a different draw and a given starting seed still replays exactly.
  seed = seed * 1664525u + 1013904223u;
  michi_bridge_seed(seed);

  // When to pass. Measured against GNU Go 3.8 rather than reasoned about: it
  // passes in every game, passes while LOSING in three games of eight, and once
  // passed at move 34 with 47 of 81 points still empty. So the rule is not
  // "stop when ahead" and not "fill the board first". It is: stop when nothing
  // is worth playing.
  //
  // Under area scoring that has an exact meaning. A point already surrounded by
  // one colour is counted for them whether or not a stone sits on it, so playing
  // there gains nothing. A point belonging to nobody is worth one, so it is
  // worth taking. Hence: once the opponent has passed, pass unless a point
  // belonging to nobody is still playable.
  //
  // What this replaces was the Leela Zero rule -- pass only if the opponent
  // passed AND passing WINS -- which never stopped a game the machine was
  // losing. Measured, its first pass was the LAST move of the game, after the
  // board had been filled to within a dozen points. That is the "it goes on
  // forever" a beginner meets.
  //
  // Note michi cannot make this call itself: expand() only offers PASS as a
  // move when a position has two or fewer legal moves, so in a normal position
  // it is not in the tree at all.
  if (game.passes >= 1) {
    // How many points still belong to nobody, and can they change the result?
    //
    // Instrumented on real games: when GNU Go passes there are typically four
    // to eight of these left, and under AREA scoring each is worth one, so
    // taking them is correct play rather than stubbornness. Leaving them is a
    // territory-rules habit. That is why "is anything left" is the wrong test
    // and cost forty moves a game -- it is right, and it is unbearable.
    //
    // The test that matters is whether they can change who wins. If the margin
    // already exceeds what every remaining neutral point is worth, the result
    // is settled and playing them out decides nothing.
    const int neutral = go::freePoints(game, game.toMove);
    const go::Score counted = go::score(game);
    const int marginHalves = counted.blackHalves - counted.whiteHalves;
    const int margin = (marginHalves < 0 ? -marginHalves : marginHalves) / 2;
    // Strictly greater: a margin equal to the points on the table can still be
    // erased by them, so that one is played out.
    if (margin > neutral) return go::kPass;
  }
  // And the rules fact underneath it: with no legal move that is not filling one
  // of our own eyes, passing is the only move there is.
  if (!go::hasUsefulMove(game, game.toMove)) return go::kPass;

  const Settings settings = settingsFor(level);
  uint8_t board[go::kMaxPoints];
  flatten(game, board);

  // The search never returns a pass: the two cases above are the only two this
  // app passes in, and michi liking a pass at sixty simulations is not one of
  // them. The bridge hands back its best non-pass move instead.
  const int ko = game.ko < game.points() ? static_cast<int>(game.ko) : -1;
  const int lastMove = game.lastMove < game.points() ? static_cast<int>(game.lastMove) : -1;
  const int move =
      michi_bridge_genmove(game.size, board, game.toMove, game.komiHalves, ko, lastMove,
                           static_cast<int>(game.moveNumber), settings.simulations, settings.budgetMs, clock);

  // Whatever the search produced has to survive OUR rules, and when it does not
  // the answer is the search's NEXT choice rather than a pass. michi keeps its
  // own superko hash and this game keeps its own ring; a pass here threw away a
  // move in the middle of a fight and, if the human passed back, the game.
  if (move >= 0 && go::legal(game, move, game.toMove)) return move;
  int ranked[8] = {};
  const int count = michi_bridge_ranked(game.size, ranked, 8);
  for (int i = 0; i < count; ++i) {
    if (go::legal(game, ranked[i], game.toMove)) return ranked[i];
  }
  // Nothing the search looked at is playable. Take any move that is not filling
  // our own eye before considering a pass, because hasUsefulMove() above said
  // there is one.
  const int points = game.points();
  for (int point = 0; point < points; ++point) {
    if (go::legal(game, point, game.toMove) && !go::isEye(game, point, game.toMove)) return point;
  }
  return go::kPass;
}

Context lastContext(const int size) {
  int ko = -1;
  int lastMove = -1;
  int moveNumber = 0;
  michi_bridge_context(size, &ko, &lastMove, &moveNumber);
  Context out{};
  out.ko = ko >= 0 ? ko : go::kNoPoint;
  out.lastMove = lastMove == -2 ? go::kPass : (lastMove >= 0 ? lastMove : go::kNoPoint);
  out.moveNumber = moveNumber;
  return out;
}

void forget() { michi_bridge_forget(); }

int lastSimulations() { return michi_bridge_last_simulations(); }

uint32_t lastMs() { return michi_bridge_last_ms(); }

// Dead stones are NOT michi's job here, and that is a measurement rather than
// a preference.
//
// michi's own `compute_all_status` crashes on a nearly-full board. Reproduced
// in forty lines of pure C with no C++ anywhere near it: five empty points is
// fine, three is a segmentation fault. The counting screen is always a nearly
// full board -- that is what counting is -- so the one position this app would
// ask the question in is the one position michi cannot answer it in.
//
// Upstream never meets this because `genmove` passes out of a decided game
// before the board gets that full, and its own use of the routine is at the end
// of a GTP game where it evidently does not. Fixing somebody else's search is a
// bigger commitment than this app needs to make for a function it already has,
// tested, next door.
//
// So: michi chooses the moves, which is what it is here for and what it is two
// or three stones better at. The owner-map estimator in GoEngine keeps the
// counting screen, where it is pinned against three settled positions whose
// answer is not in doubt.

}  // namespace gomichi
