#include "PocketGamesCore.h"

#include <algorithm>
#include <cstring>

namespace pocketgames {
namespace {
void afterMove(Othello& game) {
  game.turn = 3 - game.turn;
  game.passed = false;
  if (hasMove(game, game.turn)) return;
  game.turn = 3 - game.turn;
  game.passed = true;
  game.over = !hasMove(game, game.turn);
}
uint32_t hash(const uint8_t* bytes, size_t size) {
  uint32_t value = 2166136261u;
  while (size--) value = (value ^ *bytes++) * 16777619u;
  return value;
}
void put32(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = static_cast<uint8_t>(value >> (8 * n));
}
uint32_t get32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
}  // namespace
uint32_t random(uint32_t& state) {
  if (!state) state = 1;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}
void start(State& state, Kind kind, uint32_t seed, int level) {
  state = {};
  state.kind = kind;
  state.seed = seed ? seed : 1;
  state.level = static_cast<uint8_t>(std::max(0, std::min(level, 2)));
  switch (kind) {
    case Kind::Othello:
      state.othello.board[27] = state.othello.board[36] = 2;
      state.othello.board[28] = state.othello.board[35] = 1;
      break;
  }
}
int flips(const Othello& game, int square, int colour) {
  if (square < 0 || square >= 64 || game.board[square] || colour < 1 || colour > 2) return 0;
  int total = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      if (!dx && !dy) continue;
      int x = square % 8 + dx, y = square / 8 + dy, count = 0;
      while (x >= 0 && x < 8 && y >= 0 && y < 8 && game.board[y * 8 + x] == 3 - colour) {
        ++count;
        x += dx;
        y += dy;
      }
      if (x >= 0 && x < 8 && y >= 0 && y < 8 && game.board[y * 8 + x] == colour) total += count;
    }
  return total;
}
bool hasMove(const Othello& game, int colour) {
  for (int n = 0; n < 64; ++n)
    if (flips(game, n, colour)) return true;
  return false;
}
int disks(const Othello& game, int colour) {
  int count = 0;
  for (auto cell : game.board) count += cell == colour;
  return count;
}
bool play(Othello& game, int square) {
  if (game.over || !flips(game, square, game.turn)) return false;
  const int colour = game.turn;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      if (!dx && !dy) continue;
      int x = square % 8 + dx, y = square / 8 + dy, count = 0;
      while (x >= 0 && x < 8 && y >= 0 && y < 8 && game.board[y * 8 + x] == 3 - colour) {
        ++count;
        x += dx;
        y += dy;
      }
      if (x < 0 || x >= 8 || y < 0 || y >= 8 || game.board[y * 8 + x] != colour) continue;
      for (int n = 1; n <= count; ++n)
        game.board[(square / 8 + dy * n) * 8 + square % 8 + dx * n] = static_cast<uint8_t>(colour);
    }
  game.board[square] = static_cast<uint8_t>(colour);
  afterMove(game);
  return true;
}
int chooseBotMove(const Othello& game, int level) {
  if (game.over) return -1;
  const int colour = game.turn;
  int best = -1, score = -10000;
  for (int n = 0; n < 64; ++n) {
    const int count = flips(game, n, colour);
    if (!count) continue;
    const int x = n % 8, y = n / 8;
    int value = count + ((x == 0 || x == 7) && (y == 0 || y == 7) ? 1000 : 0);
    if ((x == 1 || x == 6) && (y == 1 || y == 6)) value -= 80;
    if (x == 0 || x == 7 || y == 0 || y == 7) value += 20;
    if (level == 0) value = count;
    if (level >= 2) {
      Othello after = game;
      play(after, n);
      int reply = 0;
      if (!after.over && after.turn != colour) {
        for (int at = 0; at < 64; ++at) {
          const int take = flips(after, at, after.turn);
          if (take) {
            const bool corner = (at % 8 == 0 || at % 8 == 7) && (at / 8 == 0 || at / 8 == 7);
            reply = std::max(reply, take + (corner ? 1000 : 0));
          }
        }
      }
      value -= reply;
    }
    if (value > score) {
      score = value;
      best = n;
    }
  }
  return best;
}
bool bot(Othello& game, int level) { return play(game, chooseBotMove(game, level)); }
bool finished(const State& s) { return s.othello.over; }
void encode(const State& s, uint32_t generation, uint8_t* out) {
  std::memset(out, 0, kSaveBytes);
  std::memcpy(out, "PGM1", 4);
  put32(out + 4, generation);
  out[8] = static_cast<uint8_t>(s.kind);
  out[9] = s.level;
  put32(out + 10, s.seed);
  auto* p = out + 16;
  switch (s.kind) {
    case Kind::Othello:
      std::memcpy(p, s.othello.board, 64);
      p[64] = s.othello.turn;
      p[65] = s.othello.over;
      p[66] = s.othello.passed;
      p[67] = s.humanColour;
      break;
  }
  put32(out + kSaveBytes - 4, hash(out, kSaveBytes - 4));
}
bool decode(const uint8_t* in, Kind kind, State& s, uint32_t& generation) {
  if (kind != Kind::Othello || std::memcmp(in, "PGM1", 4) || in[8] != static_cast<uint8_t>(kind) || in[9] > 2 ||
      get32(in + kSaveBytes - 4) != hash(in, kSaveBytes - 4))
    return false;
  start(s, kind, get32(in + 10), in[9]);
  const auto* p = in + 16;
  switch (kind) {
    case Kind::Othello:
      if (p[64] < 1 || p[64] > 2 || p[65] > 1 || p[66] > 1) return false;
      for (int n = 0; n < 64; ++n)
        if (p[n] > 2) return false;
      std::memcpy(s.othello.board, p, 64);
      s.othello.turn = p[64];
      s.othello.over = p[65];
      s.othello.passed = p[66];
      if (p[67] > 2) return false;
      s.humanColour = p[67] ? p[67] : 1;
      if (disks(s.othello, 1) + disks(s.othello, 2) < 4 ||
          s.othello.over != (!hasMove(s.othello, 1) && !hasMove(s.othello, 2)) ||
          (!s.othello.over && !hasMove(s.othello, s.othello.turn)))
        return false;
      break;
  }
  generation = get32(in + 4);
  return true;
}
}  // namespace pocketgames
