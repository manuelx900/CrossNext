#include "HomeMenuSettingsActivity.h"

#include <Memory.h>

#include <algorithm>

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
  plugins = SETTINGS.pluginsEnabled;
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
      return 5;
    case View::GameOrder:
    case View::AppOrder:
      return shelf::folders()[orderFolder()].count;
    case View::Order:
      return homemenu::visibleCount(draft, services);
    case View::Names:
      return menuRows();
    case View::Visibility:
      return menuRows() + 2 + (plugins ? 1 : 0);
    case View::ChoosePlugin:
      return static_cast<int>(pluginChoices.size());
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
    case View::GameOrder:
      return tr(STR_SHELF_ORDER_GAMES);
    case View::AppOrder:
      return tr(STR_SHELF_ORDER_APPS);
    case View::ChooseGame:
      return tr(STR_HOME_ADD_GAME);
    case View::ChooseApp:
      return tr(STR_HOME_ADD_APP);
    case View::ChoosePlugin:
      return tr(STR_HOME_ADD_PLUGIN);
  }
  return tr(STR_HOME_MENU);
}
void HomeMenuSettingsActivity::show(View next) {
  RenderLock lock(*this);
  app.clearTapFlash();
  view = next;
  if (view == View::ChoosePlugin) {
    pluginChoices = discoverPlugins();
    pluginChoices.erase(std::remove_if(pluginChoices.begin(), pluginChoices.end(),
                                       [](const PluginRef& plugin) {
                                         return plugin.deviceKind != PluginLocations::DeviceKind::Catalog ||
                                                plugin.name.size() >= homemenu::kItemBytes;
                                       }),
                        pluginChoices.end());
  } else {
    std::vector<PluginRef>().swap(pluginChoices);
  }
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
    case View::ChoosePlugin:
      show(View::Visibility);
      return;
    case View::Names:
    case View::Visibility:
    case View::Order:
    case View::GameOrder:
    case View::AppOrder:
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
  if (self.ordersShelf()) {
    const int row = self.nav.selected;
    if ((event.value != -1 && event.value != 1) || row + event.value < 0 || row + event.value >= self.listCount())
      return;
    RenderLock lock(self);
    self.app.clearTapFlash();
    self.notice = shelf::moveOrderedItem(self.orderFolder(), row, event.value) ? StrId::STR_NONE_OPT
                                                                               : StrId::STR_HOME_MENU_SAVE_FAILED;
    if (self.notice == StrId::STR_NONE_OPT) self.moveSelectionTo(row + event.value);
    self.requestUpdate();
    return;
  }
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
    switch (index) {
      case 0:
        show(View::Names);
        break;
      case 1:
        show(View::Visibility);
        break;
      case 2:
        show(View::Order);
        break;
      case 3:
        show(View::GameOrder);
        break;
      case 4:
        show(View::AppOrder);
        break;
    }
    return;
  }
  if (view == View::Order || ordersShelf()) {
    requestUpdate();
    return;
  }
  if (view == View::ChooseGame || view == View::ChooseApp || view == View::ChoosePlugin) {
    const bool plugin = view == View::ChoosePlugin;
    const int folder = plugin ? homemenu::kPluginFolder : pickerFolder();
    const auto* title = plugin ? pluginChoices[index].name.c_str() : shelf::folders()[folder].items[index].title;
    const auto existing = homemenu::findShortcut(draft, folder, title);
    if (existing != homemenu::Id::Count) {
      homemenu::removeShortcut(draft, existing);
      save();
    } else if (homemenu::addShortcut(draft, folder, title) == homemenu::Id::Count) {
      notice = StrId::STR_HOME_MENU_LIMIT;
    } else {
      const auto added = homemenu::findShortcut(draft, folder, title);
      if (plugin) {
        const auto& label = pluginChoices[index].title;
        if (label.size() < homemenu::kLabelBytes) homemenu::setLabel(draft, added, label.c_str());
      }
      if (!save()) return;
      show(View::Visibility);
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
    show(index == menuRows() ? View::ChooseGame : index == menuRows() + 1 ? View::ChooseApp : View::ChoosePlugin);
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
    switch (index) {
      case 0:
        row.label = tr(STR_HOME_MENU_RENAME);
        break;
      case 1:
        row.label = tr(STR_HOME_MENU_VISIBILITY);
        break;
      case 2:
        row.label = tr(STR_HOME_MENU_ORDER);
        break;
      case 3:
        row.label = tr(STR_SHELF_ORDER_GAMES);
        break;
      case 4:
        row.label = tr(STR_SHELF_ORDER_APPS);
        break;
    }
    return;
  }
  if (self.ordersShelf()) {
    const int folder = self.orderFolder();
    const int item = shelf::orderedItem(folder, index);
    if (item < 0) return;
    const auto& entry = shelf::folders()[folder].items[item];
    row.label = shelf::label(entry.title);
    row.icon = freeink::ui::bitmapFromIcon(*entry.icon);
    return;
  }
  if (self.view == View::ChoosePlugin) {
    const auto& plugin = self.pluginChoices[index];
    row.label = plugin.title.c_str();
    row.toggleChecked =
        homemenu::findShortcut(self.draft, homemenu::kPluginFolder, plugin.name.c_str()) != homemenu::Id::Count;
    row.icon = freeink::ui::bitmapFromIcon(row.toggleChecked ? extra_checked_32 : extra_unchecked_32);
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
    row.label = index == self.menuRows()       ? tr(STR_HOME_ADD_GAME)
                : index == self.menuRows() + 1 ? tr(STR_HOME_ADD_APP)
                                               : tr(STR_HOME_ADD_PLUGIN);
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
  if (view == View::Order || ordersShelf()) {
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
