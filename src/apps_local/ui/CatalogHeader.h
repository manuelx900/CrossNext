#pragma once

#include "PixelMenu.h"
#include "ToyboxScreen.h"

namespace catalogui {
template <typename Screen>
void header(Screen& screen, const char* title, const toybox::HeaderStatus& status,
            const freeink::ui::BitmapRef& search = {}, freeink::ui::ActionId searchAction = freeink::ui::NO_ACTION,
            bool homeBattery = false) {
  toybox::Screen chromeScreen(screen.frame(), screen.theme());
  pixelui::Header model;
  model.title = title;
  model.back = screen.device().hasTouch;
  model.battery = status.percent;
  model.charging = status.charging;
  model.showPercent = status.showPercent;
  model.homeBattery = homeBattery;
  model.clockText = status.clock;
  model.mark = search;
  model.markAction = searchAction;
  pixelui::header(chromeScreen, model);
}
}  // namespace catalogui
