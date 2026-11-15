#pragma once

#include "../ui/PixelMenu.h"
#include "../ui/ToyboxScreen.h"

namespace pluginui {
namespace fui = freeink::ui;

template <typename Screen>
void header(Screen& screen, const char* title, const toybox::HeaderStatus& status, const fui::BitmapRef& search = {},
            fui::ActionId searchAction = fui::NO_ACTION) {
  toybox::Screen chromeScreen(screen.frame(), screen.theme());
  pixelui::Header model;
  model.title = title;
  model.back = screen.device().hasTouch;
  model.battery = status.percent;
  model.charging = status.charging;
  model.showPercent = status.showPercent;
  model.clockText = status.clock;
  model.mark = search;
  model.markAction = searchAction;
  pixelui::header(chromeScreen, model);
  pixelui::begin(screen);
}

inline void styleRows(fui::ListProps& props, bool picker) {
  pixelui::styleList(props);
  props.labelText.font = pixelui::kBody;
  props.labelText.bold = false;
  props.labelText.maxLines = 2;
  props.subtitleText.font = pixelui::kSmall;
  props.subtitleText.bold = false;
  props.subtitleText.maxLines = picker ? 2 : 3;
  props.valueText.font = pixelui::kSmall;
  props.valueText.bold = false;
  props.rowHeight = picker ? 88 : 72;
  props.rowPaddingY = 12;
  props.textGap = 8;
  props.trailingCentered = true;
  props.partialTrailingRow = false;
  props.selectionMarker = fui::SelectionMarker::None;
  if (chrome::lightHeader) {
    props.rowStyles = lightui::controlStyles();
    props.rowRadius = lightui::kControlRadius;
    props.rowGap = 10;
    props.separatorPaint = fui::Paint::solid(fui::Color::Transparent);
  } else {
    props.rowStyles.normal.borderWidth = 0;
    props.rowStyles.selected.borderWidth = 0;
    props.separatorPaint = fui::Paint::solid(fui::Color::Black);
  }
}

template <typename Screen>
void list(Screen& screen, fui::ListProps& props) {
  // Keep selection feedback for ListNav without painting a touch cursor.
  if (screen.device().hasTouch) {
    props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
  }
  pixelui::list(screen, props);
}
}  // namespace pluginui
