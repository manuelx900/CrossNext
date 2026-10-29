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
  static constexpr freeink::ui::BitmapRef icons[] = {settingsart::Display, settingsart::Book, settingsart::Hand,
                                                     settingsart::Folder,  settingsart::Wifi, settingsart::Gear,
                                                     settingsart::Info,    settingsart::Grid};
  item = {};
  item.label = I18N.get(homeNames[i]);
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
  p.rowGap = 0;
  p.rowStyles.normal.borderWidth = 0;
  p.rowStyles.selected.borderWidth = 0;
  p.rowStyles.focused.borderWidth = 0;
  p.rowStyles.active.borderWidth = 0;
  p.separatorPaint = freeink::ui::Paint::solid(freeink::ui::Color::Transparent);
  p.iconSize = 108;
  p.labelText.font = 1;
  p.labelText.bold = true;
  p.labelText.maxLines = 2;
}
struct AppearanceActions {
  freeink::ui::Rect recent, font;
};
inline AppearanceActions appearanceActions(freeink::ui::Rect body, int themeBottom, bool wide) {
  const int height = wide ? 44 : 56, gap = 12;
  const int top = std::max(themeBottom + gap, body.bottom() - 2 * height - gap);
  return {freeink::ui::makeRect(body.x, top, body.width, height),
          freeink::ui::makeRect(body.x, top + height + gap, body.width, height)};
}
template <typename Screen>
void appearanceList(Screen& screen, freeink::ui::ListProps& props) {
  pixelui::list(screen, props);
  if (props.count < 2 || !props.items) return;
  const auto band = pixelui::rowArea(screen.body(), props);
  const int height =
      freeink::ui::measureListRow(screen.target(), screen.frame().assets(), band.width, props, props.items[0]).height;
  if (height < band.height)
    screen.target().fill(
        freeink::ui::makeRect(band.x, band.y + height - pixelui::kBorder / 2, band.width, pixelui::kBorder),
        freeink::ui::Paint::solid(freeink::ui::Color::Black));
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
