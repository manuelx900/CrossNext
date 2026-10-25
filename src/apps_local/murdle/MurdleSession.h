#pragma once
#include "MurdleCore.h"
namespace murdlesession {
inline constexpr int kLives = 3;
inline int lives(int wrong) { return kLives - (wrong < 0 ? 0 : wrong > kLives ? kLives : wrong); }
enum class Accusation : uint8_t { Incomplete, Closed, Wrong, Solved };
inline Accusation accuse(const murdle::Puzzle& p, const uint8_t* picks, int& wrong, bool solved) {
  if (solved || !lives(wrong)) return Accusation::Closed;
  for (int c = 0; c < p.shape.cats; ++c)
    if (picks[c] >= p.shape.items) return Accusation::Incomplete;
  for (int c = 0; c < p.shape.cats; ++c)
    if (picks[c] != p.assign[c][p.murderRow]) {
      ++wrong;
      return Accusation::Wrong;
    }
  return Accusation::Solved;
}
}  // namespace murdlesession
