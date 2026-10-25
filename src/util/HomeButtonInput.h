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

class HomeButtonInput {
 public:
  static constexpr uint32_t DOUBLE_TAP_MS = 350;

  uint32_t nextActionDelayMs(uint32_t now, uint32_t maximum) const {
    if (!pending || secondContact) return maximum;
    const uint32_t elapsed = now - tappedAt;
    // update() emits the single tap strictly after the double-tap window.
    const uint32_t remaining = elapsed > DOUBLE_TAP_MS ? 0 : DOUBLE_TAP_MS + 1 - elapsed;
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
    if (pending && !secondContact && now - tappedAt > DOUBLE_TAP_MS) {
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
  uint32_t tappedAt = 0;
  bool pending = false;
  bool secondContact = false;
};
