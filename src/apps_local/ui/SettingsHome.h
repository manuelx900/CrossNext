#pragma once
#include <FreeInkUIIcon.h>
#include <I18n.h>

#include "../../components/icons/listIcons.h"
#include "SettingsArt.h"
#include "SettingsMenuStyle.h"
#include "ToyboxIcons.h"
namespace settingsui {
inline constexpr int homeCount = 8;
inline constexpr StrId homeNames[] = {StrId::STR_CAT_DISPLAY,           StrId::STR_SETTINGS_READING,
                                      StrId::STR_CAT_CONTROLS,          StrId::STR_SETTINGS_LIBRARY,
                                      StrId::STR_SETTINGS_CONNECTIVITY, StrId::STR_CAT_SYSTEM,
                                      StrId::STR_SETTINGS_DEVICE,       StrId::STR_SETTINGS_APPEARANCE};
inline void homeRow(int i, freeink::ui::ListItem& item) {
  static constexpr StrId descriptions[] = {StrId::STR_SETTINGS_DISPLAY_HELP,  StrId::STR_SETTINGS_READER_HELP,
                                           StrId::STR_SETTINGS_CONTROLS_HELP, StrId::STR_SETTINGS_LIBRARY_HELP,
                                           StrId::STR_SETTINGS_NETWORK_HELP,  StrId::STR_SETTINGS_SYSTEM_HELP,
                                           StrId::STR_SETTINGS_DEVICE_HELP,   StrId::STR_APPEARANCE_HELP};
  static constexpr freeink::ui::BitmapRef icons[] = {settingsart::Display, settingsart::Book, settingsart::Hand,
                                                     settingsart::Folder,  settingsart::Wifi, settingsart::Gear,
                                                     settingsart::Info,    settingsart::Grid};
  item = {};
  item.label = I18N.get(homeNames[i]);
  item.subtitle = I18N.get(descriptions[i]);
  item.icon = icons[i];
  item.value = ">";
  item.actionValue = i;
}
inline void styleHome(freeink::ui::ListProps& props) {
  styleRows(props);
  props.rowHeight = 84;
  props.rowGap = 0;
  props.iconSize = 48;
  props.sidePadding = 20;
  props.rowPaddingY = 6;
  props.textGap = 22;
  props.labelText.font = 1;
  props.labelText.bold = false;
  props.labelText.maxLines = 1;
  props.subtitleText.font = 4;
  props.subtitleText.maxLines = 2;
  props.rowStyles.normal.borderWidth = 1;
}
inline void styleAppearance(freeink::ui::ListProps& p) {
  styleRows(p);
  p.rowHeight = 140;
  p.rowGap = 12;
  p.iconSize = 108;
  p.labelText.font = 1;
  p.labelText.bold = false;
  p.labelText.maxLines = 2;
}
}  // namespace settingsui
