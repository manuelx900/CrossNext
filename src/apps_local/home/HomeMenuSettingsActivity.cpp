#include "HomeMenuSettingsActivity.h"

#include <Memory.h>

#include "../../OpdsServerStore.h"
#include "../../activities/plugins/PluginCatalogActivity.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../Shelf.h"
#include "../ui/ExtraIcons.h"
#include "../ui/PixelMenuTheme.h"
#include "HomeMenuOrderUi.h"

void HomeMenuSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  app.on(ACTION_USER, &HomeMenuSettingsActivity::moveAction, this);
  draft = homemenu::config();
  plugins = anyPluginInstalled();
  services = plugins || OPDS_STORE.hasServers();
}
int HomeMenuSettingsActivity::menuRows() const {
  int count = homemenu::kBaseCount;
  for (const auto& entry : draft.shortcuts)
    if (*entry.title) ++count;
  return count;
}
homemenu::Id HomeMenuSettingsActivity::idAtRow(int index) const {
  if (view == View::Order) return homemenu::at(draft, index, services);
  if (index < homemenu::kBaseCount) return static_cast<homemenu::Id>(index);
  int row = homemenu::kBaseCount;
  for (int n = 0; n < homemenu::kShortcutCount; ++n)
    if (*draft.shortcuts[n].title && index == row++) return static_cast<homemenu::Id>(homemenu::kBaseCount + n);
  return homemenu::Id::Count;
}
int HomeMenuSettingsActivity::listCount() const {
  switch (view) {
    case View::Root:
      return 3;
    case View::Order:
      return homemenu::visibleCount(draft, services);
    case View::Names:
      return menuRows();
    case View::Visibility:
      return menuRows() + 2;
    case View::ChooseGame:
    case View::ChooseApp:
      return shelf::folders()[pickerFolder()].count;
  }
  return 0;
}
const char* HomeMenuSettingsActivity::headerTitle() const {
  switch (view) {
    case View::Root:
      return tr(STR_HOME_MENU);
    case View::Names:
      return tr(STR_HOME_MENU_RENAME);
    case View::Visibility:
      return tr(STR_HOME_MENU_VISIBILITY);
    case View::Order:
      return tr(STR_HOME_MENU_ORDER);
    case View::ChooseGame:
      return tr(STR_HOME_ADD_GAME);
    case View::ChooseApp:
      return tr(STR_HOME_ADD_APP);
  }
  return tr(STR_HOME_MENU);
}
void HomeMenuSettingsActivity::show(View next) {
  app.clearTapFlash();
  view = next;
  notice = StrId::STR_NONE_OPT;
  nav.reset();
  resetUi();
  requestUpdate();
}
void HomeMenuSettingsActivity::onBackButton() {
  switch (view) {
    case View::Root:
      finish();
      return;
    case View::ChooseGame:
    case View::ChooseApp:
      show(View::Visibility);
      return;
    case View::Names:
    case View::Visibility:
    case View::Order:
      show(View::Root);
      return;
  }
}
bool HomeMenuSettingsActivity::save() {
  if (homemenu::Store::instance().save(draft)) return true;
  draft = homemenu::config();
  notice = StrId::STR_HOME_MENU_SAVE_FAILED;
  requestUpdate();
  return false;
}
void HomeMenuSettingsActivity::moveAction(const freeink::ui::ActionEvent& event, void* context) {
  auto& self = *static_cast<HomeMenuSettingsActivity*>(context);
  if (self.view != View::Order) return;
  const auto id = self.idAtRow(self.nav.selected);
  if (!homemenu::move(self.draft, id, event.value, self.services)) return;
  self.app.clearTapFlash();
  self.notice = StrId::STR_NONE_OPT;
  self.save();
  self.moveSelectionTo(homemenu::indexOf(self.draft, id, self.services));
}
void HomeMenuSettingsActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (index < 0 || index >= listCount()) return;
  if (view == View::Root) {
    show(index == 0 ? View::Names : index == 1 ? View::Visibility : View::Order);
    return;
  }
  if (view == View::Order) {
    requestUpdate();
    return;
  }
  if (view == View::ChooseGame || view == View::ChooseApp) {
    const int folder = pickerFolder();
    const auto* title = shelf::folders()[folder].items[index].title;
    const auto existing = homemenu::findShortcut(draft, folder, title);
    if (existing != homemenu::Id::Count) {
      homemenu::removeShortcut(draft, existing);
      save();
    } else if (homemenu::addShortcut(draft, folder, title) == homemenu::Id::Count) {
      notice = StrId::STR_HOME_MENU_LIMIT;
    } else if (save()) {
      show(View::Visibility);
      const auto added = homemenu::findShortcut(draft, folder, title);
      for (int row = homemenu::kBaseCount; row < menuRows(); ++row)
        if (idAtRow(row) == added) {
          moveSelectionTo(row);
          break;
        }
    }
    requestUpdate();
    return;
  }
  if (view == View::Visibility && index >= menuRows()) {
    show(index == menuRows() ? View::ChooseGame : View::ChooseApp);
    return;
  }
  const auto id = idAtRow(index);
  if (view == View::Visibility) {
    const bool shown = (draft.visible & (1u << static_cast<int>(id))) != 0;
    if (id == homemenu::Id::Services && !services && !shown) return;
    if (homemenu::isShortcut(id)) {
      homemenu::removeShortcut(draft, id);
      nav.reset();
      save();
    } else if (!homemenu::setVisible(draft, id, !shown))
      notice = id == homemenu::Id::Settings ? StrId::STR_HOME_MENU_REQUIRED : StrId::STR_HOME_MENU_LIMIT;
    else {
      notice = StrId::STR_NONE_OPT;
      save();
    }
    requestUpdate();
    return;
  }
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_HOME_MENU_RENAME),
                                                           homemenu::label(id, plugins), homemenu::kLabelBytes - 1);
  if (!keyboard) {
    LOG_ERR("HOME-MENU", "OOM: rename keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this, id](const ActivityResult& result) {
    RenderLock lock(*this);
    if (!result.isCancelled) {
      if (!homemenu::setLabel(draft, id, std::get<KeyboardResult>(result.data).text.c_str()))
        notice = StrId::STR_HOME_MENU_INVALID_NAME;
      else {
        notice = StrId::STR_NONE_OPT;
        save();
      }
    }
    resetUi();
    requestUpdate();
  });
}
void HomeMenuSettingsActivity::drawChrome() {
  pixelui::drawHeader(renderer, headerTitle(), I18N.getLanguage() != Language::EN);
}
void HomeMenuSettingsActivity::provideRow(void* context, uint16_t index, freeink::ui::ListItem& row) {
  const auto& self = *static_cast<HomeMenuSettingsActivity*>(context);
  row = {};
  row.actionValue = index;
  if (self.view == View::Root) {
    row.label = index == 0   ? tr(STR_HOME_MENU_RENAME)
                : index == 1 ? tr(STR_HOME_MENU_VISIBILITY)
                             : tr(STR_HOME_MENU_ORDER);
    return;
  }
  if (self.view == View::ChooseGame || self.view == View::ChooseApp) {
    const auto& item = shelf::folders()[self.pickerFolder()].items[index];
    row.label = shelf::label(item.title);
    row.icon = freeink::ui::bitmapFromIcon(*item.icon);
    row.toggle = false;
    row.toggleChecked = homemenu::findShortcut(self.draft, self.pickerFolder(), item.title) != homemenu::Id::Count;
    row.icon = freeink::ui::bitmapFromIcon(row.toggleChecked ? extra_checked_32 : extra_unchecked_32);
    return;
  }
  if (self.view == View::Visibility && index >= self.menuRows()) {
    row.label = index == self.menuRows() ? tr(STR_HOME_ADD_GAME) : tr(STR_HOME_ADD_APP);
    return;
  }
  const auto id = self.idAtRow(index);
  row.label = homemenu::label(id, self.plugins);
  if (self.view == View::Order) return;
  if (self.view == View::Names) {
    row.value = ">";
    return;
  }
  row.toggle = false;
  row.toggleChecked = (self.draft.visible & (1u << static_cast<int>(id))) != 0;
  row.icon = freeink::ui::bitmapFromIcon(row.toggleChecked ? extra_checked_32 : extra_unchecked_32);
  row.enabled = id != homemenu::Id::Settings && (id != homemenu::Id::Services || self.services || row.toggleChecked);
  if (id == homemenu::Id::Settings)
    row.subtitle = tr(STR_HOME_MENU_REQUIRED);
  else if (id == homemenu::Id::Services && !self.services)
    row.subtitle = tr(STR_HOME_MENU_CONFIGURE_SERVICES);
  else if (homemenu::isShortcut(id))
    row.subtitle = tr(STR_HOME_REMOVE_SHORTCUT);
}
void HomeMenuSettingsActivity::buildScreen(UiScreen& screen) {
  bool unicode = I18N.getLanguage() != Language::EN;
  for (const auto& label : draft.labels)
    for (unsigned char c : label) unicode |= c >= 0x80;
  pixelui::FontScope fonts(uiTarget, renderer, unicode);
  useCompactSettingsRows();
  pixelui::begin(screen, notice == StrId::STR_NONE_OPT ? 0 : 52);
  if (view == View::Names) {
    if (chrome::lightHeader) {
      freeink::ui::TextStyle help;
      help.font = 4;
      help.maxLines = 2;
      screen.target().text(screen.takeTop(64, 8), tr(STR_SR_RENAME_CHOOSE), help);
      help.maxLines = 3;
      screen.target().text(screen.takeBottom(88, 8), tr(STR_SR_RENAME_RESET), help);
    } else {
      pixelui::text(screen.target(), screen.takeTop(56, 8), tr(STR_SR_RENAME_CHOOSE), 4);
      pixelui::text(screen.target(), screen.takeBottom(72, 8), tr(STR_SR_RENAME_RESET), 4);
    }
  }

  props = {};
  pixelui::styleList(props);
  props.labelText.maxLines = 1;
  props.subtitleText.maxLines = 2;
  props.rowProvider = &HomeMenuSettingsActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = listCount();
  props.action = ACTION_ROW;
  if (view == View::Order) {
    homemenuui::orderControls(screen, orderWork, nav.selected, listCount(), ACTION_USER);
    syncListViewport(screen, props);
    rowText.bind(props, screen.target(), screen.body());
    pixelui::list(screen, props);
    if (notice != StrId::STR_NONE_OPT)
      pixelui::text(
          screen.target(),
          freeink::ui::makeRect(screen.body().x, screen.device().safeRect().bottom() - 48, screen.body().width, 44),
          I18N.get(notice), pixelui::kSmall);
    return;
  }
  syncListViewport(screen, props);
  if (view == View::Names) {
    props.rowGap = 0;
    props.rowHeight = screen.device().width > screen.device().height ? 56 : 72;
    props.labelText.font = chrome::lightHeader ? pixelui::kSmall : pixelui::kBody;
    props.labelText.maxLines = 1;
    props.rowPaddingY = 10;
    props.rowStyles.normal.borderWidth = chrome::lightHeader ? 0 : 1;
    props.valueText.font = pixelui::kBody;
  }
  rowText.bind(props, screen.target(), screen.body());
  pixelui::list(screen, props);
  if (notice != StrId::STR_NONE_OPT) {
    auto safe = screen.device().safeRect();
    pixelui::text(screen.target(), freeink::ui::makeRect(safe.x + 12, safe.bottom() - 48, safe.width - 24, 44),
                  I18N.get(notice), pixelui::kSmall);
  }
}
void HomeMenuSettingsActivity::drawFooter() {}
