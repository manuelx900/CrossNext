#include "ChessPuzzleCore.h"

#include <cstring>

namespace chesspuzzle {
namespace {
uint32_t hash(const uint8_t* data, size_t count) {
  uint32_t value = 2166136261u;
  while (count--) value = (value ^ *data++) * 16777619u;
  return value;
}
uint32_t read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void write32(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = static_cast<uint8_t>(value >> (8 * n));
}
const chess::Move* findMove(const Session& session, int from, int to, int promotion = chess::Queen) {
  for (int n = 0; n < session.legal.count; ++n) {
    const auto& move = session.legal.moves[n];
    if (move.from == from && move.to == to && (!move.promotion || move.promotion == promotion)) return &move;
  }
  return nullptr;
}
void apply(Session& session, const chess::Move& move) {
  chess::Undo undo;
  session.lastFrom = move.from;
  session.lastTo = move.to;
  chess::makeMove(session.position, move, undo);
  ++session.ply;
  chess::generateLegalMoves(session.position, session.legal);
}
void assess(Session& session) {
  if (!session.legal.count) {
    session.status = !chess::isInCheck(session.position)                     ? Status::Draw
                     : session.position.whiteToMove == session.source->white ? Status::Lost
                                                                             : Status::Solved;
  } else if (session.position.halfmoveClock >= 100) {
    session.status = Status::Draw;
  }
}
}  // namespace
bool load(Session& session, int index) {
  session.source = puzzle(index);
  session.status = Status::DataError;
  session.ply = 0;
  session.playerMoves = 0;
  session.onLine = true;
  session.selected = session.lastFrom = session.lastTo = -1;
  if (!session.source) return false;
  const auto& data = *session.source;
  session.position = {};
  for (int n = 0; n < 64; ++n) session.position.squares[n] = (data.board[n / 2] >> (n % 2 * 4)) & 15;
  session.position.whiteToMove = data.white;
  session.position.castling = data.castling;
  session.position.epSquare = data.epSquare;
  chess::generateLegalMoves(session.position, session.legal);
  if (!session.legal.count || data.plies < 1 || data.plies > 5 || data.plies % 2 != 1) return false;
  session.status = Status::Playing;
  return true;
}
int remaining(const Session& session) {
  if (session.status == Status::Solved) return 0;
  const int left = goal(session) - session.playerMoves;
  return left > 0 ? left : 0;
}
int goal(const Session& session) { return session.source ? (session.source->plies + 1) / 2 : 0; }
bool terminal(const Session& session) {
  return session.status == Status::Solved || session.status == Status::Lost || session.status == Status::Draw ||
         session.status == Status::DataError;
}
bool play(Session& session, int from, int to, int promotion) {
  if (!session.source || terminal(session) || session.position.whiteToMove != session.source->white) return false;
  const auto* found = findMove(session, from, to, promotion);
  session.selected = -1;
  if (!found) {
    session.status = Status::Illegal;
    return false;
  }
  const chess::Move move = *found;
  if (session.onLine) {
    const uint16_t expected = session.ply < session.source->plies ? session.source->moves[session.ply] : 0;
    session.onLine =
        session.ply < session.source->plies && from == (expected & 63) && to == (expected >> 6) && !move.promotion;
  }
  apply(session, move);
  if (session.playerMoves < 65535) ++session.playerMoves;
  session.status = Status::Thinking;
  assess(session);
  return true;
}
bool reply(Session& session, chess::SearchBuffers& buffers) {
  if (!session.source || terminal(session) || session.position.whiteToMove == session.source->white) return false;
  chess::Move move;
  bool found = false;
  if (session.onLine && session.ply < session.source->plies) {
    const uint16_t encoded = session.source->moves[session.ply];
    if (const auto* expected = findMove(session, encoded & 63, encoded >> 6)) {
      move = *expected;
      found = true;
    }
  }
  if (!found) {
    session.onLine = false;
    const auto result = chess::search(session.position, 3, buffers, 6000);
    if (!result.hasMove) {
      assess(session);
      return false;
    }
    move = result.best;
  }
  apply(session, move);
  session.status = Status::Correct;
  assess(session);
  return true;
}
bool solved(const Progress& progress, int index) {
  return index >= 0 && index < kCount && (progress.solved[index / 8] & (1u << (index % 8)));
}
bool markSolved(Progress& progress, int index) {
  if (index < 0 || index >= kCount || solved(progress, index)) return false;
  progress.solved[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
  return true;
}
int solvedCount(const Progress& progress, int group) {
  if (group < 0 || group > 2) return 0;
  int count = 0;
  for (int n = kGroupStarts[group]; n < kGroupStarts[group] + kGroupCounts[group]; ++n) count += solved(progress, n);
  return count;
}
void encode(const Progress& progress, uint32_t generation, uint8_t* bytes) {
  std::memset(bytes, 0, kProgressBytes);
  std::memcpy(bytes, "CPZ2", 4);
  write32(bytes + 4, generation);
  for (int n = 0; n < 3; ++n) {
    bytes[8 + n * 2] = static_cast<uint8_t>(progress.cursor[n]);
    bytes[9 + n * 2] = static_cast<uint8_t>(progress.cursor[n] >> 8);
  }
  std::memcpy(bytes + 14, progress.solved, sizeof(progress.solved));
  bytes[139] = static_cast<uint8_t>(progress.lastIndex);
  bytes[140] = static_cast<uint8_t>(progress.lastIndex >> 8);
  write32(bytes + kProgressBytes - 4, hash(bytes, kProgressBytes - 4));
}
bool decode(const uint8_t* bytes, Progress& progress, uint32_t& generation) {
  if ((std::memcmp(bytes, "CPZ1", 4) && std::memcmp(bytes, "CPZ2", 4)) ||
      read32(bytes + kProgressBytes - 4) != hash(bytes, kProgressBytes - 4))
    return false;
  for (int n = 0; n < 3; ++n) {
    const int index = bytes[8 + n * 2] | uint16_t(bytes[9 + n * 2]) << 8;
    if (index >= kGroupCounts[n]) return false;
    progress.cursor[n] = static_cast<uint16_t>(index);
  }
  std::memcpy(progress.solved, bytes + 14, sizeof(progress.solved));
  progress.lastIndex = bytes[3] == '2' ? bytes[139] | uint16_t(bytes[140]) << 8 : progress.cursor[0];
  if (progress.lastIndex > kCount) return false;
  generation = read32(bytes + 4);
  return true;
}
}  // namespace chesspuzzle
