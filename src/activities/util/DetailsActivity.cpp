#include "DetailsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "activities/reader/DictionaryDefinitionActivity.h"
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
  catalogScreenHeader(screen, renderer, spec.header.c_str());
  const auto& theme = screen.theme();
  const auto asPtr = [](const std::string& s) { return s.empty() ? nullptr : s.c_str(); };
  const int16_t line = uiTarget.lineHeight(theme.bodyText.font);

  fui::PublicationPageProps& props = pageProps;
  props.book.title = asPtr(spec.title);
  props.book.author = asPtr(spec.author);
  props.book.series = asPtr(spec.source);  // the series slot is just a hero line
  props.book.titleText = theme.bodyText;
  props.book.detailText = theme.bodyText;
  props.book.seriesLines = 1;
  props.book.coverSize = fui::Size{0, 0};
  props.book.titleLines = 2;
  props.book.authorLines = 1;
  const int lines = 2 + !spec.author.empty() + !spec.source.empty();
  props.heroHeight = static_cast<int16_t>(lines * line + 2 * props.book.titleGap);
  props.metadata = asPtr(spec.metadata);
  props.metadataLines = 3;
  props.description = asPtr(summary);
  props.headingText = theme.bodyText;
  props.bodyText = theme.bodyText;

  // Same outlined themed button as the OPDS publication page.
  const auto styleButton = [&theme](fui::ButtonProps& button) {
    button.styles = theme.button;
    button.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    button.styles.normal.borderWidth = 1;
    button.text = theme.bodyText;
    button.radius = theme.controlRadius;
    button.minTouchSize = theme.minTouchSize;
  };
  props.primary.label = asPtr(spec.actionLabel);
  props.primary.action = ACTION_PRIMARY;
  styleButton(props.primary);
  if (!spec.description.empty()) {
    props.more.label = tr(STR_DETAILS);
    props.more.action = ACTION_MORE;
    styleButton(props.more);
  } else {
    props.more.label = nullptr;  // the HTML fallback can leave the description empty
  }
  props.actionHeight = theme.rowHeight;

  fui::publicationPage(screen.frame(), screen.body(), props);
}

void DetailsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  const char* moreLabel = spec.description.empty() ? "" : tr(STR_DETAILS);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), spec.actionLabel.c_str(), "", moreLabel);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
