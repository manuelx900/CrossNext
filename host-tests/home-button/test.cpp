#include <FreeInkUICore.h>

#include <cstdio>

#include "util/HomeButtonInput.h"

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL: %s\n", message);
  }
}
}  // namespace

int main() {
  using Action = HomeButtonAction;
  static_assert(static_cast<int>(Action::ReaderMenu) == 9);
  static_assert(static_cast<int>(Action::CreateClipping) == 11);
  static_assert(static_cast<int>(Action::ToggleTouchscreen) == 12);
  for (unsigned a = 0; a < static_cast<unsigned>(Action::Count); ++a) {
    const auto configured = static_cast<Action>(a);
    for (bool pendingTap : {false, true}) {
      HomeButtonInput input;
      if (pendingTap) {
        check(input.update(1, true, false, false, false, Action::Home, Action::Confirm, configured) == Action::Ignore,
              "First tap waits for double tap");
      }
      check(input.update(20, false, true, false, true, Action::Home, Action::Confirm, configured, true) ==
                Action::ToggleTouchscreen,
            "Locked hold unlocks for every configured action");
      check(input.update(21, false, false, false, true, Action::Home, Action::Confirm, configured) == Action::Ignore,
            "Holding does not repeat the long action");
      check(input.update(400, false, false, false, false, Action::Home, Action::Confirm, configured) == Action::Ignore,
            "Long press clears pending single tap");
      check(input.update(800, false, true, false, true, Action::Home, Action::Confirm, configured) == configured,
            "Unlocked hold restores saved preference");
      check(input.update(1600, true, false, false, false, Action::Home, Action::Confirm, configured) == Action::Ignore,
            "Fresh first tap waits after hold");
      check(input.update(1700, true, false, false, false, Action::Home, Action::Confirm, configured) == Action::Confirm,
            "Double tap preserved after hold");
    }
  }
  HomeButtonInput input;
  check(input.update(1, true, false, false, false, Action::Home, Action::Ignore, Action::ReaderMenu) == Action::Home,
        "Single tap stays immediate without double action");
  check(input.update(2, false, true, true, true, Action::Home, Action::Ignore, Action::ReaderMenu) == Action::Ignore,
        "Unlocked swipe keeps priority");
  check(input.update(0xfffffff0u, true, false, false, false, Action::Home, Action::Confirm, Action::ReaderMenu) ==
            Action::Ignore,
        "Pending single near timer wrap");
  check(
      input.update(400, false, false, false, false, Action::Home, Action::Confirm, Action::ReaderMenu) == Action::Home,
      "Single timeout survives timer wrap");
  freeink::ui::InteractionBuffer<2> buffer;
  buffer.addInteraction({{0, 0, 100, 40}, 1, 0, freeink::ui::InputDefault | freeink::ui::InputDrag});
  freeink::ui::InputSnapshot contact{};
  contact.touchHeld = true;
  contact.touchX = 20;
  contact.touchY = 20;
  check(static_cast<bool>(buffer.route(contact)), "Slider contact starts drag");
  check(buffer.activeIndex() == 0, "Drag owns slider");
  // On the device the suppressed lift has no release edge. Cancel explicitly.
  buffer.cancelTouchContact();
  check(buffer.activeIndex() == -1, "Lock cancels active drag without a release");
  contact.touchX = 200;
  contact.touchY = 100;
  check(!buffer.route(contact), "Fresh contact outside slider cannot continue cancelled drag");
  contact.touchHeld = false;
  contact.touchReleased = true;
  check(!buffer.route(contact), "Lift outside slider cannot commit cancelled drag");
  contact.touchReleased = false;
  contact.touchHeld = true;
  contact.touchX = 80;
  contact.touchY = 20;
  check(static_cast<bool>(buffer.route(contact)), "Fresh slider gesture still works after unlock");
  std::printf("Home button: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
