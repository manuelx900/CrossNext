#pragma once
#include <I18n.h>

#include "../../activities/UiListActivity.h"
#include "../ui/PixelMenu.h"
#include "HomeMenuIntegration.h"
#include "HomeMenuOrderUi.h"

class HomeMenuSettingsActivity final : public UiListActivity {
 public:
  HomeMenuSettingsActivity(GfxRenderer& renderer, MappedInputManager& input)
      : UiListActivity("HomeMenuSettings", renderer, input) {}
  void onEnter() override;

 protected:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void drawFooter() override;

 private:
  enum class View { Root, Names, Visibility, ChooseGame, ChooseApp, Order };
  View view = View::Root;
  homemenu::Config draft;
  pixelui::RowSource rowText;
  freeink::ui::ListProps props;
  // Large SDK descriptors live with the activity, not on the render stack.
  homemenuui::OrderWork orderWork;
  StrId notice = StrId::STR_NONE_OPT;
  bool services = false, plugins = false;
  void show(View next);
  bool save();
  int menuRows() const;
  homemenu::Id idAtRow(int index) const;
  int pickerFolder() const { return view == View::ChooseGame ? 0 : 1; }
  static void provideRow(void* context, uint16_t index, freeink::ui::ListItem& row);
  static void moveAction(const freeink::ui::ActionEvent& event, void* context);
};
