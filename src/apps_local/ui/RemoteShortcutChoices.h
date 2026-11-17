#pragma once

#include <I18n.h>

#include "../../util/SideButtonActions.h"

namespace remoteshortcuts {
// Shortcut records are host-owned bytes. Preserve the existing side-action IDs.
enum class Extra : uint8_t { Previous = 128, Back, Library, Files, Settings, BrightnessUp, BrightnessDown };
struct Choice {
  uint8_t action;
  StrId label, help, section;
};
inline constexpr Choice CHOICES[] = {
    {uint8_t(SideButtonAction::Navigation), StrId::STR_REMOTE_ADVANCE, StrId::STR_REMOTE_ADVANCE_HELP,
     StrId::STR_REMOTE_NAVIGATION},
    {uint8_t(Extra::Previous), StrId::STR_REMOTE_PREVIOUS, StrId::STR_REMOTE_PREVIOUS_HELP,
     StrId::STR_REMOTE_NAVIGATION},
    {uint8_t(SideButtonAction::Confirm), StrId::STR_REMOTE_CONFIRM, StrId::STR_REMOTE_CONFIRM_HELP,
     StrId::STR_REMOTE_NAVIGATION},
    {uint8_t(Extra::Back), StrId::STR_REMOTE_BACK, StrId::STR_REMOTE_BACK_HELP, StrId::STR_REMOTE_NAVIGATION},
    {uint8_t(SideButtonAction::NextPage), StrId::STR_REMOTE_READER_PAGE, StrId::STR_BUTTON_READER_ONLY,
     StrId::STR_REMOTE_READING},
    {uint8_t(SideButtonAction::Bookmark), StrId::STR_BOOKMARK_OPTION, StrId::STR_REMOTE_BOOKMARK_HELP,
     StrId::STR_REMOTE_READING},
    {uint8_t(SideButtonAction::ReaderMenu), StrId::STR_READER_MENU, StrId::STR_REMOTE_MENU_HELP,
     StrId::STR_REMOTE_READING},
    {uint8_t(SideButtonAction::Sync), StrId::STR_KOSYNC, StrId::STR_REMOTE_SYNC_HELP, StrId::STR_REMOTE_READING},
    {uint8_t(SideButtonAction::ToggleFrontlight), StrId::STR_TOGGLE_FRONTLIGHT, StrId::STR_REMOTE_LIGHT_HELP,
     StrId::STR_REMOTE_DISPLAY},
    {uint8_t(Extra::BrightnessUp), StrId::STR_REMOTE_BRIGHTNESS_UP, StrId::STR_REMOTE_BRIGHTNESS_HELP,
     StrId::STR_REMOTE_DISPLAY},
    {uint8_t(Extra::BrightnessDown), StrId::STR_REMOTE_BRIGHTNESS_DOWN, StrId::STR_REMOTE_BRIGHTNESS_HELP,
     StrId::STR_REMOTE_DISPLAY},
    {uint8_t(SideButtonAction::ToggleWarmth), StrId::STR_SIDE_TOGGLE_WARMTH, StrId::STR_REMOTE_WARMTH_HELP,
     StrId::STR_REMOTE_DISPLAY},
    {uint8_t(SideButtonAction::Refresh), StrId::STR_FORCE_REFRESH, StrId::STR_REMOTE_REFRESH_HELP,
     StrId::STR_REMOTE_DISPLAY},
    {uint8_t(SideButtonAction::Home), StrId::STR_REMOTE_HOME, StrId::STR_REMOTE_HOME_HELP, StrId::STR_REMOTE_OPEN},
    {uint8_t(Extra::Library), StrId::STR_LIBRARY, StrId::STR_REMOTE_LIBRARY_HELP, StrId::STR_REMOTE_OPEN},
    {uint8_t(Extra::Files), StrId::STR_BROWSE_FILES, StrId::STR_REMOTE_FILES_HELP, StrId::STR_REMOTE_OPEN},
    {uint8_t(Extra::Settings), StrId::STR_SETTINGS_TITLE, StrId::STR_REMOTE_SETTINGS_HELP, StrId::STR_REMOTE_OPEN},
    {uint8_t(SideButtonAction::OpenShortcut), StrId::STR_HOME_OPEN_SHORTCUT, StrId::STR_REMOTE_APP_HELP,
     StrId::STR_REMOTE_OPEN},
    {uint8_t(SideButtonAction::Sleep), StrId::STR_SLEEP, StrId::STR_REMOTE_SLEEP_HELP, StrId::STR_REMOTE_DEVICE},
    {uint8_t(SideButtonAction::ToggleTouchscreen), StrId::STR_TOGGLE_TOUCHSCREEN, StrId::STR_REMOTE_TOUCH_HELP,
     StrId::STR_REMOTE_DEVICE},
    {uint8_t(SideButtonAction::Ignore), StrId::STR_REMOTE_NOTHING, StrId::STR_REMOTE_NOTHING_HELP,
     StrId::STR_REMOTE_DEVICE},
};
inline constexpr int COUNT = sizeof(CHOICES) / sizeof(CHOICES[0]);
inline const Choice* choice(uint8_t action) {
  for (const auto& entry : CHOICES)
    if (entry.action == action) return &entry;
  return nullptr;
}
}  // namespace remoteshortcuts
