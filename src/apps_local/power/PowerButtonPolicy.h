#pragma once
#include <cstdint>
namespace powermenu {
enum class ButtonAction : uint8_t { None, ShortPress, LongPress };
enum class PressAction : uint8_t { Sleep, Menu, ToggleLight, Warmth, Refresh, Home, Ignore, Count };
inline PressAction configuredAction(ButtonAction event, uint8_t shortAction, uint8_t longAction) {
  const unsigned value = event == ButtonAction::ShortPress ? shortAction : longAction;
  if (event == ButtonAction::None) return PressAction::Ignore;
  return value < uint8_t(PressAction::Count) ? PressAction(value)
         : event == ButtonAction::ShortPress ? PressAction::Sleep
                                             : PressAction::Menu;
}
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
      return sleep ? ButtonAction::ShortPress : ButtonAction::None;
    }
    if (state == State::Ready) state = State::Pressed;
    if (state == State::Pressed && heldMs >= 700) {
      state = State::MenuOpened;
      return ButtonAction::LongPress;
    }
    return ButtonAction::None;
  }
};
}  // namespace powermenu
