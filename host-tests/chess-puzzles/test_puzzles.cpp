#include <HalStorage.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "../../src/apps_local/chess/puzzles/ChessPuzzleCore.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleStore.h"

namespace cp = chesspuzzle;
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
int main() {
  cp::Session session;
  chess::SearchBuffers buffers;
  cp::Progress progress;
  std::set<std::string> ids;
  int alternatives = 0, wrong = 0;
  for (int n = 0; n < cp::kCount; ++n) {
    CHECK(cp::load(session, n));
    const auto& data = *session.source;
    CHECK(ids.insert(data.id).second);
    CHECK(data.rating <= 1400 ? n < 400 : data.rating < 2000 ? n >= 400 && n < 750 : n >= 750);
    for (int k = 0; k < session.legal.count; ++k) CHECK(!session.legal.moves[k].promotion);
    const uint64_t original = chess::positionKey(session.position);
    // Every legal attempt changes the board, even away from the source solution.
    const auto expected = data.moves[0];
    for (int k = 0; k < session.legal.count; ++k) {
      const auto move = session.legal.moves[k];
      if (move.from == (expected & 63) && move.to == (expected >> 6)) continue;
      cp::play(session, move.from, move.to);
      if (session.status == cp::Status::Solved)
        ++alternatives;
      else {
        CHECK(session.status == cp::Status::Thinking || session.status == cp::Status::Draw);
        CHECK(chess::positionKey(session.position) != original && session.ply == 1 && session.playerMoves == 1);
        if (n < 24 && session.status == cp::Status::Thinking) {
          CHECK(cp::reply(session, buffers));
          CHECK(session.position.whiteToMove == data.white || cp::terminal(session));
        }
        ++wrong;
      }
      CHECK(cp::load(session, n));
      break;
    }
    CHECK(!cp::play(session, 64, 64));
    CHECK(session.status == cp::Status::Illegal && chess::positionKey(session.position) == original);
    while (session.status != cp::Status::Solved && session.ply < data.plies) {
      for (int k = 0; k < session.legal.count; ++k) CHECK(!session.legal.moves[k].promotion);
      const auto move = data.moves[session.ply];
      CHECK(cp::play(session, move & 63, move >> 6));
      if (session.status == cp::Status::Thinking) CHECK(cp::reply(session, buffers));
      CHECK(session.status == cp::Status::Correct || session.status == cp::Status::Solved);
    }
    CHECK(session.status == cp::Status::Solved);
    CHECK(session.legal.count == 0 && chess::isInCheck(session.position));
    CHECK(cp::remaining(session) == 0);
    CHECK(cp::markSolved(progress, n));
    CHECK(!cp::markSolved(progress, n));
    CHECK(cp::load(session, n));
    CHECK(chess::positionKey(session.position) == original && session.ply == 0);
  }
  CHECK(wrong > 900);
  // A different legal mate must win even when it is absent from the supplied line.
  cp::Puzzle alternative{};
  alternative.white = true;
  alternative.plies = 1;
  alternative.moves[0] = 46 | 54 << 6;  // Qg7 is the supplied mate.
  session.source = &alternative;
  session.status = cp::Status::Playing;
  session.ply = 0;
  CHECK(chess::parseFen("7k/5K2/6Q1/8/8/8/8/8 w - - 0 1", session.position));
  chess::generateLegalMoves(session.position, session.legal);
  CHECK(cp::play(session, 46, 47));  // Qh6 is also mate.
  CHECK(session.status == cp::Status::Solved);
  ++alternatives;
  CHECK(cp::solvedCount(progress, 0) == 400 && cp::solvedCount(progress, 1) == 350 &&
        cp::solvedCount(progress, 2) == 250);
  CHECK(!cp::load(session, -1) && !cp::load(session, cp::kCount));
  uint8_t bytes[cp::kProgressBytes];
  progress.lastIndex = 988;
  progress.cursor[0] = 399;
  progress.cursor[1] = 349;
  progress.cursor[2] = 249;
  cp::encode(progress, 21, bytes);
  cp::Progress restored;
  uint32_t generation = 0;
  CHECK(cp::decode(bytes, restored, generation) && generation == 21 && restored.lastIndex == 988);
  CHECK(!std::memcmp(progress.solved, restored.solved, sizeof(progress.solved)));
  for (size_t n = 0; n < sizeof(bytes); ++n) {
    bytes[n] ^= 1;
    CHECK(!cp::decode(bytes, restored, generation));
    bytes[n] ^= 1;
  }
  // Legacy CPZ1 has no last category; migrate its Easy cursor without losing results.
  bytes[3] = '1';
  uint32_t legacyHash = 2166136261u;
  for (size_t n = 0; n < cp::kProgressBytes - 4; ++n) legacyHash = (legacyHash ^ bytes[n]) * 16777619u;
  for (int n = 0; n < 4; ++n) bytes[cp::kProgressBytes - 4 + n] = static_cast<uint8_t>(legacyHash >> (8 * n));
  CHECK(cp::decode(bytes, restored, generation) && restored.lastIndex == 399);
  CHECK(cp::solvedCount(restored, 2) == 250);
  testStorage = {};
  cp::Store store;
  CHECK(store.load(restored));
  CHECK(store.save(progress));
  const int writes = testStorage.writes;
  CHECK(store.save(progress) && testStorage.writes == writes);
  cp::Store reloaded;
  CHECK(reloaded.load(restored));
  CHECK(cp::solvedCount(restored, 2) == 250 && restored.cursor[0] == 399);
  auto changed = progress;
  changed.cursor[0] = 17;
  testStorage.writeLimit = 8;
  CHECK(!reloaded.save(changed));
  testStorage.writeLimit = -1;
  cp::Store recovered;
  CHECK(recovered.load(restored) && restored.cursor[0] == 399);
  testStorage.files["/.crosspoint/chess-puzzles-b.bin"] = {'C', 'P', 'Z', '3'};
  cp::Store future;
  CHECK(!future.load(restored));
  CHECK(!future.save(changed));
  testStorage.files["/.crosspoint/chess-puzzles-b.bin"] = {};
  cp::Store resetter;
  CHECK(resetter.load(restored));
  testStorage.available = false;
  CHECK(!resetter.reset(restored));
  testStorage.available = true;
  CHECK(resetter.reset(restored));
  CHECK(restored.lastIndex == cp::kCount && cp::solvedCount(restored, 0) == 0);
  cp::Store cleared;
  CHECK(cleared.load(restored) && restored.lastIndex == cp::kCount);
  // Play continues past the advertised move budget; there is no solution lock.
  CHECK(cp::load(session, 0));
  session.source = &alternative;
  session.ply = 0;
  session.playerMoves = 0;
  session.onLine = false;
  session.status = cp::Status::Playing;
  chess::setStartPosition(session.position);
  chess::generateLegalMoves(session.position, session.legal);
  CHECK(cp::play(session, 12, 28));
  CHECK(cp::reply(session, buffers));
  const auto next = session.legal.moves[0];
  CHECK(cp::play(session, next.from, next.to, next.promotion));
  CHECK(session.playerMoves == 2 && cp::goal(session) == 1 && cp::remaining(session) == 0);
  // Promotions are available after free play diverges from the source line.
  for (int type : {chess::Queen, chess::Rook, chess::Bishop, chess::Knight}) {
    CHECK(cp::load(session, 0));
    session.source = &alternative;
    CHECK(chess::parseFen("7k/P7/1K6/8/8/8/8/8 w - - 0 1", session.position));
    chess::generateLegalMoves(session.position, session.legal);
    CHECK(cp::play(session, 48, 56, type));
    CHECK(chess::pieceType(session.position.squares[56]) == type);
  }
  std::printf("chess puzzles: %d checks, %d failed; 1000 mating lines, %d free legal attempts, %d alternate mates\n",
              checks, failures, wrong, alternatives);
  return failures ? 1 : 0;
}
