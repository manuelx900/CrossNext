#pragma once
#include <cstring>

#include "PicrossCore.h"
#include "PicrossLegacyMap.h"
namespace picrosssave {
constexpr uint8_t kVersion = 7;
constexpr size_t kBytes = 168, kLegacyBytes = 132;
inline uint32_t get(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void put(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = value >> (8 * n);
}
inline uint32_t checksum(const uint8_t* p) {
  uint32_t h = 2166136261u;
  for (size_t n = 0; n < kBytes - 4; ++n) h = (h ^ p[n]) * 16777619u;
  return h;
}
inline void encode(const picross::Board& board, const picross::Progress& progress, uint8_t* p) {
  static_assert(picross::kPuzzleCount == 450 && picross::kMaxSize == 10,
                "change save version when bank identities change");
  std::memset(p, 0, kBytes);
  p[0] = kVersion;
  p[1] = board.index();
  p[2] = board.index() >> 8;
  p[3] = board.mistakes() > 255 ? 255 : board.mistakes();
  std::memcpy(p + 4, board.cells(), 100);
  for (int n = 0; n < picross::kProgressWords; ++n) put(p + 104 + n * 4, progress.solved[n]);
  put(p + 164, checksum(p));
}
inline bool decode(const uint8_t* p, size_t size, picross::Board& board, picross::Progress& progress) {
  if (!p || size < 4) return false;
  const int index = uint16_t(p[1]) | uint16_t(p[2]) << 8;
  if (size == kLegacyBytes && p[0] == 6) {
    if (index >= 199 || (get(p + 128) & ~uint32_t(127))) return false;
    progress = {};
    for (int n = 0; n < 199; ++n)
      if (get(p + 104 + n / 32 * 4) & (uint32_t(1) << (n % 32))) progress.markSolved(picross::kLegacyV6[n]);
    board.restore(picross::kLegacyV6[index], p + 4, p[3]);
    return true;
  }
  if (size != kBytes || p[0] != kVersion || index >= 450 || get(p + 164) != checksum(p) ||
      (get(p + 160) & ~uint32_t(3)))
    return false;
  for (int n = 0; n < picross::kProgressWords; ++n) progress.solved[n] = get(p + 104 + n * 4);
  board.restore(index, p + 4, p[3]);
  return true;
}
}  // namespace picrosssave
