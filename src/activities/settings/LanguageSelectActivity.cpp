#include "LanguageSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <iterator>

#include "CrossPointSettings.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/words/DictionaryDefaults.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

LanguageSelectActivity::LanguageSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("LanguageSelect", renderer, mappedInput, false, true) {}

void LanguageSelectActivity::onEnter() {
  UiListActivity::onEnter();

  // Open on the current language, which may sit past the first page; the first
  // screen build pulls the viewport to it (ListNav follow-on-build).
  const auto currentLang = static_cast<uint8_t>(I18N.getLanguage());
  const auto* begin = std::begin(availableLanguages);
  const auto* end = std::end(availableLanguages);
  const auto* it = std::find(begin, end, currentLang);
  nav.selected = (it != end) ? static_cast<int>(std::distance(begin, it)) : 0;

  // Built once here rather than every buildScreen() call: labels are static,
  // and the "Selected" marker can't go stale mid-visit since activateIndex()
  // finishes the activity immediately on selection.
  for (int i = 0; i < totalItems; ++i) {
    fui::ListItem item;
    item.label = I18N.getLanguageName(static_cast<Language>(availableLanguages[i]));
    if (availableLanguages[i] == currentLang) {
      item.subtitle = tr(STR_SELECTED);
    }
    item.actionValue = static_cast<int16_t>(i);
    rowItems[i] = item;
  }
}

const char* LanguageSelectActivity::headerTitle() const { return tr(STR_LANGUAGE); }

void LanguageSelectActivity::activateIndex(const int index) {
  // The activated row leaves this screen; a lingering flash would gray an
  // unrelated element on the next render.
  app.clearTapFlash();
  nav.selected = index;
  const uint8_t langIndex = availableLanguages[index];

  {
    RenderLock lock(*this);
    I18N.setLanguage(static_cast<Language>(langIndex));
  }

  SETTINGS.language = langIndex;
  dictionarydefaults::apply(SETTINGS);
  SETTINGS.saveToFile();

  // Return to previous page
  finish();
}

void LanguageSelectActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, true);
  useCompactSettingsRows();
  const auto& metrics = UITheme::getInstance().getMetrics();
  // Content: the safe area minus the header band GUI.drawHeader paints.
  pixelui::begin(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);

  // rowItems was built once in onEnter() and is reused here on every repaint.
  fui::ListProps props;
  props.items = rowItems;
  props.count = static_cast<uint16_t>(totalItems);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  pixelui::list(screen, props);
}
