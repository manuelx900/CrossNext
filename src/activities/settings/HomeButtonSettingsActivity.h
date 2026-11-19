#pragma once

#include "HomeButtonSettings.h"
#include "SideButtonSettings.h"
#include "activities/UiListActivity.h"
#include "apps_local/ui/RemoteShortcutChoices.h"

class HomeButtonSettingsActivity final : public UiListActivity {
 public:
  enum class Binding : uint8_t { Home, SideLeft, SideRight, Remote1, Remote2 };
  HomeButtonSettingsActivity(GfxRenderer& renderer, MappedInputManager& input, Binding binding = Binding::Home,
                             bleturner::RemoteTable* remote = nullptr)
      : UiListActivity("HomeButtonSettings", renderer, input, false, true),
        binding(binding),
        page(binding == Binding::Home ? Page::Gestures : Page::Actions),
        remote(remote) {}
  void onEnter() override;

 private:
  enum class Page : uint8_t { Gestures, Actions, Apps, Timing };
  static constexpr int GESTURE_COUNT = 2;
  static constexpr int HOME_ACTION_COUNT = int(HomeButtonAction::OpenShortcut) + 1;
  static constexpr int ROW_COUNT = remoteshortcuts::COUNT > int(SideButtonAction::Count) ? remoteshortcuts::COUNT
                                                                                         : int(SideButtonAction::Count);
  static_assert(ROW_COUNT >= int(HomeButtonInput::TAP_OPTION_COUNT));
  freeink::ui::ListItem rows[ROW_COUNT]{};
  Binding binding;
  Page page;
  bleturner::RemoteTable* remote;
  int gesture = 1;
  bool saveFailed = false;

  int listCount() const override;
  const char* headerTitle() const override;
  void onBackButton() override;
  void show(Page next, int selected = 0);
  uint8_t& storedAction() const;
  char* shortcutTarget() const;
  bool isShortcut() const;
  int shortcutIndex() const;
  int actionAt(int row) const;
  int actionRow(int action) const;
  bool saveAction(uint8_t action, const char* target = nullptr);
  static void shortcutRow(void*, uint16_t, freeink::ui::ListItem&);
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
};
