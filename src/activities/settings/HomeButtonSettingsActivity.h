#pragma once

#include "HomeButtonSettings.h"
#include "activities/UiListActivity.h"

class HomeButtonSettingsActivity final : public UiListActivity {
 public:
  HomeButtonSettingsActivity(GfxRenderer& renderer, MappedInputManager& input)
      : UiListActivity("HomeButtonSettings", renderer, input, false, true) {}

 private:
  enum class Page : uint8_t { Gestures, Actions, Apps };
  static constexpr int GESTURE_COUNT = 3;
  static constexpr int ACTION_COUNT = int(HomeButtonAction::OpenShortcut) + 1;
  freeink::ui::ListItem rows[ACTION_COUNT]{};
  Page page = Page::Gestures;
  int gesture = 0;
  bool saveFailed = false;

  int listCount() const override;
  const char* headerTitle() const override;
  void onBackButton() override;
  void show(Page next, int selected = 0);
  bool saveAction(HomeButtonAction action, const char* target = nullptr);
  static void shortcutRow(void*, uint16_t, freeink::ui::ListItem&);
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
};
