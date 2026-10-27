#pragma once

#include <BoardConfig.h>
#include <HalClock.h>

#include "../../CrossPointSettings.h"
#include "../../components/HeaderBackTapTarget.h"
#include "../../components/UIScale.h"
#include "../../components/UITheme.h"
#include "PixelMenu.h"
#include "ToyboxTheme.h"

namespace pixelui {
inline void bindFonts(freeink::ui::GfxRendererTarget& target, GfxRenderer& renderer, bool = false) {
  toybox::ensureFonts(renderer);
  target.setFont(0, toybox::kButtonFontId);
  target.setFont(1, toybox::kUiFontId);
  target.setFont(2, toybox::kDisplayFontId);
  target.setFont(3, toybox::kTileFontId);
  target.setFont(4, toybox::kLibraryFontId);
  target.setFont(fui::HEADER_CLOCK_FONT, toybox::kButtonFontId);
}

// Keep the host's shared theme derivation on its original UI font bindings.
class FontScope {
 public:
  FontScope(fui::GfxRendererTarget& target, GfxRenderer& renderer, bool unicode = false) : target(target) {
    bindFonts(target, renderer, unicode);
  }
  ~FontScope() {
    const auto fonts = uiScaleSpec();
    target.setFont(0, fonts.smallFontId);
    target.setFont(1, fonts.bodyFontId);
    target.setFont(2, fonts.titleFontId);
    target.setFont(3, SMALL_FONT_ID);
    target.setFont(fui::GfxRendererTarget::FONT_CLOCK, UI_12_FONT_ID);
  }
  FontScope(const FontScope&) = delete;
  FontScope& operator=(const FontScope&) = delete;

 private:
  fui::GfxRendererTarget& target;
};

inline void drawHeader(GfxRenderer& renderer, const char* title, bool unicode = false, const char* subtitle = nullptr) {
  auto target = toybox::makeTarget(renderer);
  bindFonts(target, renderer, unicode);
  Header model;
  model.title = title;
  model.rightLabel = subtitle;
  model.back = BoardConfig::hasTouch();
  // Reuse GUI's battery visibility/charging contract without its version slot.
  // Chrome runs on the shared render task; reuse its large SDK descriptor.
  static fui::HeaderProps status;
  status.status.battery.label = nullptr;
  GUI.applyHeaderStatus(renderer, status);
  static char clock[10];
  clock[0] = 0;
  if (SETTINGS.clockShowInHeader && halClock.formatTime(clock, sizeof(clock), SETTINGS.clockFormat == 1))
    model.clockText = clock;
  model.battery = status.status.battery.percent;
  model.charging = status.status.battery.charging;
  model.showPercent = status.status.battery.label != nullptr;
  header(target, target.deviceContext(), model);
  const auto back = geometry(target.deviceContext()).back;
  if (model.back)
    HeaderBackTapTarget::set(back.x, back.y, back.width, back.height);
  else
    HeaderBackTapTarget::clear();
}
}  // namespace pixelui
