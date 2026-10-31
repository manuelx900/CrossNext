#pragma once
#include "Game2048Core.h"

namespace game2048 {
enum class Result { None, Won, Lost };
inline Result afterMove(const State& state, bool& winShown) {
  if (!winShown && highest(state) >= 11) {
    winShown = true;
    return Result::Won;
  }
  return canMove(state) ? Result::None : Result::Lost;
}
}  // namespace game2048
