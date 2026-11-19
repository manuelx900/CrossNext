#pragma once
#include "../../activities/UiListActivity.h"
#include "../../components/OptionPopup.h"
#include "../ui/PixelMenu.h"
class PresentationSettingsActivity final : public UiListActivity {
 public:
  enum class Kind { Theme, ReadingFormats, FontOverview, GameFonts, AppFonts, ThemeOverview, CoverLoading };
  PresentationSettingsActivity(GfxRenderer& r, MappedInputManager& i, Kind k)
      : UiListActivity("PresentationSettings", r, i), kind(k) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  Kind kind;
  enum class PendingPicker : uint8_t { None, Theme, Header };
  PendingPicker pendingPicker = PendingPicker::None;
  OptionPopup optionPopup;
  bool saveFailed = false;
  bool buttonFocus = false;
  bool showingCoverHelp = false;
  void navigateButtons() override;
  freeink::ui::ListItem items[menufont::kSectionCount]{};
  char labels[menufont::kSectionCount][64]{};
  bool contentFonts() const { return kind == Kind::GameFonts || kind == Kind::AppFonts; }
  int fontFolder() const { return kind == Kind::GameFonts ? 0 : 1; }
  menufont::Section fontSection() const {
    return kind == Kind::GameFonts ? menufont::Section::Games : menufont::Section::Apps;
  }
  uint8_t& fontChoice(int row);
  uint8_t& themeChoice(int row);
  void showThemePicker(int row);
  void showHeaderPicker();
  const char* fontTargetName(int row) const;
  static void fontRow(void*, uint16_t, freeink::ui::ListItem&);
  static void themeRow(void*, uint16_t, freeink::ui::ListItem&);
  int listCount() const override;
  const char* screenTitle() const;
  void buildScreen(UiScreen&) override;
  void activateIndex(int) override;
  void onBackButton() override;
};
