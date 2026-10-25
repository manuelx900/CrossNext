#pragma once
#include <I18n.h>

#include "../ui/PixelMenu.h"
namespace powermenu {
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
}  // namespace powermenu
