#pragma once
#include <string>

#include "activities/Activity.h"
#include "apps_local/reading/ReadingGoal.h"

class Bitmap;
class HalFile;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;

 private:
  // De-ghost the panel before painting a sleep image: HALF/FULL are both
  // single-shot absolute paints with no flush, so a mostly-black inverted sleep
  // image ghosts the prior high-contrast screen (a reader page) through. One
  // paint to white resets the baseline so the image paint lands clean (#2588).
  void flushPanelWhite() const;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap, bool preserveBackground = false) const;
  bool renderSleepOverlayFile(HalFile& file, const char* pathForLog) const;
  bool renderTransparentOverlayPng(const std::string& path) const;
  bool renderSleepOverlayPath(const std::string& path) const;
  void renderLastScreenSleepScreen() const;
  void renderTransparentCustomSleepScreen() const;
  void renderBlankSleepScreen() const;
  void prepareGoalOverlay();
  void drawGoalOverlay() const;
  readinggoal::WallpaperData goalOverlay;

  bool fromTimeout = false;
};
