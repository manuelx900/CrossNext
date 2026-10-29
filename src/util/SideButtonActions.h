#pragma once

#include "HomeButtonInput.h"

enum class SideButtonAction : uint8_t {
  Navigation,
  Sleep,
  ToggleFrontlight,
  Home,
  ReaderMenu,
  Bookmark,
  Dictionary,
  SaveClipping,
  Sync,
  Refresh,
  ToggleTouchscreen,
  Ignore,
  ToggleWarmth,
  OpenShortcut,
  NextPage,
  Footnotes,
  Confirm,
  Count
};

enum class SideButtonKey : uint8_t { Left, Right };
struct SideButtonEvent {
  SideButtonAction action = SideButtonAction::Navigation;
  SideButtonKey key = SideButtonKey::Left;
};

constexpr bool sideNavigates(uint8_t action) { return action == static_cast<uint8_t>(SideButtonAction::Navigation); }

constexpr SideButtonEvent sideReleasedEvent(uint8_t left, uint8_t right, bool leftReleased, bool rightReleased) {
  if (leftReleased && !sideNavigates(left)) return {static_cast<SideButtonAction>(left), SideButtonKey::Left};
  return rightReleased ? SideButtonEvent{static_cast<SideButtonAction>(right), SideButtonKey::Right}
                       : SideButtonEvent{};
}
constexpr SideButtonAction sideReleasedAction(uint8_t left, uint8_t right, bool leftReleased, bool rightReleased) {
  return sideReleasedEvent(left, right, leftReleased, rightReleased).action;
}
constexpr bool sideStandaloneAction(SideButtonAction action) {
  return action == SideButtonAction::Sleep || action == SideButtonAction::ToggleWarmth ||
         action == SideButtonAction::OpenShortcut;
}
constexpr uint8_t sideToggledWarmth(bool lightOn, uint8_t current, uint8_t remembered) {
  return lightOn && current ? 0 : remembered < 1 ? 1 : remembered > 100 ? 100 : remembered;
}

constexpr HomeButtonAction sideHomeAction(SideButtonAction action) {
  switch (action) {
    case SideButtonAction::Home:
      return HomeButtonAction::Home;
    case SideButtonAction::ReaderMenu:
      return HomeButtonAction::ReaderMenu;
    case SideButtonAction::Bookmark:
      return HomeButtonAction::Bookmark;
    case SideButtonAction::Dictionary:
      return HomeButtonAction::Dictionary;
    case SideButtonAction::SaveClipping:
      return HomeButtonAction::CreateClipping;
    case SideButtonAction::Sync:
      return HomeButtonAction::Sync;
    case SideButtonAction::Refresh:
      return HomeButtonAction::Refresh;
    case SideButtonAction::ToggleTouchscreen:
      return HomeButtonAction::ToggleTouchscreen;
    case SideButtonAction::ToggleFrontlight:
      return HomeButtonAction::ToggleFrontlight;
    case SideButtonAction::NextPage:
      return HomeButtonAction::NextPage;
    case SideButtonAction::Footnotes:
      return HomeButtonAction::Footnotes;
    case SideButtonAction::Confirm:
      return HomeButtonAction::Confirm;
    case SideButtonAction::Navigation:
    case SideButtonAction::Sleep:
    case SideButtonAction::Ignore:
    case SideButtonAction::ToggleWarmth:
    case SideButtonAction::OpenShortcut:
    case SideButtonAction::Count:
      return HomeButtonAction::Ignore;
  }
  return HomeButtonAction::Ignore;
}

constexpr bool selectableSideAction(SideButtonAction action) {
  return action < SideButtonAction::Count && action != SideButtonAction::Dictionary &&
         action != SideButtonAction::SaveClipping && action != SideButtonAction::Footnotes;
}
