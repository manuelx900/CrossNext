#include "HomeButtonSettingsActivity.h"

#include <cstdio>
#include <cstring>

#include "apps_local/Shelf.h"
#include "apps_local/ui/PixelMenuTheme.h"
namespace fui = freeink::ui;
namespace {
const shelf::Item* shortcutAt(int index) {
  if (index < 0) return nullptr;
  for (int folder = 0; folder < shelf::folderCount(); ++folder) {
    const auto& choices = shelf::folders()[folder];
    if (index < choices.count) return &choices.items[index];
    index -= choices.count;
  }
  return nullptr;
}
}  // namespace
int HomeButtonSettingsActivity::listCount() const {
  switch (page) {
    case Page::Gestures:
      return GESTURE_COUNT + 1;
    case Page::Timing:
      return HomeButtonInput::TAP_WINDOW_COUNT;
    case Page::Actions:
      return ACTION_COUNT;
    case Page::Apps: {
      int count = 0;
      for (int folder = 0; folder < shelf::folderCount(); ++folder) count += shelf::folders()[folder].count;
      return count;
    }
  }
  return 0;
}
const char* HomeButtonSettingsActivity::headerTitle() const {
  switch (page) {
    case Page::Gestures:
      return tr(STR_HOME_BUTTON);
    case Page::Timing:
      return tr(STR_HOME_TAP_INTERVAL);
    case Page::Actions:
      return I18N.get(home_button::GESTURE_LABELS[gesture]);
    case Page::Apps:
      return tr(STR_HOME_SHORTCUT_TARGET);
  }
  return "";
}
void HomeButtonSettingsActivity::show(Page next, int selected) {
  page = next;
  nav = {};
  nav.selected = selected;
  nav.followOnBuild = true;
  resetUi();
  requestUpdate();
}
void HomeButtonSettingsActivity::onBackButton() {
  switch (page) {
    case Page::Gestures:
      finish();
      return;
    case Page::Actions:
      show(Page::Gestures, gesture);
      return;
    case Page::Apps:
      show(Page::Actions, int(HomeButtonAction::OpenShortcut));
      return;
    case Page::Timing:
      show(Page::Gestures, GESTURE_COUNT);
      return;
  }
}
bool HomeButtonSettingsActivity::saveAction(HomeButtonAction action, const char* target) {
  auto& stored = SETTINGS.*home_button::FIELDS[gesture];
  const auto previous = stored;
  char oldTarget[40];
  std::snprintf(oldTarget, sizeof(oldTarget), "%s", home_button::shortcutTarget(gesture));
  stored = uint8_t(action);
  if (target) std::snprintf(home_button::shortcutTarget(gesture), sizeof(oldTarget), "%s", target);
  saveFailed = !SETTINGS.saveToFile();
  if (saveFailed) {
    stored = previous;
    std::snprintf(home_button::shortcutTarget(gesture), sizeof(oldTarget), "%s", oldTarget);
    requestUpdate();
    return false;
  }
  show(Page::Gestures, gesture);
  return true;
}
void HomeButtonSettingsActivity::activateIndex(int index) {
  if (index < 0 || index >= listCount()) return;
  mappedInput.resetHomeButtonInput();
  app.clearTapFlash();
  switch (page) {
    case Page::Gestures: {
      if (index == GESTURE_COUNT) {
        show(Page::Timing, SETTINGS.homeButtonDoubleTapTiming);
        return;
      }
      gesture = index;
      const auto action = HomeButtonAction(SETTINGS.*home_button::FIELDS[gesture]);
      show(Page::Actions, home_button::opensShortcut(action) ? int(HomeButtonAction::OpenShortcut) : int(action));
      return;
    }
    case Page::Actions:
      if (index == int(HomeButtonAction::OpenShortcut)) {
        int selected = 0;
        for (int n = 0; const auto* item = shortcutAt(n); ++n)
          if (!std::strcmp(item->title, home_button::shortcutTarget(gesture))) {
            selected = n;
            break;
          }
        show(Page::Apps, selected);
      } else
        saveAction(HomeButtonAction(index));
      return;
    case Page::Apps:
      if (const auto* item = shortcutAt(index)) saveAction(home_button::shortcutAction(gesture), item->title);
      return;
    case Page::Timing: {
      const auto previous = SETTINGS.homeButtonDoubleTapTiming;
      SETTINGS.homeButtonDoubleTapTiming = static_cast<uint8_t>(index);
      saveFailed = !SETTINGS.saveToFile();
      if (saveFailed) {
        SETTINGS.homeButtonDoubleTapTiming = previous;
        requestUpdate();
      } else
        show(Page::Gestures, GESTURE_COUNT);
      return;
    }
  }
}
void HomeButtonSettingsActivity::shortcutRow(void*, uint16_t index, fui::ListItem& row) {
  row = {};
  if (const auto* item = shortcutAt(index)) {
    row.label = shelf::label(item->title);
    row.icon = fui::bitmapFromIcon(*item->icon);
    row.actionValue = index;
  }
}
void HomeButtonSettingsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  useCompactSettingsRows();
  pixelui::begin(screen, saveFailed ? 52 : 0);
  fui::ListProps props;
  if (page == Page::Apps) {
    props.rowProvider = &HomeButtonSettingsActivity::shortcutRow;
  } else {
    for (int i = 0; i < listCount(); ++i) {
      rows[i] = {};
      rows[i].actionValue = i;
      if (page == Page::Timing || (page == Page::Gestures && i == GESTURE_COUNT)) {
        const unsigned current = SETTINGS.homeButtonDoubleTapTiming < HomeButtonInput::TAP_WINDOW_COUNT
                                     ? SETTINGS.homeButtonDoubleTapTiming
                                     : 0;
        rows[i].label = I18N.get(page == Page::Timing ? home_button::TIMING_LABELS[i] : StrId::STR_HOME_TAP_INTERVAL);
        if (page == Page::Timing)
          rows[i].toggleChecked = unsigned(i) == current;
        else
          rows[i].subtitle = I18N.get(home_button::TIMING_LABELS[current]);
        continue;
      }
      if (page == Page::Gestures) {
        const auto action = HomeButtonAction(SETTINGS.*home_button::FIELDS[i]);
        rows[i].label = I18N.get(home_button::GESTURE_LABELS[i]);
        rows[i].subtitle =
            home_button::opensShortcut(action)
                ? shelf::label(home_button::shortcutTarget(i))
                : I18N.get(
                      home_button::ACTION_LABELS[unsigned(action) < unsigned(HomeButtonAction::Count) ? unsigned(action)
                                                                                                      : 0]);
      } else {
        const auto current = HomeButtonAction(SETTINGS.*home_button::FIELDS[gesture]);
        rows[i].label = I18N.get(home_button::ACTION_LABELS[i]);
        rows[i].toggleChecked =
            home_button::opensShortcut(current) ? i == int(HomeButtonAction::OpenShortcut) : i == int(current);
        if (i == int(HomeButtonAction::OpenShortcut))
          rows[i].subtitle = shelf::label(home_button::shortcutTarget(gesture));
      }
    }
    props.items = rows;
  }
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  pixelui::list(screen, props);
  if (saveFailed) {
    const auto safe = screen.device().safeRect();
    pixelui::text(screen.target(), fui::makeRect(safe.x + 12, safe.bottom() - 48, safe.width - 24, 44),
                  tr(STR_HOME_MENU_SAVE_FAILED), 3);
  }
}
