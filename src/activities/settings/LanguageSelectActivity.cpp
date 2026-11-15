#include "LanguageSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/util/ConfirmationActivity.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;
namespace {
constexpr Language BUILTIN_LANGUAGES[] = {Language::EN, Language::ES};
}

LanguageSelectActivity::LanguageSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("LanguageSelect", renderer, mappedInput, false, true) {}

void LanguageSelectActivity::onEnter() {
  UiListActivity::onEnter();
  catalogFailed = !catalog.scan();
  nav.selected = 0;
  for (int index = 0; index < listCount(); ++index) {
    if (isActive(index)) {
      nav.selected = index;
      break;
    }
  }
}

void LanguageSelectActivity::onExit() {
  Activity::onExit();
  catalog.clear();
}

const char* LanguageSelectActivity::headerTitle() const { return tr(STR_LANGUAGE); }

bool LanguageSelectActivity::isActive(int index) const {
  if (index < BUILTIN_COUNT) return !I18N.hasLanguagePack() && I18N.getLanguage() == BUILTIN_LANGUAGES[index];
  return I18N.hasLanguagePack() && strcmp(SETTINGS.languagePack, catalog[index - BUILTIN_COUNT].filename) == 0;
}

void LanguageSelectActivity::provideRow(void* ctx, uint16_t index, fui::ListItem& item) {
  auto& self = *static_cast<LanguageSelectActivity*>(ctx);
  if (index >= self.listCount()) return;
  if (index < BUILTIN_COUNT) {
    item.label = I18N.getLanguageName(BUILTIN_LANGUAGES[index]);
    item.value = tr(STR_LANGUAGE_BUILT_IN);
    if (self.catalogFailed) item.subtitle = tr(STR_LANGUAGE_PACK_SCAN_FAILED);
  } else {
    const auto& entry = self.catalog[index - BUILTIN_COUNT];
    item.label = entry.info.name;
    item.value = strcmp(SETTINGS.languagePack, entry.filename) == 0 && !I18N.hasLanguagePack()
                     ? tr(STR_LANGUAGE_UNAVAILABLE)
                     : tr(STR_LANGUAGE_ON_SD);
  }
  if (self.isActive(index)) item.value = tr(STR_LANGUAGE_IN_USE);
  item.labelBold = false;
  item.actionValue = static_cast<int16_t>(index);
}

void LanguageSelectActivity::activateIndex(int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  if (isActive(index) && !(index < BUILTIN_COUNT && SETTINGS.languagePack[0])) {
    finish();
    return;
  }
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_LANGUAGE), tr(STR_LANGUAGE_RESTART_PROMPT));
  if (!confirmation) {
    LOG_ERR("I18N", "OOM: language confirmation");
    return;
  }
  startActivityForResult(std::move(confirmation), [this, index](const ActivityResult& result) {
    if (!result.isCancelled) applySelection(index);
  });
}

void LanguageSelectActivity::applySelection(int index) {
  char previousPack[sizeof(SETTINGS.languagePack)];
  memcpy(previousPack, SETTINGS.languagePack, sizeof(previousPack));
  const uint8_t previousLanguage = SETTINGS.language;
  if (index < BUILTIN_COUNT) {
    SETTINGS.language = static_cast<uint8_t>(BUILTIN_LANGUAGES[index]);
    SETTINGS.languagePack[0] = '\0';
  } else {
    const auto& entry = catalog[index - BUILTIN_COUNT];
    SETTINGS.language = static_cast<uint8_t>(I18n::languageFromCode(entry.info.code));
    memcpy(SETTINGS.languagePack, entry.filename, sizeof(SETTINGS.languagePack));
  }
  if (!SETTINGS.saveToFile()) {
    SETTINGS.language = previousLanguage;
    memcpy(SETTINGS.languagePack, previousPack, sizeof(previousPack));
    if (auto error = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_LANGUAGE),
                                                             tr(STR_TOOLS_SAVE_FAILED), true)) {
      startActivityForResult(std::move(error), nullptr);
    } else {
      LOG_ERR("I18N", "OOM: save failure dialog");
    }
    return;
  }
  restartToSettingsAfterLanguageChange();
}

void LanguageSelectActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, true);
  useCompactSettingsRows();
  const auto& metrics = UITheme::getInstance().getMetrics();
  pixelui::begin(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);
  fui::ListProps props;
  props.rowProvider = &LanguageSelectActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  pixelui::list(screen, props);
}
