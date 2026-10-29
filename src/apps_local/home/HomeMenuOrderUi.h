#pragma once
#include <I18n.h>

#include "../ui/PixelMenu.h"

namespace homemenuui {
struct OrderWork {
  freeink::ui::ButtonProps button;
};
template <typename Screen>
void orderControls(Screen& screen, OrderWork& work, int selected, int count, freeink::ui::ActionId action) {
  namespace fui = freeink::ui;
  fui::TextStyle help;
  help.font = 4;
  help.maxLines = 3;
  screen.target().text(screen.takeTop(92, 8), tr(STR_HOME_MENU_ORDER_HELP), help);
  const auto band = screen.takeBottom(62, 12);
  auto& button = work.button;
  button = {};
  button.styles = pixelui::buttonStyles();
  button.text.font = pixelui::kBody;
  button.text.align = fui::TextAlign::Center;
  button.text.maxLines = 2;
  button.action = action;
  button.minTouchSize = 44;
  button.radius = 0;
  for (int n = 0; n < 2; ++n) {
    char label[48];
    pixelui::copyCaps(label, sizeof(label), n ? tr(STR_HOME_MENU_MOVE_DOWN) : tr(STR_HOME_MENU_MOVE_UP));
    button.label = label;
    button.value = n ? 1 : -1;
    button.enabled = n ? selected + 1 < count : selected > 0;
    screen.button(button,
                  fui::makeRect(band.x + n * (band.width + 12) / 2, band.y, (band.width - 12) / 2, band.height));
  }
}
}  // namespace homemenuui
