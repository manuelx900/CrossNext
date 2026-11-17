#pragma once
#include <BleKeyBinding.h>

#include "RemoteShortcutChoices.h"

namespace remotebindings {
using Action = bleturner::Action;
struct Direct {
  uint8_t host;
  Action action;
};
inline constexpr Direct DIRECT[] = {{uint8_t(SideButtonAction::Navigation), Action::Advance},
                                    {uint8_t(remoteshortcuts::Extra::Previous), Action::Previous},
                                    {uint8_t(SideButtonAction::Confirm), Action::Confirm},
                                    {uint8_t(remoteshortcuts::Extra::Back), Action::Back},
                                    {uint8_t(SideButtonAction::NextPage), Action::NextPage},
                                    {uint8_t(SideButtonAction::Bookmark), Action::Bookmark},
                                    {uint8_t(SideButtonAction::ReaderMenu), Action::ReaderMenu},
                                    {uint8_t(SideButtonAction::Sync), Action::Sync},
                                    {uint8_t(SideButtonAction::ToggleFrontlight), Action::ToggleFrontlight},
                                    {uint8_t(SideButtonAction::ToggleWarmth), Action::ToggleWarmth},
                                    {uint8_t(SideButtonAction::Refresh), Action::Refresh},
                                    {uint8_t(SideButtonAction::Home), Action::Home},
                                    {uint8_t(remoteshortcuts::Extra::Library), Action::Library},
                                    {uint8_t(remoteshortcuts::Extra::Files), Action::Files},
                                    {uint8_t(remoteshortcuts::Extra::Settings), Action::Settings},
                                    {uint8_t(remoteshortcuts::Extra::BrightnessUp), Action::BrightnessUp},
                                    {uint8_t(remoteshortcuts::Extra::BrightnessDown), Action::BrightnessDown},
                                    {uint8_t(SideButtonAction::Sleep), Action::Sleep},
                                    {uint8_t(SideButtonAction::ToggleTouchscreen), Action::Touch},
                                    {uint8_t(SideButtonAction::Ignore), Action::Disabled}};
inline constexpr uint8_t hostAction(Action action) {
  for (const auto& row : DIRECT)
    if (row.action == action) return row.host;
  return bleturner::kShortcutUnset;
}
inline constexpr Action directAction(uint8_t host) {
  for (const auto& row : DIRECT)
    if (row.host == host) return row.action;
  return Action::None;
}
// Unlike action-first learning, button-first editing preserves other buttons with the same action.
inline bool assign(bleturner::RemoteTable& table, uint32_t code, bool hold, Action action) {
  if (!bleturner::valid(bleturner::makeBinding(code, hold, action))) return false;
  int slot = -1;
  for (uint8_t i = 0; i < table.count; ++i)
    if (bleturner::matches(table.bindings[i], code, hold)) {
      slot = i;
      break;
    }
  if (slot < 0) {
    if (table.count == bleturner::kMaxBindings) return false;
    slot = table.count++;
  }
  table.bindings[slot] = bleturner::makeBinding(code, hold, action);
  return true;
}
inline bool remove(bleturner::RemoteTable& table, uint32_t code, bool hold) {
  uint8_t kept = 0;
  for (uint8_t i = 0; i < table.count; ++i)
    if (!bleturner::matches(table.bindings[i], code, hold)) table.bindings[kept++] = table.bindings[i];
  const bool changed = kept != table.count;
  table.count = kept;
  return changed;
}
inline bool restoreDefault(bleturner::RemoteTable& table, const char* name, uint32_t code, bool hold) {
  const auto* defaults = bleturner::defaultTableFor(name);
  uint8_t removed = 0, added = 0;
  for (uint8_t i = 0; i < table.count; ++i)
    if (bleturner::matches(table.bindings[i], code, hold)) ++removed;
  if (defaults)
    for (uint8_t i = 0; i < defaults->count; ++i)
      if (bleturner::matches(defaults->bindings[i], code, hold)) ++added;
  if (table.count - removed + added > bleturner::kMaxBindings) return false;
  uint8_t kept = 0;
  for (uint8_t i = 0; i < table.count; ++i)
    if (!bleturner::matches(table.bindings[i], code, hold)) table.bindings[kept++] = table.bindings[i];
  if (defaults)
    for (uint8_t i = 0; i < defaults->count; ++i)
      if (bleturner::matches(defaults->bindings[i], code, hold)) table.bindings[kept++] = defaults->bindings[i];
  table.count = kept;
  return true;
}
inline int appSlot(const bleturner::RemoteTable& table, uint32_t code, bool hold, const char* title) {
  bool used[2]{};
  for (uint8_t i = 0; i < table.count; ++i) {
    if (bleturner::matches(table.bindings[i], code, hold)) continue;
    const auto action = bleturner::actionOf(table.bindings[i]);
    if (action == Action::Shortcut1) used[0] = true;
    if (action == Action::Shortcut2) used[1] = true;
  }
  for (int i = 0; i < 2; ++i)
    if (table.shortcuts[i].action == uint8_t(SideButtonAction::OpenShortcut) &&
        !std::strcmp(table.shortcuts[i].title, title))
      return i;
  for (int i = 0; i < 2; ++i)
    if (!used[i]) return i;
  return -1;
}
inline const remoteshortcuts::Choice* choice(const bleturner::RemoteTable& table, Action action) {
  if (action == Action::Shortcut1 || action == Action::Shortcut2)
    return remoteshortcuts::choice(table.shortcuts[action == Action::Shortcut1 ? 0 : 1].action);
  return remoteshortcuts::choice(hostAction(action));
}
static_assert(sizeof(bleturner::Binding) == 4);
static_assert(uint8_t(Action::Disabled) < 64);
}  // namespace remotebindings
