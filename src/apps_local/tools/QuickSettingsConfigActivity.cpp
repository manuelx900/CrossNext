#include "QuickSettingsConfigActivity.h"

#include <CrossPointSettings.h>

#include "../ui/PixelMenuTheme.h"
#include "../ui/SettingsMenuStyle.h"
void QuickSettingsConfigActivity::drawChrome() {
  pixelui::drawHeader(renderer, headerTitle(), I18N.getLanguage() != Language::EN);
}
void QuickSettingsConfigActivity::onBackButton() {
  if (slot < 0) {
    finish();
    return;
  }
  const int previousSlot = slot;
  slot = -1;
  nav.reset(previousSlot);
  resetUi();
  requestUpdate();
}
void QuickSettingsConfigActivity::activateIndex(int index) {
  if (index < 0 || index >= listCount()) return;
  int selection = 0;
  if (slot < 0) {
    slot = index;
    selection = SETTINGS.quickActions[slot];
  } else {
    selection = slot;
    const uint8_t previous = SETTINGS.quickActions[slot];
    for (int n = 0; n < 4; ++n)
      if (n != slot && SETTINGS.quickActions[n] == index) SETTINGS.quickActions[n] = previous;
    SETTINGS.quickActions[slot] = uint8_t(index);
    saveFailed = !SETTINGS.saveToFile();
    slot = -1;
  }
  nav.reset(selection);
  app.clearTapFlash();
  resetUi();
  requestUpdate();
}
void QuickSettingsConfigActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  useCompactSettingsRows();
  pixelui::begin(screen, saveFailed ? 52 : 0);
  for (int n = 0; n < listCount(); ++n) {
    rows[n] = {};
    rows[n].actionValue = n;
    const auto shortcut = static_cast<quickui::Shortcut>(slot < 0 ? SETTINGS.quickActions[n] : n);
    rows[n].icon = freeink::ui::bitmapFromIcon(quickui::icon(shortcut));
    if (slot < 0) {
      std::snprintf(names[n], sizeof(names[n]), tr(STR_QUICK_SLOT), n + 1);
      rows[n].label = names[n];
      rows[n].subtitle = quickui::label(shortcut);
    } else {
      rows[n].label = quickui::label(shortcut);
      rows[n].toggle = true;
      rows[n].toggleChecked = SETTINGS.quickActions[slot] == n;
    }
  }
  props = {};
  settingsui::styleRows(props);
  props.items = rows;
  props.count = listCount();
  props.action = ACTION_ROW;
  syncListViewport(screen, props);
  quickui::styleConfiguration(props);
  pixelui::list(screen, props);
  if (saveFailed) {
    const auto safe = screen.device().safeRect();
    pixelui::text(screen.target(), freeink::ui::makeRect(safe.x + 12, safe.bottom() - 48, safe.width - 24, 44),
                  tr(STR_HOME_MENU_SAVE_FAILED), 3);
  }
}
