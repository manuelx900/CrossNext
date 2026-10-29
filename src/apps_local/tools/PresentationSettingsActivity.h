#pragma once
#include "../../activities/UiListActivity.h"
#include "../../components/OptionPopup.h"
#include "../ui/PixelMenu.h"
class PresentationSettingsActivity final : public UiListActivity {
 public:
  enum class Kind { Theme, ReadingFormats, FontOverview, GameFonts, AppFonts };
  PresentationSettingsActivity(GfxRenderer& r, MappedInputManager& i, Kind k)
      : UiListActivity("PresentationSettings", r, i), kind(k) {}

  void loop() override;
  void render(RenderLock&&) override;

 private:
  Kind kind;
  OptionPopup optionPopup;
  bool saveFailed = false;
  freeink::ui::ListItem items[menufont::kSectionCount]{};
  char labels[menufont::kSectionCount][64]{};
  bool contentFonts() const { return kind == Kind::GameFonts || kind == Kind::AppFonts; }
  int fontFolder() const { return kind == Kind::GameFonts ? 0 : 1; }
  menufont::Section fontSection() const {
    return kind == Kind::GameFonts ? menufont::Section::Games : menufont::Section::Apps;
  }
  uint8_t& fontChoice(int row);
  const char* fontTargetName(int row) const;
  static void fontRow(void*, uint16_t, freeink::ui::ListItem&);
  int listCount() const override;
  const char* screenTitle() const;
  void buildScreen(UiScreen&) override;
  void activateIndex(int) override;
  void onBackButton() override { finish(); }
};
