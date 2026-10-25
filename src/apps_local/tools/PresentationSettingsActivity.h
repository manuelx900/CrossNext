#pragma once
#include "../../activities/UiListActivity.h"
#include "../ui/PixelMenu.h"
class PresentationSettingsActivity final : public UiListActivity {
 public:
  enum class Kind { Theme, ReadingFormats };
  PresentationSettingsActivity(GfxRenderer& r, MappedInputManager& i, Kind k)
      : UiListActivity("PresentationSettings", r, i), kind(k) {}
  void loop() override;

 private:
  Kind kind;
  freeink::ui::ListItem items[6]{};
  char labels[6][64]{};
  int listCount() const override;
  void buildScreen(UiScreen&) override;
  void activateIndex(int) override;
  void onBackButton() override { finish(); }
};
