#pragma once
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "apps_local/readertext/ReferenceSlider.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/UiAppHost.h"
#include "fontIds.h"

struct UiSliderDialogSpec {
  const char* title = nullptr;  // dialog caption
  const char* subtitle = nullptr;
  const char* readout = nullptr;  // preformatted current-value text
  int value = 0;                  // slider position (0-based within max)
  int max = 1;                    // slider range
  // Range endpoints, drawn small over the slider band's ends ("0%" / "100%").
  const char* minLabel = nullptr;
  const char* maxLabel = nullptr;
  freeink::ui::ActionId sliderAction = 0;
  freeink::ui::ActionId stepAction = 0;  // dispatched with value -1 / +1
  freeink::ui::ActionId okAction = 0;
  // Absorbs taps on the dialog body (needs no handler); the activity treats
  // an unrouted release with real coordinates as tap-outside = cancel.
  freeink::ui::ActionId chromeAction = 0;
  // Step hints for button boards (small step, large step); skipped on touch.
  const char* hintLine1 = nullptr;
  const char* hintLine2 = nullptr;
};

inline void buildSliderDialogScreen(UiAppHost::UiScreen& screen, freeink::ui::GfxRendererTarget& uiTarget,
                                    const MappedInputManager& mappedInput, const UiSliderDialogSpec& spec) {
  uiTarget.setFont(0, UI_12_FONT_ID);
  uiTarget.setFont(1, UI_12_FONT_ID);
  uiTarget.setFont(3, UI_10_FONT_ID);
  referenceslider::build(screen, spec, mappedInput.hasTouch());
}
