#include "EndOfBookOptions.h"

#include "MappedInputManager.h"
#include "ReaderUtils.h"
#include "apps_local/readertext/EndOfBookScreen.h"
#include "apps_local/ui/ToyboxTheme.h"

namespace fui = freeink::ui;

EndOfBookOptions::EndOfBookOptions(GfxRenderer& renderer) : UiAppHost(renderer) {}

void EndOfBookOptions::loadOnce() {
  if (isLoaded.load(std::memory_order_acquire)) return;
  resetUi();
  uiTarget.setFont(fui::GfxRendererTarget::FONT_SMALL, toybox::kTileFontId);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_BODY, toybox::kUiFontId);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, toybox::kDisplayFontId);
  uiTarget.setFont(3, toybox::kTileFontId);
  uiTarget.setFont(4, UI_10_FONT_ID);
  app.on(readerend::kHome, &EndOfBookOptions::onAction, this);
  app.on(readerend::kLastPage, &EndOfBookOptions::onAction, this);
  app.setScreen(&EndOfBookOptions::endScreen, this);
  isLoaded.store(true, std::memory_order_release);
}

bool EndOfBookOptions::menuActive() const { return isLoaded.load(std::memory_order_acquire); }

void EndOfBookOptions::onAction(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<EndOfBookOptions*>(user);
  self->app.clearTapFlash();
  self->pendingAction = event.action == readerend::kHome ? Action::GoHome : Action::LastPage;
}

EndOfBookOptions::Action EndOfBookOptions::handleMenuInput(const MappedInputManager& input) {
  pendingAction = Action::None;
  const auto route = routeTouch(input);
  if (route && pendingAction != Action::None) return pendingAction;
  if (route.routed && app.invalidated()) return Action::Redraw;
  if (input.wasReleased(MappedInputManager::Button::Confirm)) return Action::GoHome;
  if (input.wasReleased(MappedInputManager::Button::Back) && input.getHeldTime() < ReaderUtils::GO_HOME_MS)
    return Action::LastPage;
  return Action::None;
}

void EndOfBookOptions::endScreen(UiScreen& screen, void*) {
  toybox::Screen themed(screen.frame());
  readerend::build(themed);
}

void EndOfBookOptions::render() { renderUi(); }
