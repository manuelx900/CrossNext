#pragma once
#include <cstdint>

namespace readertext {
enum class MenuTrigger : uint8_t {
  SwipeUp,
  CenterTap,
  HomeHold,
  LeftHold,
  RightHold,
  EitherHold,
  BottomTap,
  TopTap,
  Count
};
inline constexpr unsigned long MenuHoldMs = 700;
constexpr MenuTrigger menuTrigger(uint8_t value) {
  return value < uint8_t(MenuTrigger::Count) ? MenuTrigger(value) : MenuTrigger::SwipeUp;
}
constexpr bool holdsSide(MenuTrigger trigger, bool right) {
  return trigger == MenuTrigger::EitherHold ||
         (right ? trigger == MenuTrigger::RightHold : trigger == MenuTrigger::LeftHold);
}
constexpr bool centerMenuTap(MenuTrigger trigger, int x, int y, int width, int height) {
  return trigger == MenuTrigger::CenterTap && width > 0 && height > 0 && x >= width / 3 && x < width * 2 / 3 &&
         y >= height / 3 && y < height * 2 / 3;
}
inline constexpr int MenuEdgeTapPixels = 48;
constexpr bool menuTap(MenuTrigger trigger, int x, int y, int width, int height) {
  if (width <= 0 || height <= 0 || x < 0 || x >= width || y < 0 || y >= height) return false;
  switch (trigger) {
    case MenuTrigger::CenterTap:
      return centerMenuTap(trigger, x, y, width, height);
    case MenuTrigger::BottomTap:
      return y >= height - MenuEdgeTapPixels;
    case MenuTrigger::TopTap:
      return y < MenuEdgeTapPixels;
    case MenuTrigger::SwipeUp:
    case MenuTrigger::HomeHold:
    case MenuTrigger::LeftHold:
    case MenuTrigger::RightHold:
    case MenuTrigger::EitherHold:
    case MenuTrigger::Count:
      return false;
  }
  return false;
}
class MenuButtonInput {
 public:
  void cancel() {
    for (auto& state : phase) {
      if (state != Phase::Idle) state = Phase::Unowned;
    }
  }
  uint8_t update(bool active, MenuTrigger trigger, uint8_t held, uint32_t now) {
    uint8_t triggered = 0;
    for (unsigned side = 0; side < 2; ++side) {
      const uint8_t bit = uint8_t(1u << side);
      if (!(held & bit)) {
        phase[side] = Phase::Idle;
        continue;
      }
      const bool enabled = active && holdsSide(trigger, side != 0);
      if (phase[side] == Phase::Idle) {
        started[side] = now;
        phase[side] = enabled ? Phase::Waiting : Phase::Unowned;
      }
      if (!enabled) {
        phase[side] = Phase::Unowned;
        continue;
      }
      if (phase[side] == Phase::Waiting && uint32_t(now - started[side]) >= MenuHoldMs) {
        phase[side] = Phase::Fired;
        triggered |= bit;
      }
    }
    return triggered;
  }

 private:
  enum class Phase : uint8_t { Idle, Unowned, Waiting, Fired };
  Phase phase[2]{};
  uint32_t started[2]{};
};
}  // namespace readertext
