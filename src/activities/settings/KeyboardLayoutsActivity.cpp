#include "KeyboardLayoutsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/ui/SettingsMenuStyle.h"
#include "components/LanguageUiFonts.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

void KeyboardLayoutsActivity::onEnter() {
  workingMask = keyboard_layouts::enabled();
  installed = keyboard_layouts::scanCardLayouts();
  workingLanguages = SETTINGS.keyboardLanguageLayouts | (installed & keyboard_layouts::languageBit(I18N.getLanguage()));
  edited = false;
  rowCount = 0;
  for (const auto& layout : keyboard_layouts::ALL) languages[rowCount++] = layout.language;
  for (uint8_t n = 0; n < getLanguageCount(); ++n) {
    const auto language = static_cast<Language>(n);
    if (language != Language::OOK && !keyboard_layouts::builtinLanguage(language)) languages[rowCount++] = language;
  }
  UiListActivity::onEnter();
}

void KeyboardLayoutsActivity::onExit() {
  if (edited && (workingMask != SETTINGS.keyboardLayouts || workingLanguages != SETTINGS.keyboardLanguageLayouts)) {
    SETTINGS.keyboardLayouts = workingMask;
    SETTINGS.keyboardLanguageLayouts = workingLanguages;
    SETTINGS.saveToFile();
  }
  Activity::onExit();
}

const char* KeyboardLayoutsActivity::headerTitle() const { return tr(STR_KEYBOARD_LAYOUTS); }

bool KeyboardLayoutsActivity::checked(uint8_t i) const {
  if (i >= rowCount) return false;
  return i < keyboard_layouts::COUNT ? (workingMask & keyboard_layouts::bitAt(i)) != 0
                                     : (workingLanguages & keyboard_layouts::languageBit(languages[i])) != 0;
}

bool KeyboardLayoutsActivity::isLocked(uint8_t i) const {
  if (i >= rowCount || !checked(i)) return false;
  if (languages[i] == I18N.getLanguage()) return true;
  return i < keyboard_layouts::COUNT && (workingMask & ~keyboard_layouts::bitAt(i) & keyboard_layouts::LATIN_BITS) == 0;
}

void KeyboardLayoutsActivity::activateIndex(int index) {
  RenderLock lock;
  if (index < 0 || index >= rowCount) return;
  nav.selected = index;
  app.clearTapFlash();
  if (isAvailable(uint8_t(index)) && !isLocked(uint8_t(index))) {
    if (index < keyboard_layouts::COUNT)
      workingMask ^= keyboard_layouts::bitAt(uint8_t(index));
    else
      workingLanguages ^= keyboard_layouts::languageBit(languages[index]);
    edited = true;
  }
  requestUpdate();
}

bool KeyboardLayoutsActivity::handleCustomInput() {
  const auto swipe = mappedInput.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::Up && swipe != MappedInputManager::SwipeDir::Down) return false;
  if (!routingReady()) return true;
  const int rows = std::max(1, std::min(3, nav.inputPageRows() / 2));
  app.cancelTouchContact();
  app.clearTapFlash();
  nav.requestScroll(swipe == MappedInputManager::SwipeDir::Up ? rows : -rows);
  requestUpdate();
  return true;
}

bool KeyboardLayoutsActivity::isAvailable(uint8_t index) const {
  return index < rowCount &&
         (index < keyboard_layouts::COUNT || (installed & keyboard_layouts::languageBit(languages[index])) != 0);
}

void KeyboardLayoutsActivity::provideRow(void* context, uint16_t index, fui::ListItem& item) {
  auto& self = *static_cast<KeyboardLayoutsActivity*>(context);
  if (index >= self.rowCount) return;
  const auto language = self.languages[index];
  const char* name = I18N.getLanguageName(language);
  item.label = LanguageUiFonts::supportsText(self.renderer, name, 10) ? name : LANGUAGE_CODES[unsigned(language)];
  item.value = index < keyboard_layouts::COUNT ? tr(STR_LANGUAGE_BUILT_IN) : tr(STR_LANGUAGE_ON_SD);
  item.labelBold = false;
  item.actionValue = int16_t(index);
  item.enabled = self.isAvailable(uint8_t(index));
  if (item.enabled) {
    GUI.setCheckboxRow(item, self.checked(uint8_t(index)));
    if (self.isLocked(uint8_t(index))) item.value = tr(STR_DEFAULT_VALUE);
  } else {
    item.value = tr(STR_LANGUAGE_UNAVAILABLE);
  }
}

void KeyboardLayoutsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, true);
  // Native names use the size-matched SD font fallback.
  uiTarget.setFont(4, UI_10_FONT_ID);
  const auto& metrics = UITheme::getInstance().getMetrics();
  pixelui::begin(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);
  fui::ListProps props;
  props.rowProvider = &KeyboardLayoutsActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = rowCount;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  settingsui::styleRows(props);
  props.rowHeight = 52;
  props.toggleWidth = props.toggleHeight = 26;
  props.rowPaddingY = 5;
  props.sidePadding = 12;
  props.textGap = 8;
  props.labelText.font = props.valueText.font = 4;
  props.labelText.maxLines = props.valueText.maxLines = 1;
  props.toggleCheckbox = true;
  screen.syncListViewport(nav, props, rowCount);
  // Keep Confirm on a visible row after a touch scroll, without replacing a
  // newer button-navigation request made during this build.
  const int selected = props.selectedIndex;
  const int visible = std::clamp(selected, nav.top, std::min(int(rowCount) - 1, nav.top + nav.visibleRows - 1));
  if (visible != selected) {
    int expected = selected;
    nav.selected.compare_exchange_strong(expected, visible);
    props.selectedIndex = visible;
  }
  pixelui::list(screen, props);
}
