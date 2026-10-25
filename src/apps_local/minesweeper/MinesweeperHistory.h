#pragma once
#include <cstddef>
#include <cstring>

#include "MinesweeperCore.h"
namespace minehistory {
constexpr size_t kBytes = 420;
inline uint32_t read(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void write(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = value >> (8 * n);
}
inline uint32_t hash(const uint8_t* p) {
  uint32_t h = 2166136261u;
  for (size_t n = 0; n < 416; ++n) h = (h ^ p[n]) * 16777619u;
  return h;
}
inline void encode(const minesweeper::Game& g, uint32_t wins, uint32_t losses, uint8_t* p) {
  std::memcpy(p, "MSH2", 4);
  write(p + 4, wins);
  write(p + 8, losses);
  p[12] = g.columns;
  p[13] = g.rows;
  p[14] = g.mines;
  p[15] = uint8_t(g.status);
  std::memcpy(p + 16, g.cell, 400);
  write(p + 416, hash(p));
}
inline bool decode(const uint8_t* p, minesweeper::Game& g, int& wins, int& losses) {
  if (std::memcmp(p, "MSH2", 4) || read(p + 416) != hash(p) || p[12] < 8 || p[12] > 20 || p[13] < 8 || p[13] > 20 ||
      p[14] > 100 || p[15] < 2 || p[15] > 3 || read(p + 4) > 1000000 || read(p + 8) > 1000000)
    return false;
  int mines = 0;
  for (int c = 0; c < 20; ++c)
    for (int r = 0; r < 20; ++r) {
      const auto value = p[16 + c * 20 + r];
      if (value & ~7) return false;
      if (c < p[12] && r < p[13] && (value & minesweeper::kMine)) ++mines;
    }
  if (mines != p[14]) return false;
  g = {};
  g.columns = p[12];
  g.rows = p[13];
  g.mines = p[14];
  g.status = minesweeper::Status(p[15]);
  std::memcpy(g.cell, p + 16, 400);
  wins = read(p + 4);
  losses = read(p + 8);
  return true;
}
}  // namespace minehistory
