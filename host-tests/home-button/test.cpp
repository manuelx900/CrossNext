#include <FreeInkUICore.h>

#include <cstdio>

#include "util/HomeButtonInput.h"
#include "util/SideButtonActions.h"

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
  static_assert(!selectableHomeAction(HomeButtonAction::Dictionary));
  static_assert(!selectableHomeAction(HomeButtonAction::CreateClipping));
  static_assert(!selectableHomeAction(HomeButtonAction::Footnotes));
  static_assert(!selectableSideAction(SideButtonAction::Dictionary));
  static_assert(!selectableSideAction(SideButtonAction::SaveClipping));
  static_assert(!selectableSideAction(SideButtonAction::Footnotes));
  static_assert(selectableHomeAction(HomeButtonAction::Home));
  static_assert(selectableSideAction(SideButtonAction::ToggleWarmth));

  using Side = SideButtonAction;
  for (uint8_t left = 0; left < uint8_t(Side::Count); ++left) {
    for (uint8_t right = 0; right < uint8_t(Side::Count); ++right) {
      check(sideNavigates(left) == (left == 0), "Only Navigation exposes normal left-key events");
      check(sideNavigates(right) == (right == 0), "Only Navigation exposes normal right-key events");
      check(sideReleasedAction(left, right, false, false) == Side::Navigation, "No release emits no custom action");
      check(sideReleasedAction(left, right, true, false) == Side(left), "Left release uses the left binding");
      check(sideReleasedAction(left, right, false, true) == Side(right), "Right release uses the right binding");
      check(sideReleasedAction(left, right, true, true) == Side(left ? left : right),
            "Simultaneous releases emit one action");
    }
  }
  check(sideReleasedEvent(uint8_t(Side::OpenShortcut), uint8_t(Side::OpenShortcut), true, false).key ==
            SideButtonKey::Left,
        "Left shortcut keeps its own target");
  check(sideReleasedEvent(uint8_t(Side::OpenShortcut), uint8_t(Side::OpenShortcut), false, true).key ==
            SideButtonKey::Right,
        "Right shortcut keeps its own target");
  check(sideStandaloneAction(Side::ToggleWarmth) && sideStandaloneAction(Side::OpenShortcut),
        "Warmth and app shortcuts survive synchronous input deferral");
  for (uint8_t remembered : {uint8_t(0), uint8_t(25), uint8_t(50), uint8_t(100), uint8_t(255)}) {
    const int restored = remembered < 1 ? 1 : remembered > 100 ? 100 : remembered;
    check(sideToggledWarmth(false, 0, remembered) == restored, "Warmth turns on at remembered level");
    check(sideToggledWarmth(true, 0, remembered) == restored, "Cool light restores remembered warmth");
    check(sideToggledWarmth(true, 50, remembered) == 0, "Warm light switches back to cool");
  }
  check(sideHomeAction(Side::ToggleFrontlight) == HomeButtonAction::ToggleFrontlight,
        "Side light reaches global light action");
  check(sideHomeAction(Side::ReaderMenu) == HomeButtonAction::ReaderMenu, "Side menu reaches the reader sheet");
  check(sideHomeAction(Side::Bookmark) == HomeButtonAction::Bookmark, "Side bookmark reaches reader bookmark action");
  check(sideHomeAction(Side::Sleep) == HomeButtonAction::Ignore, "Sleep does not also dispatch a Home action");
  check(sideHomeAction(Side::Navigation) == HomeButtonAction::Ignore, "Default keys retain ordinary navigation only");
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
  static_assert(HomeButtonInput::DOUBLE_TAP_MS == 350);
  check(HomeButtonInput::windowFor(255) == 350, "Invalid timing uses the existing default");
  for (unsigned setting = 0; setting < HomeButtonInput::TAP_WINDOW_COUNT; ++setting) {
    const auto window = HomeButtonInput::windowFor(setting);
    for (uint32_t start : {100u, 0xfffffff0u}) {
      HomeButtonInput timed;
      timed.setWindow(setting);
      check(timed.update(start, true, false, false, false, Action::Home, Action::ToggleFrontlight, Action::Ignore) ==
                Action::Ignore,
            "Configured first tap waits");
      check(timed.nextActionDelayMs(start, 1000) == window + 1, "Idle deadline uses the selected interval");
      check(timed.update(start + window, false, false, false, false, Action::Home, Action::ToggleFrontlight,
                         Action::Ignore) == Action::Ignore,
            "Single tap does not fire at the boundary");
      check(timed.update(start + window + 1, false, false, false, false, Action::Home, Action::ToggleFrontlight,
                         Action::Ignore) == Action::Home,
            "Single tap fires once after the selected interval");
      check(timed.nextActionDelayMs(start + window + 2, 1000) == 1000,
            "Completed gesture releases the polling deadline");
      timed.reset();
      timed.update(start, true, false, false, false, Action::Home, Action::ToggleFrontlight, Action::Ignore);
      timed.update(start + window - 1, false, false, false, true, Action::Home, Action::ToggleFrontlight,
                   Action::Ignore);
      check(timed.update(start + window + 80, true, false, false, false, Action::Home, Action::ToggleFrontlight,
                         Action::Ignore) == Action::ToggleFrontlight,
            "Second contact inside the interval survives a later release");
      check(timed.update(start + window + 500, false, false, false, false, Action::Home, Action::ToggleFrontlight,
                         Action::Ignore) == Action::Ignore,
            "Double tap does not also navigate Home");
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
