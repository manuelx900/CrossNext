#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "apps_local/ui/ToyboxFonts.h"
#include "fontIds.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  toybox::ensureFonts(renderer);
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, pageHeight / 2 - 90, 120, 120);
  renderer.drawCenteredText(toybox::kUiFontId, pageHeight / 2 + 42, tr(STR_CROSSNEXT));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 86, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
