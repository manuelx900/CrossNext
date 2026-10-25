#pragma once
#include <cstdint>
namespace powermenu {
enum class ButtonAction : uint8_t { None, Sleep, Menu };
class ButtonPolicy {
  enum class State : uint8_t { WaitingForRelease, Ready, Pressed, MenuOpened };
  State state = State::WaitingForRelease;

 public:
  ButtonAction update(bool held, bool released, uint32_t heldMs, bool allowed) {
    if (!allowed) {
      state = State::WaitingForRelease;
      return ButtonAction::None;
    }
    if (!held) {
      const bool sleep = released && state == State::Pressed;
      state = State::Ready;
      return sleep ? ButtonAction::Sleep : ButtonAction::None;
    }
    if (state == State::Ready) state = State::Pressed;
    if (state == State::Pressed && heldMs >= 700) {
      state = State::MenuOpened;
      return ButtonAction::Menu;
    }
    return ButtonAction::None;
  }
};
}  // namespace powermenu
