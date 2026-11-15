#pragma once

#include "activities/UiListActivity.h"
#include "apps_local/readerbars/ReaderBars.h"
#include "components/OptionPopup.h"

class StatusBarSettingsActivity final : public UiListActivity {
 public:
  explicit StatusBarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  enum class View { Overview, Top, Bottom };
  static constexpr int ROW_COUNT = 10;
  View view = View::Overview;
  OptionPopup optionPopup;
  readerbars::Workspace previewWorkspace;
  char values[ROW_COUNT][48]{};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  int listCount() const override { return view == View::Overview ? 4 : ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  void onBackButton() override;
  readerbars::Position position() const;
  void save(const readerbars::Config& config);
};
