#pragma once

#include <I18n.h>

#include "../../util/MenuFontStyle.h"
#include "Dashboard.h"

namespace menufontui {
inline const char* sectionName(menufont::Section section) {
  static constexpr StrId labels[] = {
      StrId::STR_MENU_FONT_GENERAL, StrId::STR_MENU_FONT_HOME,      StrId::STR_SHORT_FILES,       StrId::STR_LIBRARY,
      StrId::STR_FILE_TRANSFER,     StrId::STR_READING_STATS_TITLE, StrId::STR_SHORT_GAMES,       StrId::STR_SHORT_APPS,
      StrId::STR_SETTINGS_TITLE,    StrId::STR_MENU_FONT_READER,    StrId::STR_MENU_FONT_CONTROLS};
  static_assert(sizeof(labels) / sizeof(labels[0]) == menufont::kSectionCount);
  return I18N.get(labels[int(section)]);
}
inline constexpr StrId choiceIds[] = {StrId::STR_MENU_FONT_INHERIT, StrId::STR_MENU_FONT_PIXEL,
                                      StrId::STR_MENU_FONT_CLASSIC, StrId::STR_MENU_FONT_NOTO_SANS,
                                      StrId::STR_MENU_FONT_NOTO_SERIF};
static_assert(sizeof(choiceIds) / sizeof(choiceIds[0]) == int(menufont::Override::Count));
inline const char* choiceName(uint8_t value) { return I18N.get(choiceIds[int(menufont::normalizedOverride(value))]); }
inline void styleSections(freeink::ui::ListProps& props, bool wide) {
  pixelui::styleList(props);
  props.rowHeight = wide ? 52 : 60;
  props.rowPaddingY = 6;
  props.labelText.font = 0;
  props.labelText.bold = false;
  props.valueText.font = 3;
  props.valueText.bold = false;
  props.rowStyles.normal.borderWidth = 1;
}
}  // namespace menufontui
