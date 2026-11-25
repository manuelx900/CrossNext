#pragma once

#include "activities/UiListActivity.h"

namespace bookorbit {

class SettingsActivity final : public UiListActivity {
 public:
  SettingsActivity(GfxRenderer& renderer, MappedInputManager& input);

 private:
  enum class Row {
    Server,
    Username,
    Password,
    Authenticate,
    Catalog,
    DownloadFolder,
    SyncBehavior,
    SyncOnSleep,
    SyncOnWake,
    SyncNow,
    Count
  };
  static constexpr int ROW_COUNT = static_cast<int>(Row::Count);
  freeink::ui::ListItem rows[ROW_COUNT]{};
  std::string details[3];
  bool invalidFolder = false;

  int listCount() const override { return ROW_COUNT; }
  const char* headerTitle() const override;
  void activateIndex(int index) override;
  void buildScreen(UiScreen& screen) override;
};

}  // namespace bookorbit
