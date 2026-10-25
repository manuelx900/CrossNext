#pragma once
#include "../../activities/UiListActivity.h"
#include "../ui/QuickSettingsScreen.h"
class QuickSettingsConfigActivity final : public UiListActivity {
 public:
  QuickSettingsConfigActivity(GfxRenderer& renderer, MappedInputManager& input)
      : UiListActivity("QuickSettingsConfig", renderer, input) {}

 protected:
  int listCount() const override { return slot < 0 ? 4 : int(quickui::Shortcut::Count); }
  const char* headerTitle() const override { return tr(STR_QUICK_CUSTOMISE); }
  void buildScreen(UiScreen&) override;
  void drawChrome() override;
  void drawFooter() override {}
  void activateIndex(int) override;
  void onBackButton() override;

 private:
  int slot = -1;
  bool saveFailed = false;
  freeink::ui::ListItem rows[int(quickui::Shortcut::Count)]{};
  freeink::ui::ListProps props;
  char names[4][48]{};
};
