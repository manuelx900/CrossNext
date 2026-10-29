#include "HomeButtonSettingsActivity.h"

#include <cstdio>
#include <cstring>
#include <span>

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
void HomeButtonSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  if (binding != Binding::Home) show(Page::Actions, actionRow(SETTINGS.*actionField()));
}
uint8_t CrossPointSettings::* HomeButtonSettingsActivity::actionField() const {
  switch (binding) {
    case Binding::Home:
      return home_button::FIELDS[gesture];
    case Binding::SideLeft:
      return &CrossPointSettings::sideLeftAction;
    case Binding::SideRight:
      return &CrossPointSettings::sideRightAction;
  }
  return home_button::FIELDS[0];
}
char* HomeButtonSettingsActivity::shortcutTarget() const {
  switch (binding) {
    case Binding::Home:
      return home_button::shortcutTarget(gesture);
    case Binding::SideLeft:
      return SETTINGS.sideLeftShortcutTitle;
    case Binding::SideRight:
      return SETTINGS.sideRightShortcutTitle;
  }
  return home_button::shortcutTarget(0);
}
bool HomeButtonSettingsActivity::isShortcut() const {
  const auto value = SETTINGS.*actionField();
  return binding == Binding::Home ? home_button::opensShortcut(HomeButtonAction(value))
                                  : value == uint8_t(SideButtonAction::OpenShortcut);
}
int HomeButtonSettingsActivity::shortcutIndex() const {
  return binding == Binding::Home ? int(HomeButtonAction::OpenShortcut) : int(SideButtonAction::OpenShortcut);
}
int HomeButtonSettingsActivity::actionAt(int row) const {
  const int count = binding == Binding::Home ? HOME_ACTION_COUNT : int(SideButtonAction::Count);
  for (int action = 0; action < count; ++action) {
    const bool selectable = binding == Binding::Home ? selectableHomeAction(HomeButtonAction(action))
                                                     : selectableSideAction(SideButtonAction(action));
    if (selectable && row-- == 0) return action;
  }
  return -1;
}
int HomeButtonSettingsActivity::actionRow(int value) const {
  for (int row = 0; actionAt(row) >= 0; ++row)
    if (actionAt(row) == value) return row;
  return 0;
}
int HomeButtonSettingsActivity::listCount() const {
  switch (page) {
    case Page::Gestures:
      return GESTURE_COUNT + 1;
    case Page::Timing:
      return HomeButtonInput::TAP_WINDOW_COUNT;
    case Page::Actions: {
      int count = 0;
      while (actionAt(count) >= 0) ++count;
      return count;
    }
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
      if (binding == Binding::SideLeft) return tr(STR_SIDE_LEFT_ACTION);
      if (binding == Binding::SideRight) return tr(STR_SIDE_RIGHT_ACTION);
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
      if (binding == Binding::Home)
        show(Page::Gestures, gesture);
      else
        finish();
      return;
    case Page::Apps:
      show(Page::Actions, actionRow(shortcutIndex()));
      return;
    case Page::Timing:
      show(Page::Gestures, GESTURE_COUNT);
      return;
  }
}
bool HomeButtonSettingsActivity::saveAction(uint8_t action, const char* target) {
  auto& stored = SETTINGS.*actionField();
  const auto previous = stored;
  char oldTarget[40];
  std::snprintf(oldTarget, sizeof(oldTarget), "%s", shortcutTarget());
  if (previous != action || (target && std::strcmp(oldTarget, target))) {
    stored = action;
    if (target) std::snprintf(shortcutTarget(), sizeof(oldTarget), "%s", target);
    saveFailed = !SETTINGS.saveToFile();
    if (saveFailed) {
      stored = previous;
      std::snprintf(shortcutTarget(), sizeof(oldTarget), "%s", oldTarget);
      requestUpdate();
      return false;
    }
  }
  saveFailed = false;
  if (binding == Binding::Home)
    show(Page::Gestures, gesture);
  else
    show(Page::Actions, actionRow(action));
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
      show(Page::Actions, actionRow(isShortcut() ? shortcutIndex() : SETTINGS.*actionField()));
      return;
    }
    case Page::Actions:
      if (actionAt(index) == shortcutIndex()) {
        int selected = 0;
        for (int n = 0; const auto* item = shortcutAt(n); ++n)
          if (!std::strcmp(item->title, shortcutTarget())) {
            selected = n;
            break;
          }
        show(Page::Apps, selected);
      } else
        saveAction(uint8_t(actionAt(index)));
      return;
    case Page::Apps:
      if (const auto* item = shortcutAt(index)) {
        const uint8_t action = binding == Binding::Home ? uint8_t(home_button::shortcutAction(gesture))
                                                        : uint8_t(SideButtonAction::OpenShortcut);
        saveAction(action, item->title);
      }
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
  if (binding != Binding::Home && page == Page::Actions) {
    const auto body = screen.body();
    const int helpH = screen.device().width > screen.device().height ? 52 : 80;
    fui::TextStyle help;
    help.font = 4;
    help.maxLines = 3;
    screen.target().text(fui::makeRect(body.x + 8, body.y + 4, body.width - 16, helpH - 12), tr(STR_SIDE_ACTION_HELP),
                         help);
    screen.setContentMarginAbsolute(fui::Insets{static_cast<int16_t>(body.y + helpH),
                                                static_cast<int16_t>(screen.device().width - body.right()),
                                                static_cast<int16_t>(screen.device().height - body.bottom()), body.x});
  }
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
        const auto current = SETTINGS.*actionField();
        const auto labels = binding == Binding::Home ? std::span<const StrId>(home_button::ACTION_LABELS)
                                                     : std::span<const StrId>(side_button::LABELS);
        const int action = actionAt(i);
        rows[i].label = I18N.get(labels[action]);
        rows[i].toggle = binding != Binding::Home;
        rows[i].toggleChecked = isShortcut() ? action == shortcutIndex() : action == int(current);
        if (action == shortcutIndex())
          rows[i].subtitle = shelf::label(shortcutTarget());
        else if (readerHomeAction(binding == Binding::Home ? HomeButtonAction(action)
                                                           : sideHomeAction(SideButtonAction(action))))
          rows[i].subtitle = tr(STR_BUTTON_READER_ONLY);
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
