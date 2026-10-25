#include "HomeMenuIntegration.h"

#include <I18n.h>
#include <Logging.h>

#include <cstring>

#include "../../activities/Activity.h"
#include "../../activities/ActivityManager.h"
#include "../Shelf.h"

namespace homemenu {
const shelf::Item* shortcutItem(Id id) {
  const auto* saved = shortcut(config(), id);
  if (!saved || saved->folder >= shelf::folderCount()) return nullptr;
  const auto& folder = shelf::folders()[saved->folder];
  for (int n = 0; n < folder.count; ++n)
    if (!std::strcmp(folder.items[n].title, saved->title)) return &folder.items[n];
  return nullptr;
}
const char* label(Id id, bool plugins) {
  if (id == Id::Count) return "";
  const auto* custom = config().labels[static_cast<int>(id)];
  if (*custom) return custom;
  if (const auto* entry = shortcut(config(), id)) return shelf::label(entry->title);
  switch (id) {
    case Id::Files:
      return tr(STR_BROWSE_FILES);
    case Id::Recent:
      return tr(STR_MENU_RECENT_BOOKS);
    case Id::Stats:
      return tr(STR_READING_STATS_TITLE);
    case Id::Settings:
      return tr(STR_SETTINGS_TITLE);
    case Id::Games:
      return tr(STR_HOME_GAMES);
    case Id::Apps:
      return tr(STR_HOME_APPS);
    case Id::Transfer:
      return tr(STR_FILE_TRANSFER);
    case Id::Services:
      return plugins ? tr(STR_PLUGINS) : tr(STR_OPDS_BROWSER);
    case Id::Count:
    case Id::Shortcut0:
    case Id::Shortcut1:
    case Id::Shortcut2:
    case Id::Shortcut3:
    case Id::Shortcut4:
      break;
  }
  return "";
}
homeui::Icon icon(Id id) {
  if (const auto* entry = shortcut(config(), id)) return entry->folder == 0 ? homeui::Icon::Games : homeui::Icon::Apps;
  switch (id) {
    case Id::Files:
      return homeui::Icon::Files;
    case Id::Recent:
      return homeui::Icon::Library;
    case Id::Stats:
      return homeui::Icon::Stats;
    case Id::Settings:
      return homeui::Icon::Settings;
    case Id::Games:
      return homeui::Icon::Games;
    case Id::Apps:
      return homeui::Icon::Apps;
    case Id::Transfer:
      return homeui::Icon::Transfer;
    case Id::Services:
      return homeui::Icon::Plugins;
    case Id::Count:
    case Id::Shortcut0:
    case Id::Shortcut1:
    case Id::Shortcut2:
    case Id::Shortcut3:
    case Id::Shortcut4:
      break;
  }
  return homeui::Icon::Files;
}
HomeMenuItem item(Id id) {
  switch (id) {
    case Id::Files:
      return HomeMenuItem::FILE_BROWSER;
    case Id::Recent:
      return HomeMenuItem::LIBRARY;
    case Id::Stats:
      return HomeMenuItem::READING_STATS;
    case Id::Settings:
      return HomeMenuItem::SETTINGS_MENU;
    case Id::Games:
      return HomeMenuItem::GAMES;
    case Id::Apps:
      return HomeMenuItem::APPS;
    case Id::Transfer:
      return HomeMenuItem::FILE_TRANSFER;
    case Id::Services:
      return HomeMenuItem::OPDS_BROWSER;
    case Id::Count:
    case Id::Shortcut0:
    case Id::Shortcut1:
    case Id::Shortcut2:
    case Id::Shortcut3:
    case Id::Shortcut4:
      return HomeMenuItem::NONE;
  }
  return HomeMenuItem::NONE;
}
Id idFor(HomeMenuItem value) {
  switch (value) {
    case HomeMenuItem::FILE_BROWSER:
      return Id::Files;
    case HomeMenuItem::LIBRARY:
      return Id::Recent;
    case HomeMenuItem::READING_STATS:
      return Id::Stats;
    case HomeMenuItem::SETTINGS_MENU:
      return Id::Settings;
    case HomeMenuItem::GAMES:
      return Id::Games;
    case HomeMenuItem::APPS:
      return Id::Apps;
    case HomeMenuItem::FILE_TRANSFER:
      return Id::Transfer;
    case HomeMenuItem::OPDS_BROWSER:
      return Id::Services;
    case HomeMenuItem::NONE:
      return Id::Count;
  }
  return Id::Count;
}
int count(bool services) { return visibleCount(config(), services); }
bool launchShortcut(Id id, GfxRenderer& renderer, MappedInputManager& input) {
  const auto* entry = shortcut(config(), id);
  if (!entry || !*entry->title || entry->folder >= shelf::folderCount()) return false;
  const auto& folder = shelf::folders()[entry->folder];
  for (int n = 0; n < folder.count; ++n)
    if (!std::strcmp(folder.items[n].title, entry->title))
      return shelf::openItem(entry->folder, n, renderer, input, shelf::LaunchOrigin::Home);
  LOG_ERR("HOME-MENU", "Shortcut unavailable: %s", entry->title);
  return false;
}
}  // namespace homemenu
