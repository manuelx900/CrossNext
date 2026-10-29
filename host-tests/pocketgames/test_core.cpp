#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../src/apps_local/pocketgames/PocketGamesCore.h"
#include "../../src/apps_local/pocketgames/PocketGamesStore.h"
namespace pg = pocketgames;
int checks = 0, failures = 0;
#define CHECK(value)                                  \
  do {                                                \
    ++checks;                                         \
    if (!(value)) {                                   \
      ++failures;                                     \
      std::printf("FAIL %d: %s\n", __LINE__, #value); \
    }                                                 \
  } while (0)

// Reference captures walk from existing friendly anchors toward the candidate.
uint64_t referenceCaptures(const pg::Othello& game, int square, int colour) {
  if (square < 0 || square >= 64 || game.board[square]) return 0;
  uint64_t captured = 0;
  for (int anchor = 0; anchor < 64; ++anchor) {
    if (game.board[anchor] != colour) continue;
    const int dx = square % 8 - anchor % 8, dy = square / 8 - anchor / 8;
    const int ax = std::abs(dx), ay = std::abs(dy), length = std::max(ax, ay);
    if (length < 2 || (dx && dy && ax != ay)) continue;
    const int step = (dx > 0 ? 1 : dx < 0 ? -1 : 0) + (dy > 0 ? 8 : dy < 0 ? -8 : 0);
    uint64_t ray = 0;
    bool enclosed = true;
    for (int n = 1; n < length; ++n) {
      const int at = anchor + step * n;
      if (game.board[at] != 3 - colour) {
        enclosed = false;
        break;
      }
      ray |= uint64_t(1) << at;
    }
    if (enclosed) captured |= ray;
  }
  return captured;
}
void captureRules() {
  for (int colour : {1, 2}) {
    pg::Othello game;
    game.turn = colour;
    const int origin = 27;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dy) continue;
        game.board[origin + dy * 8 + dx] = 3 - colour;
        game.board[origin + 2 * (dy * 8 + dx)] = colour;
      }
    CHECK(pg::flips(game, origin, colour) == 8);
    CHECK(pg::play(game, origin));
    CHECK(pg::disks(game, colour) == 17 && pg::disks(game, 3 - colour) == 0);
    CHECK(game.over);
  }
  for (uint32_t seed = 1; seed <= 120; ++seed) {
    pg::State state;
    pg::start(state, pg::Kind::Othello, seed);
    uint32_t random = seed;
    for (int move = 0; move < 60 && !state.othello.over; ++move) {
      int legal[64], count = 0;
      for (int square = 0; square < 64; ++square) {
        const uint64_t expected = referenceCaptures(state.othello, square, state.othello.turn);
        int flips = 0;
        for (int n = 0; n < 64; ++n) flips += (expected >> n) & 1;
        CHECK(pg::flips(state.othello, square, state.othello.turn) == flips);
        if (expected) legal[count++] = square;
      }
      CHECK(count > 0);
      if (!count) break;
      const int square = legal[pg::random(random) % count];
      const auto before = state.othello;
      const uint64_t expected = referenceCaptures(before, square, before.turn) | (uint64_t(1) << square);
      CHECK(pg::play(state.othello, square));
      for (int n = 0; n < 64; ++n)
        CHECK(state.othello.board[n] == ((expected >> n) & 1 ? before.turn : before.board[n]));
      CHECK(state.othello.over || pg::hasMove(state.othello, state.othello.turn));
    }
  }
}
int main() {
  captureRules();
  pg::State s, restored;
  uint8_t bytes[pg::kSaveBytes];
  uint32_t generation = 0;
  CHECK(static_cast<uint8_t>(pg::Kind::Othello) == 1);
  for (uint32_t seed = 1; seed <= 80; ++seed) {
    pg::start(s, pg::Kind::Othello, seed);
    int legal = 0;
    for (int n = 0; n < 64; ++n) legal += pg::flips(s.othello, n, 1) > 0;
    CHECK(legal == 4);
    CHECK(!pg::play(s.othello, 27));
    CHECK(pg::play(s.othello, 19));
    CHECK(pg::disks(s.othello, 1) == 4 && pg::disks(s.othello, 2) == 1);
    uint32_t random = seed;
    int steps = 0;
    while (!s.othello.over && steps++ < 64) {
      const int before = pg::disks(s.othello, 1) + pg::disks(s.othello, 2);
      if (s.othello.turn == 2) {
        const pg::Othello pending = s.othello;
        const int square = pg::chooseBotMove(s.othello);
        CHECK(square >= 0 && pg::flips(s.othello, square, s.othello.turn) > 0);
        CHECK(!std::memcmp(pending.board, s.othello.board, sizeof(pending.board)));
        CHECK(pending.turn == s.othello.turn && pending.over == s.othello.over && pending.passed == s.othello.passed);
        pg::Othello expected = pending;
        CHECK(pg::play(expected, square));
        CHECK(pg::bot(s.othello));
        CHECK(!std::memcmp(expected.board, s.othello.board, sizeof(expected.board)));
        CHECK(expected.turn == s.othello.turn && expected.over == s.othello.over &&
              expected.passed == s.othello.passed);
      } else {
        int choices[64]{}, count = 0;
        for (int n = 0; n < 64; ++n)
          if (pg::flips(s.othello, n, 1)) choices[count++] = n;
        CHECK(count > 0);
        CHECK(pg::play(s.othello, choices[pg::random(random) % count]));
      }
      CHECK(pg::disks(s.othello, 1) + pg::disks(s.othello, 2) == before + 1);
      pg::encode(s, seed, bytes);
      CHECK(pg::decode(bytes, s.kind, restored, generation));
    }
    CHECK(s.othello.over && steps <= 60 && !pg::hasMove(s.othello, 1) && !pg::hasMove(s.othello, 2));
  }
  {
    pg::start(s, pg::Kind::Othello, 2731, 2);
    pg::encode(s, 91, bytes);
    CHECK(pg::decode(bytes, s.kind, restored, generation) && generation == 91);
    for (size_t n = 0; n < pg::kSaveBytes; ++n) {
      bytes[n] ^= 1;
      CHECK(!pg::decode(bytes, s.kind, restored, generation));
      bytes[n] ^= 1;
    }
    testStorage = {};
    pg::Store store;
    CHECK(store.load(s.kind, restored) && !store.hasSave());
    CHECK(store.save(s));
    int writes = testStorage.writes;
    CHECK(store.save(s) && testStorage.writes == writes);
    pg::Store reload;
    CHECK(reload.load(s.kind, restored) && reload.hasSave());
    const uint32_t old = s.seed;
    pg::start(s, s.kind, old + 1, 2);
    testStorage.writeLimit = 8;
    CHECK(!reload.save(s));
    testStorage.writeLimit = -1;
    pg::Store recovery;
    CHECK(recovery.load(s.kind, restored) && restored.seed == old);
  }
  for (int human : {1, 2})
    for (int difficulty = 0; difficulty < 3; ++difficulty) {
      pg::State play;
      pg::start(play, pg::Kind::Othello, 42, difficulty);
      play.humanColour = human;
      pg::encode(play, 4, bytes);
      CHECK(pg::decode(bytes, pg::Kind::Othello, restored, generation));
      CHECK(restored.humanColour == human && restored.level == difficulty);
      for (int turn = 0; turn < 120 && !play.othello.over; ++turn) {
        const int colour = play.othello.turn, before = pg::disks(play.othello, 1) + pg::disks(play.othello, 2);
        int legal = -1;
        for (int sq = 0; sq < 64; ++sq)
          if (pg::flips(play.othello, sq, colour)) {
            legal = sq;
            break;
          }
        CHECK(legal >= 0);
        if (legal < 0) break;
        if (colour == human)
          CHECK(pg::play(play.othello, legal));
        else
          CHECK(pg::bot(play.othello, difficulty));
        CHECK(pg::disks(play.othello, 1) + pg::disks(play.othello, 2) == before + 1);
      }
      CHECK(play.othello.over);
    }
  std::printf("Pocket games: %d checks, %d failed; state=%zu, store=%zu bytes\n", checks, failures, sizeof(pg::State),
              sizeof(pg::Store));
  return failures ? 1 : 0;
}
