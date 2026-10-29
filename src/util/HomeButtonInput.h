#pragma once

#include <cstdint>

// Persisted indices: append actions without reordering existing values.
enum class HomeButtonAction : uint8_t {
  Home,
  Ignore,
  NextPage,
  Refresh,
  Footnotes,
  Confirm,
  Sync,
  Bookmark,
  Dictionary,
  ReaderMenu,
  ToggleFrontlight,
  CreateClipping,
  ToggleTouchscreen,
  OpenShortcut,
  OpenDoubleShortcut,
  OpenLongShortcut,
  Count
};

constexpr bool selectableHomeAction(HomeButtonAction action) {
  return action != HomeButtonAction::Dictionary && action != HomeButtonAction::CreateClipping &&
         action != HomeButtonAction::Footnotes && uint8_t(action) <= uint8_t(HomeButtonAction::OpenShortcut);
}
constexpr bool readerHomeAction(HomeButtonAction action) {
  return action == HomeButtonAction::NextPage || action == HomeButtonAction::Bookmark ||
         action == HomeButtonAction::ReaderMenu || action == HomeButtonAction::Sync;
}

class HomeButtonInput {
 public:
  static constexpr uint32_t DOUBLE_TAP_MS = 350;
  static constexpr uint16_t TAP_WINDOWS_MS[] = {350, 450, 550, 650, 750};
  static constexpr unsigned TAP_WINDOW_COUNT = sizeof(TAP_WINDOWS_MS) / sizeof(TAP_WINDOWS_MS[0]);
  static constexpr uint32_t windowFor(unsigned index) { return TAP_WINDOWS_MS[index < TAP_WINDOW_COUNT ? index : 0]; }
  void setWindow(unsigned index) { doubleTapMs = windowFor(index); }

  uint32_t nextActionDelayMs(uint32_t now, uint32_t maximum) const {
    if (!pending || secondContact) return maximum;
    const uint32_t elapsed = now - tappedAt;
    // update() emits the single tap strictly after the double-tap window.
    const uint32_t remaining = elapsed > doubleTapMs ? 0 : doubleTapMs + 1 - elapsed;
    return remaining < maximum ? remaining : maximum;
  }

  HomeButtonAction update(uint32_t now, bool tapped, bool held, bool swiped, bool pressed, HomeButtonAction tap,
                          HomeButtonAction doubleTap, HomeButtonAction longPress, bool touchLocked = false) {
    if (swiped) {
      reset();
      return HomeButtonAction::Ignore;
    }
    if (held) {
      reset();
      // A locked touchscreen must always have a physical way out.
      return touchLocked ? HomeButtonAction::ToggleTouchscreen : longPress;
    }
    if (pending && !secondContact && now - tappedAt > doubleTapMs) {
      pending = tapped;
      tappedAt = now;
      return tap;
    }
    if (pending && pressed) secondContact = true;
    if (!tapped) return HomeButtonAction::Ignore;
    if (doubleTap == HomeButtonAction::Ignore) {
      reset();
      return tap;
    }
    if (pending) {
      reset();
      return doubleTap;
    }
    pending = true;
    tappedAt = now;
    return HomeButtonAction::Ignore;
  }

  void reset() {
    pending = false;
    secondContact = false;
  }

 private:
  uint32_t doubleTapMs = DOUBLE_TAP_MS;
  uint32_t tappedAt = 0;
  bool pending = false;
  bool secondContact = false;
};
