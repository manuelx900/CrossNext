#pragma once

#include <I18n.h>

#include "CrossPointSettings.h"
#include "util/SideButtonActions.h"

namespace side_button {
inline constexpr StrId LABELS[] = {StrId::STR_SIDE_NAVIGATION,
                                   StrId::STR_SLEEP,
                                   StrId::STR_TOGGLE_FRONTLIGHT,
                                   StrId::STR_HOME_SHORTCUT,
                                   StrId::STR_READER_BOTTOM_SHEET,
                                   StrId::STR_BOOKMARK_OPTION,
                                   StrId::STR_DICTIONARY,
                                   StrId::STR_SAVE_CLIPPING,
                                   StrId::STR_KOSYNC,
                                   StrId::STR_FORCE_REFRESH,
                                   StrId::STR_TOGGLE_TOUCHSCREEN,
                                   StrId::STR_IGNORE,
                                   StrId::STR_SIDE_TOGGLE_WARMTH,
                                   StrId::STR_HOME_OPEN_SHORTCUT,
                                   StrId::STR_PAGE_TURN,
                                   StrId::STR_FOOTNOTES,
                                   StrId::STR_CONFIRM};
static_assert(sizeof(LABELS) / sizeof(LABELS[0]) == static_cast<unsigned>(SideButtonAction::Count));
inline char* shortcutTarget(SideButtonKey key) {
  return key == SideButtonKey::Left ? SETTINGS.sideLeftShortcutTitle : SETTINGS.sideRightShortcutTitle;
}
}  // namespace side_button
