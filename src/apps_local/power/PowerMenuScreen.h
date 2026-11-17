#pragma once
#include <I18n.h>

#include "../ui/PixelMenu.h"
namespace powermenu {
inline void style(freeink::ui::ListProps& props) {
  props.labelText.bold = false;
  props.rowStyles.normal.borderWidth = props.rowStyles.selected.borderWidth = props.rowStyles.focused.borderWidth =
      props.rowStyles.active.borderWidth = 0;
  props.separatorPaint = freeink::ui::Paint::solid(freeink::ui::Color::Transparent);
}
inline void rows(freeink::ui::ListItem* items) {
  static constexpr StrId labels[] = {StrId::STR_POWER_SLEEP, StrId::STR_POWER_OFF, StrId::STR_POWER_RESTART};
  for (int n = 0; n < 3; ++n) {
    items[n] = {};
    items[n].label = I18N.get(labels[n]);
    items[n].actionValue = n;
  }
}
template <class Screen>
void place(Screen& s, freeink::ui::ListProps& p) {
  pixelui::begin(s);
  pixelui::styleList(p);
  const auto area = s.body();
  const int height = 3 * p.rowHeight;
  const auto device = s.device();
  const int y = area.y + std::max(0, (int(area.height) - height) / 2);
  s.setContentMarginAbsolute(freeink::ui::Insets{int16_t(y), int16_t(device.width - area.right()),
                                                 int16_t(std::max(0, int(device.height) - y - height)), area.x});
}
template <class Screen>
void draw(Screen& screen, freeink::ui::ListProps& props) {
  pixelui::list(screen, props, false, false, pixelui::kBorder, false, true);
  const auto area = pixelui::rowArea(screen.body(), props);
  int y = area.y;
  for (uint16_t n = props.topIndex; n < props.count; ++n) {
    freeink::ui::ListItem item;
    if (props.rowProvider) {
      props.rowProvider(props.rowProviderCtx, n, item);
    } else {
      if (!props.items || n < props.itemsWindowFirst ||
          (props.itemsWindowCount > 0 && n - props.itemsWindowFirst >= props.itemsWindowCount))
        break;
      item = props.items[n - props.itemsWindowFirst];
    }
    const int height =
        freeink::ui::cachedListRow(screen.target(), screen.frame().assets(), area.width, props, item).height;
    if (y + height > area.bottom()) break;
    if (n != props.topIndex)
      screen.target().fill(freeink::ui::makeRect(area.x, y - 1, area.width, pixelui::kBorder),
                           freeink::ui::Paint::solid(freeink::ui::Color::Black));
    y += height + props.rowGap;
  }
}
}  // namespace powermenu
