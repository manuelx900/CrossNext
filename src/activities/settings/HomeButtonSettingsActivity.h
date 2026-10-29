#pragma once

#include "HomeButtonSettings.h"
#include "SideButtonSettings.h"
#include "activities/UiListActivity.h"

class HomeButtonSettingsActivity final : public UiListActivity {
 public:
  enum class Binding : uint8_t { Home, SideLeft, SideRight };
  HomeButtonSettingsActivity(GfxRenderer& renderer, MappedInputManager& input, Binding binding = Binding::Home)
      : UiListActivity("HomeButtonSettings", renderer, input, false, true),
        binding(binding),
        page(binding == Binding::Home ? Page::Gestures : Page::Actions) {}
  void onEnter() override;

 private:
  enum class Page : uint8_t { Gestures, Actions, Apps, Timing };
  static constexpr int GESTURE_COUNT = 3;
  static constexpr int HOME_ACTION_COUNT = int(HomeButtonAction::OpenShortcut) + 1;
  static constexpr int ROW_COUNT = int(SideButtonAction::Count) > HOME_ACTION_COUNT ? int(SideButtonAction::Count)
                                                                                    : HOME_ACTION_COUNT;
  freeink::ui::ListItem rows[ROW_COUNT]{};
  Binding binding;
  Page page;
  int gesture = 0;
  bool saveFailed = false;

  int listCount() const override;
  const char* headerTitle() const override;
  void onBackButton() override;
  void show(Page next, int selected = 0);
  uint8_t CrossPointSettings::* actionField() const;
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
