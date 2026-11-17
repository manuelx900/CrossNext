#include "DetailsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "activities/reader/DictionaryDefinitionActivity.h"
#include "apps_local/plugins/PluginDetailsScreen.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/CatalogScreens.h"
#include "components/UITheme.h"
#include "util/HtmlToPlainText.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_PRIMARY = 1;
constexpr fui::ActionId ACTION_MORE = 2;
}  // namespace

DetailsActivity::DetailsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, DetailsSpec spec)
    : Activity("Details", renderer, mappedInput), UiAppHost(renderer), spec(std::move(spec)) {}

void DetailsActivity::onEnter() {
  Activity::onEnter();
  ActivityResult cancelled;
  cancelled.isCancelled = true;
  setResult(std::move(cancelled));
  summary = spec.descriptionHtml ? htmlToPlainText(spec.description) : spec.description;
  std::replace(summary.begin(), summary.end(), '\n', ' ');
  resetUi();
  app.on(ACTION_PRIMARY, &DetailsActivity::onPrimaryEvent, this);
  app.on(ACTION_MORE, &DetailsActivity::onMoreEvent, this);
  app.setScreen(&DetailsActivity::detailsScreen, this);
  requestUpdate();
}

void DetailsActivity::loop() {
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
  if (route) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    cancel();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    confirm();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
             mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    openMore();
  }
}

void DetailsActivity::confirm() {
  setResult(ActivityResult{});
  finish();
}

void DetailsActivity::cancel() { finish(); }

void DetailsActivity::openMore() {
  if (spec.description.empty()) return;
  // The viewer consumes its copy; this page keeps the original for repaints.
  auto viewer = makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, spec.title, spec.description,
                                                                spec.descriptionHtml);
  if (!viewer) {
    LOG_ERR("DETAILS", "OOM: description viewer");
    return;
  }
  startActivityForResult(std::move(viewer), nullptr);
}

void DetailsActivity::onPrimaryEvent(const fui::ActionEvent&, void* user) {
  auto* self = static_cast<DetailsActivity*>(user);
  self->app.clearTapFlash();
  self->confirm();
}

void DetailsActivity::onMoreEvent(const fui::ActionEvent&, void* user) {
  auto* self = static_cast<DetailsActivity*>(user);
  self->app.clearTapFlash();
  self->openMore();
}

void DetailsActivity::detailsScreen(UiScreen& screen, void* user) {
  static_cast<DetailsActivity*>(user)->buildScreen(screen);
}

void DetailsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer);
  pluginui::details(screen, spec, summary.c_str(), pageProps, toybox::headerStatus(renderer), ACTION_PRIMARY,
                    ACTION_MORE);
}
void DetailsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  const char* moreLabel = spec.description.empty() ? "" : tr(STR_DETAILS);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), spec.actionLabel.c_str(), "", moreLabel);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
