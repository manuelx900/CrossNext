#include "ContentsReturnUi.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "../ui/PixelMenuTheme.h"
#include "ContentsReturnScreen.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"

ContentsReturnUi::ContentsReturnUi(GfxRenderer& renderer) : UiAppHost(renderer), renderer(renderer) {
  resetUi();
  app.on(
      1,
      [](const freeink::ui::ActionEvent&, void* context) {
        auto& self = *static_cast<ContentsReturnUi*>(context);
        self.pending = true;
        self.app.clearTapFlash();
      },
      this);
  app.setScreen(&ContentsReturnUi::build, this);
}
void ContentsReturnUi::build(UiScreen& screen, void* context) {
  (void)context;
  const auto box = readertext::returnBox(screen.target(), screen.device(), UITheme::getInstance().getStatusBarHeight());
  readertext::returnControl(screen, box);
}
void ContentsReturnUi::render(bool mask) {
  pixelui::FontScope fonts(uiTarget, renderer);
  const auto device = uiTarget.deviceContext();
  if (mask) {
    readertext::paintReturn(uiTarget,
                            readertext::returnBox(uiTarget, device, UITheme::getInstance().getStatusBarHeight()), true);
    return;
  }
  if (builtWidth != device.width || builtHeight != device.height || builtOrientation != int(device.orientation)) {
    if (builtWidth) resetUi();
    builtWidth = device.width;
    builtHeight = device.height;
    builtOrientation = int(device.orientation);
    renderUi();
  } else {
    // Gray cleanup redraws the B/W pixels after the final paint. Keep the original reveal stamp.
    app.render();
  }
}
bool ContentsReturnUi::activated(const MappedInputManager& input) {
  routeTouch(input);
  return pending;
}
