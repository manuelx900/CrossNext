#pragma once
#include <FreeInkUIIcon.h>
#include <I18n.h>

#include "../../../lib/GfxRenderer/ChromeRegion.h"
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
  static constexpr freeink::ui::BitmapRef icons[] = {settingsart::Display, settingsart::Book, settingsart::Hand,
                                                     settingsart::Folder,  settingsart::Wifi, settingsart::Gear,
                                                     settingsart::Info,    settingsart::Grid};
  item = {};
  item.label = I18N.get(homeNames[i]);
  item.icon = icons[i];
  item.value = ">";
  {
    static constexpr StrId help[] = {StrId::STR_SETTINGS_DISPLAY_HELP,  StrId::STR_SETTINGS_READER_HELP,
                                     StrId::STR_SETTINGS_CONTROLS_HELP, StrId::STR_SETTINGS_LIBRARY_HELP,
                                     StrId::STR_SETTINGS_NETWORK_HELP,  StrId::STR_SETTINGS_SYSTEM_HELP,
                                     StrId::STR_SETTINGS_DEVICE_HELP,   StrId::STR_HOME_THEME_LABEL};
    item.subtitle = I18N.get(help[i]);
    if (chrome::lightHeader) item.value = nullptr;
  }
  item.actionValue = i;
}
inline void styleHome(freeink::ui::ListProps& props) {
  styleRows(props);
  props.rowHeight = 78;
  props.rowGap = 0;
  props.iconSize = 32;
  props.sidePadding = 18;
  props.rowPaddingY = 8;
  props.textGap = 18;
  props.labelText.font = 0;
  props.labelText.bold = false;
  props.labelText.maxLines = 1;
  props.subtitleText.font = 3;
  props.subtitleText.maxLines = 1;
  props.rowStyles.normal.borderWidth = 1;
  if (chrome::lightHeader) {
    props.rowHeight = 78;
    props.iconSize = 32;
    props.labelText.font = 0;
    props.subtitleText.font = 3;
    props.rowPaddingY = 4;
    props.rowStyles.normal.borderWidth = 0;
  }
}
inline void styleAppearance(freeink::ui::ListProps& p) {
  styleRows(p);
  p.rowHeight = 140;
  p.rowGap = 0;
  p.rowStyles.normal.borderWidth = 0;
  p.rowStyles.selected.borderWidth = 0;
  p.rowStyles.focused.borderWidth = 0;
  p.rowStyles.active.borderWidth = 0;
  p.separatorPaint = chrome::lightHeader ? lightui::rule : freeink::ui::Paint::solid(freeink::ui::Color::Transparent);
  p.iconSize = 108;
  p.labelText.font = 1;
  p.labelText.bold = false;
  p.labelText.maxLines = 2;
}
inline void styleThemeChoices(freeink::ui::ListProps& p, bool wide) {
  p.rowHeight = wide ? 50 : 108;
  p.iconSize = wide ? 30 : 72;
  p.labelText.font = wide ? 0 : 1;
  p.labelText.maxLines = 1;
  p.subtitleText.font = 4;
  p.subtitleText.maxLines = 1;
  p.rowPaddingY = wide ? 4 : 8;
}
struct AppearanceActions {
  freeink::ui::Rect themes, recent, font;
};
inline AppearanceActions appearanceActions(freeink::ui::Rect body, int themeBottom, bool wide) {
  const int height = wide ? 44 : 56, gap = 12;
  const int left = wide ? body.x + (body.width - 16) * 3 / 5 + 16 : body.x;
  const int width = body.right() - left;
  const int top = wide ? body.y + std::max(0, (themeBottom - body.y - 3 * height - 2 * gap) / 2)
                       : std::max(themeBottom + gap, body.bottom() - 3 * height - 2 * gap);
  return {freeink::ui::makeRect(left, top, width, height),
          freeink::ui::makeRect(left, top + height + gap, width, height),
          freeink::ui::makeRect(left, top + 2 * (height + gap), width, height)};
}
template <typename Screen>
void appearanceList(Screen& screen, freeink::ui::ListProps& props) {
  pixelui::list(screen, props);
  if (props.count < 2 || !props.items) return;
  const auto band = pixelui::rowArea(screen.body(), props);
  const int height =
      freeink::ui::measureListRow(screen.target(), screen.frame().assets(), band.width, props, props.items[0]).height;
  if (!chrome::lightHeader) {
    int offset = height;
    for (int n = props.topIndex + 1; n < props.count && offset < band.height; ++n) {
      screen.target().fill(
          freeink::ui::makeRect(band.x, band.y + offset - pixelui::kBorder / 2, band.width, pixelui::kBorder),
          freeink::ui::Paint::solid(freeink::ui::Color::Black));
      offset += props.rowGap +
                freeink::ui::measureListRow(screen.target(), screen.frame().assets(), band.width, props, props.items[n])
                    .height;
    }
  }
  const auto& carousel = props.items[1];
  if (props.topIndex != 0 || carousel.icon.data != settingsart::CarouselPreview.data || props.iconSize <= 0) return;
  const int rowHeight =
      freeink::ui::measureListRow(screen.target(), screen.frame().assets(), band.width, props, carousel).height;
  const auto slot = freeink::ui::makeRect(
      band.x + (props.rtl ? band.width - props.sidePadding - props.iconSize : props.sidePadding),
      band.y + height + props.rowGap + (rowHeight - props.iconSize) / 2, props.iconSize, props.iconSize);
  auto state = carousel.state;
  if (props.selectedIndex == 1) state |= freeink::ui::StateSelected;
  state = screen.frame().stateFor(props.action, carousel.actionValue, state);
  const auto& style = props.rowStyles.resolve(state);
  screen.target().fill(slot, style.background);
  // Keep the original art and hit targets; match the two previews' optical width.
  const int width = props.iconSize * settingsart::FramedPreview.width / settingsart::FramedPreview.height;
  const int imageHeight = props.iconSize * settingsart::CarouselPreview.height / settingsart::CarouselPreview.width;
  screen.target().bitmap(freeink::ui::makeRect(slot.x + (slot.width - width) / 2,
                                               slot.y + (slot.height - imageHeight) / 2, width, imageHeight),
                         settingsart::CarouselPreview, freeink::ui::BitmapMode::Stretch, style.foreground);
}
}  // namespace settingsui
