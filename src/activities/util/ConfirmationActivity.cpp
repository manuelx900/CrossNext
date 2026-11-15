#include "ConfirmationActivity.h"

#include <I18n.h>

#include "HalDisplay.h"
#include "components/UITheme.h"

ConfirmationActivity::ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string& heading, const std::string& body, bool acknowledgeOnly)
    : Activity("Confirmation", renderer, mappedInput), heading(heading), body(body), acknowledgeOnly(acknowledgeOnly) {}

ConfirmationActivity::ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string& heading, const std::string& body,
                                           const std::string& cancel, const std::string& confirm)
    : Activity("Confirmation", renderer, mappedInput),
      heading(heading),
      body(body),
      acknowledgeOnly(false),
      cancelLabel(cancel),
      confirmLabel(confirm) {}

void ConfirmationActivity::onEnter() {
  Activity::onEnter();

  // Both texts live inside the dialog: the heading as its caption and the
  // subject (a book title) as the wrapping headline beneath it. No
  // pre-truncation — the dialog wraps both to its own width.
  const char* options[] = {acknowledgeOnly       ? tr(STR_OK_BUTTON)
                           : cancelLabel.empty() ? tr(STR_CANCEL)
                                                 : cancelLabel.c_str(),
                           confirmLabel.empty() ? tr(STR_CONFIRM) : confirmLabel.c_str()};
  confirmPopup.show(heading.c_str(), body.c_str(), options, acknowledgeOnly ? 1 : 2, 0, [this](int idx) {
    ActivityResult res;
    res.isCancelled = (idx != 1);
    setResult(std::move(res));
    finish();
  });

  requestUpdate(true);
}

void ConfirmationActivity::render(RenderLock&& lock) {
  renderer.clearScreen();

  if (confirmPopup.processRender(renderer, mappedInput)) return;

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void ConfirmationActivity::loop() {
  if (confirmPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  // Popup dismissed without a selection (Back button or tap outside): cancel.
  ActivityResult res;
  res.isCancelled = true;
  setResult(std::move(res));
  finish();
}
